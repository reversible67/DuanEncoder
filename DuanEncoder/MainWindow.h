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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void on_selectInputBtn_clicked();      // 选择输入YUV文件
    void on_selectOutputBtn_clicked();     // 选择输出文件
    void on_startEncodeBtn_clicked();      // 开始编码
    void updateProgress(int current, int total); // 更新进度条
    void updateLog(const QString& log);    // 更新日志
    void onEncodeFinished(bool success);   // 编码完成处理
    void on_codecCombo_currentIndexChanged(int index); // 编码器选择

private:
    EncoderThread* m_encoderThread;
    int m_currentCodec = AV_CODEC_ID_H264; // 默认H.264编码器

    // 界面控件成员变量
    QLineEdit* inputEdit;
    QLineEdit* outputEdit;
    QPushButton* selectInputBtn;
    QPushButton* selectOutputBtn;
    QSpinBox* widthSpin;
    QSpinBox* heightSpin;
    QSpinBox* bitRateSpin;
    QSpinBox* frameNumSpin;
    QComboBox* codecCombo;
    QProgressBar* progressBar;
    QTextEdit* logEdit;
    QPushButton* startEncodeBtn;
};