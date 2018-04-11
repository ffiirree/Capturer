#include <QApplication>
#include <QFile>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFile file(":/qss/menu.qss");
    file.open(QFile::ReadOnly);

    if(file.isOpen()) {
        qApp->setStyleSheet(file.readAll());
        file.close();
    }

    MainWindow window;

    return a.exec();
}
