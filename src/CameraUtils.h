#pragma once
#include <QString>
#include "CameraDeviceInfo.h"

// 获取设备的VID和PID信息
CameraDeviceInfo getDeviceVidPid(const QString &deviceId); 