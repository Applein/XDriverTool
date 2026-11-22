#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("XDriverTool"));
    QApplication::setOrganizationName(QStringLiteral("Applein"));

    MainWindow window;
    window.show();
    return app.exec();
}
