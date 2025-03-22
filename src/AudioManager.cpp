#include "AudioManager.h"
#include "dbgout.h"
#include <QDebug>
#include <QtMath>
#include <QAudioFormat>
#include <QMediaDevices>
#include <complex>
#include <cmath>
#include <QMutexLocker>

// 简单的快速傅里叶变换（FFT）实现，用于频谱分析
namespace {
    // 复数类型
    using Complex = std::complex<float>;

    // 快速傅里叶变换递归实现
    void fft(std::vector<Complex>& x) {
        const size_t N = x.size();
        if (N <= 1) return;

        // 分割成奇偶两部分
        std::vector<Complex> even(N/2), odd(N/2);
        for (size_t i = 0; i < N/2; ++i) {
            even[i] = x[i*2];
            odd[i] = x[i*2+1];
        }

        // 递归计算
        fft(even);
        fft(odd);

        // 合并结果
        for (size_t k = 0; k < N/2; ++k) {
            float angle = -2.0f * static_cast<float>(M_PI) * static_cast<float>(k) / static_cast<float>(N);
            Complex t = std::polar(1.0f, angle) * odd[k];
            x[k] = even[k] + t;
            x[k + N/2] = even[k] - t;
        }
    }
}

AudioSpectrumAnalyzer::AudioSpectrumAnalyzer(QObject *parent) 
    : QObject(parent),
      m_audioSource(nullptr),
      m_audioIO(nullptr),
      m_volume(1.0f),
      m_isMuted(false),
      m_hasAudio(false)
{
    // 初始化频谱数据数组（8个频段）
    for (int i = 0; i < 8; ++i) {
        m_spectrumData.append(0.0f);
    }
    
    // 设置定时器更新频谱
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(processAudioData()));
    m_updateTimer.setInterval(50); // 每50ms更新一次频谱图
}

AudioSpectrumAnalyzer::~AudioSpectrumAnalyzer()
{
    stopCapture();
}

void AudioSpectrumAnalyzer::setAudioDevice(const QAudioDevice &device)
{
    if (m_audioSource) {
        stopCapture();
    }
    
    m_audioDevice = device;
    m_hasAudio = !device.isNull();
    
    emit audioAvailable(m_hasAudio);
}

void AudioSpectrumAnalyzer::startCapture()
{
    if (!m_hasAudio || m_audioDevice.isNull()) {
        logToConsole("无可用音频设备");
        return;
    }
    
    if (m_audioSource) {
        stopCapture();
    }
    
    // 设置音频格式
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    
    // 创建音频输入源
    m_audioSource = new QAudioSource(m_audioDevice, format, this);
    m_audioSource->setVolume(m_isMuted ? 0.0 : m_volume);
    
    // 连接状态变化信号
    connect(m_audioSource, SIGNAL(stateChanged(QAudio::State)), this, SLOT(onStateChanged(QAudio::State)));
    
    // 开始捕获音频
    m_audioIO = m_audioSource->start();
    if (m_audioIO) {
        m_updateTimer.start();
        logToConsole("开始捕获音频");
    } else {
        logToConsole("音频捕获启动失败");
        delete m_audioSource;
        m_audioSource = nullptr;
    }
}

void AudioSpectrumAnalyzer::stopCapture()
{
    m_updateTimer.stop();
    
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_audioIO = nullptr;
    }
    
    // 重置频谱数据
    m_spectrumData.clear();
    for (int i = 0; i < 8; ++i) {
        m_spectrumData.append(0.0f);
    }
    emit spectrumDataChanged(m_spectrumData);
    
    logToConsole("停止音频捕获");
}

void AudioSpectrumAnalyzer::setMuted(bool muted)
{
    if (m_isMuted != muted) {
        m_isMuted = muted;
        
        if (m_audioSource) {
            m_audioSource->setVolume(m_isMuted ? 0.0 : m_volume);
        }
        
        emit mutedChanged(m_isMuted);
    }
}

bool AudioSpectrumAnalyzer::isMuted() const
{
    return m_isMuted;
}

void AudioSpectrumAnalyzer::setVolume(float volume)
{
    if (qFabs(m_volume - volume) > 0.01f) {
        m_volume = qBound(0.0f, volume, 1.0f);
        
        if (m_audioSource && !m_isMuted) {
            m_audioSource->setVolume(m_volume);
        }
        
        emit volumeChanged(m_volume);
    }
}

