#include "audioinfo.h"

AudioInfo::AudioInfo(const QAudioFormat &format, QObject *parent)
    : QIODevice(parent), m_format(format)
{
    // تعیین حداکثر دامنه صدا بر اساس اندازه نمونه و نوع آن
    switch (m_format.sampleSize()) {
    case 8:
        m_maxAmplitude = (m_format.sampleType() == QAudioFormat::UnSignedInt) ? 255 : 127;
        break;
    case 16:
        m_maxAmplitude = (m_format.sampleType() == QAudioFormat::UnSignedInt) ? 65535 : 32767;
        break;
    case 32:
        m_maxAmplitude = (m_format.sampleType() == QAudioFormat::UnSignedInt) ? 0xffffffff : 0x7fffffff;
        break;
    default:
        m_maxAmplitude = 0;
    }

    // تنظیم فایل خروجی
    m_outputFile.setFileName("output.wav");
}

void AudioInfo::start()
{
    // باز کردن فایل برای نوشتن
    if (m_outputFile.open(QIODevice::WriteOnly)) {
        open(QIODevice::WriteOnly);
    }
}

void AudioInfo::stop()
{
    // بستن فایل و دستگاه I/O
    close();
    m_outputFile.close();
}

qint64 AudioInfo::readData(char *data, qint64 maxlen)
{
    Q_UNUSED(data)
    Q_UNUSED(maxlen)
    return 0;
}

qint64 AudioInfo::writeData(const char *data, qint64 len)
{
    if (m_maxAmplitude && m_outputFile.isOpen()) {
        m_outputFile.write(data, len);  // نوشتن داده‌های صوتی به فایل

        // محاسبه سطح صدا برای نمایش بصری
        const int channelBytes = m_format.sampleSize() / 8;
        const int sampleBytes = m_format.channelCount() * channelBytes;
        const int numSamples = len / sampleBytes;
        quint32 maxValue = 0;
        const unsigned char *ptr = reinterpret_cast<const unsigned char *>(data);

        for (int i = 0; i < numSamples; ++i) {
            for (int j = 0; j < m_format.channelCount(); ++j) {
                quint32 value = 0;

                if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
                    value = *reinterpret_cast<const quint8*>(ptr);
                } else if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::SignedInt) {
                    value = qAbs(*reinterpret_cast<const qint8*>(ptr));
                } else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
                    value = qFromLittleEndian<quint16>(ptr);
                } else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::SignedInt) {
                    value = qAbs(qFromLittleEndian<qint16>(ptr));
                } else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
                    value = qFromLittleEndian<quint32>(ptr);
                } else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::SignedInt) {
                    value = qAbs(qFromLittleEndian<qint32>(ptr));
                } else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::Float) {
                    value = qAbs(*reinterpret_cast<const float*>(ptr) * 0x7fffffff); // assumes 0-1.0
                }

                maxValue = qMax(value, maxValue);
                ptr += channelBytes;
            }
        }

        m_level = qreal(maxValue) / m_maxAmplitude;
    }

    emit update();
    return len;
}
