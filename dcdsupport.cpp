#include "dcdsupport.h"

#include "dlangdebughelper.h"
#include <dlangoptionspage.h>

#include <stdexcept>

#include <QProcess>
#include <QTextStream>
#include <QDebug>

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/project.h>
#include <cpptools/cppmodelmanager.h>

using namespace Dcd;

DcdClient::DcdClient(const QString &projectName, const QString &processName, int port, QObject *parent)
    : QObject(parent), m_projectName(projectName), m_port(port), m_processName(processName)
{
    m_portArguments << QLatin1String("--port") << QString::number(port);
}

const QString &DcdClient::projectName() const
{
    return m_projectName;
}

void DcdClient::setOutputFile(const QString &filePath)
{
    m_filePath = filePath;
}

void startProcess(QProcess &p, const QString &processName, const QStringList &args,
                  const QString &filePath, QIODevice::OpenMode mode = QIODevice::ReadWrite)
{
    if (p.state() != QProcess::NotRunning) {
        throw std::runtime_error("process is already running");
    }
    if (!filePath.isEmpty()) {
        p.setStandardOutputFile(filePath, QIODevice::Append | QIODevice::Unbuffered);
        p.setStandardErrorFile(filePath, QIODevice::Append | QIODevice::Unbuffered);
    }
    qDebug() << processName << " process " << args;
    p.start(processName, args, mode);
    if (!p.waitForStarted(1000)) {
        throw std::runtime_error("process start timeout");
    }
}

void waitForFinished(QProcess &p)
{
    if (!p.waitForFinished(1000)) {
        throw std::runtime_error("process finish timeout");
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        throw std::runtime_error(p.readAllStandardError().data());
    }
}

void DcdClient::complete(const QString &filePath, int position, CompletionList &result)
{
    DEBUG_GUARD("");
    QStringList args = m_portArguments;
    args << QLatin1String("-c") + QString::number(position) << filePath;
    qDebug() << "dcd-client process " << args;
    QProcess process;
    startProcess(process, m_processName, args, m_filePath);
    waitForFinished(process);
    QByteArray array(process.readAllStandardOutput());
    return parseOutput(array, result);
}

void DcdClient::completeFromArray(const QString &array, int position, DcdClient::CompletionList &result)
{
    DEBUG_GUARD(QString::number(position));
    QStringList args = m_portArguments;
    args << QLatin1String("-c") + QString::number(position);
    QProcess process;
    startProcess(process, m_processName, args, m_filePath);
    process.write(array.toLatin1());
    if (!process.waitForBytesWritten(5000)) {
        throw std::runtime_error("process writing data timeout");
    }
    process.closeWriteChannel();
    waitForFinished(process);
    QByteArray output(process.readAllStandardOutput());
    return parseOutput(output, result);
}

void DcdClient::appendIncludePath(const QString &includePath)
{
    DEBUG_GUARD("");
    QStringList args = m_portArguments;
    args << QLatin1String("-I") + includePath;
    qDebug() << "dcd-client process " << args;
    QProcess process;
    startProcess(process, m_processName, args, m_filePath);
    waitForFinished(process);
}

void DcdClient::findSymbolLocation(const QString &array, int position, DcdClient::Location &result)
{
    DEBUG_GUARD(QString::number(position));
    QStringList args = m_portArguments;
    args << QLatin1String("-c") + QString::number(position) << "-l";
    qDebug() << "dcd-client process " << args;
    QProcess process;
    startProcess(process, m_processName, args, m_filePath);
    process.write(array.toLatin1());
    if (!process.waitForBytesWritten(2000)) {
        throw std::runtime_error("process writing data timeout");
    }
    process.closeWriteChannel();
    waitForFinished(process);
    QString str(process.readAllStandardOutput());
    QStringList list = str.split('\t');
    result = list.size() == 2 ? Location(list.front(), list.back().toInt()) : Location(QString(), 0);
}

