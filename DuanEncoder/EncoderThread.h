#pragma once
#include <QThread>
#include <QString>
#include <QObject>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/error.h>
#include <libavutil/rational.h>
}

class EncoderThread : public QThread {
    Q_OBJECT
public:
    explicit EncoderThread(QObject* parent = nullptr);

    // 设置编码参数
    void setParams(const QString& inputYuv, const QString& outputFile,
        int width, int height, int bitRate, int frameNum,
        int codecType = AV_CODEC_ID_H264);

protected:
    void run() override; // 线程执行函数

signals:
    void encodeProgress(int current, int total); // 进度更新
    void encodeLog(const QString& log);          // 日志输出
    void encodeFinished(bool success);           // 完成通知

private:
    // 错误处理函数
    void printError(const char* msg, int errnum);
    // 刷新编码器
    int flushEncoder(AVFormatContext* fmt_ctx, AVCodecContext* enc_ctx, int stream_idx);

    // 编码参数
    QString m_inputYuv;
    QString m_outputFile;
    int m_width = 480;
    int m_height = 272;
    int m_bitRate = 400000;
    int m_frameNum = 100;
    int m_codecType = AV_CODEC_ID_H264;
};