#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Modbus 通讯助手测试"));
    QApplication::setOrganizationName(QStringLiteral("LocalToolsTest"));
    QSettings().setValue(QStringLiteral("appearance/darkTheme"), true);
    MainWindow window;

    if (!window.findChild<QNetworkAccessManager *>()) {
        qCritical() << "MainWindow must create the network manager used for update checks";
        return 1;
    }

    QMessageBox messageBox(QMessageBox::Question,
                           QStringLiteral("发现新版本"),
                           QStringLiteral("是否下载并自动更新？"),
                           QMessageBox::Yes | QMessageBox::No,
                           &window);
    messageBox.show();
    app.processEvents();

    QLabel *messageLabel = nullptr;
    const auto labels = messageBox.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label->text().contains(QStringLiteral("自动更新"))) {
            messageLabel = label;
            break;
        }
    }
    if (!messageLabel) {
        qCritical() << "Update message text label was not created";
        return 1;
    }

    const QColor textColor = messageLabel->palette().color(QPalette::WindowText);
    const QColor backgroundColor = messageBox.palette().color(QPalette::Window);
    if (qAbs(textColor.lightness() - backgroundColor.lightness()) < 90) {
        qCritical() << "Dark update dialog text contrast is too low"
                    << textColor << backgroundColor;
        return 1;
    }

    return 0;
}
