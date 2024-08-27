#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDataStream>
#include <QMessageBox>
#include <QFileInfo>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
#include <QFileDialog>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>


#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QTextFrame>
#include <QScrollBar>
#include <QLabel>
#include <QVBoxLayout>

#include <QTextBrowser>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <QElapsedTimer>


#include <QString>
#include <QDebug>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , server(new QTcpServer(this))
    , clientSocket(nullptr)
    , file(nullptr)
    , fileSize(0)
    , bytesSent(0)
    , filePosition(0)
    , audioBuffer(new QBuffer(this))
{
    ui->setupUi(this);
    ui->chatTextBrowser->setReadOnly(true);

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

    connect(ui->startServerButton, &QPushButton::clicked, this, &MainWindow::startServer);
    connect(ui->stopServerButton, &QPushButton::clicked, this, &MainWindow::stopServer);
    connect(server, &QTcpServer::newConnection, this, &MainWindow::onNewConnection);
    connect(ui->sendFileButton, &QPushButton::clicked, this, &MainWindow::onSendFile);
    connect(ui->sendMessageButton, &QPushButton::clicked, this, &MainWindow::onSendTextMessage);
    connect(ui->sendToPhpButton, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Select File to Upload");
        if (!filePath.isEmpty()) {
            onSendFileToPhp(filePath);
        } else {
            ui->chatTextBrowser->append("No file selected.");
        }
    });


    // تنظیم پایگاه داده
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("chatserver.db");

    if (!db.open()) {
        ui->chatTextBrowser->append("Failed to open database: " + db.lastError().text());
        return;
    }


    // در Constructor MainWindow
    QSqlQuery query(db);

    // ایجاد جدول جدید با فیلدهای زمان و سایر اطلاعات
    if (!query.exec("CREATE TABLE IF NOT EXISTS messages ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "message TEXT, "
                    "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)")) {
        ui->chatTextBrowser->append("Failed to create table: " + query.lastError().text());
    }


    // بارگذاری پیام‌ها از پایگاه داده
    loadMessages();



    progressBar = new QProgressBar(this);
    ui->statusbar->addPermanentWidget(progressBar);
    progressBar->setVisible(false);
}




MainWindow::~MainWindow()
{
    delete ui;
    delete server;
    delete file;
}


void MainWindow::onNewConnection()
{
    if (clientSocket) {
        clientSocket->disconnectFromHost();
        clientSocket->deleteLater();
    }

    clientSocket = server->nextPendingConnection();
    clientSockets.append(clientSocket);  // اضافه کردن کلاینت به لیست

    connect(clientSocket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);

    ui->chatTextBrowser->append("Client connected.");

    // ارسال پیام‌های قبلی به کلاینت جدید
    QSqlQuery query(db);
    if (query.exec("SELECT message, timestamp FROM messages ORDER BY timestamp ASC")) {
        while (query.next()) {
            QString message = query.value(0).toString();
            QString timestamp = query.value(1).toString();
            QDataStream out(clientSocket);
//            out << QString("%1: %2").arg(timestamp, message);
            out << QString("firstload %1: ").arg(message);
        }
    }
}




void MainWindow::onDisconnected()
{
    ui->chatTextBrowser->append("Client disconnected.");
    if (file) {
        file->close();
        delete file;
        file = nullptr;
    }

    // پاک‌سازی کلاینت از لیست
    clientSockets.removeAll(clientSocket);
    clientSocket->deleteLater();
    clientSocket = nullptr;
}


//void MainWindow::onSendTextMessage()
//{
//    QString message = ui->messageLineEdit->text();
//    if (message.isEmpty()) return;

//    ui->chatTextBrowser->append(message);
//    processChatMessage(message); // پردازش و اضافه کردن پیام
//    saveMessage(message); // ذخیره پیام در پایگاه داده
//    ui->messageLineEdit->clear();
//}
void MainWindow::onSendTextMessage()
{
    if (!clientSocket->isOpen()) {
        ui->chatTextBrowser->append("Not connected to server.");
        return;
    }

    QString message = ui->messageLineEdit->text();
    if (message.isEmpty() || message == " ") return;

    // اضافه کردن پیشوند "TEXT:" به پیام‌های متنی برای تمایز آن‌ها از سایر پیام‌ها
    QString formattedMessage = message;

    ui->chatTextBrowser->append("You: " + message);

    // ارسال پیام با استفاده از DataStream
    QDataStream out(clientSocket);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
//    out << timestamp+" "+formattedMessage;
    out << formattedMessage;




    processChatMessage(message); // پردازش و اضافه کردن پیام
    saveMessage(message); // ذخیره پیام در پایگاه داده
    ui->messageLineEdit->clear();



}




