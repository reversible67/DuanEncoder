#define _CRT_SECURE_NO_WARNINGS  // 禁用安全函数警告
#include "EncoderThread.h"
#include "stdafx.h"
#include <QDebug>
#include <cstdio>

EncoderThread::EncoderThread(QObject* parent) : QThread(parent) {}

void EncoderThread::setParams(const QString& inputYuv, const QString& outputFile,
    int width, int height, int bitRate, int frameNum, int codecType) {
    m_inputYuv = inputYuv;
    m_outputFile = outputFile;
    m_width = width;
    m_height = height;
    m_bitRate = bitRate;
    m_frameNum = frameNum;
    m_codecType = codecType;
}

void EncoderThread::printError(const char* msg, int errnum) {
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(errnum, err_buf, sizeof(err_buf));
    emit encodeLog(QString("%1: %2").arg(msg).arg(err_buf));
}

int EncoderThread::flushEncoder(AVFormatContext* fmt_ctx, AVCodecContext* enc_ctx, int stream_idx) {
    int ret = 0;
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        emit encodeLog("Failed to allocate packet");
        return AVERROR(ENOMEM);
    }

    if (!(enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY)) {
        av_packet_free(&pkt);
        return 0;
    }

    while (1) {
        ret = avcodec_send_frame(enc_ctx, NULL);
        if (ret == AVERROR_EOF) break;

        if (ret < 0) {
            printError("Error sending flush frame", ret);
            av_packet_free(&pkt);
            return ret;
        }

        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            continue;

        if (ret < 0) {
            printError("Error receiving flush packet", ret);
            av_packet_free(&pkt);
            return ret;
        }

        av_packet_rescale_ts(pkt, enc_ctx->time_base, fmt_ctx->streams[stream_idx]->time_base);
        pkt->stream_index = stream_idx;

        emit encodeLog(QString("Flush Encoder: Succeed to encode 1 frame! size:%1").arg(pkt->size));

        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);

        if (ret < 0) {
            printError("Error writing flush packet", ret);
            av_packet_free(&pkt);
            return ret;
        }
    }

    av_packet_free(&pkt);
    return 0;
}

