#pragma once

#include "xdrvstatus.h"
#include "timeseriesplot.h"

#include <QByteArray>
#include <QFile>
#include <QMainWindow>
#include <QQueue>
#include <QSerialPort>
#include <QStringList>
#include <QTimer>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QCloseEvent;
class QButtonGroup;
class QCheckBox;
class QTableWidget;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void applyStyle();
    void loadSerialSettings();
    void saveSerialSettings() const;
    QString selectedPortName() const;
    void refreshPorts();
    void toggleConnection();
    void closeSerialPort();
    void updateConnectionUi();
    void updateTargetMode();
    QString selectedMotionMode() const;
    void clearChartHistory();
    void readSerialData();
    void processLine(const QString &line);
    void queueCommand(const QString &command);
    void dispatchNextCommand();
    void sendMotionTarget();
    void confirmAndQueueSequence(const QString &title, const QString &message,
                                 const QStringList &commands);
    void pollStatus();
    void updateStatusUi(const XdrvStatus &status);
    void appendLog(const QString &direction, const QString &text);
    void handleSerialError(QSerialPort::SerialPortError error);
    void startDataRecording();
    void stopDataRecording();
    void recordStatusSnapshot(const XdrvStatus &status, qint64 sampleTime);

    QSerialPort *m_serial = nullptr;
    QTimer m_pollTimer;
    QTimer m_statusTimeout;
    QTimer m_commandTimer;
    QQueue<QString> m_commandQueue;
    QByteArray m_receiveBuffer;
    XdrvStatusParser m_statusParser;
    bool m_statusRequestPending = false;

    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QComboBox *m_flowControlCombo = nullptr;
    QPushButton *m_refreshPortsButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_lastUpdateLabel = nullptr;
    QButtonGroup *m_modeGroup = nullptr;
    QCheckBox *m_positionModeCheck = nullptr;
    QCheckBox *m_velocityModeCheck = nullptr;
    QCheckBox *m_currentModeCheck = nullptr;
    QDoubleSpinBox *m_targetSpin = nullptr;
    QLabel *m_targetUnitLabel = nullptr;
    TimeSeriesPlot *m_positionPlot = nullptr;
    TimeSeriesPlot *m_velocityPlot = nullptr;
    TimeSeriesPlot *m_currentPlot = nullptr;
    QLineEdit *m_consoleInput = nullptr;
    QPlainTextEdit *m_consoleOutput = nullptr;
    QLabel *m_stateValue = nullptr;
    QLabel *m_powerValue = nullptr;
    QLabel *m_positionValue = nullptr;
    QLabel *m_velocityValue = nullptr;
    QLabel *m_currentValue = nullptr;
    QLabel *m_encoderFaultValue = nullptr;
    QLabel *m_isrMaxValue = nullptr;
    QLabel *m_isrWarningValue = nullptr;
    QLabel *m_uartValue = nullptr;
    QLabel *m_spiValue = nullptr;
    QLabel *m_controllerValue = nullptr;
    QLabel *m_dceGainsValue = nullptr;
    QLabel *m_dceOutputValue = nullptr;
    QLabel *m_canValue = nullptr;
    QLabel *m_limitsValue = nullptr;
    QTableWidget *m_recordTable = nullptr;
    QLabel *m_recordStatusLabel = nullptr;
    QPushButton *m_startRecordingButton = nullptr;
    QPushButton *m_stopRecordingButton = nullptr;
    QFile m_recordFile;
};