//void MainWindow::onReadyRead()
//{
//    QDataStream in(clientSocket);

//    // دریافت پیام متنی
//    if (clientSocket->bytesAvailable() > 0) {
//        QString message;
//        in >> message;

//        if (!message.startsWith("FILE:")) {
//            QString logMessage = "Client: " + message;
//            ui->chatTextBrowser->append(logMessage);
//            saveMessage(logMessage); // ذخیره پیام در پایگاه داده
//            return;
//        }

//        // دریافت فایل
//        if (!file) {
//            fileName = message.mid(5);
//            file = new QFile(fileName);
//            if (!file->open(QIODevice::WriteOnly)) {
//                ui->chatTextBrowser->append("Failed to open file for writing.");
//                return;
//            }
//            fileSize = 0; // Reset file size
//            filePosition = 0; // Reset file position
//            startTime.start(); // شروع تایمر
//        }
//    }

//    while (clientSocket->bytesAvailable()) {
//        QByteArray data = clientSocket->read(CHUNK_SIZE);
//        if (file) {
//            file->write(data);
//            filePosition += data.size();
//            fileSize = qMax(fileSize, filePosition); // Update fileSize based on received data
//            updateProgress(filePosition);
//        }
//    }

//    if (clientSocket->atEnd() && file) {
//        file->close();
//        delete file;
//        file = nullptr;
//        ui->chatTextBrowser->append("File received and saved.");
//    }
//}




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
    if (!clientSocket) {
        ui->chatTextBrowser->append("No client connected.");
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

    QDataStream out(clientSocket);
    out << QString("FILE:") + fileName;

    QByteArray buffer;
    startTime.start(); // Start timer

    while (!(buffer = file->read(CHUNK_SIZE)).isEmpty()) {
        clientSocket->write(buffer);
        bytesSent += buffer.size();
        updateProgress(bytesSent, true); // Pass true for upload
        if (!clientSocket->waitForBytesWritten(3000)) {
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
    if (!clientSocket) {
        ui->chatTextBrowser->append("No client connected.");
        return;
    }

    QString baseFileName = "recorded_audio";
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString fileName = baseFileName + "_socket_" + timestamp + ".wav";
    QString fileNamePHP = baseFileName + "_php_" + timestamp + ".wav";
    QString phpFileName = "http://localhost/uploads/" + fileNamePHP;

    // ایجاد و باز کردن فایل‌های مورد نظر
    QFile file(fileName);
    QFile filePHP(fileNamePHP);

    if (!file.open(QIODevice::WriteOnly) || !filePHP.open(QIODevice::WriteOnly)) {
        ui->chatTextBrowser->append("Failed to open file(s) for writing.");
        return;
    }

    const int sampleRate = 44100;
    const int channels = 2;
    const int sampleSize = 16;
    qint64 dataSize = recordedData.size();

    // نوشتن هدر و داده‌ها در هر دو فایل
    writeWavHeader(&file, dataSize, sampleRate, channels, sampleSize);
    file.write(recordedData);
    file.close();

    writeWavHeader(&filePHP, dataSize, sampleRate, channels, sampleSize);
    filePHP.write(recordedData);
    filePHP.close();

    QFile *fileToSend = new QFile(fileName);
    if (!fileToSend->open(QIODevice::ReadOnly)) {
        ui->chatTextBrowser->append("Failed to open file for sending.");
        return;
    }

    fileSize = fileToSend->size();
    bytesSent = 0;

    ui->uploadStatus->setMaximum(fileSize);
    ui->uploadStatus->setValue(0);
    ui->uploadStatus->setVisible(true);

    QDataStream out(clientSocket);
    out << QString("FILE:") + QFileInfo(fileName).fileName();

    QByteArray buffer;
    bool sendSuccess = false;
    while (!(buffer = fileToSend->read(CHUNK_SIZE)).isEmpty()) {
        if (clientSocket->write(buffer) == -1 || !clientSocket->waitForBytesWritten(3000)) {
            ui->chatTextBrowser->append("Failed to write data to socket.");
            break;
        }
        bytesSent += buffer.size();
        ui->uploadStatus->setValue(bytesSent);
        updateProgress(bytesSent, true);
        sendSuccess = true;
    }

    fileToSend->close();
    delete fileToSend;

    if (sendSuccess) {
        QString logMessage = "Recorded audio sent via socket: " + fileName;
        ui->chatTextBrowser->append(logMessage);

        // ارسال فایل به PHP
        onSendFileToPhp(fileNamePHP);
//        ui->chatTextBrowser->append(phpFileName);

    } else {
        ui->chatTextBrowser->append("File was not sent successfully.");
    }
}



QString extractFileName(const QString& url) {
    // پیدا کردن آخرین / در رشته
    int lastSlashIndex = url.lastIndexOf('/');

    // اگر / پیدا شد، استخراج زیررشته از بعد از آن
    if (lastSlashIndex != -1) {
        return url.mid(lastSlashIndex + 1);  // +1 برای نادیده گرفتن خود /
    }

    // اگر / پیدا نشد، رشته اصلی را برگردانید
    return url;
}

void MainWindow::onSendFileToPhp(QString filePath)
{
    if (filePath.isEmpty()) {
        ui->chatTextBrowser->append("File path is empty.");
        return;
    }

    // بررسی نوع فایل
    QString fileExtension = QFileInfo(filePath).suffix().toLower();
    bool isImage = (fileExtension == "jpg" || fileExtension == "png");

    if (isImage) {
        QString localDir = QDir::currentPath() + "/images/";
        QDir().mkpath(localDir);  // ایجاد دایرکتوری در صورت عدم وجود
        QString localFilePath = localDir + QFileInfo(filePath).fileName();
        if (!QFile::copy(filePath, localFilePath)) {
            ui->chatTextBrowser->append("Failed to copy image file.");
            return;
        }
        showImageInChat(localFilePath);
    }

    QUrl url("http://localhost/upload.php");
    QNetworkRequest request(url);

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"file\"; filename=\"" + QFileInfo(filePath).fileName() + "\""));

    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        ui->chatTextBrowser->append("Failed to open file for reading.");
        delete file;  // Delete file object if not used
        delete multiPart;  // Clean up multiPart if file can't be opened
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

    connect(reply, &QNetworkReply::uploadProgress, [this, totalBytes, &bytesSent, &timer](qint64 bytesSentNow, qint64) {
        bytesSent = bytesSentNow;
        double elapsedTime = timer.elapsed() / 1000.0; // in seconds
        double speed = (elapsedTime > 0) ? (bytesSent / 1024.0) / elapsedTime : 0.0; // in KB/s
        ui->uploadStatus->setValue(static_cast<int>((bytesSent / static_cast<double>(totalBytes)) * 100));
        ui->rateStatus->setText(QString("Rate: %1 KB/s").arg(QString::number(speed, 'f', 2)));
        ui->fileSize->setText(QString("File Size: %1 KB").arg(totalBytes / 1024));
    });

    connect(reply, &QNetworkReply::finished, [this, reply, filePath]() {
        if (reply->error() == QNetworkReply::NoError) {

            // استخراج نام فایل از مسیر
            QString phpfilename;
            int lastSlashIndex = filePath.lastIndexOf('/');
            if (lastSlashIndex == -1) {
                lastSlashIndex = filePath.lastIndexOf('\\'); // بررسی در صورت استفاده از \\ به عنوان جداکننده مسیر
            }
            if (lastSlashIndex != -1) {
                phpfilename = filePath.mid(lastSlashIndex + 1);  // +1 برای نادیده گرفتن خود /
            } else {
                phpfilename = filePath; // اگر '/' یا '\\' پیدا نشد، کل مسیر را به عنوان نام فایل استفاده کنید
            }

            // ساخت URL صحیح
            QString dwlink = "http://localhost/uploads/" + phpfilename;
            ui->chatTextBrowser->append(dwlink);

            // ارسال URL به سرور
            QDataStream out(clientSocket);
            out << dwlink;

            saveMessage(dwlink);

            ui->chatTextBrowser->append("File successfully uploaded to PHP server.");

        } else {
            ui->chatTextBrowser->append("Failed to upload file to PHP server: " + reply->errorString());
        }
        ui->uploadStatus->setValue(100); // Ensure progress bar is full
        reply->deleteLater();
    });



    ui->uploadStatus->setMaximum(100);
    ui->uploadStatus->setValue(0);
    ui->uploadStatus->setVisible(true);
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


// بارگذاری پیام‌ها از پایگاه داده
void MainWindow::loadMessages()
{
    QSqlQuery query(db);
    if (!query.exec("SELECT message, timestamp FROM messages ORDER BY timestamp ASC")) {
        ui->chatTextBrowser->append("Failed to load messages from database: " + query.lastError().text());
        return;
    }

    while (query.next()) {
        QString message = query.value(0).toString();
        QString timestamp = query.value(1).toString();

        // اضافه کردن پیام به چت
//        QString fullMessage = QString("%1: %2").arg(timestamp, message);
//        ui->chatTextBrowser->append(fullMessage);
        ui->chatTextBrowser->append(message);

        // پردازش پیام برای تصاویر
        processChatMessage(message);
    }
}







// ذخیره پیام‌ها در پایگاه داده
void MainWindow::saveMessage(const QString &message)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO messages (message) VALUES (:message)");
    query.bindValue(":message", message);

    if (!query.exec()) {
        ui->chatTextBrowser->append("Failed to save message to database: " + query.lastError().text());
    }
}



