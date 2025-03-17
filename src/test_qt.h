#pragma once
#include "../build/ui_test_qt.h"
#include <QMainWindow>
#include <QString>
#include <QCamera>
#include <QMediaDevices>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QCameraFormat>
#include <QElapsedTimer>
#include <QTimer>
#include <QVariant>

// 引入分离出的头文件
#include "CameraControlDialog.h"
#include "CameraDeviceInfo.h"
#include "CameraUtils.h"

// 前向声明
class Ui_test_qt;

class test_qt : public QMainWindow {
    Q_OBJECT
    
public:
    test_qt(QWidget* parent = nullptr);
    ~test_qt();

private slots:
    void on_btnDetectCameras_clicked();
    void on_btnOpenCamera_clicked();
    void on_btnSetFormat_clicked();
    void handleVideoFrame(const QVideoFrame &frame);
    void on_comboCamera_currentIndexChanged(int index);
    void on_comboResolution_currentIndexChanged(int index);
    void updateFPSDisplay();
    void on_btnCameraControl_clicked();  // 打开摄像头控制面板

private:
    Ui_test_qt* ui;
    QCamera* camera;
    QMediaCaptureSession captureSession;
    QVideoSink* videoSink;
    void updateCameraList();
    void updateResolutionList();
    void stopCamera();
    QString formatToString(const QCameraFormat &format);
    
    // FPS计算相关
    QElapsedTimer fpsTimer;
    int frameCount;
    double currentFPS;
    QTimer* fpsUpdateTimer;
    qint64 lastFrameTime;  // 用于精确计算帧率
    
    // 摄像头控制对话框
    CameraControlDialog* cameraControlDialog;
}; 