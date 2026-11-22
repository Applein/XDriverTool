#include "timeseriesplot.h"

#include <QScrollBar>
#include <QWheelEvent>
#include <QtTest>

class TimeSeriesPlotTests : public QObject
{
    Q_OBJECT

private slots:
    void retainsFiveMinutesAndExposesHistoryScrollRange();
    void wheelZoomsAroundPointerAndUpdatesScrollPosition();
};

void TimeSeriesPlotTests::retainsFiveMinutesAndExposesHistoryScrollRange()
{
    TimeSeriesPlot plot(QStringLiteral("位置"), QStringLiteral("圈"), Qt::blue);
    for (qint64 timestamp = 0; timestamp <= 360000; timestamp += 200)
        plot.appendSample(timestamp, static_cast<double>(timestamp));

    const auto *scrollBar = plot.findChild<QScrollBar *>(QStringLiteral("plotScrollBar"));
    QVERIFY(scrollBar);
    QCOMPARE(scrollBar->minimum(), -180000);
    QCOMPARE(scrollBar->maximum(), 0);
    QCOMPARE(scrollBar->value(), 0);
}

void TimeSeriesPlotTests::wheelZoomsAroundPointerAndUpdatesScrollPosition()
{
    TimeSeriesPlot plot(QStringLiteral("位置"), QStringLiteral("圈"), Qt::blue);
    plot.resize(600, 180);
    for (qint64 timestamp = 0; timestamp <= 300000; timestamp += 1000)
        plot.appendSample(timestamp, static_cast<double>(timestamp));

    const QPointF position(319.0, 92.0);
    QWheelEvent event(position, position, QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&plot, &event);

    const auto *scrollBar = plot.findChild<QScrollBar *>(QStringLiteral("plotScrollBar"));
    QVERIFY(scrollBar);
    QCOMPARE(scrollBar->pageStep(), 96000);
    QCOMPARE(scrollBar->value(), -12000);
}

QTEST_MAIN(TimeSeriesPlotTests)
#include "test_timeseriesplot.moc"
