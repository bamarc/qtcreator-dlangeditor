DEFINES += DLANGEDITOR_LIBRARY
TARGET = DlangEditor
TEMPLATE = lib

QT += core gui

# DlangEditor files

SOURCES += src/dlangeditorplugin.cpp \
    src/dlangeditor.cpp \
    src/dlangindenter.cpp \
    src/dlangautocompleter.cpp \
    src/dlangcompletionassistprovider.cpp \
    src/dlangassistprocessor.cpp \
    src/dlangoptionspage.cpp \
    src/dlangoutline.cpp \
    src/dlangoutlinemodel.cpp \
    src/dlangdebughelper.cpp \
    src/dlanghoverhandler.cpp \
    src/dlangimagecache.cpp \
    src/dlanguseselectionupdater.cpp \
    src/codemodel/dastedmodel.cpp \
    src/codemodel/dastedoptions.cpp \
    src/codemodel/dcdmodel.cpp \
    src/codemodel/dcdoptions.cpp \
    src/codemodel/dmodel.cpp \
    src/codemodel/serverprocess.cpp \
    src/locator/dlanglocatorcurrentdocumentfilter.cpp \
    src/locator/dlanglocator.cpp \
    src/dlangeditorutils.cpp \
    src/codemodel/dummymodel.cpp

HEADERS += src/dlangeditorplugin.h \
    src/dlangeditor_global.h \
    src/dlangeditorconstants.h \
    src/dlangeditor.h \
    src/dlangindenter.h \
    src/dlangautocompleter.h \
    src/dlangcompletionassistprovider.h \
    src/dlangassistprocessor.h \
    src/dlangoptionspage.h \
    src/dlangoutline.h \
    src/dlangoutlinemodel.h \
    src/dlangdebughelper.h \
    src/dlanghoverhandler.h \
    src/dlangimagecache.h \
    src/dlanguseselectionupdater.h \
    src/codemodel/dastedmessages.h \
    src/codemodel/dastedmodel.h \
    src/codemodel/dastedoptions.h \
    src/codemodel/dcdmodel.h \
    src/codemodel/dcdoptions.h \
    src/codemodel/dmodel.h \
    src/codemodel/dmodeloptions.h \
    src/codemodel/serverprocess.h \
    src/locator/dlanglocatorcurrentdocumentfilter.h \
    src/locator/dlanglocator.h \
    src/dlangeditorutils.h \
    src/codemodel/dummymodel.h \
    thirdparty/msgpack.h \
    thirdparty/msgpack.hpp

INCLUDEPATH += src \
    thirdparty/msgpack/include/

# Qt Creator linking



## set the QTC_SOURCE environment variable to override the setting here
QTCREATOR_SOURCES = "./qtcreator-src/qt-creator/"
isEmpty(QTCREATOR_SOURCES):QTCREATOR_SOURCES=/usr/src/qtcreator
!exists($$QTCREATOR_SOURCES):\
    error("Set variable QTC_SOURCE to the QtCreator's sources path (current path is \"$$QTC_SOURCE\")")

## set the QTC_BUILD environment variable to override the setting here
IDE_BUILD_TREE = "/home/mbalda-o/Qt/Tools/QtCreator/lib/qtcreator/"
isEmpty(IDE_BUILD_TREE):IDE_BUILD_TREE=/usr/lib/qtcreator
!exists($$IDE_BUILD_TREE): \
    error("Set variable QTC_BUILD to the QtCreator's libraries path (current path is \"$$QTC_BUILD\")")

## uncomment to build plugin into user config directory
## <localappdata>/plugins/<ideversion>
##    where <localappdata> is e.g.
##    "%LOCALAPPDATA%\QtProject\qtcreator" on Windows Vista and later
##    "$XDG_DATA_HOME/data/QtProject/qtcreator" or "~/.local/share/data/QtProject/qtcreator" on Linux
##    "~/Library/Application Support/QtProject/Qt Creator" on Mac
#USE_USER_DESTDIR = yes

isEmpty(OUTPUT_PATH) {
    USE_USER_DESTDIR = yes
}

PROVIDER = cleem

QTC_PLUGIN_NAME = DlangEditor

LIBS += -L$$IDE_PLUGIN_PATH/QtProject \
        -L$$IDE_BUILD_TREE \
        -L$$IDE_BUILD_TREE/plugins/QtProject \
        -L$$IDE_BUILD_TREE/plugins

QTC_LIB_DEPENDS += \
    cplusplus \

QTC_PLUGIN_DEPENDS += \
    coreplugin \
    cpptools \
    projectexplorer

QTC_PLUGIN_RECOMMENDS += \
    # optional plugin dependencies. nothing here at this time

include($$QTCREATOR_SOURCES/src/qtcreatorplugin.pri)

DEFINES =

QTCREATOR_MINOR_VERSION = $$QTCREATOR_VERSION
QTCREATOR_MINOR_VERSION ~= s/\.[^\.]*$/
QTCREATOR_MINOR_VERSION ~= s/^[^\.]*\./

QTCREATOR_MAJOR_VERSION = $$QTCREATOR_VERSION
QTCREATOR_MAJOR_VERSION ~= s/\..*$/

message("Your QtCreator's sources version is $$QTCREATOR_VERSION")
message("Your QtCreator's sources major version is $$QTCREATOR_MAJOR_VERSION")
message("Your QtCreator's sources minor version is $$QTCREATOR_MINOR_VERSION")

isEqual(QTCREATOR_MAJOR_VERSION, 2) {
  error("Only QtCreator >= 3.0.0 is supported")
}

!isEmpty(SET_VERSION_MINOR) {
    QTCREATOR_MINOR_VERSION = $$SET_VERSION_MINOR
    message("You set minor version to $$QTCREATOR_MINOR_VERSION forcedly")
}

DEFINES += QTCREATOR_MAJOR_VERSION=$$QTCREATOR_MAJOR_VERSION \
    QTCREATOR_MINOR_VERSION=$$QTCREATOR_MINOR_VERSION \

RESOURCES += \
    dlangeditor.qrc

OTHER_FILES += \
    DlangEditor.mimetypes.xml

## define output path
!isEmpty(OUTPUT_PATH) {
    DESTDIR = $$OUTPUT_PATH
    message("You set output path to $$DESTDIR")
}
message("Plugin output path is $$DESTDIR")

