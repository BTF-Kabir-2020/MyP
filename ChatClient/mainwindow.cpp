#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDataStream>
#include <QMessageBox>


#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QFile>
#include <QFileDialog>





#include <QDataStream>
#include <QMessageBox>
#include <QFileInfo>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
//#include <QSqlDatabase>
//#include <QSqlQuery>
//#include <QSqlError>
#include <QVariant>
#include <QTextFrame>
#include <QScrollBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QFileInfo>
#include <QElapsedTimer>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(new QTcpSocket(this))
    , file(nullptr)



    , fileSize(0)
    , bytesSent(0)
    , filePosition(0)
    , audioBuffer(new QBuffer(this))
{
    ui->setupUi(this);
    ui->chatTextBrowser->setReadOnly(true);

    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, [this]() {
        ui->chatTextBrowser->append("Disconnected from server.");
    });
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
//    connect(ui->sendFileButton, &QPushButton::clicked, this, &MainWindow::onSendFile);
//    connect(ui->sendMessageButton, &QPushButton::clicked, this, &MainWindow::onSendTextMessage);
//    connect(ui->sendToPhpButton, &QPushButton::clicked, this, &MainWindow::onSendFileToPhp);










    // تنظیمات برای ضبط صدا
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (!info.isFormatSupported(format)) {
        qWarning() << "Default format not supported, trying to use the nearest.";
        format = info.nearestFormat(format);
    }

    audioInput = new QAudioInput(info, format, this);

    connect(ui->sendFileButton_2, &QPushButton::clicked, this, &MainWindow::onSendFile);
    connect(ui->sendMessageButton_2, &QPushButton::clicked, this, &MainWindow::onSendTextMessage);
    connect(ui->sendToPhpButton_2, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Select File to Upload");
        if (!filePath.isEmpty()) {
            onSendFileToPhp(filePath);
        } else {
            ui->chatTextBrowser->append("No file selected.");
        }
    });


    // بارگذاری پیام‌ها از پایگاه داده
//    loadMessages();



    progressBar = new QProgressBar(this);
    ui->statusbar->addPermanentWidget(progressBar);
    progressBar->setVisible(false);

}

MainWindow::~MainWindow()
{
    delete ui;
    delete file;
}

void MainWindow::onConnectButtonClicked()
{
    QString ip = ui->ipLineEdit->text();
    int port = ui->portLineEdit->text().toInt();

    socket->connectToHost(ip, port);
    if (socket->waitForConnected(3000)) {
        ui->chatTextBrowser->append("Connected to server.");

        // ارسال درخواست برای دریافت تاریخچه چت
//        QDataStream out(socket);
//        out << QString("REQUEST_CHAT_HISTORY");
    } else {
        ui->chatTextBrowser->append("Failed to connect to server.");
    }
}



void MainWindow::onReadyRead()
{
    QDataStream in(socket);

    while (socket->bytesAvailable() > 0) {
        QString message;
        in >> message;

        if (message.isEmpty()) {
            return;  // نمایش و ذخیره پیام متوقف شود
        }

        if (!message.startsWith("FILE:")) {
            if (!message.startsWith(("firstload"))) {
                addMessageToChat("Client: " + message);
                processChatMessage(message); // load kardane aks
            } else {
                message = message.replace("firstload Client: ","");
                message = message.replace("firstload ","");
                message = message.replace("firstload","");
                message = message.trimmed();
                //  message = message.replace(QRegularExpression("\\s*$"), "");
                message = message.replace(QRegularExpression(":$"), "");
                //  message = message.replace(QRegularExpression(":\\s*$"), "");
                addMessageToChat(message);
                processChatMessage(message); // load kardane aks
            }
        } else {
            // مدیریت دریافت فایل
            if (!file) {
                fileName = message.mid(5); // دریافت نام فایل
                file = new QFile(fileName);
                if (!file->open(QIODevice::WriteOnly)) {
                    ui->chatTextBrowser->append("Failed to open file for writing.");
                    return;
                }
                fileSize = 0;
                filePosition = 0;
                startTime.start(); // شروع تایمر
            }
        }

        // پردازش ادامه فایل
        if (file) {
            while (socket->bytesAvailable()) {
                QByteArray data = socket->read(CHUNK_SIZE);
                file->write(data);
                filePosition += data.size();
                fileSize = qMax(fileSize, filePosition);
                updateProgress(filePosition, true);
            }

            if (socket->atEnd()) {
                file->close();
                delete file;
                file = nullptr;

                // پیام ذخیره شدن فایل
                addMessageToChat("File received and saved.");

                // بررسی نام فایل برای وجود عبارت "php" و نمایش آن در چت
                if (fileName.contains("php", Qt::CaseInsensitive)) {
                    addMessageToChat("File name: " + fileName);
                }
            }
        }
    }
}








