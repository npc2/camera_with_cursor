#include "cam_qt.h"
#include "dbgout.h"
#include "AudioPanel.h"
#include "CameraControlDialog.h"
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <climits>
#include <QMap>
#include <QTimer>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QVideoSink>
#include <QVideoFrame>
#include <QSize>
#include <QCameraFormat>
#include <QVariant>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QGridLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QMainWindow>
#include <QGroupBox>
#include <QStyle>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QStandardPaths>
#include <QDir>
#include <QTime>
#include <QCoreApplication>
#include <QFileDialog>

// Windows特定头文件，用于获取USB设备信息
#include <Windows.h>
#include <SetupAPI.h>
#include <devguid.h>
#include <initguid.h>
#include <dbt.h>

#pragma comment(lib, "setupapi.lib")

// 主窗口构造函数
cam_qt::cam_qt(QWidget* parent)
    : QMainWindow(parent), ui(new Ui_cam_qt), camera(nullptr), 
      frameCount(0), currentFPS(0), lastFrameTime(0), cameraControlDialog(nullptr),
      audioPanel(nullptr), mediaRecorder(nullptr), isRecording(false), recordingDuration(0)
{
    ui->setupUi(this);
    
    // 设置经典简约白色风格，文字更显眼
    setStyleSheet("QMainWindow { background-color: white; }"
                  "QGroupBox { border: 1px solid #C0C0C0; margin-top: 10px; }"
                  "QGroupBox::title { subcontrol-origin: margin; left: 10px; font-weight: bold; color: #000000; }"
                  "QPushButton { background-color: #F0F0F0; border: 1px solid #C0C0C0; padding: 5px; font-weight: bold; color: #000000; }"
                  "QPushButton:hover { background-color: #E0E0E0; }"
                  "QPushButton:disabled { background-color: #D0D0D0; color: #808080; }"
                  "QComboBox { background-color: white; border: 1px solid #C0C0C0; padding: 3px; color: #000000; }"
                  "QComboBox:drop-down { border: none; }"
                  "QComboBox:down-arrow { image: url(down_arrow.png); width: 12px; height: 12px; }"
                  "QComboBox QAbstractItemView { background-color: white; selection-background-color: #E0E0E0; selection-color: #000000; }"
                  "QSpinBox { border: 1px solid #C0C0C0; padding: 3px; color: #000000; background-color: white; }"
                  "QLabel { color: #000000; font-weight: bold; }"
                  "QLabel#labelPreview { background-color: #F0F0F0; border: 1px solid #C0C0C0; }");
    
    // 设置窗口标题
    setWindowTitle("摄像头及音频测试工具");
    
    // 初始化视频显示
    videoSink = new QVideoSink(this);
    if (!videoSink) {
        logToConsole("错误：创建VideoSink失败");
    } else {
        // 连接视频帧信号
        connect(videoSink, &QVideoSink::videoFrameChanged, this, &cam_qt::handleVideoFrame);
        logToConsole("VideoSink创建成功并连接信号");
    }
    
    // 初始化媒体捕获会话
    captureSession.setVideoSink(videoSink);
    
    // 初始化录制相关对象
    mediaRecorder = new QMediaRecorder(this);
    captureSession.setRecorder(mediaRecorder);
    
    // 连接录制相关信号
    connect(mediaRecorder, &QMediaRecorder::recorderStateChanged, 
            this, &cam_qt::handleRecordingStateChanged);
    connect(mediaRecorder, &QMediaRecorder::durationChanged,
            this, &cam_qt::handleRecordingDurationChanged);
    connect(mediaRecorder, &QMediaRecorder::errorOccurred,
            this, &cam_qt::handleRecordingError);
    
    // 初始化录制计时器
    recordingTimer = new QTimer(this);
    
    // 初始化FPS计时器
    fpsTimer.start();
    frameCount = 0;
    currentFPS = 0;
    fpsUpdateTimer = new QTimer(this);
    connect(fpsUpdateTimer, &QTimer::timeout, this, &cam_qt::updateFPSDisplay);
    fpsUpdateTimer->start(1000); // 每秒更新一次FPS显示
    
    // 设置音频面板
    setupAudioPanel();
    
    // 初始化预览图像
    QSize labelSize = ui->labelPreview->size();
    QImage background(labelSize, QImage::Format_RGB32);
    background.fill(QColor(240, 240, 240));
    
    QPainter painter(&background);
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 12));
    painter.drawText(background.rect(), Qt::AlignCenter, "No Frame");
    
    ui->labelPreview->setPixmap(QPixmap::fromImage(background));
    
    // 更新摄像头列表
    updateCameraList();
    
    // 初始化录制按钮状态
    updateRecordButton();
    
    logToConsole("应用程序初始化完成");
}

