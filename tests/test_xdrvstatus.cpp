#include "xdrvstatus.h"

#include <QtTest>

class XdrvStatusParserTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesFirmwareStatusSnapshot();
    void startsFreshSnapshotAtNextStateLine();
};

void XdrvStatusParserTests::parsesFirmwareStatusSnapshot()
{
    XdrvStatusParser parser;
    QVERIFY(!parser.consumeLine(QStringLiteral("state   : RUNNING")));
    QVERIFY(parser.isCollectingSnapshot());
    parser.consumeLine(QStringLiteral("power   : active"));
    parser.consumeLine(QStringLiteral("mode    : idle -> pos"));
    parser.consumeLine(QStringLiteral("pos     : -1250 (mrev)"));
    parser.consumeLine(QStringLiteral("vel     : 500 (mrps)"));
    parser.consumeLine(QStringLiteral("current : 250 (mA)"));
    parser.consumeLine(QStringLiteral("calib   : yes"));
    parser.consumeLine(QStringLiteral("stall   : 0, overload 1"));
    parser.consumeLine(QStringLiteral("cfg     : node=3 base=0x180 limit=1000mA vel=1536000 acc=5120000"));
    parser.consumeLine(QStringLiteral("isr     : last=120 max=2880 cyc (40.00 us), n=1234"));
    parser.consumeLine(QStringLiteral("spi_err : 2 (mt6816 timeouts)"));
    parser.consumeLine(QStringLiteral("uart    : ore=4 err=5 (rx lost if growing)"));
    QVERIFY(parser.consumeLine(QStringLiteral("can     : rx=10 cmd=9 other=0 drop=0 txfail=0")));
    QVERIFY(!parser.isCollectingSnapshot());

    const XdrvStatus &status = parser.status();
    QCOMPARE(status.state, QStringLiteral("RUNNING"));
    QCOMPARE(status.power, QStringLiteral("active"));
    QCOMPARE(status.mode, QStringLiteral("idle -> pos"));
    QCOMPARE(*status.positionMilliRev, -1250);
    QCOMPARE(*status.velocityMilliRevPerSec, 500);
    QCOMPARE(*status.currentMilliamp, 250);
    QVERIFY(*status.calibrated);
    QCOMPARE(*status.stalled, 0);
    QCOMPARE(*status.overloaded, 1);
    QCOMPARE(*status.currentLimitMilliamp, 1000);
    QCOMPARE(*status.velocityLimitRevPerSec, 30.0);
    QCOMPARE(*status.isrMaxMicroseconds, 40.0);
    QCOMPARE(*status.spiErrors, quint64(2));
    QCOMPARE(*status.uartOverruns, quint64(4));
    QCOMPARE(*status.uartErrors, quint64(5));
}

void XdrvStatusParserTests::startsFreshSnapshotAtNextStateLine()
{
    XdrvStatusParser parser;
    parser.consumeLine(QStringLiteral("state   : RUNNING"));
    parser.consumeLine(QStringLiteral("calib   : yes"));
    parser.consumeLine(QStringLiteral("can     : rx=1 cmd=1 other=0 drop=0 txfail=0"));
    QVERIFY(parser.status().calibrated.has_value());

    parser.consumeLine(QStringLiteral("state   : STOP"));
    QVERIFY(!parser.status().calibrated.has_value());
    QCOMPARE(parser.status().state, QStringLiteral("STOP"));
}

QTEST_APPLESS_MAIN(XdrvStatusParserTests)
#include "test_xdrvstatus.moc"