void MainWindow::logMessage(const QString &message)
{
    ui->chatTextBrowser->append(message);
    saveMessage(message);
}



void MainWindow::startServer()
{
    if (server->isListening()) {
        ui->chatTextBrowser->append("Server is already running.");
        return;
    }

    if (server->listen(QHostAddress::Any, 12345)) {
        ui->chatTextBrowser->append("Server started on port 12345.");
//        saveMessage("Server started on port 12345.");
    } else {
        ui->chatTextBrowser->append("Failed to start server.");
//        saveMessage("Failed to start server.");
    }
}

void MainWindow::stopServer()
{
    if (!server->isListening()) {
        ui->chatTextBrowser->append("Server is not running.");
        return;
    }

    foreach(QTcpSocket *socket, clientSockets) {
        socket->disconnectFromHost();
        socket->deleteLater();
    }
    clientSockets.clear();

    server->close();
    ui->chatTextBrowser->append("Server stopped.");
//    saveMessage("Server stopped.");
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



    ui->chatTextBrowser->append("");
    ui->chatTextBrowser->append(imageHtml);
    ui->chatTextBrowser->append("");

    qDebug() << "Image successfully loaded and displayed.";
}








// اضافه کردن پیام متنی به QTextBrowser
void MainWindow::addMessageToChat(const QString &message)
{
    ui->chatTextBrowser->append(message);
    saveMessage(message); // add testing
}

