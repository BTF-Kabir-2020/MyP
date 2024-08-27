#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QProgressBar>
#include <QByteArray>
#include <QAudioInput>
#include <QBuffer>
#include <QListWidgetItem>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTextFrame>
#include <QScrollBar>
#include <QQueue>
#include <QDateTime>
#include <QElapsedTimer>




QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void addImageToChat(const QString &filePath);
    void addMessageToChat(const QString &message);
    void processNextMessage();
    void startDownloadOrUpload();
private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onSendFile();
    void onSendTextMessage();
    void updateProgress(qint64 currentPosition, bool isUpload);
    void onSendRecordedAudio();
    void on_recordButton_pressed();
    void on_recordButton_released();
//    void onSendFileToPhp();
//    void loadMessages();  // Load previous messages from SQLite
//    void saveMessage(const QString &message);  // Save message to SQLite

    void writeWavHeader(QFile *file, qint64 dataSize, int sampleRate, int channels, int sampleSize);

    void onSendFileToPhp(QString filePath);
    void logMessage(const QString &message);
    void startServer();
    void stopServer();
private:
    Ui::MainWindow *ui;
    QTcpServer *server;
    QTcpSocket *clientSocket;
    QFile *file;
    QString fileName;
    QProgressBar *progressBar;
    qint64 fileSize;
    qint64 bytesSent;
    QByteArray fileBuffer;
    static const qint64 CHUNK_SIZE = 5120; // 5 KB
    qint64 filePosition; // Current position in the file
    QAudioInput *audioInput;
    QBuffer *audioBuffer;
    QByteArray recordedData;
    QSqlDatabase db;  // SQLite database
    void loadMessages();
    void saveMessage(const QString &message);
    QList<QTcpSocket*> clientSockets;
    void showImageInChat(const QString &filePath);
//    void downloadImageFromUrl(const QString &url);
    void downloadImageFromUrl(const QString &url, const QString &originalMessage);
    void processChatMessage(const QString &message);
    QQueue<QString> messageQueue;
    QElapsedTimer startTime;



};

#endif // MAINWINDOW_H
