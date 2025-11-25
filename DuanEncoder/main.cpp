#include "MainWindow.h"
#include <QApplication>
#include "stdafx.h"

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);  // 初始化应用、处理事件循环、管理资源
    // MainWindow 通常是用户自定义的窗口类（继承自 QMainWindow）
    MainWindow w;
    // 显示主窗口
    w.show();
    // 启动 Qt 的事件循环
    return a.exec();  // 使程序能够响应用户操作并保持运行
}