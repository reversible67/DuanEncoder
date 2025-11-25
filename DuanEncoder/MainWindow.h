#pragma once
#include <QGroupBox>
#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include "EncoderThread.h"
/*
构建一个音视频编码工具的图形界面，并集成了编码线程的交互逻辑
*/
// MainWindow 继承自 Qt 的主窗口类 QMainWindow，具备主窗口的所有特性（如菜单栏、工具栏、中心部件等）
// Q_OBJECT：Qt 的核心宏，必须添加在使用信号槽机制的类中，用于启用元对象系统（Meta-Object System），支持信号、槽、反射等功能
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

// 槽函数
private slots:
    void on_selectInputBtn_clicked();      // 选择输入YUV文件
    void on_selectOutputBtn_clicked();     // 选择输出文件
    void on_startEncodeBtn_clicked();      // 开始编码
    void updateProgress(int current, int total); // 更新进度条
    void updateLog(const QString& log);    // 更新日志
    void onEncodeFinished(bool success);   // 编码完成处理
    void on_codecCombo_currentIndexChanged(int index); // 编码器选择

private:
    EncoderThread* m_encoderThread;        // 编码线程对象指针
    int m_currentCodec = AV_CODEC_ID_H264; // 默认H.264编码器

    // 界面控件成员变量
    QLineEdit* inputEdit;                 // 输入文件路径显示框
    QLineEdit* outputEdit;                // 输出文件路径显示框
    QPushButton* selectInputBtn;          // 选择输入文件按钮
    QPushButton* selectOutputBtn;         // 选择输出文件按钮
    QSpinBox* widthSpin;                  // 视频宽度输入框（整数）
    QSpinBox* heightSpin;                 // 视频高度输入框（整数）
    QSpinBox* bitRateSpin;                // 码率输入框（整数）
    QSpinBox* frameNumSpin;               // 编码帧数输入框（整数）
    QComboBox* codecCombo;                // 编码器选择下拉框
    QProgressBar* progressBar;            // 编码进度条
    QTextEdit* logEdit;                   // 日志显示文本框
    QPushButton* startEncodeBtn;          // 开始编码按钮
};