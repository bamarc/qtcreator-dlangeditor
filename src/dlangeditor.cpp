#include "dlangeditor.h"

#include "dlangeditorconstants.h"
#include "dlangeditorutils.h"
#include "dlangindenter.h"
#include "dlangautocompleter.h"
#include "dlangcompletionassistprovider.h"
#include "dlangassistprocessor.h"
#include "dlanghoverhandler.h"
#include "codemodel/dmodel.h"
#include "dlanguseselectionupdater.h"
#include "dlangoptionspage.h"
#include "dlangoutlinemodel.h"
#include "dlangoutline.h"

#include <texteditor/texteditorsettings.h>
#include <utils/uncommentselection.h>

#include <coreplugin/coreconstants.h>
#include <utils/mimetypes/mimedatabase.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/icontext.h>
#include <texteditor/texteditoractionhandler.h>
#include <texteditor/texteditorconstants.h>
#include <texteditor/highlighterutils.h>
#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/project.h>

#include <QTextBlock>
#include <QElapsedTimer>
#include <QDebug>
#include <QTimer>

using namespace DlangEditor;

using namespace Core;
//using MimeDatabase = ::Utils::MimeDatabase;

inline bool isFullIdentifierChar(const QChar& c)
{
    return !c.isNull() && (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.'));
}

bool DlangEditor::getFullIdentifier(const TextEditor::TextDocument *doc, int pos, int &begin, int &size)
{
    QChar c;
    begin = pos;
    do {
        c = doc->characterAt(begin--);
    } while (isFullIdentifierChar(c));
    begin += 2;
    if (begin == pos) {
        return false;
    }
    int end = pos + 1;
    do {
        c = doc->characterAt(end++);
    } while (isFullIdentifierChar(c));

    size = end - begin - 1;
    return size != 0;
}

DlangTextEditor::DlangTextEditor() :
    TextEditor::BaseTextEditor()
{
    addContext(DlangEditor::Constants::DLANG_EDITOR_CONTEXT_ID);
    setDuplicateSupported(true);
}

QString DlangTextEditor::contextHelpId() const
{
    int pos = position();
    const TextEditor::TextDocument* doc = const_cast<DlangTextEditor*>(this)->textDocument();
    int begin, size;
    return getFullIdentifier(doc, pos, begin, size) ? QLatin1String("D/") + doc->textAt(begin, size)
                                                    : QString();
}

DlangTextEditorWidget::DlangTextEditorWidget(QWidget *parent)
    : TextEditor::TextEditorWidget(parent), m_useSelectionsUpdater(0)
{
    setParenthesesMatchingEnabled(true);
    setCodeFoldingSupported(true);

    m_useSelectionsUpdater = new DlangUseSelectionUpdater(this);
    try {
        m_codeModel = DCodeModel::ModelManager::instance().getCurrentModel();
    } catch (...) {
        m_codeModel.reset();
    }

    m_ddocCompleter = new DdocAutoCompleter;
    m_outlineModel = new DlangOutlineModel(this);

    m_documentUpdater = new QTimer(this);
    m_documentUpdater->setInterval(1000);
    m_documentUpdater->setSingleShot(true);
}

DlangTextEditorWidget::~DlangTextEditorWidget()
{
    delete m_useSelectionsUpdater;
    m_useSelectionsUpdater = 0;
}

void DlangTextEditorWidget::finalizeInitialization()
{
    insertExtraToolBarWidget(TextEditor::TextEditorWidget::Left, new DlangTextEditorOutline(this));
    connect(this->document(), &QTextDocument::contentsChanged, [this]() {
        m_documentUpdater->stop();
        m_documentUpdater->start();
    });
    connect(m_documentUpdater, SIGNAL(timeout()), this, SIGNAL(documentUpdated()));
    connect(this, SIGNAL(documentUpdated()), m_outlineModel, SLOT(updateForCurrentEditor()));
    // set up the use highlighting
    // Currently not implemented in DCD
    /*connect(this, SIGNAL(cursorPositionChanged()),
            m_useSelectionsUpdater, SLOT(scheduleUpdate()));*/
}

DlangOutlineModel *DlangTextEditorWidget::outline() const
{
    return m_outlineModel;
}

void DlangTextEditorWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        QTextCursor cursor = textCursor();
        QTextCursor tmp = cursor;
        tmp.movePosition(QTextCursor::StartOfBlock);
        auto state = DdocAutoCompleter::isDdocComment(cursor);
        if (state != DdocAutoCompleter::DDOC_OUT) {
            QString ddocCompletion = m_ddocCompleter->insertParagraphSeparator(state);
            cursor.insertText(ddocCompletion);
            cursor.setPosition(tmp.position(), QTextCursor::KeepAnchor);
            textDocument()->autoIndent(cursor);
            tmp.movePosition(QTextCursor::NextBlock);
            tmp.movePosition(QTextCursor::EndOfBlock);
            setTextCursor(tmp);
            return;
        }
    }
    return TextEditorWidget::keyPressEvent(e);
}

