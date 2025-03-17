#pragma once
#include <QDialog>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QList>
#include <QString>

// Windows DirectShow头文件
#include <dshow.h>
#include <strmif.h>
#include <control.h>

// VideoProcAmp属性ID和范围结构体
struct VideoProcAmpPropertyInfo {
    long Min;
    long Max;
    long Step;
    long Default;
    long Flags;
};

// CameraControl属性ID和范围结构体
struct CameraControlPropertyInfo {
    long Min;
    long Max;
    long Step;
    long Default;
    long Flags;
};

// 摄像头控制对话框类
class CameraControlDialog : public QDialog {
    Q_OBJECT
public:
    explicit CameraControlDialog(const QString& devicePath, QWidget* parent = nullptr);
    ~CameraControlDialog();

private slots:
    void onValueChanged(int index, int value);
    void onAutoChanged(int index, bool checked);
    void onApplyClicked();
    void onDefaultClicked();
private:
    void createControls();
    void createVideoProcAmpPage(QWidget* page);
    void createCameraControlPage(QWidget* page);
    bool initializeDirectShow();
    void getPropertyRange(VideoProcAmpPropertyInfo& prop, long propertyId);
    void getCameraControlRange(CameraControlPropertyInfo& prop, long propertyId);
    void updateValue(int index);
    void getCurrentSettings();
    void resetToDefaults(bool isCameraControl);
    
    // DirectShow接口
    IBaseFilter* videoInputFilter;
    IAMVideoProcAmp* videoProcAmp;
    IAMCameraControl* cameraControl;
    
    // 控件列表
    struct ControlInfo {
        QString name;
        QSlider* slider;
        QSpinBox* spinBox;
        QCheckBox* autoBox;
        long propertyId;
        bool isCameraControl;  // true为CameraControl，false为VideoProcAmp
        QString currentPage;   // 当前页面名称
        
        bool operator==(const ControlInfo& other) const {
            return name == other.name &&
                   slider == other.slider &&
                   spinBox == other.spinBox &&
                   autoBox == other.autoBox &&
                   propertyId == other.propertyId &&
                   isCameraControl == other.isCameraControl &&
                   currentPage == other.currentPage;
        }
    };
    QList<ControlInfo> controls;
    
    // 设备路径
    QString devicePath;
    
    // 电力线频率控制
    QComboBox* powerLineCombo;
}; 