void MainWindow::updateProgress(qint64 currentPosition, bool isUpload)
{
    if (fileSize <= 0) {
        return; // Avoid division by zero
    }

    int progress = static_cast<int>(100 * currentPosition / fileSize);
    ui->uploadStatus->setValue(progress); // Update progress bar

    qint64 elapsedTimeMillis = startTime.elapsed(); // Time in milliseconds
    if (elapsedTimeMillis > 0) {
        qint64 bytesPerSecond = currentPosition / (elapsedTimeMillis / 1000.0); // Bytes per second
        QString rateText = QString("Rate: %1 KB/s").arg(bytesPerSecond / 1024.0, 0, 'f', 2);
        ui->rateStatus->setText(rateText); // Update rate label
    }

    QString fileSizeText = QString("File Size: %1 KB").arg(fileSize / 1024.0, 0, 'f', 2);
    ui->fileSize->setText(fileSizeText); // Update file size label
}


void MainWindow::onSendFile()
{
    if (!socket) {
        ui->chatTextBrowser->append("No server connected.");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(this, "Select File");
    if (filePath.isEmpty()) return;

    QString fileName = QFileInfo(filePath).fileName();
    QString baseName = QFileInfo(filePath).baseName();
    QString extension = QFileInfo(filePath).suffix();
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");

    if (fileName.contains('.')) {
        fileName = baseName + "_socket_" + timestamp + "." + extension;
    } else {
        fileName = baseName + "_socket_" + timestamp;
    }

    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        ui->chatTextBrowser->append("Failed to open file for reading.");
        return;
    }

    fileSize = file->size();
    bytesSent = 0;
    filePosition = 0;

    ui->uploadStatus->setMaximum(fileSize);
    ui->uploadStatus->setValue(0);
    ui->uploadStatus->setVisible(true);

    QDataStream out(socket);
    out << QString("FILE:") + fileName;

    QByteArray buffer;
    startTime.start(); // Start timer

    while (!(buffer = file->read(CHUNK_SIZE)).isEmpty()) {
        socket->write(buffer);
        bytesSent += buffer.size();
        updateProgress(bytesSent, true); // Pass true for upload
        if (!socket->waitForBytesWritten(3000)) {
            ui->chatTextBrowser->append("Failed to write data to socket.");
            break;
        }
    }

//    progressBar->setVisible(false);
    ui->uploadStatus->setMaximum(100);
    file->close();
    delete file;

    QString logMessage = "File sent via socket: " + fileName;
    ui->chatTextBrowser->append(logMessage);
    saveMessage(logMessage); // Save message to database
}







void MainWindow::onSendTextMessage()
{
    if (!socket->isOpen()) {
        ui->chatTextBrowser->append("Not connected to server.");
        return;
    }

    QString message = ui->messageLineEdit_2->text();
    if (message.isEmpty() || message == " ") return;

    ui->chatTextBrowser->append("You: " + message);
    QDataStream out(socket);
    out << message;
    processChatMessage(message); // load kardane aks
}



void MainWindow::onSendFileToPhp(QString filePath)
{
    if (filePath.isEmpty()) return;

    // بررسی نوع فایل
    QString fileExtension = QFileInfo(filePath).suffix().toLower();
    bool isImage = (fileExtension == "jpg" || fileExtension == "png");

    if (isImage) {
        QString localDir = QDir::currentPath() + "/images/";
        QDir().mkpath(localDir);  // ایجاد دایرکتوری در صورت عدم وجود
        QString localFilePath = localDir + QFileInfo(filePath).fileName();
        QFile::copy(filePath, localFilePath);

        showImageInChat(localFilePath);
    }

    QUrl url("http://localhost/upload.php");
    QNetworkRequest request(url);

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"" + QFileInfo(filePath).fileName() + "\""));

    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        ui->chatTextBrowser->append("Failed to open file for reading.");
        return;
    }
    filePart.setBodyDevice(file);
    file->setParent(multiPart); // File will be deleted with multiPart

    multiPart->append(filePart);

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->post(request, multiPart);
    multiPart->setParent(reply); // multiPart will be deleted with reply

    QElapsedTimer timer;
    qint64 totalBytes = file->size();
    qint64 bytesSent = 0;
    timer.start();

    connect(reply, &QNetworkReply::uploadProgress, [this, totalBytes, &bytesSent, &timer](qint64 bytesSentNow, qint64 bytesTotal) {
        bytesSent = bytesSentNow;
        double elapsedTime = timer.elapsed() / 1000.0; // in seconds
        double speed = (elapsedTime > 0) ? (bytesSent / 1024.0) / elapsedTime : 0.0; // in KB/s
        ui->uploadStatus->setValue(static_cast<int>((bytesSent / static_cast<double>(totalBytes)) * 100));
        ui->rateStatus->setText(QString("Rate: %1 KB/s").arg(QString::number(speed, 'f', 2)));
        ui->fileSize->setText(QString("File Size: %1 KB").arg(totalBytes / 1024));
    });

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            ui->chatTextBrowser->append("File successfully uploaded to PHP server.");
        } else {
            ui->chatTextBrowser->append("Failed to upload file to PHP server: " + reply->errorString());
        }
        reply->deleteLater();
        ui->uploadStatus->setValue(100); // Ensure progress bar is full
    });

    ui->uploadStatus->setMaximum(100);
    ui->uploadStatus->setValue(0);
    ui->uploadStatus->setVisible(true);
}














