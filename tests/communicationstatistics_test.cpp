#include "communicationstatistics.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
bool expectEqual(const QString &actual, const QString &expected, const char *message)
{
    if (actual == expected)
        return true;
    qCritical().noquote() << message << "expected:" << expected << "actual:" << actual;
    return false;
}

bool expectEqual(quint64 actual, quint64 expected, const char *message)
{
    if (actual == expected)
        return true;
    qCritical() << message << "expected:" << expected << "actual:" << actual;
    return false;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    CommunicationStatistics statistics;

    if (!expectEqual(statistics.displayText(),
                     QStringLiteral("成功 0  ·  失败 0  ·  成功率 --"),
                     "initial display text"))
        return 1;

    statistics.recordSuccess();
    statistics.recordFailure();
    statistics.recordSuccess();
    if (!expectEqual(statistics.displayText(),
                     QStringLiteral("成功 2  ·  失败 1  ·  成功率 67%"),
                     "updated display text"))
        return 1;

    statistics.reset();
    if (!expectEqual(statistics.successCount(), quint64(0), "reset success count")
        || !expectEqual(statistics.failureCount(), quint64(0), "reset failure count"))
        return 1;

    return 0;
}
