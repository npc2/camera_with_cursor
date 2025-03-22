#pragma once

#include <QObject>
#include <QAudioSource>
#include <QAudioDevice>
#include <QTimer>
#include <QByteArray>
#include <QMutex>
#include <QList>

// 音频频谱分析器类，用于捕获音频数据并进行频谱分析
class AudioSpectrumAnalyzer : public QObject
{
    Q_OBJECT
public:
    explicit AudioSpectrumAnalyzer(QObject *parent = nullptr);
    ~AudioSpectrumAnalyzer();

    // 设置音频设备
    void setAudioDevice(const QAudioDevice &device);
    
    // 开始/停止音频捕获
    void startCapture();
    void stopCapture();
    
    // 音量控制
    void setMuted(bool muted);
    bool isMuted() const;
    void setVolume(float volume);
    float volume() const;
    
    // 获取频谱数据
    QList<float> getSpectrumData() const;
    
    // 是否有可用音频
    bool hasAudio() const;

signals:
    // 频谱数据更新信号
    void spectrumDataChanged(const QList<float> &data);
    
    // 音量和静音状态变化信号
    void volumeChanged(float volume);
    void mutedChanged(bool muted);
    
    // 音频可用性变化信号
    void audioAvailable(bool available);

private slots:
    void processAudioData();
    void onStateChanged(QAudio::State state);

private:
    void processBuffer(const QByteArray &buffer);
    void calculateSpectrum(const QByteArray &buffer);

    QAudioSource *m_audioSource;
    QIODevice *m_audioIO;
    QAudioDevice m_audioDevice;
    QTimer m_updateTimer;
    
    float m_volume;
    bool m_isMuted;
    bool m_hasAudio;
    
    QList<float> m_spectrumData;
    QMutex m_dataMutex;
};

