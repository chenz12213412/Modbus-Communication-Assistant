#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QNetworkAccessManager>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;

    if (!window.findChild<QNetworkAccessManager *>()) {
        qCritical() << "MainWindow must create the network manager used for update checks";
        return 1;
    }

    return 0;
}