void DcdClient::getDocumentationComments(const QString &array, int position, QStringList &result)
{
    DEBUG_GUARD(QString::number(position));
    QStringList args = m_portArguments;
    args << QLatin1String("-c") + QString::number(position) << "-d";
    qDebug() << "dcd-client process " << args;
    QProcess process;
    startProcess(process, m_processName, args, m_filePath);
    process.write(array.toLatin1());
    if (!process.waitForBytesWritten(2000)) {
        throw std::runtime_error("process writing data timeout");
    }
    process.closeWriteChannel();
    waitForFinished(process);
    QString str(process.readAllStandardOutput());
    result = str.split('\n');
}

void DcdClient::parseOutput(const QByteArray &output, DcdClient::CompletionList &result)
{
    result.list.clear();
    result.type = DCD_BAD_TYPE;
    QTextStream stream(output);
    QString line = stream.readLine();
    if (line == QLatin1String("identifiers")) {
        return parseIdentifiers(stream, result);
    } else if (line == QLatin1String("calltips")) {
        return parseCalltips(stream, result);
    } else if (line.isEmpty()) {
        return;
    } else {
        throw std::runtime_error("unknown output type");
    }
}

void DcdClient::parseIdentifiers(QTextStream &stream, DcdClient::CompletionList &result)
{
    QString line;
    do {
           line = stream.readLine();
           if (line.isNull() || line.isEmpty()) break;
           QStringList tokens = line.split(QLatin1Char('\t'));
           if (tokens.size() != 2) {
               throw std::runtime_error("Failed to parse identifiers");
           }
           result.type = DCD_IDENTIFIER;
           result.list.push_back(DcdCompletion());
           result.list.back().data = tokens.front();
           result.list.back().type = DcdCompletion::fromString(tokens.back());
    } while (stream.status() == QTextStream::Ok);
}

void DcdClient::parseCalltips(QTextStream &stream, DcdClient::CompletionList &result)
{
    QString line;
    do {
           line = stream.readLine();
           if (line.isNull() || line.isEmpty()) break;
           result.type = DCD_CALLTIP;
           result.list.push_back(DcdCompletion());
           result.list.back().data = line;
           result.list.back().type = DcdCompletion::DCD_NO_TYPE;
    } while (stream.status() == QTextStream::Ok);
}


DcdCompletion::IdentifierType DcdCompletion::fromString(const QString &name)
{
    char c = name.at(0).toLatin1();
    switch (c) {
    case 'c': return DCD_CLASS;
    case 'i': return DCD_INTERFACE;
    case 's': return DCD_STRUCT;
    case 'u': return DCD_UNION;
    case 'v': return DCD_VAR;
    case 'm': return DCD_MEMBER_VAR;
    case 'k': return DCD_KEYWORD;
    case 'f': return DCD_FUNCTION;
    case 'g': return DCD_ENUM_NAME;
    case 'e': return DCD_ENUM_VAR;
    case 'P': return DCD_PACKAGE;
    case 'M': return DCD_MODULE;
    case 'a': return DCD_ARRAY;
    case 'A': return DCD_ASSOC_ARRAY;
    case 'l': return DCD_ALIAS;
    case 't': return DCD_TEMPLATE;
    case 'T': return DCD_MIXIN;
    default: return DCD_NO_TYPE;
    }
    return DCD_NO_TYPE;
}


DcdServer::DcdServer(const QString& projectName, const QString &processName, int port, QObject *parent)
    : QObject(parent), m_projectName(projectName), m_port(port), m_processName(processName)
{
    m_process = new QProcess(this);
    connect(m_process, SIGNAL(finished(int)), this, SLOT(onFinished(int)));
    connect(m_process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(onError(QProcess::ProcessError)));
}

DcdServer::~DcdServer()
{
    DEBUG_GUARD("");
    stop();
    m_process->waitForFinished(10000);
}

int DcdServer::port() const
{
    return m_port;
}

const QString &DcdServer::projectName() const
{
    return m_projectName;
}

void DcdServer::setOutputFile(const QString &filePath)
{
    m_filePath = filePath;
}

void DcdServer::start()
{
    startProcess(*m_process, m_processName, QStringList() << QLatin1String("--port") << QString::number(m_port), m_filePath);
}

void DcdServer::stop()
{
    m_process->kill();
}

