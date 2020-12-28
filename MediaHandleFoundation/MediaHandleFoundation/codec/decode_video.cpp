//
//  h264_decodec.cpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/20.
//

/*
 视频解码
 1. 查找解码器  avcodec_find_decoder
 2. 打开解码器  avcodec_open2
 3. 解码       avcodec_decode_video2
*/

#include "decode_video.hpp"
extern "C" {
#ifdef __cplusplus
 #define __STDC_CONSTANT_MACROS
 #ifdef _STDINT_H
  #undef _STDINT_H
 #endif
 #include <stdint.h>
#endif
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}

int decode_video(const char *src, const char *dst) {
    
    int ret = 0;
    FILE *f = nullptr;
    
    AVPacket pkt;
    AVFrame *frame;
    AVFrame *out_frame;
    AVStream *stream = nullptr;
    int video_stream_index = -1;
    AVFormatContext *fmt_ctx = nullptr;

    const AVCodec *codec;
    AVCodecContext *avctx = nullptr;
    SwsContext *yuv_convert_ctx = nullptr;
    
    int buffer_size;

    //开辟一块内存空间
    uint8_t *out_buffer;
    
    if (src == NULL || dst == NULL) {
        av_log(NULL, AV_LOG_ERROR, "src or dst is null\n");
        return -1;
    }
    if ((ret = avformat_open_input(&fmt_ctx, src, NULL, NULL)) < 0) goto __close;
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)  goto __close;
    av_dump_format(fmt_ctx, 0, src, 0);
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) goto __close;
    video_stream_index = ret;
    stream = fmt_ctx->streams[video_stream_index];
    
    // 重点一
    // 查找解码器
    codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        ret = AVERROR_UNKNOWN;
        av_log(NULL, AV_LOG_ERROR, "AVCodec can not alloc\n");
        goto __close;
    }
    avctx = avcodec_alloc_context3(NULL);
    if (!avctx) {
        ret = AVERROR_UNKNOWN;
        av_log(NULL, AV_LOG_ERROR, "AVCodecContext can not alloc\n");
        goto __close;
    }
    // 将输入流中的解码参数复制到解码上下文中
    if ((ret = avcodec_parameters_to_context(avctx, stream->codecpar)) < 0) goto __close;
    // 打开解码器
    if ((ret = avcodec_open2(avctx, codec, NULL)) < 0) goto __close;
    // 申请缩放颜色空间转换的上下文
    yuv_convert_ctx = sws_getContext(avctx->width, avctx->height, avctx->pix_fmt,
                                     avctx->width, avctx->height, AV_PIX_FMT_YUV420P,
                                     SWS_FAST_BILINEAR, NULL,
                                     NULL, NULL);
    if (yuv_convert_ctx == nullptr) {
        ret = AVERROR_UNKNOWN;
        av_log(NULL, AV_LOG_ERROR, "SwsContext can not create\n");
        goto __close;
    }
    
    frame = av_frame_alloc();
    out_frame = av_frame_alloc();
    if (!frame || !out_frame) {
        ret = AVERROR_UNKNOWN;
        av_log(NULL, AV_LOG_ERROR, "AVFrame can not alloc\n");
        goto __close;
    }

    // 设置解码时的缓冲区
    buffer_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, avctx->width, avctx->height, 1);
    out_buffer = (uint8_t *)av_malloc(buffer_size);
    
    // 根据指定的图像参数和提供的数组设置数据指针和行数
    av_image_fill_arrays(out_frame->data, out_frame->linesize, out_buffer, AV_PIX_FMT_YUV420P, avctx->width, avctx->height, 1);
    
    av_init_packet(&pkt);
    
    f = fopen(dst, "wb");
    
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == video_stream_index) {
            // 解码
            ret = avcodec_send_packet(avctx, &pkt);
            if (ret < 0) goto __close;
            while (avcodec_receive_frame(avctx, frame) == 0) {
                // 转换
                sws_scale(yuv_convert_ctx, (const uint8_t *const*)frame->data, frame->linesize, 0, frame->height, out_frame->data, out_frame->linesize);
                // 写入yuv数据，4y+u+v
                int y_len = avctx->width * avctx->height;
                int u_len = y_len / 4;
                int v_len = y_len / 4;
                fwrite(out_frame->data[0], 1, y_len, f);
                fwrite(out_frame->data[1], 1, u_len, f);
                fwrite(out_frame->data[2], 1, v_len, f);
            }
        }
        av_packet_unref(&pkt);
    }
    
__close:
    fclose(f);
    av_frame_free(&frame);
    av_frame_free(&out_frame);
    avcodec_free_context(&avctx);
    avformat_close_input(&fmt_ctx);
    sws_freeContext(yuv_convert_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "error: %s\n", av_err2str(ret));
        return -1;
    }
    return 0;
}
