#include "test_qt.h"

#include <QApplication>
#include <QStyleFactory>
#pragma comment(lib, "user32.lib")

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序样式为Fusion，更适合自定义样式
    a.setStyle(QStyleFactory::create("Fusion"));
    
    // 创建并显示主窗口
    test_qt w;
    w.show();
    
    return a.exec();
}