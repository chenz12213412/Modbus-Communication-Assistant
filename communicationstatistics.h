#ifndef COMMUNICATIONSTATISTICS_H
#define COMMUNICATIONSTATISTICS_H

#include <QString>
#include <QtGlobal>

class CommunicationStatistics
{
public:
    void recordSuccess();
    void recordFailure();
    void reset();

    quint64 successCount() const;
    quint64 failureCount() const;
    QString displayText() const;

private:
    quint64 m_successCount = 0;
    quint64 m_failureCount = 0;
};

#endif
