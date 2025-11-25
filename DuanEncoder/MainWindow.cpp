#include "stdafx.h"  // 或 #include "pch.h"（根据你的预编译头）
#include "MainWindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // ========== 设置全局字体，解决乱码 ==========
    QFont font = this->font();
    font.setFamily("Microsoft YaHei"); // 或 "SimHei"（Windows系统默认中文字体）
    this->setFont(font);
    QApplication::setFont(font); // 全局应用字体
    // ========== Window Basic Settings ==========
    this->setWindowTitle("Duan Qt + FFmpeg Encoder And Player");
    this->resize(700, 600);

    // Create central widget and main layout
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    this->setCentralWidget(centralWidget);

    // ========== Input File Area ==========
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(new QLabel("Input YUV File: "));
    inputEdit = new QLineEdit(this);
    inputEdit->setPlaceholderText("Select or input YUV file path");
    selectInputBtn = new QPushButton("Select File", this);
    inputLayout->addWidget(inputEdit);
    inputLayout->addWidget(selectInputBtn);
    mainLayout->addLayout(inputLayout);

    // ========== Output File Area ==========
    QHBoxLayout* outputLayout = new QHBoxLayout();
    outputLayout->addWidget(new QLabel("Output Encoded File: "));
    outputEdit = new QLineEdit(this);
    outputEdit->setPlaceholderText("Select or input output file path");
    selectOutputBtn = new QPushButton("Select Save Path", this);
    outputLayout->addWidget(outputEdit);
    outputLayout->addWidget(selectOutputBtn);
    mainLayout->addLayout(outputLayout);

    // ========== Parameter Settings Area ==========
    QGroupBox* paramGroup = new QGroupBox("Encoding Parameters", this);
    QGridLayout* paramLayout = new QGridLayout();
    paramGroup->setLayout(paramLayout);
    paramLayout->setSpacing(10);

    // Width setting
    paramLayout->addWidget(new QLabel("Video Width: "), 0, 0);
    widthSpin = new QSpinBox(this);
    widthSpin->setRange(160, 1920);
    widthSpin->setValue(480);
    widthSpin->setSuffix(" px");
    paramLayout->addWidget(widthSpin, 0, 1);

    // Height setting
    paramLayout->addWidget(new QLabel("Video Height: "), 0, 2);
    heightSpin = new QSpinBox(this);
    heightSpin->setRange(120, 1080);
    heightSpin->setValue(272);
    heightSpin->setSuffix(" px");
    paramLayout->addWidget(heightSpin, 0, 3);

    // Bitrate setting
    paramLayout->addWidget(new QLabel("Bitrate: "), 1, 0);
    bitRateSpin = new QSpinBox(this);
    bitRateSpin->setRange(100, 10000);
    bitRateSpin->setValue(400);
    bitRateSpin->setSuffix(" kbps");
    paramLayout->addWidget(bitRateSpin, 1, 1);

    // Frame count setting
    paramLayout->addWidget(new QLabel("Frame Count: "), 1, 2);
    frameNumSpin = new QSpinBox(this);
    frameNumSpin->setRange(1, 10000);
    frameNumSpin->setValue(100);
    paramLayout->addWidget(frameNumSpin, 1, 3);

    // Encoder selection
    paramLayout->addWidget(new QLabel("Encoder Type: "), 2, 0);
    codecCombo = new QComboBox(this);
    codecCombo->addItem("H.264 (AVC)", AV_CODEC_ID_H264);
    codecCombo->addItem("H.265 (HEVC)", AV_CODEC_ID_HEVC);
    paramLayout->addWidget(codecCombo, 2, 1);

    mainLayout->addWidget(paramGroup);

    // ========== Progress Bar Area ==========
    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->addWidget(new QLabel("Encoding Progress: "));
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressLayout->addWidget(progressBar);
    mainLayout->addLayout(progressLayout);

    // ========== Log Area ==========
    QGroupBox* logGroup = new QGroupBox("Encoding Log", this);
    QVBoxLayout* logLayout = new QVBoxLayout();
    logGroup->setLayout(logLayout);
    logEdit = new QTextEdit(this);
    logEdit->setReadOnly(true);
    logEdit->setPlaceholderText("Encoding log will be displayed here...");
    logLayout->addWidget(logEdit);
    mainLayout->addWidget(logGroup);

    // ========== Control Button ==========
    startEncodeBtn = new QPushButton("Start Encoding", this);
    startEncodeBtn->setMinimumHeight(40);
    startEncodeBtn->setStyleSheet("QPushButton { font-size: 14px; }");
    mainLayout->addWidget(startEncodeBtn);

    QGroupBox* playGroup = new QGroupBox("Video playback", this);
    QHBoxLayout* playLayout = new QHBoxLayout();
    playGroup->setLayout(playLayout);

    playLayout->addWidget(new QLabel("Playing the file: "));
    playFileEdit = new QLineEdit(this);
    playFileEdit->setPlaceholderText("Select a video file to play");
    selectPlayFileBtn = new QPushButton("Select a file", this);
    startPlayBtn = new QPushButton("Start playback", this);

    playLayout->addWidget(playFileEdit);
    playLayout->addWidget(selectPlayFileBtn);
    playLayout->addWidget(startPlayBtn);
    mainLayout->addWidget(playGroup);

    // ========== Signal-Slot Connection ==========
    connect(selectInputBtn, &QPushButton::clicked, this, &MainWindow::on_selectInputBtn_clicked);
    connect(selectOutputBtn, &QPushButton::clicked, this, &MainWindow::on_selectOutputBtn_clicked);
    connect(startEncodeBtn, &QPushButton::clicked, this, &MainWindow::on_startEncodeBtn_clicked);
    connect(codecCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::on_codecCombo_currentIndexChanged);

    connect(selectPlayFileBtn, &QPushButton::clicked, this, &MainWindow::on_selectPlayFileBtn_clicked);
    connect(startPlayBtn, &QPushButton::clicked, this, &MainWindow::on_startPlayBtn_clicked);

    // Initialize encoder thread
    m_encoderThread = new EncoderThread(this);
    connect(m_encoderThread, &EncoderThread::encodeProgress, this, &MainWindow::updateProgress);
    connect(m_encoderThread, &EncoderThread::encodeLog, this, &MainWindow::updateLog);
    connect(m_encoderThread, &EncoderThread::encodeFinished, this, &MainWindow::onEncodeFinished);

    // Initialize decoder thread
    m_playerThread = new PlayerThread(this);
    connect(m_playerThread, &PlayerThread::playLog, this, &MainWindow::updatePlayLog);
    connect(m_playerThread, &PlayerThread::playError, this, &MainWindow::onPlayError);
    connect(m_playerThread, &PlayerThread::playFinished, this, &MainWindow::onPlayFinished);
}

