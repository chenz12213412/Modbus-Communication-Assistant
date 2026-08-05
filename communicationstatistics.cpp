#include "communicationstatistics.h"

void CommunicationStatistics::recordSuccess()
{
    ++m_successCount;
}

void CommunicationStatistics::recordFailure()
{
    ++m_failureCount;
}

void CommunicationStatistics::reset()
{
    m_successCount = 0;
    m_failureCount = 0;
}

quint64 CommunicationStatistics::successCount() const
{
    return m_successCount;
}

quint64 CommunicationStatistics::failureCount() const
{
    return m_failureCount;
}

QString CommunicationStatistics::displayText() const
{
    const quint64 total = m_successCount + m_failureCount;
    if (total == 0) {
        return QStringLiteral("成功 0  ·  失败 0  ·  成功率 --");
    }

    const quint64 percentage = (m_successCount * 100 + total / 2) / total;
    return QStringLiteral("成功 %1  ·  失败 %2  ·  成功率 %3%")
        .arg(m_successCount)
        .arg(m_failureCount)
        .arg(percentage);
}
