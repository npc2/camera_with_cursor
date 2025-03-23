#include "AudioPanel.h"
#include "dbgout.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QStyleOption>
#include <QStyle>
#include <QIcon>
#include <QFontMetrics>

// SpectrumWidget 实现
SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent)
{
    // 设置初始大小
    setMinimumSize(200, 100);
    
    // 初始化频谱数据
    for (int i = 0; i < 16; ++i) {
        m_spectrumData.append(0.0f);
    }
    
    // 设置背景为暗色，让频谱更醒目
    setStyleSheet("background-color: #202530;");
}

void SpectrumWidget::setSpectrumData(const QList<float> &data)
{
    m_spectrumData = data;
    update(); // 触发重绘
}

void SpectrumWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制背景
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
    
    // 创建渐变
    QLinearGradient gradient(0, height(), 0, 0);
    gradient.setColorAt(0.0, QColor(0, 210, 255));   // 蓝色底部
    gradient.setColorAt(0.5, QColor(0, 255, 140));   // 绿色中部
    gradient.setColorAt(1.0, QColor(255, 60, 0));    // 红色顶部
    
    // 绘制频谱条
    const int bands = m_spectrumData.size();
    if (bands <= 0) return;
    
    const int barWidth = width() / bands;
    const int spacing = 2;
    const int actualBarWidth = barWidth - spacing;
    
    for (int i = 0; i < bands; ++i) {
        const float magnitude = m_spectrumData[i];
        const int barHeight = qRound(magnitude * height());
        
        QRect rect(i * barWidth, height() - barHeight, actualBarWidth, barHeight);
        painter.fillRect(rect, gradient);
        
        // 绘制反光效果
        QLinearGradient glassEffect(rect.topLeft(), rect.bottomRight());
        glassEffect.setColorAt(0.0, QColor(255, 255, 255, 90));
        glassEffect.setColorAt(0.5, QColor(255, 255, 255, 20));
        glassEffect.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(rect, glassEffect);
    }
    
    // 绘制网格线
    painter.setPen(QPen(QColor(255, 255, 255, 40), 1, Qt::DashLine));
    const int gridLines = 5;
    for (int i = 1; i < gridLines; ++i) {
        int y = height() * i / gridLines;
        painter.drawLine(0, y, width(), y);
    }

    // 绘制音频频谱文本
    QString text = "音频频谱";
    QRect textRect = painter.boundingRect(QRect(0, 0, width(), 30), Qt::AlignCenter, text);
    painter.setPen(Qt::gray); // 设置文本颜色
    painter.setFont(QFont("Arial", 10)); // 设置字体
    int x = (width() - textRect.width()) / 2;
    int y = textRect.height();
    painter.drawText(x, y, text); // 在指定位置绘制文本
}

// VolumeSlider 实现
VolumeSlider::VolumeSlider(QWidget *parent)
    : QWidget(parent), m_isMuted(false), m_volume(1.0)
{
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(8);
    
    // 创建标题标签
    QLabel *titleLabel = new QLabel("音量控制", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-weight: bold; color: #333333;");
    
    // 创建静音按钮
    m_muteButton = new QPushButton(this);
    m_muteButton->setCheckable(true);
    m_muteButton->setChecked(false);
    m_muteButton->setFixedSize(26, 26);
    m_muteButton->setCursor(Qt::PointingHandCursor);
    updateMuteButtonIcon();
    
    // 创建滑块
    m_slider = new QSlider(Qt::Vertical, this);
    m_slider->setRange(0, 100);
    m_slider->setValue(100);
    m_slider->setFixedHeight(120);
    m_slider->setStyleSheet(
        "QSlider::groove:vertical {"
        "  background: #E0E0E0;"
        "  width: 8px;"
        "  border-radius: 4px;"
        "}"
        "QSlider::handle:vertical {"
        "  background: #4CAF50;"
        "  border: 1px solid #C0C0C0;"
        "  height: 18px;"
        "  margin: 0 -4px;"
        "  border-radius: 9px;"
        "}"
    );
    
    // 创建音量标签
    m_volumeLabel = new QLabel("100%", this);
    m_volumeLabel->setAlignment(Qt::AlignCenter);
    m_volumeLabel->setStyleSheet("font-weight: bold; color: #333333;");
    
    // 添加到布局
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(m_slider, 0, Qt::AlignHCenter);
    mainLayout->addWidget(m_volumeLabel);
    mainLayout->addWidget(m_muteButton, 0, Qt::AlignHCenter);
    
    // 连接信号
    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
        m_volume = value / 100.0f;
        updateVolumeDisplay();
        emit volumeChanged(m_volume);
    });
    
    connect(m_muteButton, &QPushButton::toggled, this, [this](bool checked) {
        m_isMuted = checked;
        updateVolumeDisplay();
        updateMuteButtonIcon();
        emit muteToggled(m_isMuted);
    });
    
    // 设置固定宽度
    setFixedWidth(80);
}

