#include "internalchannel.h"

#include <QByteArray>
#include <QRegularExpression>

namespace {
constexpr quint16 kFirstPort = 42000;
constexpr quint16 kPortCount = 8000;
}

bool InternalChannel::validateName(const QString &name, QString *errorMessage)
{
    const QString trimmed = name.trimmed();
    QString error;
    if (trimmed.isEmpty()) {
        error = QStringLiteral("请输入通道名称");
    } else if (trimmed.size() > 32) {
        error = QStringLiteral("通道名称不能超过 32 个字符");
    } else {
        static const QRegularExpression allowed(
            QStringLiteral("^[\\p{L}\\p{N}_-]+$"));
        if (!allowed.match(trimmed).hasMatch())
            error = QStringLiteral("通道名称只能包含中文、字母、数字、下划线和短横线");
    }

    if (errorMessage)
        *errorMessage = error;
    return error.isEmpty();
}

quint16 InternalChannel::portForName(const QString &name)
{
    const QByteArray bytes = name.trimmed().toUtf8();
    quint32 hash = 2166136261u;
    for (const char byte : bytes) {
        hash ^= static_cast<quint8>(byte);
        hash *= 16777619u;
    }
    return static_cast<quint16>(kFirstPort + hash % kPortCount);
}
