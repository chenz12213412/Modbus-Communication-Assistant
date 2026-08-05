#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QLocale>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Modbus 通讯助手"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.1"));
    QApplication::setOrganizationName(QStringLiteral("LocalTools"));
    QLocale::setDefault(QLocale::Chinese);

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setWindowIcon(QIcon(QStringLiteral(":/app/icon.png")));
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(9);
    app.setFont(font);

    MainWindow window;
    window.show();
    return app.exec();
}
