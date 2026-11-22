#pragma once

#include <QColor>
#include <QWidget>

#include <QVector>

class QResizeEvent;
class QScrollBar;
class QWheelEvent;

class TimeSeriesPlot : public QWidget
{
public:
    TimeSeriesPlot(const QString &title, const QString &unit, const QColor &lineColor,
                   QWidget *parent = nullptr);

    void appendSample(qint64 timestampMs, double value);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct Sample
    {
        qint64 timestampMs;
        double value;
    };

    QRectF plotRect() const;
    void updateScrollBar();

    QString m_title;
    QString m_unit;
    QColor m_lineColor;
    QVector<Sample> m_samples;
    QScrollBar *m_scrollBar = nullptr;
    qint64 m_viewEndTimestampMs = 0;
    int m_visibleSeconds = 120;
    bool m_updatingScrollBar = false;
};