// 析构函数
cam_qt::~cam_qt()
{
    stopCamera();
    stopRecording();
    
    if (recordingTimer) {
        recordingTimer->stop();
        delete recordingTimer;
    }
    
    if (mediaRecorder) {
        delete mediaRecorder;
    }
    
    delete cameraControlDialog;
    delete ui;
}

// 音频面板设置
void cam_qt::setupAudioPanel()
{
    // 创建音频面板，但不立即显示
    audioPanel = new AudioPanel(this);
    audioPanel->setVisible(false);
    
    // 添加到主窗口布局
    ui->verticalLayout->addWidget(audioPanel);
    
    // 音频面板初始化完成，但显示状态将在updateCameraList中设置
    logToConsole("音频面板初始化完成");
}

// 检查是否有关联的音频设备
bool cam_qt::hasAudioDevice(const QString &cameraName)
{
    // 获取所有音频输入设备
    const QList<QAudioDevice> audioInputs = QMediaDevices::audioInputs();
    if (audioInputs.isEmpty()) {
        return false;
    }
    
    // 重置当前音频设备
    currentAudioDevice = QAudioDevice();
    
    // 尝试匹配摄像头名称和音频设备名称
    QString lowerCamName = cameraName.toLower();
    bool hasMatched = false;
    
    for (const QAudioDevice &audioDevice : audioInputs) {
        QString audioName = audioDevice.description();
        QString lowerAudioName = audioName.toLower();
        
        // 精确匹配：如果设备名称完全相同或高度相似
        if (lowerCamName == lowerAudioName || 
            (lowerCamName.contains(lowerAudioName) && lowerAudioName.length() > 3) ||
            (lowerAudioName.contains(lowerCamName) && lowerCamName.length() > 3)) {
            currentAudioDevice = audioDevice;
            logToConsole("找到完全匹配的音频设备: " + audioName);
            return true;
        }
        
        // 部分匹配：提取并比较品牌/型号部分
        QStringList camParts = lowerCamName.split(" ", Qt::SkipEmptyParts);
        QStringList audioParts = lowerAudioName.split(" ", Qt::SkipEmptyParts);
        
        // 至少要有两个部分重叠才算匹配
        int matchCount = 0;
        for (const QString &camPart : camParts) {
            if (camPart.length() < 3) continue; // 忽略太短的部分
            
            for (const QString &audioPart : audioParts) {
                if (audioPart.length() < 3) continue;
                
                if (camPart == audioPart || 
                    (camPart.contains(audioPart) && audioPart.length() > 3) ||
                    (audioPart.contains(camPart) && camPart.length() > 3)) {
                    matchCount++;
                    break;
                }
            }
        }
        
        if (matchCount >= 1) {
            currentAudioDevice = audioDevice;
            logToConsole("找到部分匹配的音频设备: " + audioName + ", 匹配度: " + QString::number(matchCount));
            hasMatched = true;
            break;
        }
        
        // 数字ID匹配
        QRegularExpression re("\\d+");
        QRegularExpressionMatchIterator camMatches = re.globalMatch(cameraName);
        QRegularExpressionMatchIterator audioMatches = re.globalMatch(audioName);
        
        QStringList camNumbers, audioNumbers;
        while (camMatches.hasNext()) {
            camNumbers.append(camMatches.next().captured());
        }
        while (audioMatches.hasNext()) {
            audioNumbers.append(audioMatches.next().captured());
        }
        
        // 如果有共同的长数字序列，认为是匹配的
        for (const QString &camNum : camNumbers) {
            if (camNum.length() >= 3 && audioNumbers.contains(camNum)) {
                currentAudioDevice = audioDevice;
                logToConsole("找到数字ID匹配的音频设备: " + audioName + ", 共同ID: " + camNum);
                hasMatched = true;
                break;
            }
        }
        
        if (hasMatched) break;
    }
    
    return hasMatched;
}

