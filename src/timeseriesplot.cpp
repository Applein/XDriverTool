#include "timeseriesplot.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QDateTime>
#include <QRectF>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace
{
constexpr qint64 HistoryDurationMs = 5 * 60 * 1000;
constexpr int MinimumWindowMs = 2 * 1000;
constexpr int MaximumWindowMs = static_cast<int>(HistoryDurationMs);
constexpr int ScrollBarHeight = 10;
}

TimeSeriesPlot::TimeSeriesPlot(const QString &title, const QString &unit,
                               const QColor &lineColor, QWidget *parent)
    : QWidget(parent),
      m_title(title),
      m_unit(unit),
      m_lineColor(lineColor)
{
    setMinimumSize(190, 136);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setObjectName(QStringLiteral("timeSeriesPlot"));
    setToolTip(QStringLiteral("%1（%2）").arg(title, unit));

    m_scrollBar = new QScrollBar(Qt::Horizontal, this);
    m_scrollBar->setObjectName(QStringLiteral("plotScrollBar"));
    m_scrollBar->setFixedHeight(ScrollBarHeight);
    m_scrollBar->setRange(0, 0);
    m_scrollBar->setSingleStep(1000);
    m_scrollBar->setPageStep(m_visibleSeconds * 1000);
    m_scrollBar->setToolTip(QStringLiteral("拖动以回看最近 5 分钟的数据"));
    connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int offsetMs) {
        if (m_updatingScrollBar || m_samples.isEmpty())
            return;

        m_viewEndTimestampMs = m_samples.last().timestampMs + offsetMs;
        update();
    });
}

void TimeSeriesPlot::appendSample(qint64 timestampMs, double value)
{
    const bool wasFollowingLatest =
        m_samples.isEmpty() || m_viewEndTimestampMs >= m_samples.last().timestampMs;
    m_samples.append({timestampMs, value});
    const qint64 oldestAllowed = timestampMs - HistoryDurationMs;
    while (!m_samples.isEmpty() && m_samples.first().timestampMs < oldestAllowed)
        m_samples.removeFirst();

    if (wasFollowingLatest || m_viewEndTimestampMs == 0)
        m_viewEndTimestampMs = timestampMs;

    updateScrollBar();
    update();
}

void TimeSeriesPlot::clear()
{
    m_samples.clear();
    m_viewEndTimestampMs = 0;
    updateScrollBar();
    update();
}

QRectF TimeSeriesPlot::plotRect() const
{
    const int chartHeight = qMax(1, height() - m_scrollBar->height());
    return QRectF(52.0, 48.0, qMax(1, width() - 66), qMax(1, chartHeight - 76));
}

void TimeSeriesPlot::updateScrollBar()
{
    m_updatingScrollBar = true;
    if (m_samples.isEmpty())
    {
        m_scrollBar->setRange(0, 0);
        m_scrollBar->setValue(0);
        m_scrollBar->setEnabled(false);
        m_updatingScrollBar = false;
        return;
    }

    const qint64 oldestTime = m_samples.first().timestampMs;
    const qint64 latestTime = m_samples.last().timestampMs;
    const qint64 visibleDuration = static_cast<qint64>(m_visibleSeconds) * 1000;
    const int minimumOffset =
        static_cast<int>(qMin<qint64>(0, oldestTime + visibleDuration - latestTime));
    const int currentOffset =
        static_cast<int>(qBound<qint64>(static_cast<qint64>(minimumOffset),
                                        m_viewEndTimestampMs - latestTime, qint64(0)));
    m_scrollBar->setRange(minimumOffset, 0);
    m_scrollBar->setPageStep(m_visibleSeconds * 1000);
    m_scrollBar->setValue(currentOffset);
    m_scrollBar->setEnabled(minimumOffset < 0);
    m_viewEndTimestampMs = latestTime + currentOffset;
    m_updatingScrollBar = false;
}

