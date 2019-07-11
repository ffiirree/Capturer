#-------------------------------------------------
#
# Project created by QtCreator 2018-01-23T19:30:36
#
#-------------------------------------------------

QT       += core gui webenginewidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Capturer
TEMPLATE = app
CONFIG += c++14

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
    $$PWD/widgets \
    $$PWD/core \
    $$PWD/menu \
    $$PWD/snip \
    $$PWD/video \
    $$PWD/gif \
    $$PWD/setting \
    $$PWD/3rdparty

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/capturer.cpp \
    $$PWD/widgets/colorpanel.cpp \
    $$PWD/widgets/textedit.cpp \
    $$PWD/widgets/buttongroup.cpp \
    $$PWD/widgets/shortcutinput.cpp \
    $$PWD/widgets/sizeinfo.cpp \
    $$PWD/widgets/separator.cpp \
    $$PWD/core/imagewindow.cpp \
    $$PWD/core/magnifier.cpp \
    $$PWD/core/config.cpp \
    $$PWD/core/selector.cpp \
    $$PWD/core/displayinfo.cpp \
    $$PWD/menu/imageeditmenu.cpp \
    $$PWD/menu/grapheditmenu.cpp \
    $$PWD/menu/record_menu.cpp \
    $$PWD/menu/texteditmenu.cpp \
    $$PWD/menu/lineeditmenu.cpp \
    $$PWD/menu/linewidthwidget.cpp \
    $$PWD/menu/arroweditmenu.cpp \
    $$PWD/menu/editmenu.cpp \
    $$PWD/menu/erasemenu.cpp \
    $$PWD/snip/screenshoter.cpp \
    $$PWD/video/screenrecorder.cpp \
    $$PWD/gif/gifcapturer.cpp \
    $$PWD/setting/settingdialog.cpp \
    $$PWD/snip/command.cpp \
    $$PWD/snip/circlecursor.cpp \
    $$PWD/widgets/iconbutton.cpp \
    $$PWD/widgets/apptabbar.cpp \
    $$PWD/widgets/apptabcontrol.cpp \
    $$PWD/widgets/titlebar.cpp \
    $$PWD/widgets/custombutton.cpp

HEADERS += \
    $$PWD/widgets/colorpanel.h \
    $$PWD/widgets/textedit.h \
    $$PWD/widgets/sizeinfo.h \
    $$PWD/widgets/buttongroup.h \
    $$PWD/widgets/shortcutinput.h \
    $$PWD/widgets/separator.h \
    $$PWD/core/utils.h \
    $$PWD/core/webview.h \
    $$PWD/capturer.h \
    $$PWD/core/magnifier.h \
    $$PWD/core/detectwidgets.h \
    $$PWD/core/selector.h \
    $$PWD/core/displayinfo.h \
    $$PWD/core/config.h \
    $$PWD/core/resizer.h \
    $$PWD/core/imagewindow.h \
    $$PWD/menu/imageeditmenu.h \
    $$PWD/menu/grapheditmenu.h \
    $$PWD/menu/texteditmenu.h \
    $$PWD/menu/arroweditmenu.h \
    $$PWD/menu/lineeditmenu.h \
    $$PWD/menu/erasemenu.h \
    $$PWD/menu/editmenu.h \
    $$PWD/menu/linewidthwidget.h \
    $$PWD/menu/record_menu.h \
    $$PWD/snip/screenshoter.h \
    $$PWD/video/screenrecorder.h \
    $$PWD/gif/gifcapturer.h \
    $$PWD/setting/settingdialog.h \
    $$PWD/3rdparty/json.hpp \
    $$PWD/snip/command.h \
    $$PWD/snip/circlecursor.h \
    $$PWD/widgets/iconbutton.h \
    $$PWD/widgets/apptabbar.h \
    $$PWD/widgets/apptabcontrol.h \
    $$PWD/widgets/titlebar.h \
    $$PWD/widgets/custombutton.h


RC_ICONS = $$PWD/res/icon.ico

TRANSLATIONS += $$PWD/languages/capturer_zh_CN.ts \
               $$PWD/languages/capturer_en_US.ts

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
    LIBS += -ldwmapi
}

RESOURCES += capturer.qrc

#
msvc: {
    QMAKE_CXXFLAGS += -execution-charset:utf-8 \
        -source-charset:utf-8
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
