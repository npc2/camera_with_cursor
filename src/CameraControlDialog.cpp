#include "CameraControlDialog.h"
#include <QMessageBox>
#include <QDebug>
#include <QRegularExpression>

// 定义电力线频率常量
#define VideoProcAmp_PowerlineFrequency 11
#define VideoProcAmp_PowerlineFreq_Disabled 0
#define VideoProcAmp_PowerlineFreq_50Hz 1
#define VideoProcAmp_PowerlineFreq_60Hz 2
#define VideoProcAmp_PowerlineFreq_Auto 3

// 构造函数
CameraControlDialog::CameraControlDialog(const QString& devicePath, QWidget* parent)
    : QDialog(parent), devicePath(devicePath), videoInputFilter(nullptr), 
      videoProcAmp(nullptr), cameraControl(nullptr)
{
    // 设置窗口标题和大小
    setWindowTitle(tr("图像控制"));
    resize(400, 500);  // 缩小对话框尺寸
    
    // 设置经典简约白色风格，文字更显眼
    setStyleSheet("QDialog { background-color: white; }"
                  "QTabWidget::pane { border: 1px solid #C0C0C0; }"
                  "QTabBar::tab { background-color: #F0F0F0; border: 1px solid #C0C0C0; padding: 5px; font-weight: bold; color: #000000; }"
                  "QTabBar::tab:selected { background-color: white; }"
                  "QGroupBox { border: 1px solid #C0C0C0; margin-top: 10px; }"
                  "QGroupBox::title { subcontrol-origin: margin; left: 10px; font-weight: bold; color: #000000; }"
                  "QPushButton { background-color: #F0F0F0; border: 1px solid #C0C0C0; padding: 5px; font-weight: bold; color: #000000; }"
                  "QPushButton:hover { background-color: #E0E0E0; }"
                  "QSlider::groove:horizontal { height: 4px; background: #C0C0C0; }"
                  "QSlider::handle:horizontal { background: #404040; width: 12px; margin: -4px 0; border-radius: 6px; }"
                  "QLabel { color: #000000; font-weight: bold; }"
                  "QSpinBox { color: #000000; background-color: white; }"
                  "QCheckBox { color: #000000; font-weight: bold; }"
                  "QComboBox { background-color: white; border: 1px solid #C0C0C0; padding: 3px; color: #000000; }"
                  "QComboBox:drop-down { border: none; }"
                  "QComboBox:down-arrow { image: url(down_arrow.png); width: 12px; height: 12px; }"
                  "QComboBox QAbstractItemView { background-color: white; selection-background-color: #E0E0E0; selection-color: #000000; }");
    
    // 初始化DirectShow
    if (!initializeDirectShow()) {
        return;
    }
    
    // 创建控件
    createControls();
    
    // 获取当前设置
    getCurrentSettings();
}

// 析构函数
CameraControlDialog::~CameraControlDialog()
{
    // 释放DirectShow接口
    if (videoProcAmp) {
        videoProcAmp->Release();
        videoProcAmp = nullptr;
    }
    
    if (cameraControl) {
        cameraControl->Release();
        cameraControl = nullptr;
    }
    
    if (videoInputFilter) {
        videoInputFilter->Release();
        videoInputFilter = nullptr;
    }
    
    // 关闭COM
    CoUninitialize();
}