// 检查并设置音频设备
void cam_qt::checkForAudioDevice(const QString &cameraName)
{
    // 检查是否有关联的音频设备
    bool hasAudio = hasAudioDevice(cameraName);
    
    if (hasAudio && !currentAudioDevice.isNull()) {
        logToConsole("检测到音频设备: " + currentAudioDevice.description());
        audioPanel->setAudioDevice(currentAudioDevice);
        audioPanel->setVisible(true);
    } else {
        logToConsole("未检测到关联的音频设备");
        audioPanel->setAudioDevice(QAudioDevice());
        audioPanel->setVisible(false);
    }
}

// 更新摄像头列表
void cam_qt::updateCameraList()
{
    ui->comboCamera->clear();
    
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cameraDevice : cameras) {
        ui->comboCamera->addItem(cameraDevice.description(), QVariant::fromValue(cameraDevice));
    }
    
    // 如果有摄像头设备，初始化音频面板
    if (ui->comboCamera->count() > 0) {
        // 选择第一个摄像头
        ui->comboCamera->setCurrentIndex(0);
        
        // 检查关联的音频设备
        QString cameraName = ui->comboCamera->currentText();
        checkForAudioDevice(cameraName);
        
        // 更新分辨率列表
        updateResolutionList();
    } else {
        // 没有摄像头设备，隐藏音频面板
        if (audioPanel) {
            audioPanel->setVisible(false);
        }
    }
}

// 更新分辨率列表
void cam_qt::updateResolutionList()
{
    ui->comboResolution->clear();
    ui->comboFormat->clear();
    
    if (ui->comboCamera->count() == 0) {
        return;
    }
    
    QCameraDevice device = ui->comboCamera->currentData().value<QCameraDevice>();
    
    // 创建格式到分辨率的映射
    QMap<QString, QList<QSize>> formats;
    
    // 只收集YUY2和MJPEG格式
    for (const QCameraFormat &format : device.videoFormats()) {
        QString formatStr;
        switch(format.pixelFormat()) {
            case QVideoFrameFormat::Format_YUYV:
                formatStr = "YUY2";
                break;
            case QVideoFrameFormat::Format_Jpeg:
                formatStr = "MJPEG";
                break;
            default:
                continue;  // 跳过其他格式
        }
        
        if (!formats.contains(formatStr)) {
            formats[formatStr] = QList<QSize>();
        }
        formats[formatStr].append(format.resolution());
    }
    
    // 添加到格式下拉列表
    for (auto it = formats.begin(); it != formats.end(); ++it) {
        ui->comboFormat->addItem(it.key());
    }
    
    // 如果有支持的格式，选择第一个并更新分辨率列表
    if (!formats.isEmpty()) {
        ui->comboFormat->setCurrentIndex(0);
        on_comboFormat_currentIndexChanged(0);
    }
}

// 格式选择改变处理
void cam_qt::on_comboFormat_currentIndexChanged(int index)
{
    if (index >= 0 && ui->comboCamera->count() > 0) {
        QCameraDevice device = ui->comboCamera->currentData().value<QCameraDevice>();
        QString selectedFormat = ui->comboFormat->currentText();
        
        // 清空分辨率列表
        ui->comboResolution->clear();
        
        // 获取选中格式支持的分辨率，使用QSet去重
        QSet<QSize> uniqueResolutions;
        int maxFps = 0;
        
        for (const QCameraFormat &format : device.videoFormats()) {
            QString formatStr;
            switch(format.pixelFormat()) {
                case QVideoFrameFormat::Format_YUYV:
                    formatStr = "YUY2";
                    break;
                case QVideoFrameFormat::Format_Jpeg:
                    formatStr = "MJPEG";
                    break;
                default:
                    continue;
            }
            
            if (formatStr == selectedFormat) {
                uniqueResolutions.insert(format.resolution());
                maxFps = qMax(maxFps, qRound(format.maxFrameRate()));
            }
        }
        
        // 转换为列表并排序
        QList<QSize> supportedResolutions(uniqueResolutions.begin(), uniqueResolutions.end());
        std::sort(supportedResolutions.begin(), supportedResolutions.end(),
                 [](const QSize &a, const QSize &b) {
                     return a.width() * a.height() > b.width() * b.height();
                 });
        
        // 添加到分辨率下拉列表
        for (const QSize &size : supportedResolutions) {
            QString resKey = QString("%1x%2").arg(size.width()).arg(size.height());
            ui->comboResolution->addItem(resKey, QVariant::fromValue(size));
        }
        
        // 设置第一个分辨率为默认值
        if (!supportedResolutions.isEmpty()) {
            ui->comboResolution->setCurrentIndex(0);
            // 更新帧率范围
            ui->spinFrameRate->setMaximum(maxFps);
            if (ui->spinFrameRate->value() > maxFps) {
                ui->spinFrameRate->setValue(maxFps);
            }
        }
    }
}