void VolumeSlider::setVolume(float volume)
{
    m_volume = qBound(0.0f, volume, 1.0f);
    m_slider->blockSignals(true);
    m_slider->setValue(qRound(m_volume * 100));
    m_slider->blockSignals(false);
    updateVolumeDisplay();
}

float VolumeSlider::volume() const
{
    return m_volume;
}

void VolumeSlider::setMuted(bool muted)
{
    if (m_isMuted != muted) {
        m_isMuted = muted;
        m_muteButton->blockSignals(true);
        m_muteButton->setChecked(muted);
        m_muteButton->blockSignals(false);
        updateVolumeDisplay();
    }
}

bool VolumeSlider::isMuted() const
{
    return m_isMuted;
}

void VolumeSlider::updateVolumeDisplay()
{
    int displayVolume = m_isMuted ? 0 : static_cast<int>(m_volume * 100);
    m_volumeLabel->setText(QString("%1%").arg(displayVolume));
    
    // 更新滑块状态
    if (m_isMuted) {
        m_slider->setEnabled(false);
    } else {
        m_slider->setEnabled(true);
        m_slider->setValue(m_volume * 100);
    }
}

// 更新静音按钮图标
void VolumeSlider::updateMuteButtonIcon()
{
    if (m_isMuted) {
        // 使用静音图标和红色背景
        m_muteButton->setStyleSheet(
            "QPushButton {"
            "  background-color: #FF5555;"
            "  border: 1px solid #C0C0C0;"
            "  border-radius: 3px;"
            "  padding: 2px;"
            "  min-width: 22px;"
            "  min-height: 22px;"
            "  text-align: center;"
            "  color: white;"
            "}"
            "QPushButton:hover {"
            "  background-color: #FF7777;"
            "}"
        );
        m_muteButton->setText("\u274F"); // 静音符号 (❏)
    } else {
        // 使用非静音图标和绿色背景
        m_muteButton->setStyleSheet(
            "QPushButton {"
            "  background-color: #4CAF50;"
            "  border: 1px solid #C0C0C0;"
            "  border-radius: 3px;"
            "  padding: 2px;"
            "  min-width: 22px;"
            "  min-height: 22px;"
            "  text-align: center;"
            "  color: white;"
            "}"
            "QPushButton:hover {"
            "  background-color: #45a049;"
            "}"
        );
        m_muteButton->setText("\u266B"); // 音符符号 (♫)
    }
    m_muteButton->setFont(QFont("Arial", 12, QFont::Bold));
}