// 初始化DirectShow
bool CameraControlDialog::initializeDirectShow()
{
    HRESULT hr;
    
    // 初始化COM
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        QString errorMessage;
        switch (hr) {
            case RPC_E_CHANGED_MODE:
                errorMessage = tr("COM库已经初始化为单线程模式");
                break;
            case E_OUTOFMEMORY:
                errorMessage = tr("内存不足，无法初始化COM");
                break;
            default:
                errorMessage = tr("COM初始化失败，错误码: ") + QString::number(hr, 16);
                break;
        }
        QMessageBox::warning(this, tr("错误"), errorMessage);
        return false;
    }
    
    // 创建系统设备枚举器
    ICreateDevEnum* pDevEnum = nullptr;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                         IID_ICreateDevEnum, (void**)&pDevEnum);
    if (FAILED(hr)) {
        QMessageBox::warning(this, tr("错误"), tr("无法创建设备枚举器"));
        return false;
    }
    
    // 创建视频输入设备枚举器
    IEnumMoniker* pEnum = nullptr;
    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
    pDevEnum->Release();
    
    if (hr != S_OK) {
        QMessageBox::warning(this, tr("错误"), tr("没有找到视频输入设备"));
        return false;
    }
    
    // 枚举视频输入设备
    IMoniker* pMoniker = nullptr;
    ULONG fetched;
    
    while (pEnum->Next(1, &pMoniker, &fetched) == S_OK) {
        IPropertyBag* pPropBag = nullptr;
        hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);
        
        if (SUCCEEDED(hr)) {
            VARIANT varName;
            VariantInit(&varName);
            
            // 获取设备路径
            hr = pPropBag->Read(L"DevicePath", &varName, 0);
            if (SUCCEEDED(hr)) {
                QString currentPath = QString::fromWCharArray(varName.bstrVal);
                
                // 使用正则表达式提取PID和VID
                QRegularExpression re("vid_(\\w+)&pid_(\\w+)");
                QRegularExpressionMatch match = re.match(currentPath.toLower());
                QRegularExpressionMatch targetMatch = re.match(devicePath.toLower());
                
                if (match.hasMatch() && targetMatch.hasMatch()) {
                    QString currentVid = match.captured(1);
                    QString currentPid = match.captured(2);
                    QString targetVid = targetMatch.captured(1);
                    QString targetPid = targetMatch.captured(2);
                    
                    // 比较VID和PID
                    if (currentVid == targetVid && currentPid == targetPid) {
                        // 找到目标设备
                        hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&videoInputFilter);
                        
                        if (SUCCEEDED(hr)) {
                            // 获取VideoProcAmp接口
                            hr = videoInputFilter->QueryInterface(IID_IAMVideoProcAmp, (void**)&videoProcAmp);
                            if (FAILED(hr)) {
                                videoProcAmp = nullptr;
                            }
                            
                            // 获取CameraControl接口
                            hr = videoInputFilter->QueryInterface(IID_IAMCameraControl, (void**)&cameraControl);
                            if (FAILED(hr)) {
                                cameraControl = nullptr;
                            }
                            
                            VariantClear(&varName);
                            pPropBag->Release();
                            pMoniker->Release();
                            pEnum->Release();
                            
                            return true;
                        }
                    }
                }
            }
            
            VariantClear(&varName);
            pPropBag->Release();
        }
        
        pMoniker->Release();
    }
    
    pEnum->Release();
    QMessageBox::warning(this, tr("错误"), tr("未找到指定的摄像头设备"));
    return false;
}

// 创建控件
void CameraControlDialog::createControls()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 创建标签页控件
    QTabWidget* tabWidget = new QTabWidget(this);
    tabWidget->setDocumentMode(true);
    
    // 创建视频处理页面
    QWidget* videoProcAmpPage = new QWidget(tabWidget);
    createVideoProcAmpPage(videoProcAmpPage);
    tabWidget->addTab(videoProcAmpPage, tr("图像处理"));
    
    // 创建摄像机控制页面
    QWidget* cameraControlPage = new QWidget(tabWidget);
    createCameraControlPage(cameraControlPage);
    tabWidget->addTab(cameraControlPage, tr("摄像机控制"));
    
    // 创建按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* applyButton = new QPushButton(tr("应用"), this);
    QPushButton* closeButton = new QPushButton(tr("关闭"), this);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(closeButton);
    
    // 连接信号和槽
    connect(applyButton, &QPushButton::clicked, this, &CameraControlDialog::onApplyClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    
    // 添加到主布局
    mainLayout->addWidget(tabWidget);
    mainLayout->addLayout(buttonLayout);
    
    setLayout(mainLayout);
}

