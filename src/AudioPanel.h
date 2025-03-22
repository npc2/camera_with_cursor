#pragma once

#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QAudioDevice>
#include "AudioManager.h"

class SpectrumWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget *parent = nullptr);
    void setSpectrumData(const QList<float> &data);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<float> m_spectrumData;
};

class VolumeSlider : public QWidget
{
    Q_OBJECT
public:
    explicit VolumeSlider(QWidget *parent = nullptr);
    float volume() const;
    bool isMuted() const;

public slots:
    void setVolume(float volume);
    void setMuted(bool muted);

signals:
    void volumeChanged(float volume);
    void muteToggled(bool muted);

private:
    QSlider *m_slider;
    QPushButton *m_muteButton;
    QLabel *m_volumeLabel;
    bool m_isMuted;
    float m_volume;
    
    void updateVolumeDisplay();
    void updateMuteButtonIcon();
};

// 前向声明
class AudioSpectrumAnalyzer;

class AudioPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AudioPanel(QWidget *parent = nullptr);
    ~AudioPanel();

    void setAudioDevice(const QAudioDevice &device);
    void startAudio();
    void stopAudio();
    bool hasAudioSupport() const;

private slots:
    void updateSpectrum(const QList<float> &data);
    void onVolumeChanged(float volume);
    void onMuteToggled(bool muted);
    void updateUI();

private:
    AudioSpectrumAnalyzer *m_audioAnalyzer;
    SpectrumWidget *m_spectrumWidget;
    VolumeSlider *m_volumeSlider;
    QLabel *m_statusLabel;
    
    void setupUI();
}; 