void EncoderThread::run() {
    AVFormatContext* fmt_ctx = NULL;
    AVCodecContext* codec_ctx = NULL;
    const AVCodec* codec = NULL;
    AVStream* video_stream = NULL;
    AVFrame* frame = NULL;
    AVPacket* pkt = NULL;
    uint8_t* picture_buf = NULL;
    FILE* in_file = NULL;

    int ret = 0;
    int frame_count = 0;
    int y_size = 0;

    // 打开输入YUV文件
    in_file = fopen(m_inputYuv.toUtf8().constData(), "rb");
    if (!in_file) {
        emit encodeLog(QString("Could not open input file '%1'").arg(m_inputYuv));
        emit encodeFinished(false);
        return;
    }

    // 创建输出格式上下文
    ret = avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, m_outputFile.toUtf8().constData());
    if (ret < 0) {
        printError("Could not create output context", ret);
        goto cleanup;
    }

    // 查找编码器
    codec = avcodec_find_encoder((AVCodecID)m_codecType);
    if (!codec) {
        emit encodeLog("Could not find encoder");
        goto cleanup;
    }

    // 创建视频流
    video_stream = avformat_new_stream(fmt_ctx, NULL);
    if (!video_stream) {
        emit encodeLog("Could not create video stream");
        goto cleanup;
    }

    // 分配编码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        emit encodeLog("Could not allocate codec context");
        goto cleanup;
    }

    // 设置编码器参数
    codec_ctx->codec_id = (AVCodecID)m_codecType;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->width = m_width;
    codec_ctx->height = m_height;
    codec_ctx->bit_rate = m_bitRate;
    codec_ctx->gop_size = 250;
    codec_ctx->time_base.num = 1;
    codec_ctx->time_base.den = 25;
    codec_ctx->framerate.num = 25;
    codec_ctx->framerate.den = 1;

    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 设置编码器选项
    if (codec_ctx->codec_id == AV_CODEC_ID_H264) {
        av_opt_set(codec_ctx->priv_data, "preset", "slow", 0);
        av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
    }
    else if (codec_ctx->codec_id == AV_CODEC_ID_HEVC) {
        av_opt_set(codec_ctx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(codec_ctx->priv_data, "tune", "zero-latency", 0);
    }

    // 打开编码器
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        printError("Could not open codec", ret);
        goto cleanup;
    }

    // 复制编码器参数到流
    ret = avcodec_parameters_from_context(video_stream->codecpar, codec_ctx);
    if (ret < 0) {
        printError("Could not copy codec parameters", ret);
        goto cleanup;
    }
    video_stream->time_base = codec_ctx->time_base;

    // 打印格式信息
    av_dump_format(fmt_ctx, 0, m_outputFile.toUtf8().constData(), 1);

    // 打开输出文件IO
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&fmt_ctx->pb, m_outputFile.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            printError("Could not open output file", ret);
            goto cleanup;
        }
    }

    // 写入文件头
    ret = avformat_write_header(fmt_ctx, NULL);
    if (ret < 0) {
        printError("Error writing header", ret);
        goto cleanup;
    }

    // 分配帧结构
    frame = av_frame_alloc();
    if (!frame) {
        emit encodeLog("Could not allocate frame");
        goto cleanup;
    }
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;

    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        printError("Could not allocate frame buffer", ret);
        goto cleanup;
    }

    // 分配数据包
    pkt = av_packet_alloc();
    if (!pkt) {
        emit encodeLog("Could not allocate packet");
        goto cleanup;
    }

    // 计算YUV数据大小
    y_size = codec_ctx->width * codec_ctx->height;
    picture_buf = (uint8_t*)av_malloc(y_size * 3 / 2);
    if (!picture_buf) {
        emit encodeLog("Could not allocate picture buffer");
        goto cleanup;
    }

    // 编码主循环
    for (int i = 0; i < m_frameNum; i++) {
        // 读取YUV数据
        size_t read_size = fread(picture_buf, 1, y_size * 3 / 2, in_file);
        if (read_size != y_size * 3 / 2) {
            emit encodeLog(QString("Warning: Not enough data for frame %1").arg(i));
            break;
        }

        // 确保帧可写
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            printError("Could not make frame writable", ret);
            goto cleanup;
        }

        // 填充YUV数据
        memcpy(frame->data[0], picture_buf, y_size);
        memcpy(frame->data[1], picture_buf + y_size, y_size / 4);
        memcpy(frame->data[2], picture_buf + y_size * 5 / 4, y_size / 4);

        frame->pts = i;

        // 发送帧到编码器
        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0) {
            printError("Error sending frame to encoder", ret);
            break;
        }

        // 接收编码后的数据包
        while (ret >= 0) {
            ret = avcodec_receive_packet(codec_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;

            if (ret < 0) {
                printError("Error receiving packet", ret);
                goto cleanup;
            }

            av_packet_rescale_ts(pkt, codec_ctx->time_base, video_stream->time_base);
            pkt->stream_index = video_stream->index;

            frame_count++;
            emit encodeLog(QString("Encoded frame: %1 size:%2").arg(frame_count).arg(pkt->size));
            emit encodeProgress(frame_count, m_frameNum);

            ret = av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);

            if (ret < 0) {
                printError("Error writing packet", ret);
                goto cleanup;
            }
        }
    }

    // 刷新编码器
    ret = flushEncoder(fmt_ctx, codec_ctx, video_stream->index);
    if (ret < 0) {
        emit encodeLog("Flushing encoder failed");
        goto cleanup;
    }

    // 写入文件尾
    av_write_trailer(fmt_ctx);
    emit encodeLog("Encoding completed successfully!");
    emit encodeFinished(true);

cleanup:
    // 资源清理
    av_free(picture_buf);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&codec_ctx);

    if (fmt_ctx) {
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&fmt_ctx->pb);
        }
        avformat_free_context(fmt_ctx);
    }

    if (in_file) {
        fclose(in_file);
    }

    if (ret < 0) {
        emit encodeFinished(false);
    }
}