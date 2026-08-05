#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QEvent;
class QMouseEvent;
class QPaintEvent;

class TrendChartWidget final : public QWidget
{
public:
    explicit TrendChartWidget(QWidget *parent = nullptr);

    bool appendSample(const QStringList &seriesNames, const QVector<double> &values);
    void clearSamples();
    void setMaximumSamples(int maximum);
    void setDarkTheme(bool darkTheme);
    int sampleCount() const;
    int seriesCount() const;

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Series
    {
        QString name;
        QVector<double> values;
    };

    QRectF plotRect() const;
    int maximumPointCount() const;
    int sampleIndexAt(qreal x) const;
    QColor seriesColor(int index) const;
    QString hoverText(int sampleIndex) const;

    QVector<Series> m_series;
    int m_maximumSamples = 120;
    qint64 m_firstSampleNumber = 1;
    qint64 m_nextSampleNumber = 1;
    int m_hoverSampleIndex = -1;
    bool m_darkTheme = false;
};
