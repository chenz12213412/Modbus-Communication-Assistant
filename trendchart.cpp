#include "trendchart.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kLeftMargin = 58;
constexpr int kRightMargin = 18;
constexpr int kBottomMargin = 38;
constexpr int kLegendItemWidth = 132;
constexpr int kLegendRowHeight = 22;
}

TrendChartWidget::TrendChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(320);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(QStringLiteral("主站数据趋势曲线"));
}

bool TrendChartWidget::appendSample(const QStringList &seriesNames,
                                    const QVector<double> &values)
{
    if (seriesNames.isEmpty() || seriesNames.size() != values.size())
        return false;

    bool reset = m_series.size() != seriesNames.size();
    if (!reset) {
        for (int index = 0; index < seriesNames.size(); ++index) {
            if (m_series.at(index).name != seriesNames.at(index)) {
                reset = true;
                break;
            }
        }
    }

    if (reset) {
        m_series.clear();
        m_firstSampleNumber = 1;
        m_nextSampleNumber = 1;
        for (const QString &name : seriesNames)
            m_series.append({name, {}});
    }

    for (int index = 0; index < values.size(); ++index)
        m_series[index].values.append(values.at(index));

    ++m_nextSampleNumber;
    while (maximumPointCount() > m_maximumSamples) {
        for (Series &series : m_series) {
            if (!series.values.isEmpty())
                series.values.removeFirst();
        }
        ++m_firstSampleNumber;
    }

    m_hoverSampleIndex = -1;
    update();
    return reset;
}

void TrendChartWidget::clearSamples()
{
    m_series.clear();
    m_firstSampleNumber = 1;
    m_nextSampleNumber = 1;
    m_hoverSampleIndex = -1;
    QToolTip::hideText();
    update();
}

void TrendChartWidget::setMaximumSamples(int maximum)
{
    m_maximumSamples = qBound(20, maximum, 500);
    while (maximumPointCount() > m_maximumSamples) {
        for (Series &series : m_series) {
            if (!series.values.isEmpty())
                series.values.removeFirst();
        }
        ++m_firstSampleNumber;
    }
    update();
}

void TrendChartWidget::setDarkTheme(bool darkTheme)
{
    if (m_darkTheme == darkTheme)
        return;
    m_darkTheme = darkTheme;
    update();
}

int TrendChartWidget::sampleCount() const
{
    return maximumPointCount();
}

int TrendChartWidget::seriesCount() const
{
    return m_series.size();
}

QSize TrendChartWidget::minimumSizeHint() const
{
    return {520, 320};
}

QRectF TrendChartWidget::plotRect() const
{
    const int columns = qMax(1, (width() - 28) / kLegendItemWidth);
    const int legendRows = qMax(1, (m_series.size() + columns - 1) / columns);
    const int topMargin = 18 + legendRows * kLegendRowHeight;
    return QRectF(kLeftMargin, topMargin,
                  qMax(40, width() - kLeftMargin - kRightMargin),
                  qMax(40, height() - topMargin - kBottomMargin));
}

int TrendChartWidget::maximumPointCount() const
{
    int count = 0;
    for (const Series &series : m_series)
        count = qMax(count, series.values.size());
    return count;
}

int TrendChartWidget::sampleIndexAt(qreal x) const
{
    const int count = maximumPointCount();
    if (count <= 0)
        return -1;
    if (count == 1)
        return 0;
    const QRectF area = plotRect();
    const qreal ratio = qBound<qreal>(0.0, (x - area.left()) / area.width(), 1.0);
    return qBound(0, qRound(ratio * (count - 1)), count - 1);
}