// 分辨率改变处理
void cam_qt::on_comboResolution_currentIndexChanged(int index)
{
    if (index >= 0 && ui->comboCamera->count() > 0) {
        QCameraDevice device = ui->comboCamera->currentData().value<QCameraDevice>();
        QSize resolution = ui->comboResolution->itemData(index).value<QSize>();
        
        // 查找该分辨率下的最大帧率
        int maxFps = 0;
        for (const QCameraFormat &format : device.videoFormats()) {
            if (format.resolution() == resolution) {
                maxFps = qMax(maxFps, qRound(format.maxFrameRate()));
            }
        }
        
        // 更新帧率范围
        if (maxFps > 0) {
            ui->spinFrameRate->setMaximum(maxFps);
            if (ui->spinFrameRate->value() > maxFps) {
                ui->spinFrameRate->setValue(maxFps);
            }
        }
    }
}

// 更新FPS显示
void cam_qt::updateFPSDisplay()
{
    if (camera && camera->isActive()) {
        qint64 elapsed = fpsTimer.elapsed();
        if (elapsed > 0) {
            // 计算实时帧率
            double instantFPS = frameCount * 1000.0 / elapsed;
            
            // 平滑处理
            if (currentFPS == 0) {
                currentFPS = instantFPS;
            } else {
                currentFPS = (currentFPS * 0.7) + (instantFPS * 0.3);
            }
            
            // 重置计数器
            frameCount = 0;
            fpsTimer.restart();
        }
    } else {
        currentFPS = 0;
    }
}

// 摄像头选择改变处理
void cam_qt::on_comboCamera_currentIndexChanged(int index)
{
    if (index < 0) return;
    
    // 获取当前摄像头名称
    QString cameraName = ui->comboCamera->currentText();
    
    // 检查并设置音频设备
    checkForAudioDevice(cameraName);
    
    // 原有代码
    updateResolutionList();
}

// 停止摄像头
void cam_qt::stopCamera()
{
    // 如果正在录制，先停止录制
    if (isRecording) {
        stopRecording();
    }
    
    // 先停止音频捕获
    if (audioPanel) {
        audioPanel->stopAudio();
    }
    
    // 停止摄像头
    if (camera && camera->isActive()) {
        try {
            camera->stop();
            logToConsole("摄像头已停止");
        } catch (...) {
            logToConsole("停止摄像头时出错");
        }
    }
    
    // 清理资源前先清空会话
    captureSession.setCamera(nullptr);
    
    // 删除并重置摄像头对象
    if (camera) {
        delete camera;
        camera = nullptr;
        logToConsole("摄像头资源已释放");
    }
    
    ui->btnOpenCamera->setText("打开摄像头");
    
    // 更新录制按钮状态
    updateRecordButton();
    
    // 重置预览图像
    QSize labelSize = ui->labelPreview->size();
    QImage background(labelSize, QImage::Format_RGB32);
    background.fill(QColor(240, 240, 240));
    
    QPainter painter(&background);
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 12));
    painter.drawText(background.rect(), Qt::AlignCenter, "No Frame");
    
    ui->labelPreview->setPixmap(QPixmap::fromImage(background));
}