// اضافه کردن تصویر به QTextBrowser
void MainWindow::addImageToChat(const QString &filePath)
{
    QString imageHtml = QString("<img src='%1' width='200' />").arg(filePath);
    ui->chatTextBrowser->append(imageHtml);
}

// پردازش پیام برای شناسایی لینک تصویر
//void MainWindow::processChatMessage(const QString &message)
//{
//    // بررسی وجود لینک تصویر در پیام
//    QRegularExpression regex("(http(s)?://[^\\s]+\\.(jpg|png))", QRegularExpression::CaseInsensitiveOption);
//    QRegularExpressionMatch match = regex.match(message);

//    if (match.hasMatch()) {
//        QString imageUrl = match.captured(1);
//        downloadImageFromUrl(imageUrl, message);
////    } else {
////        addMessageToChat(message); // اگر پیام شامل تصویر نباشد، به چت اضافه کن
//    }
//}


void MainWindow::processChatMessage(const QString &message)
{
    // بررسی وجود لینک تصویر در پیام
    QRegularExpression regex("(http(s)?://[^\\s]+\\.(jpg|png))", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = regex.match(message);

    if (match.hasMatch()) {
        QString imageUrl = match.captured(1);
        downloadImageFromUrl(imageUrl, message);
    } else {
        // اگر پیام شامل تصویر نباشد، به چت اضافه کن
        addMessageToChat(message);
    }
}





void MainWindow::onReadyRead()
{
    QDataStream in(clientSocket);

    while (clientSocket->bytesAvailable() > 0) {
        QString message;
        in >> message;


//        if (message.isEmpty() || message == "Client:" || message == "Client: " || message == "client:" || message == "client: " || message == "Client: " || message == " Client:" || message == "Client: Client:" || message == ": Client: \"" ) {
//            return;  // نمایش و ذخیره پیام متوقف شود
//        }

        if (message.isEmpty()) {
            return;  // نمایش و ذخیره پیام متوقف شود
        }

        if (message == "REQUEST_CHAT_HISTORY") {
            // The client requested chat history, send it from the database
            QSqlQuery query(db);
            if (query.exec("SELECT message, timestamp FROM messages ORDER BY timestamp ASC")) {
                while (query.next()) {
                    QString historyMessage = query.value(0).toString();


                    if (historyMessage == "Client:" || historyMessage == "Client: " || historyMessage == " Client:" || historyMessage == "Client: Client:" || historyMessage == ": Client: \"" ) {
                        return;  // نمایش و ذخیره پیام متوقف شود
                    }

                    QString timestamp = query.value(1).toString();
                    QDataStream out(clientSocket);
//                    out << QString("%1: %2").arg(timestamp, historyMessage);
                    out << QString("%1").arg(historyMessage);
                }
            } else {
                ui->chatTextBrowser->append("Failed to load chat history: " + query.lastError().text());
            }
        } else if (!message.startsWith("FILE:")) {
            // Normal chat message handling
            QString logMessage = "Client: " + message;
            saveMessage(logMessage); // Save the message to the database
//            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
//            ui->chatTextBrowser->append(timestamp+" "+logMessage);
            ui->chatTextBrowser->append(logMessage);
            processChatMessage(message); // load kardane aks
        } else {
            // Handle file reception (code remains unchanged)
            if (!file) {
                fileName = message.mid(5); // Extract the file name from the message
                file = new QFile(fileName);
                if (!file->open(QIODevice::WriteOnly)) {
                    ui->chatTextBrowser->append("Failed to open file for writing.");
                    return;
                }
                fileSize = 0;
                filePosition = 0;
                startTime.start(); // Start the timer
            }
        }

        // Handle file data reception (code remains unchanged)
        while (clientSocket->bytesAvailable()) {
            QByteArray data = clientSocket->read(CHUNK_SIZE);
            if (file) {
                file->write(data);
                filePosition += data.size();
                fileSize = qMax(fileSize, filePosition);
                updateProgress(filePosition, true);
            }
        }

        // If file transfer is complete, close the file (code remains unchanged)
        if (clientSocket->atEnd() && file) {
            file->close();
            delete file;
            file = nullptr;
            ui->chatTextBrowser->append("File received and saved.");
//            saveMessage("File received and saved."); // Save the confirmation message
        }
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
            QString imagePath = QDir::currentPath() + "/temp_image.png";

            QFile imageFile(imagePath);
            if (imageFile.open(QIODevice::WriteOnly)) {
                imageFile.write(imageData);
                imageFile.close();

                // نمایش تصویر در چت
                ui->chatTextBrowser->append("");
                QString imageHtml = QString("<img src='%1' width='200' />").arg(imagePath);
                ui->chatTextBrowser->append("");
                ui->chatTextBrowser->append(originalMessage);
                ui->chatTextBrowser->append("");
                ui->chatTextBrowser->append(imageHtml);
                ui->chatTextBrowser->append("");
            } else {
                ui->chatTextBrowser->append("Failed to save image file.");
            }
        } else {
            ui->chatTextBrowser->append("Failed to download image: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

//void MainWindow::downloadImageFromUrl(const QString &url, const QString &originalMessage)
//{
//    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
//    QNetworkRequest request(url);
//    QNetworkReply *reply = manager->get(request);

//    connect(reply, &QNetworkReply::finished, [this, reply, originalMessage]() {
//        if (reply->error() == QNetworkReply::NoError) {
//            QByteArray imageData = reply->readAll();
//            QPixmap image;
//            image.loadFromData(imageData);
//            if (!image.isNull()) {

////                addMessageToChat(originalMessage);
//                ui->chatTextBrowser->append(""); // افزودن خط جدید برای تصویر
//                ui->chatTextBrowser->append("<img width='200' src='data:image/png;base64," + imageData.toBase64() + "' />");
//                ui->chatTextBrowser->append(""); // افزودن خط جدید برای تصویر
//            } else {
//                ui->chatTextBrowser->append("Failed to load image.");
//            }
//        } else {
//            ui->chatTextBrowser->append("Failed to download image: " + reply->errorString());
//        }
//        reply->deleteLater();
//        processNextMessage(); // پردازش پیام بعدی
//    });
//}








void MainWindow::startDownloadOrUpload()
{
    startTime.start();
}

