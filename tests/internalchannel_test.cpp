#include "internalchannel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    bool passed = true;

    QString error;
    passed &= expect(!InternalChannel::validateName(QString(), &error),
                     "empty channel name must be rejected");
    passed &= expect(!error.isEmpty(), "empty channel name must explain the error");

    error.clear();
    passed &= expect(InternalChannel::validateName(QStringLiteral("生产线_1-A"), &error),
                     "Chinese, letters, digits, underscore and hyphen must be accepted");
    passed &= expect(error.isEmpty(), "valid channel name must not return an error");

    error.clear();
    passed &= expect(!InternalChannel::validateName(QStringLiteral("line/1"), &error),
                     "path separators must be rejected");
    passed &= expect(!InternalChannel::validateName(QString(33, QLatin1Char('a')), &error),
                     "channel names longer than 32 characters must be rejected");

    const quint16 first = InternalChannel::portForName(QStringLiteral("生产线_1-A"));
    const quint16 repeated = InternalChannel::portForName(QStringLiteral("生产线_1-A"));
    const quint16 differentCase = InternalChannel::portForName(QStringLiteral("生产线_1-a"));
    passed &= expect(first >= 42000 && first <= 49999,
                     "mapped port must stay inside the reserved range");
    passed &= expect(first == repeated, "same channel name must map deterministically");
    passed &= expect(first != differentCase, "channel names are case-sensitive");

    QTcpServer server;
    passed &= expect(server.listen(QHostAddress::LocalHost, first),
                     "internal channel must listen on loopback");
    passed &= expect(server.serverAddress().isLoopback(),
                     "internal channel must not bind to a network interface");

    QTcpSocket master;
    master.connectToHost(QHostAddress::LocalHost, first);
    passed &= expect(master.waitForConnected(1000),
                     "master must connect to a same-name internal channel");
    passed &= expect(server.waitForNewConnection(1000),
                     "slave must accept the internal channel connection");
    QTcpSocket *slave = server.nextPendingConnection();
    passed &= expect(slave != nullptr, "slave socket must be available");
    if (slave) {
        const QByteArray request = QByteArray::fromHex("010300000001840A");
        master.write(request);
        passed &= expect(master.waitForBytesWritten(1000),
                         "master request must be written");
        passed &= expect(slave->waitForReadyRead(1000),
                         "slave must receive the master request");
        passed &= expect(slave->readAll() == request,
                         "internal channel must preserve RTU frame bytes");
        slave->deleteLater();
    }

    return passed ? 0 : 1;
}
