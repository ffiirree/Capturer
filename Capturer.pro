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

include(qxtglobalshortcut/qxt.pri)
SOURCES += \
        main.cpp \
    mainwindow.cpp \
    fixedwindow.cpp \
    mainmenu.cpp \
    graphmenu.cpp \
    screenrecorder.cpp \
    selector.cpp \
    screencapturer.cpp \
    gifcapturer.cpp \
    settingdialog.cpp \
    shortcutinput.cpp

HEADERS += \
    mainwindow.h \
    fixedwindow.h \
    mainmenu.h \
    graphmenu.h \
    screenrecorder.h \
    selector.h \
    screencapturer.h \
    gifcapturer.h \
    settingdialog.h \
    shortcutinput.h \
    json.hpp

FORMS +=

RESOURCES += \
    capturer.qrc

msvc: QMAKE_CXXFLAGS += -execution-charset:utf-8
msvc: QMAKE_CXXFLAGS += -source-charset:utf-8