QColor TrendChartWidget::seriesColor(int index) const
{
    static const QVector<QColor> lightColors = {
        QColor(QStringLiteral("#1879C9")), QColor(QStringLiteral("#D97706")),
        QColor(QStringLiteral("#23855B")), QColor(QStringLiteral("#A23E48")),
        QColor(QStringLiteral("#7455B8")), QColor(QStringLiteral("#00838F")),
        QColor(QStringLiteral("#B35C1E")), QColor(QStringLiteral("#3D6F91"))
    };
    static const QVector<QColor> darkColors = {
        QColor(QStringLiteral("#58A6FF")), QColor(QStringLiteral("#F2B84B")),
        QColor(QStringLiteral("#4DD39A")), QColor(QStringLiteral("#FF7B72")),
        QColor(QStringLiteral("#C09CFF")), QColor(QStringLiteral("#39C5CF")),
        QColor(QStringLiteral("#F0905A")), QColor(QStringLiteral("#8DB7D1"))
    };
    const QVector<QColor> &palette = m_darkTheme ? darkColors : lightColors;
    return palette.at(index % palette.size());
}

QString TrendChartWidget::hoverText(int sampleIndex) const
{
    if (sampleIndex < 0)
        return {};

    QStringList lines;
    lines.append(QStringLiteral("采样 %1").arg(m_firstSampleNumber + sampleIndex));
    for (const Series &series : m_series) {
        if (sampleIndex < series.values.size()) {
            lines.append(QStringLiteral("%1：%2")
                             .arg(series.name)
                             .arg(series.values.at(sampleIndex), 0, 'g', 10));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

void TrendChartWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor background = m_darkTheme ? QColor(QStringLiteral("#11171B"))
                                           : QColor(QStringLiteral("#FFFFFF"));
    const QColor border = m_darkTheme ? QColor(QStringLiteral("#53636E"))
                                       : QColor(QStringLiteral("#8997A2"));
    const QColor grid = m_darkTheme ? QColor(QStringLiteral("#2C3941"))
                                     : QColor(QStringLiteral("#DCE3E7"));
    const QColor axis = m_darkTheme ? QColor(QStringLiteral("#AEBBC4"))
                                     : QColor(QStringLiteral("#5D6B75"));
    const QColor foreground = m_darkTheme ? QColor(QStringLiteral("#E7EDF0"))
                                           : QColor(QStringLiteral("#17212B"));

    painter.fillRect(rect(), background);
    painter.setPen(QPen(border, 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    if (m_series.isEmpty() || maximumPointCount() == 0) {
        painter.setPen(foreground);
        QFont titleFont = font();
        titleFont.setPointSize(12);
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.drawText(rect().adjusted(24, 0, -24, -18),
                         Qt::AlignCenter, QStringLiteral("等待主站采样数据"));
        painter.setPen(axis);
        painter.setFont(font());
        painter.drawText(rect().adjusted(24, 42, -24, 0), Qt::AlignCenter,
                         QStringLiteral("发送读取请求或开启定时轮询后自动绘制趋势"));
        return;
    }

    const QRectF area = plotRect();
    const int columns = qMax(1, (width() - 28) / kLegendItemWidth);
    painter.setFont(font());
    for (int index = 0; index < m_series.size(); ++index) {
        const int column = index % columns;
        const int row = index / columns;
        const qreal x = 16 + column * kLegendItemWidth;
        const qreal y = 14 + row * kLegendRowHeight;
        painter.setPen(QPen(seriesColor(index), 3));
        painter.drawLine(QPointF(x, y + 5), QPointF(x + 20, y + 5));
        painter.setPen(foreground);
        const QString label = painter.fontMetrics().elidedText(
            m_series.at(index).name, Qt::ElideRight, kLegendItemWidth - 32);
        painter.drawText(QPointF(x + 27, y + 9), label);
    }

    double minimum = m_series.first().values.first();
    double maximum = minimum;
    for (const Series &series : m_series) {
        for (double value : series.values) {
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }
    }
    if (qFuzzyCompare(minimum + 1.0, maximum + 1.0)) {
        const double padding = qMax(1.0, std::abs(minimum) * 0.08);
        minimum -= padding;
        maximum += padding;
    } else {
        const double padding = (maximum - minimum) * 0.08;
        minimum -= padding;
        maximum += padding;
    }

    painter.setPen(QPen(grid, 1, Qt::DashLine));
    for (int step = 0; step <= 5; ++step) {
        const qreal y = area.bottom() - area.height() * step / 5.0;
        painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        const double value = minimum + (maximum - minimum) * step / 5.0;
        painter.setPen(axis);
        painter.drawText(QRectF(2, y - 9, kLeftMargin - 8, 18),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(value, 'f', maximum - minimum < 10 ? 1 : 0));
        painter.setPen(QPen(grid, 1, Qt::DashLine));
    }

    const int pointCount = maximumPointCount();
    const int verticalSteps = qMin(5, qMax(1, pointCount - 1));
    for (int step = 0; step <= verticalSteps; ++step) {
        const qreal ratio = verticalSteps == 0 ? 0.0 : step / qreal(verticalSteps);
        const qreal x = area.left() + area.width() * ratio;
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        const int pointIndex = pointCount == 1 ? 0 : qRound(ratio * (pointCount - 1));
        painter.setPen(axis);
        painter.drawText(QRectF(x - 30, area.bottom() + 7, 60, 20), Qt::AlignHCenter,
                         QString::number(m_firstSampleNumber + pointIndex));
        painter.setPen(QPen(grid, 1, Qt::DashLine));
    }

    painter.setPen(QPen(border, 1));
    painter.drawRect(area);

    auto pointForValue = [&](int index, int count, double value) {
        const qreal x = count <= 1 ? area.center().x()
                                   : area.left() + area.width() * index / (count - 1.0);
        const qreal y = area.bottom()
                        - area.height() * (value - minimum) / (maximum - minimum);
        return QPointF(x, y);
    };

    painter.setClipRect(area.adjusted(-2, -2, 2, 2));
    for (int seriesIndex = 0; seriesIndex < m_series.size(); ++seriesIndex) {
        const Series &series = m_series.at(seriesIndex);
        if (series.values.isEmpty())
            continue;
        QPainterPath path;
        path.moveTo(pointForValue(0, series.values.size(), series.values.first()));
        for (int index = 1; index < series.values.size(); ++index)
            path.lineTo(pointForValue(index, series.values.size(), series.values.at(index)));
        painter.setPen(QPen(seriesColor(seriesIndex), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        if (series.values.size() <= 60) {
            painter.setBrush(background);
            for (int index = 0; index < series.values.size(); ++index)
                painter.drawEllipse(pointForValue(index, series.values.size(),
                                                  series.values.at(index)), 2.6, 2.6);
        }
    }

    if (m_hoverSampleIndex >= 0 && m_hoverSampleIndex < pointCount) {
        const qreal x = pointCount <= 1 ? area.center().x()
                                        : area.left() + area.width()
                                              * m_hoverSampleIndex / (pointCount - 1.0);
        painter.setPen(QPen(axis, 1, Qt::DashLine));
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        for (int seriesIndex = 0; seriesIndex < m_series.size(); ++seriesIndex) {
            const Series &series = m_series.at(seriesIndex);
            if (m_hoverSampleIndex >= series.values.size())
                continue;
            painter.setPen(QPen(seriesColor(seriesIndex), 2));
            painter.setBrush(background);
            painter.drawEllipse(pointForValue(m_hoverSampleIndex, series.values.size(),
                                              series.values.at(m_hoverSampleIndex)), 4.5, 4.5);
        }
    }
}

void TrendChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QRectF area = plotRect();
    if (!area.contains(event->pos()) || maximumPointCount() == 0) {
        if (m_hoverSampleIndex != -1) {
            m_hoverSampleIndex = -1;
            QToolTip::hideText();
            update();
        }
        return;
    }

    const int index = sampleIndexAt(event->pos().x());
    if (index != m_hoverSampleIndex) {
        m_hoverSampleIndex = index;
        QToolTip::showText(mapToGlobal(event->pos()), hoverText(index), this);
        update();
    }
}

void TrendChartWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hoverSampleIndex = -1;
    QToolTip::hideText();
    update();
}
