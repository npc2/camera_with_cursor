#pragma once
#include "ui_cam_qt.h"
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
#include <QAudioDevice>
#include <QMediaRecorder>
#include <QMediaFormat>
#include <QFileDialog>

// 前向声明
class CameraControlDialog;
class AudioPanel;

#include "CameraDeviceInfo.h"
#include "CameraUtils.h"

// 不需要前向声明，因为已经包含了头文件
// class Ui_cam_qt;

class cam_qt : public QMainWindow {
    Q_OBJECT
    
public:
    cam_qt(QWidget* parent = nullptr);
    ~cam_qt();

private slots:
    void on_btnDetectCameras_clicked();
    void on_btnOpenCamera_clicked();
    void on_btnSetFormat_clicked();
    void handleVideoFrame(const QVideoFrame &frame);
    void on_comboCamera_currentIndexChanged(int index);
    void on_comboResolution_currentIndexChanged(int index);
    void on_comboFormat_currentIndexChanged(int index);
    void updateFPSDisplay();
    void on_btnCameraControl_clicked();  // 打开摄像头控制面板
    void on_btnRecordVideo_clicked();    // 开始/停止录制视频
    void handleRecordingStateChanged(QMediaRecorder::RecorderState state);
    void handleRecordingDurationChanged(qint64 duration);
    void handleRecordingError(QMediaRecorder::Error error, const QString &errorString);
    
private:
    Ui_cam_qt* ui;
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
    
    // 音频相关
    AudioPanel* audioPanel;
    QAudioDevice currentAudioDevice;
    void checkForAudioDevice(const QString &cameraName);
    void setupAudioPanel();
    bool hasAudioDevice(const QString &cameraName);
    
    // 录制相关
    QMediaRecorder* mediaRecorder;
    bool isRecording;
    void startRecording();
    void stopRecording();
    void updateRecordButton();
    QString getDefaultSavePath();
    QTimer* recordingTimer;
    qint64 recordingDuration;
}; 

