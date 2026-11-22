#include "xdrvstatus.h"

#include <QRegularExpression>

#include <limits>

namespace
{
std::optional<qint64> firstInteger(const QString &text)
{
    static const QRegularExpression integerPattern(QStringLiteral("-?\\d+"));
    const auto match = integerPattern.match(text);
    if (!match.hasMatch())
        return std::nullopt;

    bool ok = false;
    const qint64 value = match.captured(0).toLongLong(&ok);
    if (!ok)
        return std::nullopt;
    return value;
}

std::optional<quint64> namedUnsigned(const QString &text, const QString &name)
{
    const QRegularExpression pattern(
        QStringLiteral("(?:^|\\s)%1=(\\d+)").arg(QRegularExpression::escape(name)),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(text);
    if (!match.hasMatch())
        return std::nullopt;

    bool ok = false;
    const quint64 value = match.captured(1).toULongLong(&ok);
    if (!ok)
        return std::nullopt;
    return value;
}
}

bool XdrvStatusParser::consumeLine(const QString &line)
{
    static const QRegularExpression fieldPattern(
        QStringLiteral("^\\s*([a-z_]+)\\s*:\\s*(.*?)\\s*$"),
        QRegularExpression::CaseInsensitiveOption);

    const auto match = fieldPattern.match(line);
    if (!match.hasMatch())
        return false;

    const QString key = match.captured(1).toLower();
    const QString value = match.captured(2).trimmed();

    if (key == QStringLiteral("state"))
    {
        m_status = XdrvStatus{};
        m_status.state = value;
        m_inSnapshot = true;
        return false;
    }

    if (!m_inSnapshot)
        return false;

    if (key == QStringLiteral("power"))
    {
        m_status.power = value;
    }
    else if (key == QStringLiteral("mode"))
    {
        m_status.mode = value;
    }
    else if (key == QStringLiteral("pos"))
    {
        const auto number = firstInteger(value);
        if (number && *number >= std::numeric_limits<qint32>::min()
            && *number <= std::numeric_limits<qint32>::max())
            m_status.positionMilliRev = static_cast<qint32>(*number);
    }
    else if (key == QStringLiteral("vel"))
    {
        const auto number = firstInteger(value);
        if (number && *number >= std::numeric_limits<qint32>::min()
            && *number <= std::numeric_limits<qint32>::max())
            m_status.velocityMilliRevPerSec = static_cast<qint32>(*number);
    }
    else if (key == QStringLiteral("current"))
    {
        const auto number = firstInteger(value);
        if (number && *number >= std::numeric_limits<qint32>::min()
            && *number <= std::numeric_limits<qint32>::max())
            m_status.currentMilliamp = static_cast<qint32>(*number);
    }
    else if (key == QStringLiteral("calib"))
    {
        m_status.calibrated = value.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0;
    }
    else if (key == QStringLiteral("stall"))
    {
        const auto numbers = QRegularExpression(QStringLiteral("(\\d+)\\s*,\\s*overload\\s+(\\d+)"))
                                 .match(value);
        if (numbers.hasMatch())
        {
            m_status.stalled = numbers.captured(1).toInt();
            m_status.overloaded = numbers.captured(2).toInt();
        }
    }
    else if (key == QStringLiteral("cfg"))
    {
        const auto currentLimit = namedUnsigned(value, QStringLiteral("limit"));
        const auto velocityLimit = namedUnsigned(value, QStringLiteral("vel"));
        if (currentLimit && *currentLimit <= static_cast<quint64>(std::numeric_limits<int>::max()))
            m_status.currentLimitMilliamp = static_cast<int>(*currentLimit);
        if (velocityLimit)
            m_status.velocityLimitRevPerSec = static_cast<double>(*velocityLimit) / 51200.0;
    }
    else if (key == QStringLiteral("isr"))
    {
        static const QRegularExpression maxUsPattern(QStringLiteral("max=\\d+\\s+cyc\\s+\\(([\\d.]+)\\s+us\\)"));
        const auto maxMatch = maxUsPattern.match(value);
        if (maxMatch.hasMatch())
        {
            bool ok = false;
            const double maxUs = maxMatch.captured(1).toDouble(&ok);
            if (ok)
                m_status.isrMaxMicroseconds = maxUs;
        }
    }
    else if (key == QStringLiteral("isr_warn"))
    {
        m_status.isrWarning = value;
    }
    else if (key == QStringLiteral("spi_err"))
    {
        const auto count = firstInteger(value);
        if (count && *count >= 0)
            m_status.spiErrors = static_cast<quint64>(*count);
    }
    else if (key == QStringLiteral("spi_fail"))
    {
        m_status.spiFailure = value;
    }
    else if (key == QStringLiteral("uart"))
    {
        m_status.uartOverruns = namedUnsigned(value, QStringLiteral("ore"));
        m_status.uartErrors = namedUnsigned(value, QStringLiteral("err"));
    }
    else if (key == QStringLiteral("ctrl"))
    {
        m_status.controller = value;
    }
    else if (key == QStringLiteral("dce"))
    {
        m_status.dce = value;
    }
    else if (key == QStringLiteral("can"))
    {
        m_status.can = value;
        m_inSnapshot = false;
        return true;
    }

    return false;
}

const XdrvStatus &XdrvStatusParser::status() const
{
    return m_status;
}

bool XdrvStatusParser::isCollectingSnapshot() const
{
    return m_inSnapshot;
}

void XdrvStatusParser::reset()
{
    m_status = XdrvStatus{};
    m_inSnapshot = false;
}
