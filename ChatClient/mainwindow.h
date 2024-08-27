#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QProgressBar>  // اضافه کردن QProgressBar
#include <QThread>  // اضافه کردن این خط
#include <QFile>




#include <QByteArray>
#include <QAudioInput>
#include <QBuffer>
#include <QListWidgetItem>
//#include <QSqlDatabase>
//#include <QSqlQuery>
//#include <QSqlError>
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

private slots:
    void onConnectButtonClicked();
    void onReadyRead();
    void onSendFile();
    void onSendTextMessage();

    void on_recordButton_pressed();
    void on_recordButton_released();

    void onSendFileToPhp(QString filePath);
    void onSendRecordedAudio();
    void updateProgress(qint64 currentPosition, bool isUpload);
    void writeWavHeader(QFile *file, qint64 dataSize, int sampleRate, int channels, int sampleSize);
    void processNextMessage();
    void addImageToChat(const QString &filePath);
    void addMessageToChat(const QString &message);
private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QFile *file;
    QString fileName;
    qint64 fileSize; // اضافه کردن این خط
    qint64 bytesReceived; // اضافه کردن این خط
    qint64 bytesSent; // اضافه کردن این خط
    QProgressBar *progressBar; // اضافه کردن این خط

    static const qint64 CHUNK_SIZE = 1024;






    QByteArray fileBuffer;
//    static const qint64 CHUNK_SIZE = 5120; // 5 KB
    qint64 filePosition; // Current position in the file
    QAudioInput *audioInput;
    QBuffer *audioBuffer;
    QByteArray recordedData;
//    QSqlDatabase db;  // SQLite database
    void loadMessages();
    void saveMessage(const QString &message);
    QList<QTcpSocket*> sockets;
    void showImageInChat(const QString &filePath);
//    void downloadImageFromUrl(const QString &url);
    void downloadImageFromUrl(const QString &url, const QString &originalMessage);
    void processChatMessage(const QString &message);
    QQueue<QString> messageQueue;
    QElapsedTimer startTime;

};

#endif // MAINWINDOW_H
