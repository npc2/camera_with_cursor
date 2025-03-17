#include "CameraUtils.h"
#include <Windows.h>
#include <SetupAPI.h>
#include <QRegularExpression>
#include <QDebug>

#pragma comment(lib, "setupapi.lib")

// 获取设备的VID和PID信息
CameraDeviceInfo getDeviceVidPid(const QString &deviceId)
{
    CameraDeviceInfo info;
    
    // 创建设备信息集
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(nullptr, "USB", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return info;
    }
    
    // 枚举设备
    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    
    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        char deviceName[256];
        DWORD bufferSize = sizeof(deviceName);
        
        // 获取设备友好名称
        if (SetupDiGetDeviceRegistryPropertyA(deviceInfoSet, &deviceInfoData, SPDRP_FRIENDLYNAME, 
                                             nullptr, (PBYTE)deviceName, sizeof(deviceName), &bufferSize)) {
            QString name = QString::fromLatin1(deviceName);
            
            // 检查名称是否包含在deviceId中，以确定是否是目标设备
            // 由于每个平台的设备ID格式不同，这里使用模糊匹配
            if (deviceId.contains(name, Qt::CaseInsensitive) || 
                name.contains(deviceId, Qt::CaseInsensitive)) {
                
                // 找到匹配的设备，获取硬件ID
                char hwIds[512];
                if (SetupDiGetDeviceRegistryPropertyA(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, 
                                                    nullptr, (PBYTE)hwIds, sizeof(hwIds), nullptr)) {
                    QString hwIdStr = QString::fromLatin1(hwIds);
                    
                    // 解析VID和PID
                    QRegularExpression vidRegex("VID_([0-9A-F]{4})", QRegularExpression::CaseInsensitiveOption);
                    QRegularExpression pidRegex("PID_([0-9A-F]{4})", QRegularExpression::CaseInsensitiveOption);
                    
                    QRegularExpressionMatch vidMatch = vidRegex.match(hwIdStr);
                    QRegularExpressionMatch pidMatch = pidRegex.match(hwIdStr);
                    
                    if (vidMatch.hasMatch()) {
                        info.vid = vidMatch.captured(1);
                    }
                    
                    if (pidMatch.hasMatch()) {
                        info.pid = pidMatch.captured(1);
                    }
                    
                    info.name = name;
                    break;
                }
            }
        }
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return info;
} 