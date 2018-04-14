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
INCLUDEPATH += $$PWD/include/ $$PWD/3rdparty
SOURCES += \
    $$PWD/src/main.cpp \
    $$PWD/src/mainwindow.cpp \
    $$PWD/src/fixedwindow.cpp \
    $$PWD/src/mainmenu.cpp \
    $$PWD/src/graphmenu.cpp \
    $$PWD/src/screenrecorder.cpp \
    $$PWD/src/selector.cpp \
    $$PWD/src/screencapturer.cpp \
    $$PWD/src/gifcapturer.cpp \
    $$PWD/src/settingdialog.cpp \
    $$PWD/src/shortcutinput.cpp \
    $$PWD/src/magnifier.cpp

HEADERS += \
    $$PWD/include/mainwindow.h \
    $$PWD/include/fixedwindow.h \
    $$PWD/include/mainmenu.h \
    $$PWD/include/graphmenu.h \
    $$PWD/include/screenrecorder.h \
    $$PWD/include/selector.h \
    $$PWD/include/screencapturer.h \
    $$PWD/include/gifcapturer.h \
    $$PWD/include/settingdialog.h \
    $$PWD/include/shortcutinput.h \
    $$PWD/3rdparty/json.hpp \
    $$PWD/include/magnifier.h

FORMS +=

RESOURCES += \
    capturer.qrc

msvc: QMAKE_CXXFLAGS += -execution-charset:utf-8
msvc: QMAKE_CXXFLAGS += -source-charset:utf-8
