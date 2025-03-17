#include "test_qt.h"
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
test_qt::test_qt(QWidget* parent)
    : QMainWindow(parent), ui(new Ui_test_qt), camera(nullptr), 
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
    connect(videoSink, &QVideoSink::videoFrameChanged, this, &test_qt::handleVideoFrame);
    captureSession.setVideoSink(videoSink);
    
    // 创建FPS更新定时器
    fpsUpdateTimer = new QTimer(this);
    connect(fpsUpdateTimer, &QTimer::timeout, this, &test_qt::updateFPSDisplay);
    fpsUpdateTimer->start(1000); // 每秒更新一次
    
    // 初始化FPS计时器
    fpsTimer.start();
    
    // 更新摄像头列表
    updateCameraList();
}

// 析构函数
test_qt::~test_qt()
{
    stopCamera();
    delete cameraControlDialog;
    delete ui;
}

// 更新摄像头列表
void test_qt::updateCameraList()
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
void test_qt::updateResolutionList()
{
    ui->comboResolution->clear();
    
    if (ui->comboCamera->count() == 0) {
        return;
    }
    
    QCameraDevice device = ui->comboCamera->currentData().value<QCameraDevice>();
    
    // 创建分辨率到最大帧率的映射
    QMap<QString, QPair<QSize, int>> resolutions;
    
    for (const QCameraFormat &format : device.videoFormats()) {
        QString resKey = QString("%1x%2").arg(format.resolution().width()).arg(format.resolution().height());
        
        if (!resolutions.contains(resKey) || format.maxFrameRate() > resolutions[resKey].second) {
            resolutions[resKey] = qMakePair(format.resolution(), qRound(format.maxFrameRate()));
        }
    }
    
    // 添加到下拉列表
    for (auto it = resolutions.begin(); it != resolutions.end(); ++it) {
        QString key = it.key();
        QSize size = it.value().first;
        ui->comboResolution->addItem(key, QVariant::fromValue(size));
        
        // 如果是第一个分辨率，设置最大帧率
        if (it == resolutions.begin()) {
            ui->spinFrameRate->setMaximum(it.value().second);
            ui->spinFrameRate->setValue(it.value().second);
        }
    }
}

// 分辨率改变处理
void test_qt::on_comboResolution_currentIndexChanged(int index)
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
void test_qt::updateFPSDisplay()
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
void test_qt::on_comboCamera_currentIndexChanged(int index)
{
    if (index >= 0) {
        updateResolutionList();
    }
}

// 停止摄像头
void test_qt::stopCamera()
{
    if (camera) {
        camera->stop();
        delete camera;
        camera = nullptr;
    }
}

// 打开摄像头按钮点击处理
void test_qt::on_btnOpenCamera_clicked()
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
                
                // 查找匹配的格式
                QCameraFormat bestFormat;
                int bestFpsDiff = INT_MAX;
                
                for (const QCameraFormat &format : device.videoFormats()) {
                    if (format.resolution() == resolution) {
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
void test_qt::on_btnSetFormat_clicked()
{
    if (camera && camera->isActive() && ui->comboResolution->count() > 0) {
        QCameraDevice device = ui->comboCamera->currentData().value<QCameraDevice>();
        QSize resolution = ui->comboResolution->itemData(ui->comboResolution->currentIndex()).value<QSize>();
        int fps = ui->spinFrameRate->value();
        
        // 查找匹配的格式
        QCameraFormat bestFormat;
        int bestFpsDiff = INT_MAX;
        
        for (const QCameraFormat &format : device.videoFormats()) {
            if (format.resolution() == resolution) {
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
                                    tr("已设置格式为 %1x%2 @ %3 FPS")
                                    .arg(resolution.width())
                                    .arg(resolution.height())
                                    .arg(fps));
        }
    }
}

// 查找摄像头按钮点击处理
void test_qt::on_btnDetectCameras_clicked()
{
    updateCameraList();
    QMessageBox::information(this, tr("信息"), 
                            tr("已检测到 %1 个摄像头设备").arg(ui->comboCamera->count()));
}

// 处理视频帧
void test_qt::handleVideoFrame(const QVideoFrame &frame)
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
void test_qt::on_btnCameraControl_clicked()
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
QString test_qt::formatToString(const QCameraFormat &format)
{
    return QString("%1x%2 @ %3 FPS")
            .arg(format.resolution().width())
            .arg(format.resolution().height())
            .arg(format.maxFrameRate(), 0, 'f', 1);
}
