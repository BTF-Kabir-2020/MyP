#include "FileTransferWorker.h"
#include <QDataStream>
#include <QFileInfo>
#include <QTcpSocket>

FileTransferWorker::FileTransferWorker(QTcpSocket *socket, const QString &filePath, QObject *parent)
    : QThread(parent), socket(socket), filePath(filePath) {}

void FileTransferWorker::run()
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit finished();
        return;
    }

    QDataStream out(socket);
    out << QString("FILE:") + QFileInfo(filePath).fileName();

    qint64 totalBytes = file.size();
    qint64 bytesSent = 0;
    const qint64 chunkSize = 8192; // افزایش اندازه بافر به 8 کیلوبایت
    QByteArray buffer;

    while (!file.atEnd()) {
        buffer = file.read(chunkSize);
        socket->write(buffer);
        socket->waitForBytesWritten();
        bytesSent += buffer.size();
        emit progressUpdated(bytesSent, totalBytes);
    }

    file.close();
    emit finished();
}
