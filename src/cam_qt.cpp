#include "cam_qt.h"
#include "dbgout.h"
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
      frameCount(0), currentFPS(0), lastFrameTime(0), cameraControlDialog(nullptr)
{
    ui->setupUi(this);
    
    // 设置经典简约白色风格，文字更显眼
    setStyleSheet("QMainWindow { background-color: white; }"
                  "QGroupBox { border: 1px solid #C0C0C0; margin-top: 10px; }"
                  "QGroupBox::title { subcontrol-origin: margin; left: 10px; font-weight: bold; color: #000000; }"
                  "QPushButton { background-color: #F0F0F0; border: 1px solid #C0C0C0; padding: 5px; font-weight: bold; color: #000000; }"
                  "QPushButton:hover { background-color: #E0E0E0; }"
                  "QComboBox { background-color: white; border: 1px solid #C0C0C0; padding: 3px; color: #000000; }"
                  "QComboBox:drop-down { border: none; }"
                  "QComboBox:down-arrow { image: url(down_arrow.png); width: 12px; height: 12px; }"
                  "QComboBox QAbstractItemView { background-color: white; selection-background-color: #E0E0E0; selection-color: #000000; }"
                  "QSpinBox { border: 1px solid #C0C0C0; padding: 3px; color: #000000; background-color: white; }"
                  "QLabel { color: #000000; font-weight: bold; }"
                  "QLabel#labelPreview { background-color: #F0F0F0; border: 1px solid #C0C0C0; }");
    
    // 创建视频接收器
    videoSink = new QVideoSink(this);
    connect(videoSink, &QVideoSink::videoFrameChanged, this, &cam_qt::handleVideoFrame);
    captureSession.setVideoSink(videoSink);
    
    // 创建FPS更新定时器
    fpsUpdateTimer = new QTimer(this);
    connect(fpsUpdateTimer, &QTimer::timeout, this, &cam_qt::updateFPSDisplay);
    fpsUpdateTimer->start(1000); // 每秒更新一次
    
    // 初始化FPS计时器
    fpsTimer.start();
    
    // 更新摄像头列表
    updateCameraList();
}

// 析构函数
cam_qt::~cam_qt()
{
    stopCamera();
    delete cameraControlDialog;
    delete ui;
}

// 更新摄像头列表
void cam_qt::updateCameraList()
{
    ui->comboCamera->clear();
    
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cameraDevice : cameras) {
        ui->comboCamera->addItem(cameraDevice.description(), QVariant::fromValue(cameraDevice));
    }
    
    // 更新分辨率列表
    updateResolutionList();
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
            
            // 显示FPS，同时显示设置的目标帧率
            int targetFPS = ui->spinFrameRate->value();
            ui->labelFPS->setText(QString("实时帧率: %1 FPS (目标: %2 FPS)")
                                .arg(qRound(currentFPS))
                                .arg(targetFPS));
            
            // 重置计数器
            frameCount = 0;
            fpsTimer.restart();
        }
    } else {
        ui->labelFPS->setText("实时帧率: 0 FPS");
    }
}

// 摄像头选择改变处理
void cam_qt::on_comboCamera_currentIndexChanged(int index)
{
    if (index >= 0) {
        updateResolutionList();
    }
}

// 停止摄像头
void cam_qt::stopCamera()
{
    if (camera) {
        camera->stop();
        delete camera;
        camera = nullptr;
        
        // 清空预览画面，显示白色背景
        QSize labelSize = ui->labelPreview->size();
        QImage background(labelSize, QImage::Format_RGB32);
        background.fill(Qt::white);
        ui->labelPreview->setPixmap(QPixmap::fromImage(background));
    }
}

// 打开摄像头按钮点击处理
void cam_qt::on_btnOpenCamera_clicked()
{
    if (camera && camera->isActive()) {
        stopCamera();
        ui->btnOpenCamera->setText("打开摄像头");
    } else {
        if (!camera) {
            // 打开摄像头
            if (ui->comboCamera->count() == 0) {
                QMessageBox::warning(this, tr("错误"), tr("没有可用的摄像头设备"));
                return;
            }
            
            QCameraDevice device = ui->comboCamera->currentData().value<QCameraDevice>();
            camera = new QCamera(device, this);
            captureSession.setCamera(camera);
            
            // 设置格式
            if (ui->comboResolution->count() > 0) {
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
                    camera->setCameraFormat(bestFormat);
                }
            }
        }
        
        // 启动摄像头
        camera->start();
        ui->btnOpenCamera->setText("关闭摄像头");
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