// 创建视频处理页面
void CameraControlDialog::createVideoProcAmpPage(QWidget* page)
{
    // 创建布局
    QVBoxLayout* procAmpLayout = new QVBoxLayout(page);
    QGridLayout* gridLayout = new QGridLayout();
    
    // 创建默认按钮
    QPushButton* defaultButton = new QPushButton(tr("恢复默认值"), page);
    connect(defaultButton, &QPushButton::clicked, this, [this]() {
        resetToDefaults(false);
    });
    
    // 创建电力线频率控制
    QHBoxLayout* powerLineLayout = new QHBoxLayout();
    QLabel* powerLineLabel = new QLabel(tr("电力线频率:"), page);
    powerLineCombo = new QComboBox(page);
    powerLineCombo->addItem(tr("禁用"), VideoProcAmp_PowerlineFreq_Disabled);
    powerLineCombo->addItem(tr("50Hz"), VideoProcAmp_PowerlineFreq_50Hz);
    powerLineCombo->addItem(tr("60Hz"), VideoProcAmp_PowerlineFreq_60Hz);
    powerLineCombo->addItem(tr("自动"), VideoProcAmp_PowerlineFreq_Auto);
    
    powerLineLayout->addWidget(powerLineLabel);
    powerLineLayout->addWidget(powerLineCombo);
    powerLineLayout->addStretch();
    
    // 定义视频处理控制项
    struct ControlDef {
        QString name;
        long propertyId;
    };
    
    // 创建控制项列表
    QList<ControlDef> controlDefs;
    controlDefs.append({tr("亮度"), VideoProcAmp_Brightness});
    controlDefs.append({tr("对比度"), VideoProcAmp_Contrast});
    controlDefs.append({tr("色调"), VideoProcAmp_Hue});
    controlDefs.append({tr("饱和度"), VideoProcAmp_Saturation});
    controlDefs.append({tr("清晰度"), VideoProcAmp_Sharpness});
    controlDefs.append({tr("伽马"), VideoProcAmp_Gamma});
    controlDefs.append({tr("白平衡"), VideoProcAmp_WhiteBalance});
    
    int row = 0;
    for (const auto& def : controlDefs) {
        if (!videoProcAmp) continue;
        
        // 获取属性范围
        VideoProcAmpPropertyInfo prop;
        getPropertyRange(prop, def.propertyId);
        long min = prop.Min;
        long max = prop.Max;
        long step = prop.Step;
        long defaultVal = prop.Default;
        long flags = prop.Flags;
        
        // 创建标签
        QLabel* label = new QLabel(def.name + ":", page);
        
        // 创建滑块
        QSlider* slider = new QSlider(Qt::Horizontal, page);
        slider->setRange(min, max);
        slider->setSingleStep(step);
        slider->setPageStep(step * 10);
        slider->setValue(defaultVal);
        
        // 创建数值显示
        QSpinBox* spinBox = new QSpinBox(page);
        spinBox->setRange(min, max);
        spinBox->setSingleStep(step);
        spinBox->setValue(defaultVal);
        
        // 创建自动复选框
        QCheckBox* autoBox = nullptr;
        if (flags & VideoProcAmp_Flags_Auto) {
            autoBox = new QCheckBox(tr("自动"), page);
            autoBox->setChecked(false);
        }
        
        // 连接信号和槽
        int index = controls.count();
        QObject::connect(slider, &QSlider::valueChanged, this, [this, index](int value) {
            onValueChanged(index, value);
        });
        QObject::connect(spinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), slider, &QSlider::setValue);
        if (autoBox) {
            QObject::connect(autoBox, &QCheckBox::toggled, this, [this, index](bool checked) {
                onAutoChanged(index, checked);
            });
        }
        
        // 保存控件信息
        ControlInfo info;
        info.name = def.name;
        info.slider = slider;
        info.spinBox = spinBox;
        info.autoBox = autoBox;
        info.propertyId = def.propertyId;
        info.isCameraControl = false;
        info.currentPage = "VideoProcAmp";
        controls.append(info);
        
        // 添加到布局
        gridLayout->addWidget(label, row, 0);
        gridLayout->addWidget(slider, row, 1);
        gridLayout->addWidget(spinBox, row, 2);
        if (autoBox) {
            gridLayout->addWidget(autoBox, row, 3);
        }
        
        row++;
    }
    
    // 添加到主布局
    procAmpLayout->addLayout(gridLayout);
    procAmpLayout->addLayout(powerLineLayout);
    procAmpLayout->addWidget(defaultButton);
    procAmpLayout->addStretch();
    
    page->setLayout(procAmpLayout);
}