void MainWindow::on_recordButton_pressed()
{
    ui->recordButton->setText("Recording...");
    recordedData.clear();
    audioBuffer->setBuffer(&recordedData);
    if (!audioBuffer->open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open audio buffer for writing.";
        return;
    }
    audioInput->start(audioBuffer);
}


void MainWindow::on_recordButton_released()
{
    ui->recordButton->setText("Record");
    audioInput->stop();
    audioBuffer->close();
    onSendRecordedAudio();
}


void MainWindow::onSendRecordedAudio()
{
    if (!socket) {
        ui->chatTextBrowser->append("No server connected.");
        return;
    }

    QString baseFileName = "recorded_audio";
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString fileName = baseFileName + "_socket_" + timestamp + ".wav";

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        ui->chatTextBrowser->append("Failed to save recorded audio.");
        return;
    }

    const int sampleRate = 44100;
    const int channels = 2;
    const int sampleSize = 16;
    qint64 dataSize = recordedData.size();
    writeWavHeader(&file, dataSize, sampleRate, channels, sampleSize);

    file.write(recordedData);
    file.close();

    // Send audio file via socket
    QFile *fileToSend = new QFile(fileName);
    if (!fileToSend->open(QIODevice::ReadOnly)) {
        ui->chatTextBrowser->append("Failed to open recorded audio for sending.");
        return;
    }

    fileSize = fileToSend->size();
    bytesSent = 0;
    filePosition = 0;

    ui->uploadStatus->setMaximum(fileSize);
    ui->uploadStatus->setValue(0);
    ui->uploadStatus->setVisible(true);

    QDataStream out(socket);
    out << QString("FILE:") + QFileInfo(fileName).fileName();

    QByteArray buffer;
    while (!(buffer = fileToSend->read(CHUNK_SIZE)).isEmpty()) {
        socket->write(buffer);
        bytesSent += buffer.size();

        ui->uploadStatus->setValue(bytesSent);

        updateProgress(bytesSent, true); // Pass true for upload

        if (!socket->waitForBytesWritten(3000)) {
            ui->chatTextBrowser->append("Failed to write data to socket.");
            break;
        }
    }

    //    ui->uploadStatus->setVisible(false);

    fileToSend->close();
    delete fileToSend;

//    QString logMessage = "http://localhost/uploads/" + fileName;
//    ui->chatTextBrowser->append(logMessage);
//    saveMessage(logMessage); // Save message to database



    QString logMessage = "http://localhost/uploads/" + fileName;
    ui->chatTextBrowser->append(logMessage);

    // Send the message over the socket
    if (socket && socket->isOpen()) {
        QDataStream out(socket);
        out << logMessage;
    } else {
        ui->chatTextBrowser->append("Failed to send message to server.");
    }


    // Send the same file to PHP
    onSendFileToPhp(fileName); // Use the correct file name
}










// ذخیره پیام‌ها در پایگاه داده
void MainWindow::saveMessage(const QString &message)
{
//    if (!socket || !socket->isOpen()) {
//        ui->chatTextBrowser->append("No server connected.");
//        return;
//    }

    if (message.isEmpty()) return;
    if (socket && socket->isOpen()) {
        QDataStream out(socket);
        out << message;
    } else {
        ui->chatTextBrowser->append("Failed to send message to server.");
    }

//    if (message.isEmpty()) return;
//    QDataStream out(socket);
//    out << message;
}