bool DcdServer::isRunning() const
{
    return (m_process && m_process->state() == QProcess::Running);
}

void DcdServer::onFinished(int errorCode)
{
    qDebug() << "DCD server finished";
    if (errorCode != 0) {
        emit error(tr("DCD server process has been terminated with exit code %1").arg(errorCode));
        qWarning("DCD server: %s", static_cast<QProcess*>(sender())->readAllStandardError().data());
    }
}

void DcdServer::onError(QProcess::ProcessError error)
{
    qDebug() << "DCD server error";
    switch (error) {
    case QProcess::FailedToStart:
        emit this->error(tr("DCD server failed to start"));
        break;
    case QProcess::Crashed:
        emit this->error(tr("DCD server crashed"));
        break;
    case QProcess::Timedout:
        emit this->error(tr("DCD server starting timeout"));
        break;
    default:
        emit this->error(tr("DCD server unknown error"));
        break;
    }
    stop();
}

// Factory
DcdFactory::ClientPointer DcdFactory::client(const QString &projectName)
{
    try {
        MapString::iterator it = mapChannels.find(projectName);
        if (it == mapChannels.end()) {
            int port =  m_firstPort + currentPortOffset % (m_lastPort - m_firstPort + 1);
            ServerPointer server(new Dcd::DcdServer(projectName, DlangEditor::DlangOptionsPage::dcdServerExecutable(), port, this));
            server->setOutputFile(DlangEditor::DlangOptionsPage::dcdServerLogPath());
            server->start();
            connect(server.data(), SIGNAL(error(QString)), this, SLOT(onError(QString)));
            ClientPointer client(new Dcd::DcdClient(projectName, DlangEditor::DlangOptionsPage::dcdClientExecutable(), port, this));
            appendIncludePaths(client);

            it = mapChannels.insert(projectName, qMakePair(client, server));
            ++currentPortOffset;
        } else {
            if (!it.value().second->isRunning()) {
                it.value().second->stop();
                mapChannels.erase(it);
                return ClientPointer();
            }
        }
        return it.value().first;
    } catch (std::exception &ex) {
        qDebug("Client exception: %s", ex.what());
    } catch (...) {
        qDebug("Client exception: unknown");
    }
    return ClientPointer();
}

void DcdFactory::appendIncludePaths(ClientPointer client)
{
    // append default include paths from options page
    QStringList list = DlangEditor::DlangOptionsPage::includePaths();

    // append include paths from project settings
    CppTools::CppModelManager *modelmanager =
            CppTools::CppModelManager::instance();
    if (modelmanager) {
        ProjectExplorer::Project *currentProject = ProjectExplorer::ProjectExplorerPlugin::currentProject();
        if (currentProject) {
            CppTools::ProjectInfo pinfo = modelmanager->projectInfo(currentProject);
            if (pinfo.isValid()) {
                foreach (const CppTools::ProjectPart::HeaderPath &header, pinfo.headerPaths()) {
                    if (header.isValid()) {
                        list.push_back(header.path);
                    }
                }
            }
        }
    }
    list.removeDuplicates();

    foreach (const QString& l, list) {
        client->appendIncludePath(l);
    }
}

void DcdFactory::setPortRange(int first, int last)
{
    m_firstPort = first;
    m_lastPort = std::max(last, first);
}

QPair<int, int> DcdFactory::portRange() const
{
    return qMakePair(m_firstPort, m_lastPort);
}

DcdFactory *DcdFactory::instance()
{
    static DcdFactory inst(DlangEditor::DlangOptionsPage::portsRange());
    return &inst;
}

void DcdFactory::onError(QString error)
{
    qDebug("DcdFactory::onError: %s", error.toStdString().data());
    qWarning("DcdFactory::onError: %s", error.toStdString().data());
    Dcd::DcdServer *server = qobject_cast<Dcd::DcdServer*>(sender());
    server->stop();
    mapChannels.remove(server->projectName());
}

DcdFactory::DcdFactory(QPair<int, int> range)
    : currentPortOffset(0)
{
    setPortRange(range.first, range.second);
}