// 打开摄像头按钮点击处理
void cam_qt::on_btnOpenCamera_clicked()
{
    // 如果摄像头已经打开，则关闭它
    if (camera && camera->isActive()) {
        stopCamera();
        return;
    }
    
    // 打开摄像头
    if (ui->comboCamera->count() == 0) {
        QMessageBox::warning(this, tr("错误"), tr("没有可用的摄像头设备"));
        return;
    }
    
    // 先停止可能存在的旧摄像头
    stopCamera();
    
    QCameraDevice device = ui->comboCamera->currentData().value<QCameraDevice>();
    logToConsole("尝试打开摄像头：" + device.description());
    
    // 创建摄像头对象
    camera = new QCamera(device, this);
    if (!camera) {
        QMessageBox::warning(this, tr("错误"), tr("创建摄像头对象失败"));
        return;
    }
    
    // 设置预览
    captureSession.setCamera(camera);
    
    // 确保VideoSink已经设置
    if (!videoSink) {
        videoSink = new QVideoSink(this);
        connect(videoSink, &QVideoSink::videoFrameChanged, this, &cam_qt::handleVideoFrame);
    }
    captureSession.setVideoSink(videoSink);
    
    // 设置格式
    if (ui->comboResolution->count() > 0 && ui->comboFormat->count() > 0) {
        QSize resolution = ui->comboResolution->itemData(ui->comboResolution->currentIndex()).value<QSize>();
        int fps = ui->spinFrameRate->value();
        QString selectedFormat = ui->comboFormat->currentText();
        
        logToConsole(QString("设置格式: %1, 分辨率: %2x%3, 帧率: %4").arg(
            selectedFormat).arg(resolution.width()).arg(resolution.height()).arg(fps));
        
        // 查找匹配的格式
        QCameraFormat bestFormat;
        int bestFpsDiff = INT_MAX;
        
        for (const QCameraFormat &format : device.videoFormats()) {
            QString formatStr;
            switch(format.pixelFormat()) {
                case QVideoFrameFormat::Format_YUYV:
                    formatStr = "YUY2";
                    break;
                case QVideoFrameFormat::Format_Jpeg:
                    formatStr = "MJPEG";
                    break;
                default:
                    continue;
            }
            
            if (formatStr == selectedFormat && format.resolution() == resolution) {
                int fpsDiff = qAbs(qRound(format.maxFrameRate()) - fps);
                if (fpsDiff < bestFpsDiff) {
                    bestFpsDiff = fpsDiff;
                    bestFormat = format;
                }
            }
        }
        
        if (bestFormat.resolution().isValid()) {
            logToConsole("找到匹配的格式，设置摄像头格式");
            camera->setCameraFormat(bestFormat);
        } else {
            logToConsole("警告：未找到匹配的摄像头格式");
        }
    }

    try {
        // 启动摄像头
        logToConsole("开始启动摄像头...");
        camera->start();
        logToConsole("摄像头启动完成");
        ui->btnOpenCamera->setText("关闭摄像头");
        
        // 更新录制按钮状态
        updateRecordButton();
        
        // 如果有关联的音频设备，也启动音频捕获
        if (audioPanel && audioPanel->isVisible() && audioPanel->hasAudioSupport()) {
            audioPanel->startAudio();
        }
    } catch (const std::exception& e) {
        logToConsole("摄像头启动异常: " + QString(e.what()));
        QMessageBox::critical(this, tr("错误"), tr("摄像头启动失败：%1").arg(e.what()));
        stopCamera();
    } catch (...) {
        logToConsole("摄像头启动时发生未知异常");
        QMessageBox::critical(this, tr("错误"), tr("摄像头启动时发生未知错误"));
        stopCamera();
    }
}

// 设置格式按钮点击处理
void cam_qt::on_btnSetFormat_clicked()
{
    if (camera && camera->isActive() && ui->comboResolution->count() > 0) {
        QCameraDevice device = ui->comboCamera->currentData().value<QCameraDevice>();
        QSize resolution = ui->comboResolution->itemData(ui->comboResolution->currentIndex()).value<QSize>();
        int fps = ui->spinFrameRate->value();
        QString selectedFormat = ui->comboFormat->currentText();
        
        // 查找匹配的格式
        QCameraFormat bestFormat;
        int bestFpsDiff = INT_MAX;
        
        for (const QCameraFormat &format : device.videoFormats()) {
            QString formatStr;
            switch(format.pixelFormat()) {
                case QVideoFrameFormat::Format_YUYV:
                    formatStr = "YUY2";
                    break;
                case QVideoFrameFormat::Format_Jpeg:
                    formatStr = "MJPEG";
                    break;
                default:
                    continue;
            }
            
            if (formatStr == selectedFormat && format.resolution() == resolution) {
                int fpsDiff = qAbs(qRound(format.maxFrameRate()) - fps);
                if (fpsDiff < bestFpsDiff) {
                    bestFpsDiff = fpsDiff;
                    bestFormat = format;
                }
            }
        }
        
        if (bestFormat.resolution().isValid()) {
            // 需要重新启动摄像头
            camera->stop();
            camera->setCameraFormat(bestFormat);
            camera->start();
            
            QMessageBox::information(this, tr("信息"), 
                                    tr("已设置格式为 %1x%2 @ %3 FPS\n视频格式: %4")
                                    .arg(resolution.width())
                                    .arg(resolution.height())
                                    .arg(fps)
                                    .arg(selectedFormat));
        }
    }
}

