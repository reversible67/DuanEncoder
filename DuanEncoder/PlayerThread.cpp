#define _CRT_SECURE_NO_WARNINGS
#include "PlayerThread.h"
#include <QDebug>

PlayerThread::PlayerThread(QObject* parent)
    : QThread(parent), m_stopFlag(false), m_sdlWindow(nullptr),
    m_sdlRenderer(nullptr), m_sdlTexture(nullptr) {}

PlayerThread::~PlayerThread() {
    stopPlayback();
    wait();
}

void PlayerThread::setFilePath(const QString& filePath) {
    m_filePath = filePath;
}

void PlayerThread::stopPlayback() {
    m_stopFlag = true;
}

void PlayerThread::printError(const char* msg, int errnum) {
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(errnum, err_buf, sizeof(err_buf));
    emit playLog(QString("%1: %2").arg(msg).arg(err_buf));
}

void PlayerThread::run() {
    m_stopFlag = false;
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    const AVCodec* codec = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* frame_yuv = nullptr;
    AVPacket* pkt = nullptr;
    SwsContext* sws_ctx = nullptr;
    AVCodecParameters* codec_par = nullptr;
    int video_stream_index = -1;
    int ret = 0;
    SDL_Rect sdl_rect{};
    uint8_t* out_buffer = nullptr;
    int buffer_size = 0;
    char info_buf[1024] = { 0 };

    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        emit playError(QString("SDL init failure: %1").arg(SDL_GetError()));
        goto cleanup;
    }

    // 打开输入文件
    ret = avformat_open_input(&fmt_ctx, m_filePath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        printError("Unable to open the input file.", ret);
        goto cleanup;
    }

    // 获取流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        printError("Unable to retrieve stream information.", ret);
        goto cleanup;
    }

    // 查找视频流
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        emit playError("No video stream found.");
        goto cleanup;
    }

    // 获取解码器参数
    codec_par = fmt_ctx->streams[video_stream_index]->codecpar;
    
    // 查找解码器
    codec = avcodec_find_decoder(codec_par->codec_id);

    if (!codec) {
        emit playError("No suitable decoder found.");
        goto cleanup;
    }

    // 创建解码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        emit playError("Unable to allocate the decoder context.");
        goto cleanup;
    }

    // 复制解码器参数
    ret = avcodec_parameters_to_context(codec_ctx, codec_par);
    if (ret < 0) {
        printError("Unable to copy codec parameters.", ret);
        goto cleanup;
    }

    // 打开解码器
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        printError("Unable to open the decoder.", ret);
        goto cleanup;
    }

    // 创建SDL窗口和渲染器
    m_sdlWindow = SDL_CreateWindow("duan video player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, codec_ctx->width, codec_ctx->height, SDL_WINDOW_SHOWN);
    if (!m_sdlWindow) {
        emit playError(QString("Unable to create the SDL window.: %1").arg(SDL_GetError()));
        goto cleanup;
    }

    m_sdlRenderer = SDL_CreateRenderer(m_sdlWindow, -1, SDL_RENDERER_ACCELERATED);
    if (!m_sdlRenderer) {
        emit playError(QString("Unable to create the SDL renderer.: %1").arg(SDL_GetError()));
        goto cleanup;
    }

    // 创建YUV纹理
    m_sdlTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, codec_ctx->width, codec_ctx->height);

    if (!m_sdlTexture) {
        emit playError(QString("Unable to create the SDL texture.: %1").arg(SDL_GetError()));
        goto cleanup;
    }

    // 初始化帧和数据包
    frame = av_frame_alloc();
    frame_yuv = av_frame_alloc();
    pkt = av_packet_alloc();

    if (!frame || !frame_yuv || !pkt) {
        emit playError("Unable to allocate a frame or packet.");
        goto cleanup;
    }

    // 分配YUV缓冲区
    buffer_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codec_ctx->width, codec_ctx->height, 1);
    out_buffer = (uint8_t*)av_malloc(buffer_size * sizeof(uint8_t));
    av_image_fill_arrays(frame_yuv->data, frame_yuv->linesize, out_buffer, AV_PIX_FMT_YUV420P, codec_ctx->width, codec_ctx->height, 1);

    // 创建图像转换上下文
    sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);

    if (!sws_ctx) {
        emit playError("Unable to create the image conversion context.");
        goto cleanup;
    }

    // 打印文件信息
    emit playLog("fileInfo:");
    av_dump_format(fmt_ctx, 0, m_filePath.toUtf8().constData(), 0);
    emit playLog(QString("video width: %1, height: %2").arg(codec_ctx->width).arg(codec_ctx->height));

    sdl_rect.x = 0;
    sdl_rect.y = 0;
    sdl_rect.w = codec_ctx->width;
    sdl_rect.h = codec_ctx->height;

    // 读取数据包并解码
    while (!m_stopFlag && av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            // 发送数据包到解码器
            ret = avcodec_send_packet(codec_ctx, pkt);
            if (ret < 0) {
                printError("send packet to decoder failure.", ret);
                break;
            }

            // 接收解码后的帧
            while (!m_stopFlag && ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                else if (ret < 0) {
                    printError("receive decode frame failure", ret);
                    goto cleanup;
                }

                // 转换图像格式为YUV420P
                sws_scale(sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, codec_ctx->height, frame_yuv->data, frame_yuv->linesize);

                // 更新纹理并渲染
                SDL_UpdateYUVTexture(m_sdlTexture, &sdl_rect,
                    frame_yuv->data[0], frame_yuv->linesize[0],
                    frame_yuv->data[1], frame_yuv->linesize[1],
                    frame_yuv->data[2], frame_yuv->linesize[2]);

                SDL_RenderClear(m_sdlRenderer);
                SDL_RenderCopy(m_sdlRenderer, m_sdlTexture, nullptr, &sdl_rect);
                SDL_RenderPresent(m_sdlRenderer);

                // 控制播放速度
                SDL_Delay(40); // 约25fps
            }
        }
        av_packet_unref(pkt);
    }

    // 处理解码器中剩余的帧
    emit playLog("Processing remaining frames...");
    ret = avcodec_send_packet(codec_ctx, nullptr);
    while (!m_stopFlag && ret >= 0) {
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        else if (ret < 0) {
            printError("Failed to receive remaining frames.", ret);
            goto cleanup;
        }

        sws_scale(sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0,
            codec_ctx->height, frame_yuv->data, frame_yuv->linesize);

        SDL_UpdateYUVTexture(m_sdlTexture, &sdl_rect,
            frame_yuv->data[0], frame_yuv->linesize[0],
            frame_yuv->data[1], frame_yuv->linesize[1],
            frame_yuv->data[2], frame_yuv->linesize[2]);

        SDL_RenderClear(m_sdlRenderer);
        SDL_RenderCopy(m_sdlRenderer, m_sdlTexture, nullptr, &sdl_rect);
        SDL_RenderPresent(m_sdlRenderer);
        SDL_Delay(40);
    }

    emit playLog("player finished");

cleanup:
    // 释放资源
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (out_buffer) av_free(out_buffer);
    if (frame_yuv) av_frame_free(&frame_yuv);
    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (fmt_ctx) avformat_close_input(&fmt_ctx);

    // 释放SDL资源
    if (m_sdlTexture) SDL_DestroyTexture(m_sdlTexture);
    if (m_sdlRenderer) SDL_DestroyRenderer(m_sdlRenderer);
    if (m_sdlWindow) SDL_DestroyWindow(m_sdlWindow);

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);

    emit playFinished();
}