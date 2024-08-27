#ifndef FILETRANSFERWORKER_H
#define FILETRANSFERWORKER_H

#include <QThread>
#include <QTcpSocket>
#include <QFile>

class FileTransferWorker : public QThread
{
    Q_OBJECT

public:
    FileTransferWorker(QTcpSocket *socket, const QString &filePath, QObject *parent = nullptr);
    void run() override;

signals:
    void progressUpdated(qint64 bytesSent, qint64 totalBytes);
    void finished();

private:
    QTcpSocket *socket;
    QString filePath;
};

#endif // FILETRANSFERWORKER_H