// 查找摄像头按钮点击处理
void cam_qt::on_btnDetectCameras_clicked()
{
    updateCameraList();
    QMessageBox::information(this, tr("信息"), 
                            tr("已检测到 %1 个摄像头设备").arg(ui->comboCamera->count()));
}

// 处理视频帧
void cam_qt::handleVideoFrame(const QVideoFrame &frame)
{
    if (frame.isValid()) {
        // 更新帧计数
        frameCount++;
        
        // 计算帧间隔
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (lastFrameTime > 0) {
            qint64 frameInterval = currentTime - lastFrameTime;
            // 可以用于更精确的帧率计算或其他目的
        }
        lastFrameTime = currentTime;
        
        // 转换为QImage并显示
        QImage image = frame.toImage();
        if (!image.isNull()) {
            // 获取标签大小
            QSize labelSize = ui->labelPreview->size();
            
            // 创建背景图像
            QImage background(labelSize, QImage::Format_RGB32);
            background.fill(Qt::white);  // 改为白色背景
            
            // 按比例缩放图像以适应标签
            QImage scaledImage = image.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            
            // 在背景中央绘制缩放后的图像
            QPainter painter(&background);
            int x = (labelSize.width() - scaledImage.width()) / 2;
            int y = (labelSize.height() - scaledImage.height()) / 2;
            painter.drawImage(x, y, scaledImage);

            // 绘制实时帧率文本
            painter.setPen(Qt::gray); // 设置文本颜色
            painter.setFont(QFont("Arial", 8)); // 设置字体
            painter.drawText(10, labelSize.height() - 10, QString("实时帧率: %1 FPS").arg(currentFPS, 0, 'f', 1)); // 在左下角绘制文本
            
            // 显示图像
            ui->labelPreview->setPixmap(QPixmap::fromImage(background));
        }
    }
}

// 打开摄像头控制面板
void cam_qt::on_btnCameraControl_clicked()
{
    if (camera && camera->isActive()) {
        QCameraDevice device = ui->comboCamera->currentData().value<QCameraDevice>();
        
        // 删除旧的对话框实例，确保每次都创建新的
        if (cameraControlDialog) {
            delete cameraControlDialog;
            cameraControlDialog = nullptr;
        }
        
        // 创建新的对话框实例
        cameraControlDialog = new CameraControlDialog(device.id(), this);
        
        // 显示对话框
        if (cameraControlDialog) {
            cameraControlDialog->show();
        } else {
            QMessageBox::warning(this, tr("错误"), tr("无法创建图像控制对话框"));
        }
    } else {
        QMessageBox::warning(this, tr("错误"), tr("请先打开摄像头"));
    }
}

// 格式转字符串
QString cam_qt::formatToString(const QCameraFormat &format)
{
    QString pixelFormat;
    switch(format.pixelFormat()) {
        case QVideoFrameFormat::Format_YUYV:
            pixelFormat = "YUY2";
            break;
        case QVideoFrameFormat::Format_Jpeg:
            pixelFormat = "MJPEG";
            break;
        default:
            pixelFormat = "其他格式";
            break;
    }
    return QString("%1x%2 @ %3 FPS (%4)")
            .arg(format.resolution().width())
            .arg(format.resolution().height())
            .arg(format.maxFrameRate(), 0, 'f', 1)
            .arg(pixelFormat);
}

// 获取默认保存路径
QString cam_qt::getDefaultSavePath()
{
    // 使用程序所在路径
    QString appPath = QCoreApplication::applicationDirPath();
    
    // 创建当前时间戳作为文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename = QString("video_%1.mp4").arg(timestamp);
    
    return QDir(appPath).filePath(filename);
}

