#-------------------------------------------------
#
# Project created by QtCreator 2018-01-23T19:30:36
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Capturer
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

include(3rdparty/qxtglobalshortcut/qxt.pri)
INCLUDEPATH += \
    $$PWD/core \
    $$PWD/snip \
    $$PWD/video \
    $$PWD/gif \
    $$PWD/setting \
    $$PWD/3rdparty

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/capturer.cpp \
    $$PWD/core/imagewindow.cpp \
    $$PWD/core/shortcutinput.cpp \
    $$PWD/core/magnifier.cpp \
    $$PWD/core/info.cpp \
    $$PWD/core/selector.cpp \
    $$PWD/snip/mainmenu.cpp \
    $$PWD/snip/graphmenu.cpp \
    $$PWD/snip/screenshoter.cpp \
    $$PWD/video/screenrecorder.cpp \
    $$PWD/gif/gifcapturer.cpp \
    $$PWD/setting/settingdialog.cpp \
    $$PWD/core/colorbutton.cpp \
    $$PWD/core/textedit.cpp \
    $$PWD/snip/fontmenu.cpp \
    $$PWD/snip/command.cpp


HEADERS += \
    $$PWD/capturer.h \
    $$PWD/core/magnifier.h \
    $$PWD/core/detectwidgets.h \
    $$PWD/core/info.h \
    $$PWD/core/imagewindow.h \
    $$PWD/core/shortcutinput.h \
    $$PWD/core/selector.h \
    $$PWD/snip/mainmenu.h \
    $$PWD/snip/graphmenu.h \
    $$PWD/snip/screenshoter.h \
    $$PWD/video/screenrecorder.h \
    $$PWD/gif/gifcapturer.h \
    $$PWD/setting/settingdialog.h \
    $$PWD/3rdparty/json.hpp \
    $$PWD/core/colorbutton.h \
    $$PWD/core/textedit.h \
    $$PWD/snip/fontmenu.h \
    $$PWD/snip/command.h

RC_ICONS = $$PWD/res/icon.ico

# Linux
unix:!macx {
    SOURCES += $$PWD/core/detectwidgets/detectwidgets_x11.cpp
}

# OS X
macx: {
    SOURCES += $$PWD/core/detectwidgets/detectwidgets_mac.cpp
}

# Windows
win32: {
    SOURCES += $$PWD/core/detectwidgets/detectwidgets_win.cpp
}

RESOURCES += capturer.qrc

#
msvc: {
    QMAKE_CXXFLAGS += -execution-charset:utf-8 \
        -source-charset:utf-8

    LIBS += -ldwmapi
}