// 创建摄像机控制页面
void CameraControlDialog::createCameraControlPage(QWidget* page)
{
    // 创建布局
    QVBoxLayout* cameraLayout = new QVBoxLayout(page);
    QGridLayout* gridLayout = new QGridLayout();
    
    // 创建默认按钮
    QPushButton* defaultButton = new QPushButton(tr("恢复默认值"), page);
    connect(defaultButton, &QPushButton::clicked, this, [this]() {
        resetToDefaults(true);
    });
    
    // 定义摄像机控制项
    struct ControlDef {
        QString name;
        long propertyId;
    };
    
    // 创建控制项列表
    QList<ControlDef> controlDefs;
    controlDefs.append({tr("平移"), CameraControl_Pan});
    controlDefs.append({tr("倾斜"), CameraControl_Tilt});
    controlDefs.append({tr("滚动"), CameraControl_Roll});
    controlDefs.append({tr("变焦"), CameraControl_Zoom});
    controlDefs.append({tr("曝光"), CameraControl_Exposure});
    controlDefs.append({tr("光圈"), CameraControl_Iris});
    controlDefs.append({tr("对焦"), CameraControl_Focus});
    
    int row = 0;
    for (const auto& def : controlDefs) {
        if (!cameraControl) continue;
        
        // 获取属性范围
        CameraControlPropertyInfo prop;
        getCameraControlRange(prop, def.propertyId);
        long min = prop.Min;
        long max = prop.Max;
        long step = prop.Step;
        long defaultVal = prop.Default;
        long flags = prop.Flags;
        
        // 创建标签
        QLabel* label = new QLabel(def.name + ":", page);
        
        // 创建滑块
        QSlider* slider = new QSlider(Qt::Horizontal, page);
        slider->setRange(min, max);
        slider->setSingleStep(step);
        slider->setPageStep(step * 10);
        slider->setValue(defaultVal);
        
        // 创建数值显示
        QSpinBox* spinBox = new QSpinBox(page);
        spinBox->setRange(min, max);
        spinBox->setSingleStep(step);
        spinBox->setValue(defaultVal);
        
        // 创建自动复选框
        QCheckBox* autoBox = nullptr;
        if (flags & CameraControl_Flags_Auto) {
            autoBox = new QCheckBox(tr("自动"), page);
            autoBox->setChecked(false);
        }
        
        // 连接信号和槽
        int index = controls.count();
        QObject::connect(slider, &QSlider::valueChanged, this, [this, index](int value) {
            onValueChanged(index, value);
        });
        QObject::connect(spinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), slider, &QSlider::setValue);
        if (autoBox) {
            QObject::connect(autoBox, &QCheckBox::toggled, this, [this, index](bool checked) {
                onAutoChanged(index, checked);
            });
        }
        
        // 保存控件信息
        ControlInfo info;
        info.name = def.name;
        info.slider = slider;
        info.spinBox = spinBox;
        info.autoBox = autoBox;
        info.propertyId = def.propertyId;
        info.isCameraControl = true;
        info.currentPage = "CameraControl";
        controls.append(info);
        
        // 添加到布局
        gridLayout->addWidget(label, row, 0);
        gridLayout->addWidget(slider, row, 1);
        gridLayout->addWidget(spinBox, row, 2);
        if (autoBox) {
            gridLayout->addWidget(autoBox, row, 3);
        }
        
        row++;
    }
    
    // 添加到主布局
    cameraLayout->addLayout(gridLayout);
    cameraLayout->addWidget(defaultButton);
    cameraLayout->addStretch();
    
    page->setLayout(cameraLayout);
}

// 获取当前设置
void CameraControlDialog::getCurrentSettings()
{
    for (int i = 0; i < controls.count(); i++) {
        updateValue(i);
    }
}