void MainWindow::writeWavHeader(QFile *file, qint64 dataSize, int sampleRate, int channels, int sampleSize)
{
    // WAV header fields
    const int byteRate = sampleRate * channels * (sampleSize / 8);
    const int blockAlign = channels * (sampleSize / 8);

    // Writing WAV header
    QDataStream out(file);
    out.setByteOrder(QDataStream::LittleEndian);

    out.writeRawData("RIFF", 4);
    out << static_cast<quint32>(36 + dataSize);  // ChunkSize
    out.writeRawData("WAVE", 4);
    out.writeRawData("fmt ", 4);
    out << static_cast<quint32>(16);  // Subchunk1Size (16 for PCM)
    out << static_cast<quint16>(1);   // AudioFormat (1 for PCM)
    out << static_cast<quint16>(channels);  // NumberOfChannels
    out << static_cast<quint32>(sampleRate);  // SampleRate
    out << static_cast<quint32>(byteRate);  // ByteRate
    out << static_cast<quint16>(blockAlign);  // BlockAlign
    out << static_cast<quint16>(sampleSize);  // BitsPerSample
    out.writeRawData("data", 4);
    out << static_cast<quint32>(dataSize);  // Subchunk2Size
}









void MainWindow::showImageInChat(const QString &filePath)
{
    qDebug() << "Attempting to load image from:" << filePath;

    if (!QFile::exists(filePath)) {
        ui->chatTextBrowser->append("Image file does not exist.");
        return;
    }

    QImage image(filePath);
    if (image.isNull()) {
        ui->chatTextBrowser->append("Failed to load image.");
        return;
    }

//    QString imageHtml = QString("<img src='%1' width='100%%'/>").arg(filePath);
    QString imageHtml = QString("<img src='%1' width='200'/>").arg(filePath);


    saveMessage("");
    ui->chatTextBrowser->append("");
    saveMessage(imageHtml);
    ui->chatTextBrowser->append(imageHtml);
    saveMessage("");
    ui->chatTextBrowser->append("");

    qDebug() << "Image successfully loaded and displayed.";
}








// اضافه کردن پیام متنی به QTextBrowser
void MainWindow::addMessageToChat(const QString &message)
{
    ui->chatTextBrowser->append(message);
//    saveMessage(message); // ezafe kardam
}

// اضافه کردن تصویر به QTextBrowser
void MainWindow::addImageToChat(const QString &filePath)
{
    QString imageHtml = QString("<img src='%1' width='200' />").arg(filePath);
    ui->chatTextBrowser->append(imageHtml);
}

// پردازش پیام برای شناسایی لینک تصویر
void MainWindow::processChatMessage(const QString &message)
{
    // بررسی وجود لینک تصویر در پیام
    QRegularExpression regex("(http(s)?://[^\\s]+\\.(jpg|png))", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = regex.match(message);

    if (match.hasMatch()) {
        QString imageUrl = match.captured(1);
        downloadImageFromUrl(imageUrl, message);
//    } else {
//        addMessageToChat(message); // اگر پیام شامل تصویر نباشد، به چت اضافه کن
    }
}




void MainWindow::processNextMessage()
{
    if (messageQueue.isEmpty()) {
        return;
    }

    QString message = messageQueue.dequeue();

    // پردازش پیام
    QRegularExpression regex("(http(s)?://[^\\s]+\\.(jpg|png))", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = regex.match(message);

    if (match.hasMatch()) {
        QString imageUrl = match.captured(1);
        downloadImageFromUrl(imageUrl, message);
    } else {
        addMessageToChat(message);
        processNextMessage();
    }
}

void MainWindow::downloadImageFromUrl(const QString &url, const QString &originalMessage)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(url);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, [this, reply, originalMessage]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray imageData = reply->readAll();
            QPixmap image;
            image.loadFromData(imageData);
            if (!image.isNull()) {

//                addMessageToChat(originalMessage);
                ui->chatTextBrowser->append(""); // افزودن خط جدید برای تصویر
                ui->chatTextBrowser->append("<img width='200' src='data:image/png;base64," + imageData.toBase64() + "' />");
                ui->chatTextBrowser->append(""); // افزودن خط جدید برای تصویر
            } else {
                ui->chatTextBrowser->append("Failed to load image.");
            }
        } else {
            ui->chatTextBrowser->append("Failed to download image: " + reply->errorString());
        }
        reply->deleteLater();
        processNextMessage(); // پردازش پیام بعدی
    });
}