MainWindow::~MainWindow()
{
    // No need to manually release widgets (Qt parent-child mechanism)
}

void MainWindow::on_selectInputBtn_clicked()
{
    // 连续两个分号 (;;) 是 Qt 文件过滤器的语法规则，用于分隔不同的文件类型选项
    QString path = QFileDialog::getOpenFileName(this,
        "Select YUV File", "", "YUV Files (*.yuv);;All Files (*.*)");
    if (!path.isEmpty()) {
        inputEdit->setText(path);
    }
}

void MainWindow::on_selectOutputBtn_clicked()
{
    QString filter = "H264 Files (*.h264)";
    QString defaultSuffix = "h264";
    if (m_currentCodec == AV_CODEC_ID_HEVC) {
        filter = "H265 Files (*.h265)";
        defaultSuffix = "h265";
    }

    QString path = QFileDialog::getSaveFileName(this,
        "Save Encoded File", "", filter, nullptr,
        QFileDialog::DontConfirmOverwrite);

    if (!path.isEmpty()) {
        // Auto add suffix
        if (!path.endsWith("." + defaultSuffix, Qt::CaseInsensitive)) {
            path += "." + defaultSuffix;
        }
        outputEdit->setText(path);
    }
}

void MainWindow::on_startEncodeBtn_clicked()
{
    QString input = inputEdit->text().trimmed();
    QString output = outputEdit->text().trimmed();

    if (input.isEmpty() || output.isEmpty()) {
        QMessageBox::warning(this, "Parameter Error", "Please select input YUV file and output encoded file!");
        return;
    }

    // Get parameters from UI
    int width = widthSpin->value();
    int height = heightSpin->value();
    int bitRate = bitRateSpin->value() * 1000; // kbps to bps
    int frameNum = frameNumSpin->value();

    // Disable controls to prevent duplicate operations
    startEncodeBtn->setEnabled(false);
    selectInputBtn->setEnabled(false);
    selectOutputBtn->setEnabled(false);
    codecCombo->setEnabled(false);

    // Clear log and progress
    logEdit->clear();
    progressBar->setValue(0);

    // Set thread parameters and start encoding
    m_encoderThread->setParams(input, output, width, height, bitRate, frameNum, m_currentCodec);
    m_encoderThread->start();
}

