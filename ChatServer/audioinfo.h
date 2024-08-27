#ifndef AUDIOINFO_H
#define AUDIOINFO_H

#include <QAudioFormat>
#include <QFile>
#include <QIODevice>



class AudioInfo : public QIODevice
{
    Q_OBJECT

public:
    AudioInfo(const QAudioFormat &format, QObject *parent = nullptr);

    void start();
    void stop();

    qreal level() const { return m_level; }

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    const QAudioFormat m_format;
    QFile m_outputFile;
    quint32 m_maxAmplitude = 0;
    qreal m_level = 0.0; // 0.0 <= m_level <= 1.0

signals:
    void update();
};

#endif // AUDIOINFO_H