void TimeSeriesPlot::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int chartHeight = qMax(1, height() - m_scrollBar->height());
    painter.fillRect(QRect(0, 0, width(), chartHeight), QColor(QStringLiteral("#111111")));

    const QColor textColor(QStringLiteral("#8f8f8f"));
    const QColor gridColor(QStringLiteral("#242424"));
    painter.setPen(textColor);
    painter.drawText(QRect(12, 7, width() - 24, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     m_title);

    if (!m_samples.isEmpty())
    {
        const QString latest = QStringLiteral("%1 %2")
                                   .arg(m_samples.last().value, 0, 'f', 3)
                                   .arg(m_unit);
        painter.setPen(m_lineColor);
        painter.drawText(QRect(12, 25, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, latest);
    }

    const QRectF graphRect = plotRect();
    painter.setPen(QPen(gridColor, 1));
    for (int i = 0; i <= 4; ++i)
    {
        const qreal y = graphRect.top() + graphRect.height() * i / 4.0;
        painter.drawLine(QPointF(graphRect.left(), y), QPointF(graphRect.right(), y));
    }
    for (int i = 0; i <= 4; ++i)
    {
        const qreal x = graphRect.left() + graphRect.width() * i / 4.0;
        painter.drawLine(QPointF(x, graphRect.top()), QPointF(x, graphRect.bottom()));
    }

    const qint64 endTime = m_samples.isEmpty() ? 0 : m_viewEndTimestampMs;
    const qint64 duration = static_cast<qint64>(m_visibleSeconds) * 1000;
    const qint64 startTime = endTime - duration;
    QVector<const Sample *> visibleSamples;
    for (const Sample &sample : m_samples)
    {
        if (sample.timestampMs >= startTime && sample.timestampMs <= endTime)
            visibleSamples.append(&sample);
    }

    double minimum = 0.0;
    double maximum = 0.0;
    if (!visibleSamples.isEmpty())
    {
        minimum = visibleSamples.first()->value;
        maximum = minimum;
        for (const Sample *sample : visibleSamples)
        {
            minimum = std::min(minimum, sample->value);
            maximum = std::max(maximum, sample->value);
        }
        if (qFuzzyCompare(minimum, maximum))
        {
            const double padding = qMax(0.1, qAbs(minimum) * 0.05);
            minimum -= padding;
            maximum += padding;
        }
        else
        {
            const double padding = (maximum - minimum) * 0.08;
            minimum -= padding;
            maximum += padding;
        }
    }

    painter.setPen(textColor);
    painter.drawText(QRectF(2, graphRect.top() - 7, 46, 16), Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("%1").arg(maximum, 0, 'f', 2));
    painter.drawText(QRectF(2, graphRect.bottom() - 9, 46, 16), Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("%1").arg(minimum, 0, 'f', 2));

    const QString rangeLabel = QStringLiteral("%1 秒").arg(m_visibleSeconds);
    painter.drawText(QRectF(graphRect.left(), graphRect.bottom() + 5, 52, 16),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("-%1秒").arg(m_visibleSeconds));
    painter.drawText(QRectF(graphRect.center().x() - 30, graphRect.bottom() + 5, 60, 16),
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     QStringLiteral("-%1秒").arg(m_visibleSeconds / 2));
    QString endLabel;
    if (!m_samples.isEmpty())
    {
        const bool showingLatest = endTime >= m_samples.last().timestampMs - 1000;
        endLabel = showingLatest
                       ? QStringLiteral("现在")
                       : QDateTime::fromMSecsSinceEpoch(endTime).toString(
                             QStringLiteral("HH:mm:ss"));
    }
    painter.drawText(QRectF(graphRect.right() - 56, graphRect.bottom() + 5, 56, 16),
                     Qt::AlignRight | Qt::AlignVCenter, endLabel);
    painter.drawText(QRectF(graphRect.right() - 70, 7, 58, 20),
                     Qt::AlignRight | Qt::AlignVCenter, rangeLabel);

    if (visibleSamples.isEmpty())
    {
        painter.setPen(textColor);
        painter.drawText(graphRect, Qt::AlignCenter,
                         m_samples.isEmpty() ? QStringLiteral("等待状态数据")
                                             : QStringLiteral("该时间段暂无数据"));
        return;
    }

    QPainterPath line;
    bool firstPoint = true;
    for (const Sample *sample : visibleSamples)
    {
        const qreal x = graphRect.left()
                        + static_cast<qreal>(sample->timestampMs - startTime) / duration
                              * graphRect.width();
        const qreal y = graphRect.bottom()
                        - (sample->value - minimum) / (maximum - minimum) * graphRect.height();
        const QPointF point(x, y);
        if (firstPoint)
        {
            line.moveTo(point);
            firstPoint = false;
        }
        else
        {
            line.lineTo(point);
        }
    }

    painter.save();
    painter.setClipRect(graphRect);
    painter.setPen(QPen(m_lineColor, 2));
    painter.drawPath(line);
    painter.setBrush(m_lineColor);
    painter.setPen(Qt::NoPen);
    for (const Sample *sample : visibleSamples)
    {
        const qreal x = graphRect.left()
                        + static_cast<qreal>(sample->timestampMs - startTime) / duration
                              * graphRect.width();
        const qreal y = graphRect.bottom()
                        - (sample->value - minimum) / (maximum - minimum) * graphRect.height();
        painter.drawEllipse(QPointF(x, y), 2.0, 2.0);
    }
    painter.restore();
}

void TimeSeriesPlot::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_scrollBar->setGeometry(0, height() - m_scrollBar->height(), width(), m_scrollBar->height());
}

void TimeSeriesPlot::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF wheelPosition = event->position();
#else
    const QPointF wheelPosition = event->posF();
#endif
    const QRectF graphRect = plotRect();
    if (m_samples.isEmpty() || event->angleDelta().y() == 0
        || !graphRect.contains(wheelPosition))
    {
        event->ignore();
        return;
    }

    const qint64 oldDuration = static_cast<qint64>(m_visibleSeconds) * 1000;
    const qreal ratio = qBound<qreal>(
        0.0, (wheelPosition.x() - graphRect.left()) / graphRect.width(), 1.0);
    const qint64 anchorTime =
        m_viewEndTimestampMs - oldDuration
        + static_cast<qint64>(ratio * static_cast<qreal>(oldDuration));
    const qreal scale = event->angleDelta().y() > 0 ? 0.8 : 1.25;
    const int newVisibleSeconds =
        qBound(MinimumWindowMs / 1000,
               static_cast<int>(std::lround(oldDuration * scale / 1000.0)),
               MaximumWindowMs / 1000);
    if (newVisibleSeconds == m_visibleSeconds)
    {
        event->accept();
        return;
    }

    m_visibleSeconds = newVisibleSeconds;
    const int newDuration = m_visibleSeconds * 1000;
    const qint64 newStart =
        anchorTime - static_cast<qint64>(ratio * static_cast<qreal>(newDuration));
    const qint64 latestTime = m_samples.last().timestampMs;
    m_viewEndTimestampMs = qMin(latestTime, newStart + newDuration);
    updateScrollBar();
    update();
    event->accept();
}