// 开始录制视频
void cam_qt::startRecording()
{
    if (!camera || !camera->isActive() || !mediaRecorder) {
        logToConsole("错误：无法开始录制，摄像头未激活或录制器未初始化");
        return;
    }
    
    if (mediaRecorder->recorderState() == QMediaRecorder::RecordingState) {
        logToConsole("录制已经在进行中");
        return;
    }
    
    // 设置录制的格式为MP4
    QMediaFormat mediaFormat;
    mediaFormat.setFileFormat(QMediaFormat::MPEG4);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::H264);
    
    // 如果有音频设备，添加音频编码
    if (audioPanel && audioPanel->isVisible() && audioPanel->hasAudioSupport()) {
        mediaFormat.setAudioCodec(QMediaFormat::AudioCodec::AAC);
        logToConsole("录制将包含音频");
    } else {
        logToConsole("录制不包含音频，未检测到音频设备");
    }
    
    mediaRecorder->setMediaFormat(mediaFormat);
    
    // 弹出保存文件对话框
    QString defaultPath = getDefaultSavePath();
    QString filePath = QFileDialog::getSaveFileName(this, tr("保存录制文件"),
                                                   defaultPath,
                                                   tr("MP4文件 (*.mp4)"));
    
    if (filePath.isEmpty()) {
        logToConsole("用户取消了录制");
        return;
    }
    
    mediaRecorder->setOutputLocation(QUrl::fromLocalFile(filePath));
    
    // 重置录制时长
    recordingDuration = 0;
    
    // 开始录制
    mediaRecorder->record();
    
    logToConsole("开始录制视频到: " + filePath);
    isRecording = true;
    updateRecordButton();
}

// 停止录制视频
void cam_qt::stopRecording()
{
    if (mediaRecorder && mediaRecorder->recorderState() == QMediaRecorder::RecordingState) {
        mediaRecorder->stop();
        logToConsole("停止录制视频");
    }
    
    isRecording = false;
    updateRecordButton();
}

// 更新录制按钮状态
void cam_qt::updateRecordButton()
{
    // 只有在摄像头活动时才启用录制按钮
    bool cameraActive = camera && camera->isActive();
    ui->btnRecordVideo->setEnabled(cameraActive);
    
    // 根据录制状态更新按钮文本
    if (isRecording) {
        ui->btnRecordVideo->setText("停止录制");
        ui->btnRecordVideo->setStyleSheet("background-color: #FF6060; "
                                          "border: 1px solid #C0C0C0; "
                                          "padding: 5px; "
                                          "font-weight: bold; "
                                          "color: white;");
    } else {
        ui->btnRecordVideo->setText("开始录制");
        ui->btnRecordVideo->setStyleSheet("background-color: #F0F0F0; "
                                          "border: 1px solid #C0C0C0; "
                                          "padding: 5px; "
                                          "font-weight: bold; "
                                          "color: #000000;");
    }
}

// 录制按钮点击处理
void cam_qt::on_btnRecordVideo_clicked()
{
    if (isRecording) {
        stopRecording();
    } else {
        startRecording();
    }
}

// 处理录制状态变化
void cam_qt::handleRecordingStateChanged(QMediaRecorder::RecorderState state)
{
    switch (state) {
        case QMediaRecorder::RecordingState:
            logToConsole("录制状态：录制中");
            isRecording = true;
            updateRecordButton();
            break;
        
        case QMediaRecorder::PausedState:
            logToConsole("录制状态：暂停");
            break;
        
        case QMediaRecorder::StoppedState:
            logToConsole("录制状态：停止");
            isRecording = false;
            updateRecordButton();
            break;
    }
}

// 处理录制时长变化
void cam_qt::handleRecordingDurationChanged(qint64 duration)
{
    recordingDuration = duration;
    
    // 将时长转换为可读时间格式 HH:MM:SS
    QTime time = QTime(0, 0).addMSecs(duration);
    QString timeStr = time.toString("hh:mm:ss");
    
    // 可以在这里更新录制时长显示，如果需要的话
    logToConsole(QString("录制时长: %1").arg(timeStr));
}

// 处理录制错误
void cam_qt::handleRecordingError(QMediaRecorder::Error error, const QString &errorString)
{
    logToConsole(QString("录制错误: %1").arg(errorString));
    QMessageBox::critical(this, tr("录制错误"),
                         tr("录制过程中出现错误：%1").arg(errorString));
    
    // 停止录制
    stopRecording();
}