float AudioSpectrumAnalyzer::volume() const
{
    return m_volume;
}

QList<float> AudioSpectrumAnalyzer::getSpectrumData() const
{
    QMutexLocker<QMutex> locker(&const_cast<QMutex&>(m_dataMutex));
    return m_spectrumData;
}

bool AudioSpectrumAnalyzer::hasAudio() const
{
    return m_hasAudio;
}

void AudioSpectrumAnalyzer::processAudioData()
{
    if (!m_audioIO || !m_audioSource) {
        return;
    }
    
    // 读取可用的音频数据
    qint64 bytesReady = m_audioSource->bytesAvailable();
    if (bytesReady < 4096) { // 至少需要一定量的数据进行频谱分析
        return;
    }
    
    // 限制缓冲区大小
    bytesReady = qMin(bytesReady, qint64(16384));
    
    QByteArray buffer(bytesReady, 0);
    qint64 bytesRead = m_audioIO->read(buffer.data(), bytesReady);
    if (bytesRead <= 0) {
        return;
    }
    
    buffer.resize(bytesRead);
    processBuffer(buffer);
}

void AudioSpectrumAnalyzer::onStateChanged(QAudio::State state)
{
    switch (state) {
        case QAudio::ActiveState:
            logToConsole("音频状态: 活动中");
            break;
        case QAudio::SuspendedState:
            logToConsole("音频状态: 已暂停");
            break;
        case QAudio::StoppedState:
            logToConsole("音频状态: 已停止");
            break;
        case QAudio::IdleState:
            logToConsole("音频状态: 空闲");
            break;
        default:
            logToConsole("音频状态: 未知");
            break;
    }
}

void AudioSpectrumAnalyzer::processBuffer(const QByteArray &buffer)
{
    calculateSpectrum(buffer);
}

void AudioSpectrumAnalyzer::calculateSpectrum(const QByteArray &buffer)
{
    const int bytesPerSample = 2; // 16位采样
    const int sampleCount = buffer.size() / bytesPerSample;
    const int numBands = 8; // 8个频段
    
    // 如果数据太少，不进行处理
    if (sampleCount < 256) {
        return;
    }
    
    // 取最接近的2的幂次方
    int fftSize = 256;
    while (fftSize * 2 <= sampleCount && fftSize < 1024) {
        fftSize *= 2;
    }
    
    // 准备FFT输入数据
    std::vector<Complex> fftData(fftSize);
    const qint16 *samples = reinterpret_cast<const qint16*>(buffer.constData());
    
    // 应用汉宁窗函数，并转换为复数
    for (int i = 0; i < fftSize; ++i) {
        float window = 0.5f * (1.0f - cos(2.0f * static_cast<float>(M_PI) * i / (fftSize - 1))); // 汉宁窗
        fftData[i] = Complex(samples[i] * window / 32768.0f, 0);
    }
    
    // 执行FFT
    fft(fftData);
    
    // 计算各频段能量
    QList<float> bands;
    for (int i = 0; i < numBands; ++i) {
        bands.append(0.0f);
    }
    
    int binSize = (fftSize / 2) / numBands;
    
    for (int band = 0; band < numBands; ++band) {
        float sum = 0.0f;
        int startBin = band * binSize;
        int endBin = (band + 1) * binSize;
        
        // 求和该频段的能量
        for (int bin = startBin; bin < endBin; ++bin) {
            if (bin > 0 && bin < fftSize / 2) {
                float mag = std::abs(fftData[bin]);
                sum += mag * mag;
            }
        }
        
        // 对能量进行对数缩放
        if (sum > 0) {
            sum = 20.0f * log10(sum);
            sum = qBound(-60.0f, sum, 0.0f);
            sum = (sum + 60.0f) / 60.0f; // 归一化到0-1
        }
        
        bands[band] = sum;
    }
    
    // 平滑处理
    QMutexLocker<QMutex> locker(&m_dataMutex);
    for (int i = 0; i < numBands; ++i) {
        // 平滑系数
        float smoothingFactor = 0.6f;
        m_spectrumData[i] = m_spectrumData[i] * smoothingFactor + bands[i] * (1.0f - smoothingFactor);
    }
    
    // 发射更新信号
    emit spectrumDataChanged(m_spectrumData);
} 