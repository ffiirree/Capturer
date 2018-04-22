#include <QApplication>
#include <QFile>
#include "capturer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    QFile file(":/qss/menu.qss");
    file.open(QFile::ReadOnly);

    if(file.isOpen()) {
        qApp->setStyleSheet(file.readAll());
        file.close();
    }

    Capturer window;

    return a.exec();
}