void DlangTextEditorWidget::contextMenuEvent(QContextMenuEvent *e)
{
    showDefaultContextMenu(e, DlangEditor::Constants::DLANG_EDITOR_CONTEXT_MENU);
}

TextEditor::TextEditorWidget::Link DlangTextEditorWidget::findLinkAt(
        const QTextCursor &c, bool resolveTarget, bool inNextSplit)
{
    if (!resolveTarget) {
        return Link();
    }
    Q_UNUSED(inNextSplit);

    DCodeModel::Symbol symbol;
    try {
        DCodeModel::Sources sources(this->textDocument()->filePath().toString(),
                                    this->document()->toPlainText(),
                                    this->document()->revision());
        m_codeModel->findSymbolLocation(Utils::currentProjectName(),
                                        sources,
                                        c.position() + 1, symbol);
    }
    catch (...) {
        qWarning() << "failed to find symbol location";
        return Link();
    }
    if (symbol.location.isNull()) {
        return Link();
    }

    if (symbol.location.filename == "stdin") {
        symbol.location.filename = textDocument()->filePath().toString();
    }

    QFile f(symbol.location.filename);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return Link();
    }
    int lines = 0;
    int linePos = 0;
    for (int i = 0; i < symbol.location.position; ) {
        char buff[1024];
        qint64 b = f.read(buff, std::min(1024, symbol.location.position - i));
        if (!b) break;
        for (int j = 0; j < b; ++j) {
            if (buff[j] == '\n') {
                ++lines;
                linePos = i + j;
            }
        }
        i += b;
    }

    return Link(symbol.location.filename , lines + 1, symbol.location.position - linePos - 1);
}


DlangEditorFactory::DlangEditorFactory()
    : TextEditor::TextEditorFactory()
{

    setId(DlangEditor::Constants::DLANG_EDITOR_ID);
    setDisplayName(tr(DlangEditor::Constants::DLANG_EDITOR_DISPLAY_NAME));
    addMimeType(DlangEditor::Constants::DLANG_MIMETYPE);

    setDocumentCreator([]() { return new DlangDocument; });
    setEditorWidgetCreator([]() { return new DlangTextEditorWidget; });
    setEditorCreator([]() { return new DlangTextEditor; });

    setIndenterCreator([]() { return new DlangIndenter; });
    setAutoCompleterCreator([]() { return new DlangAutoCompleter; });

    setUseGenericHighlighter(true);

    setCompletionAssistProvider(new DlangCompletionAssistProvider);

    addHoverHandler(new DlangHoverHandler);

    //setCommentStyle(::Utils::CommentDefinition::CppStyle);

    setParenthesesMatchingEnabled(true);
    setMarksVisible(true);
    setCodeFoldingSupported(true);

    /*new TextEditor::TextEditorActionHandler(this, DlangEditor::Constants::DLANG_EDITOR_CONTEXT_ID,
            TextEditor::TextEditorActionHandler::UnCommentSelection
            | TextEditor::TextEditorActionHandler::Format
            | TextEditor::TextEditorActionHandler::FollowSymbolUnderCursor);
    */
    Core::ActionContainer *contextMenu =
            Core::ActionManager::createMenu(DlangEditor::Constants::DLANG_EDITOR_CONTEXT_MENU);
    Core::Command *cmd;
    Core::Context dlangEditorContext = Core::Context(DlangEditor::Constants::DLANG_EDITOR_CONTEXT_ID);

    cmd = Core::ActionManager::command(TextEditor::Constants::FOLLOW_SYMBOL_UNDER_CURSOR);
    contextMenu->addAction(cmd);

    contextMenu->addSeparator(dlangEditorContext);

    cmd = Core::ActionManager::command(TextEditor::Constants::AUTO_INDENT_SELECTION);
    contextMenu->addAction(cmd);

    cmd = Core::ActionManager::command(TextEditor::Constants::UN_COMMENT_SELECTION);
    contextMenu->addAction(cmd);
}


DlangDocument::DlangDocument()
    : TextEditor::TextDocument()
{
    setId(Constants::DLANG_EDITOR_ID);
    setMimeType(DlangEditor::Constants::DLANG_MIMETYPE);
    setIndenter(new DlangIndenter);
}
