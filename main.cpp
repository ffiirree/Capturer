#include <QApplication>
#include <QFile>
#include "displayinfo.h"
#include "capturer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    // log pattern
    qSetMessagePattern("%{time yyyy-MM-dd hh:mm:ss.zzz} (%{type}) %{file}:%{line}] %{message}");

    LOG(INFO) << "Logging for Capturer";  LOG(INFO);

    // displays
    DisplayInfo::instance();

    QFile file(":/qss/menu.qss");
    file.open(QFile::ReadOnly);

    if(file.isOpen()) {
        qApp->setStyleSheet(file.readAll());
        file.close();
    }

    Capturer window;

    return a.exec();
}
