#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>

struct XdrvStatus
{
    QString state;
    QString power;
    QString mode;
    std::optional<qint32> positionMilliRev;
    std::optional<qint32> velocityMilliRevPerSec;
    std::optional<qint32> currentMilliamp;
    std::optional<bool> calibrated;
    std::optional<int> stalled;
    std::optional<int> overloaded;
    std::optional<int> currentLimitMilliamp;
    std::optional<double> velocityLimitRevPerSec;
    std::optional<double> isrMaxMicroseconds;
    std::optional<quint64> spiErrors;
    std::optional<quint64> uartOverruns;
    std::optional<quint64> uartErrors;
    QString isrWarning;
    QString spiFailure;
    QString controller;
    QString dce;
    QString can;
};

class XdrvStatusParser
{
public:
    bool consumeLine(const QString &line);
    const XdrvStatus &status() const;
    bool isCollectingSnapshot() const;
    void reset();

private:
    XdrvStatus m_status;
    bool m_inSnapshot = false;
};