// AudioPanel 实现
AudioPanel::AudioPanel(QWidget *parent)
    : QWidget(parent),
      m_audioAnalyzer(new AudioSpectrumAnalyzer(this))
{
    setupUI();
    
    // 连接信号槽，使用旧式连接方式
    connect(m_audioAnalyzer, SIGNAL(spectrumDataChanged(const QList<float>&)),
            this, SLOT(updateSpectrum(const QList<float>&)));
    connect(m_audioAnalyzer, SIGNAL(volumeChanged(float)),
            m_volumeSlider, SLOT(setVolume(float)));
    connect(m_audioAnalyzer, SIGNAL(mutedChanged(bool)),
            m_volumeSlider, SLOT(setMuted(bool)));
    connect(m_audioAnalyzer, SIGNAL(audioAvailable(bool)),
            this, SLOT(updateUI()));
    
    connect(m_volumeSlider, SIGNAL(volumeChanged(float)),
            this, SLOT(onVolumeChanged(float)));
    connect(m_volumeSlider, SIGNAL(muteToggled(bool)),
            this, SLOT(onMuteToggled(bool)));
    
    // 初始化UI状态
    updateUI();
}

AudioPanel::~AudioPanel()
{
    stopAudio();
}

void AudioPanel::setupUI()
{
    // 创建水平布局
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(15);
    
    // 创建左侧面板（频谱显示）
    QVBoxLayout *leftLayout = new QVBoxLayout();
    
    // 添加频谱标题
    /*QLabel *spectrumTitle = new QLabel("音频频谱", this);
    spectrumTitle->setAlignment(Qt::AlignCenter);
    spectrumTitle->setStyleSheet("font-weight: bold; color: #333333;");*/
    
    // 创建频谱显示控件
    m_spectrumWidget = new SpectrumWidget(this);
    m_spectrumWidget->setMinimumSize(300, 200);
    m_spectrumWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    //leftLayout->addWidget(spectrumTitle);
    leftLayout->addWidget(m_spectrumWidget);
    
    // 状态标签（初始时隐藏）
    m_statusLabel = new QLabel("未检测到音频设备", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #FF5555; font-weight: bold; padding: 5px; background-color: #FFEEEE; border-radius: 3px;");
    leftLayout->addWidget(m_statusLabel);
    
    // 创建右侧控制面板
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setAlignment(Qt::AlignCenter);
    
    // 添加音量控制
    m_volumeSlider = new VolumeSlider(this);
    rightLayout->addWidget(m_volumeSlider, 0, Qt::AlignHCenter);
    
    // 设置左右两侧大小比例
    mainLayout->addLayout(leftLayout, 7);
    mainLayout->addLayout(rightLayout, 1);
    
    // 设置整体样式
    setStyleSheet("AudioPanel {"
                 "  background-color: white;"
                 "  border: 1px solid #C0C0C0;"
                 "  border-radius: 5px;"
                 "}");
    
    // 设置最小高度
    setMinimumHeight(250);
}

void AudioPanel::setAudioDevice(const QAudioDevice &device)
{
    m_audioAnalyzer->setAudioDevice(device);
    updateUI();
}

void AudioPanel::startAudio()
{
    m_audioAnalyzer->startCapture();
    updateUI();
}

void AudioPanel::stopAudio()
{
    m_audioAnalyzer->stopCapture();
    updateUI();
}

bool AudioPanel::hasAudioSupport() const
{
    return m_audioAnalyzer->hasAudio();
}

void AudioPanel::updateSpectrum(const QList<float> &data)
{
    m_spectrumWidget->setSpectrumData(data);
}

void AudioPanel::onVolumeChanged(float volume)
{
    m_audioAnalyzer->setVolume(volume);
}

void AudioPanel::onMuteToggled(bool muted)
{
    m_audioAnalyzer->setMuted(muted);
}

void AudioPanel::updateUI()
{
    bool hasAudio = m_audioAnalyzer->hasAudio();
    
    // 更新控制状态
    m_volumeSlider->setEnabled(hasAudio);
    
    // 更新状态标签 - 只在没有音频设备时显示警告
    if (!hasAudio) {
        m_statusLabel->setText("未检测到音频设备");
        m_statusLabel->setStyleSheet("color: #FF5555;");
        m_statusLabel->setVisible(true);
    } else {
        // 有音频设备时隐藏状态标签
        m_statusLabel->setVisible(false);
    }
} 