// 更新控件值
void CameraControlDialog::updateValue(int index)
{
    if (index >= 0 && index < controls.count()) {
        const ControlInfo& info = controls[index];
        long value = 0, flags = 0;
        
        if (info.isCameraControl && cameraControl) {
            cameraControl->Get(info.propertyId, &value, &flags);
            if (info.autoBox) {
                info.autoBox->setChecked((flags & CameraControl_Flags_Auto) != 0);
            }
        }
        else if (!info.isCameraControl && videoProcAmp) {
            videoProcAmp->Get(info.propertyId, &value, &flags);
            if (info.autoBox) {
                info.autoBox->setChecked((flags & VideoProcAmp_Flags_Auto) != 0);
            }
        }
        
        info.slider->setValue(value);
        info.spinBox->setValue(value);
        
        // 更新电力线频率控制
        if (!info.isCameraControl && info.propertyId == VideoProcAmp_PowerlineFrequency && videoProcAmp) {
            for (int i = 0; i < powerLineCombo->count(); i++) {
                if (powerLineCombo->itemData(i).toLongLong() == value) {
                    powerLineCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }
}

// 重置为默认值
void CameraControlDialog::resetToDefaults(bool isCameraControl)
{
    for (int i = 0; i < controls.count(); i++) {
        const ControlInfo& info = controls[i];
        
        if (info.isCameraControl == isCameraControl) {
            long value = 0, flags = 0;
            
            if (info.isCameraControl && cameraControl) {
                CameraControlPropertyInfo prop;
                getCameraControlRange(prop, info.propertyId);
                value = prop.Default;
                flags = prop.Flags;
                if (info.autoBox) {
                    info.autoBox->setChecked((flags & CameraControl_Flags_Auto) != 0);
                }
            }
            else if (!info.isCameraControl && videoProcAmp) {
                VideoProcAmpPropertyInfo prop;
                getPropertyRange(prop, info.propertyId);
                value = prop.Default;
                flags = prop.Flags;
                if (info.autoBox) {
                    info.autoBox->setChecked((flags & VideoProcAmp_Flags_Auto) != 0);
                }
            }
            
            info.slider->setValue(value);
        }
    }
}

// 值改变处理
void CameraControlDialog::onValueChanged(int index, int value)
{
    if (index >= 0 && index < controls.count()) {
        const ControlInfo& info = controls[index];
        info.spinBox->setValue(value);
        
        // 检查是否是电力线频率控制
        if (!info.isCameraControl && info.propertyId == VideoProcAmp_PowerlineFrequency && videoProcAmp) {
            long flags = VideoProcAmp_Flags_Manual;
            videoProcAmp->Set(VideoProcAmp_PowerlineFrequency, value, flags);
            return;
        }
        
        if (info.isCameraControl && cameraControl) {
            // 获取当前的flags
            long currentValue, flags;
            HRESULT hr = cameraControl->Get(info.propertyId, &currentValue, &flags);
            if (SUCCEEDED(hr)) {
                // 保持原来的自动/手动设置
                cameraControl->Set(info.propertyId, value, flags);
            } else {
                // 默认使用手动模式
                cameraControl->Set(info.propertyId, value, CameraControl_Flags_Manual);
            }
        }
        else if (!info.isCameraControl && videoProcAmp) {
            // 获取当前的flags
            long currentValue, flags;
            HRESULT hr = videoProcAmp->Get(info.propertyId, &currentValue, &flags);
            if (SUCCEEDED(hr)) {
                // 保持原来的自动/手动设置
                videoProcAmp->Set(info.propertyId, value, flags);
            } else {
                // 默认使用手动模式
                videoProcAmp->Set(info.propertyId, value, VideoProcAmp_Flags_Manual);
            }
        }
        
        // 记录日志
        qDebug() << "设置参数:" << info.name << "值:" << value;
    }
}

// 自动模式改变处理
void CameraControlDialog::onAutoChanged(int index, bool checked)
{
    if (index >= 0 && index < controls.count()) {
        const ControlInfo& info = controls[index];
        long flags;
        if (info.isCameraControl) {
            flags = checked ? CameraControl_Flags_Auto : CameraControl_Flags_Manual;
        } else {
            flags = checked ? VideoProcAmp_Flags_Auto : VideoProcAmp_Flags_Manual;
        }
        
        // 获取当前值
        long currentValue = info.slider->value();
        
        if (info.isCameraControl && cameraControl) {
            // 先获取当前值，确保设置正确
            long oldValue, oldFlags;
            HRESULT hr = cameraControl->Get(info.propertyId, &oldValue, &oldFlags);
            if (SUCCEEDED(hr)) {
                currentValue = oldValue; // 使用设备当前值
            }
            
            // 设置新的标志
            hr = cameraControl->Set(info.propertyId, currentValue, flags);
            if (FAILED(hr)) {
                qDebug() << "设置摄像机控制自动模式失败:" << info.name << "错误码:" << hr;
            }
        }
        else if (!info.isCameraControl && videoProcAmp) {
            // 先获取当前值，确保设置正确
            long oldValue, oldFlags;
            HRESULT hr = videoProcAmp->Get(info.propertyId, &oldValue, &oldFlags);
            if (SUCCEEDED(hr)) {
                currentValue = oldValue; // 使用设备当前值
            }
            
            // 设置新的标志
            hr = videoProcAmp->Set(info.propertyId, currentValue, flags);
            if (FAILED(hr)) {
                qDebug() << "设置视频处理自动模式失败:" << info.name << "错误码:" << hr;
            }
        }
        
        // 更新UI状态
        info.slider->setEnabled(!checked);
        info.spinBox->setEnabled(!checked);
        
        // 记录日志
        qDebug() << "设置参数:" << info.name << "自动模式:" << checked;
    }
}

// 默认按钮点击处理
void CameraControlDialog::onDefaultClicked()
{
    QTabWidget* tabWidget = findChild<QTabWidget*>();
    if (tabWidget) {
        int currentIndex = tabWidget->currentIndex();
        resetToDefaults(currentIndex == 1); // 1是摄像机控制页面
    }
}

// 应用按钮点击处理
void CameraControlDialog::onApplyClicked()
{
    // 应用电力线频率设置
    if (videoProcAmp) {
        int index = powerLineCombo->currentIndex();
        if (index >= 0) {
            long value = powerLineCombo->itemData(index).toLongLong();
            videoProcAmp->Set(VideoProcAmp_PowerlineFrequency, value, VideoProcAmp_Flags_Manual);
            qDebug() << "设置电力线频率:" << value;
        }
    }
    
    // 应用其他控制项设置
    for (int i = 0; i < controls.count(); i++) {
        const ControlInfo& info = controls[i];
        onValueChanged(i, info.slider->value());
        if (info.autoBox) {
            onAutoChanged(i, info.autoBox->isChecked());
        }
    }
    
    QMessageBox::information(this, tr("信息"), tr("设置已应用"));
}

// 获取属性范围
void CameraControlDialog::getPropertyRange(VideoProcAmpPropertyInfo& prop, long propertyId)
{
    if (videoProcAmp) {
        HRESULT hr = videoProcAmp->GetRange(propertyId, &prop.Min, &prop.Max, 
                                           &prop.Step, &prop.Default, &prop.Flags);
        if (FAILED(hr)) {
            prop.Min = 0;
            prop.Max = 100;
            prop.Step = 1;
            prop.Default = 50;
            prop.Flags = VideoProcAmp_Flags_Manual;
        }
    }
}

// 获取摄像机控制范围
void CameraControlDialog::getCameraControlRange(CameraControlPropertyInfo& prop, long propertyId)
{
    if (cameraControl) {
        HRESULT hr = cameraControl->GetRange(propertyId, &prop.Min, &prop.Max, 
                                            &prop.Step, &prop.Default, &prop.Flags);
        if (FAILED(hr)) {
            prop.Min = 0;
            prop.Max = 100;
            prop.Step = 1;
            prop.Default = 50;
            prop.Flags = CameraControl_Flags_Manual;
        }
    }
} 