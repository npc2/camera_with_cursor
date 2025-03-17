#include <QFile>
#include <QTextStream>
#include <QDateTime>

#include "dbgout.h"
void logToFile(const QString &message) {
    QFile file("debug_log.txt");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << message << "\n";
        file.close();
    }
}

void logToConsole(const QString &message) {
    qDebug() << message;
}