void MainWindow::updateProgress(int current, int total)
{
    progressBar->setRange(0, total);
    progressBar->setValue(current);
}

void MainWindow::updateLog(const QString& log)
{
    logEdit->append(log);
    // Auto scroll to last line
    QTextCursor cursor = logEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    logEdit->setTextCursor(cursor);
}

void MainWindow::onEncodeFinished(bool success)
{
    // Restore control states
    startEncodeBtn->setEnabled(true);
    selectInputBtn->setEnabled(true);
    selectOutputBtn->setEnabled(true);
    codecCombo->setEnabled(true);

    if (success) {
        QMessageBox::information(this, "Encode Success", "Video encoding completed! Output file saved.");
    }
    else {
        QMessageBox::critical(this, "Encode Failed", "Video encoding error! Check log for details.");
    }
}

void MainWindow::on_codecCombo_currentIndexChanged(int index)
{
    m_currentCodec = codecCombo->itemData(index).toInt();
    // Auto update output file suffix
    QString currentOutput = outputEdit->text();
    if (!currentOutput.isEmpty()) {
        QString suffix = (m_currentCodec == AV_CODEC_ID_H264) ? "h264" : "h265";
        if (currentOutput.contains(".")) {
            currentOutput = currentOutput.left(currentOutput.lastIndexOf(".")) + "." + suffix;
        }
        else {
            currentOutput += "." + suffix;
        }
        outputEdit->setText(currentOutput);
    }
}

void MainWindow::on_selectPlayFileBtn_clicked()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Select a video file", "", "Video file (*.h264 *.h265 *.mp4 *.avi);;All files (*.*)");
    if (!path.isEmpty()) {
        playFileEdit->setText(path);
    }
}

void MainWindow::on_startPlayBtn_clicked()
{
    QString playFile = playFileEdit->text().trimmed();
    if (playFile.isEmpty()) {
        QMessageBox::warning(this, "Parameter error", "Please select a video file to play!");
        return;
    }

    // 禁用播放按钮防止重复点击
    startPlayBtn->setEnabled(false);
    selectPlayFileBtn->setEnabled(false);

    m_playerThread->setFilePath(playFile);
    m_playerThread->start();
}

void MainWindow::updatePlayLog(const QString& log)
{
    logEdit->append("[play] " + log);
    QTextCursor cursor = logEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    logEdit->setTextCursor(cursor);
}

void MainWindow::onPlayError(const QString& error)
{
    logEdit->append("[Playback error] " + error);
    QMessageBox::critical(this, "Playback error", error);

    // 恢复按钮状态
    startPlayBtn->setEnabled(true);
    selectPlayFileBtn->setEnabled(true);
}

void MainWindow::onPlayFinished()
{
    logEdit->append("[play] Playback ended");
    // 恢复按钮状态
    startPlayBtn->setEnabled(true);
    selectPlayFileBtn->setEnabled(true);
}