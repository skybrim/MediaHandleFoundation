//
//  h264_encodec.cpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/20.
//

/*
 视频编码
 1. 查找编码器             avcodec_find_encoder_by_name
 2. 设置编码参数，打开编码器  avcodec_open2
 3. 编码                  avcodec_encode_video2
*/
#include "encode_video.hpp"
extern "C" {
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
}

int encode(const char *src, const char *dst,
           const char *codec_name,
           int width, int height, int bit_rate, int fps) {
    
    int ret;
    int index;
    AVPacket pkt;
    AVFrame *frame;
    const AVCodec *codec;
    AVCodecContext *avctx;
    FILE *in_f = nullptr;
    FILE *out_f = nullptr;
    uint8_t endcode[] = {0, 0, 1, 0xb7};
    AVPixelFormat default_pix_fmt = AV_PIX_FMT_YUV420P;
    
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        ret = AVERROR_UNKNOWN;
        av_log(NULL, AV_LOG_ERROR, "AVCodec can not alloc\n");
        goto __close;
    }
    
    avctx = avcodec_alloc_context3(codec);
    if (!avctx) {
        ret = AVERROR_UNKNOWN;
        av_log(NULL, AV_LOG_ERROR, "AVCodecContext can not alloc\n");
        goto __close;
    }
    
    avctx->bit_rate = bit_rate;
    avctx->width = width;
    avctx->height = height;
    avctx->time_base = (AVRational){1, fps};
    avctx->framerate = (AVRational){fps, 1};
    avctx->gop_size = 10;
    avctx->max_b_frames = 1;
    avctx->pix_fmt = default_pix_fmt;
    
    // preset表示采用一个预先设定好的参数集，级别是slow
    // slow表示压缩速度是慢的，慢的可以保证视频质量，用快的会降低视频质量
//    av_opt_set(avctx->priv_data, "preset", "slow", 0);

    if ((ret = avcodec_open2(avctx, codec, NULL)) < 0) goto __close;
    
    frame = av_frame_alloc();
    if (!frame) {
        ret = AVERROR_UNKNOWN;
        av_log(NULL, AV_LOG_ERROR, "AVFrame can not alloc\n");
        goto __close;
    }
    frame->format = default_pix_fmt;
    frame->width = width;
    frame->height = height;
        
    if ((ret = av_frame_get_buffer(frame, 0)) < 0) goto __close;

    in_f = fopen(src, "rb");
    out_f = fopen(dst, "wb");
    
    index = 0;
    
    while (!feof(in_f)) {
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        if ((ret = av_frame_make_writable(frame)) < 0) goto __close;

        // 读取
        fread(frame->data[0], 1, avctx->width * avctx->height, in_f); // y
        fread(frame->data[1], 1, avctx->width * avctx->height / 4, in_f); // u
        fread(frame->data[2], 1, avctx->width * avctx->height / 4, in_f); // v
        frame->pts = index++;

        ret = avcodec_send_frame(avctx, frame);
        if (ret < 0) goto __close;
        
        if (avcodec_receive_packet(avctx, &pkt) == 0) {
            fwrite(pkt.data, 1, pkt.size, out_f);
        }
        
        av_packet_unref(&pkt);
        av_log(NULL, AV_LOG_INFO, "encoding index %d\n", index);
    }
    fwrite(endcode, 1, sizeof(endcode), out_f);
    
__close:
    avcodec_free_context(&avctx);
    fclose(in_f);
    fclose(out_f);
    av_frame_free(&frame);
    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "error: %s\n", av_err2str(ret));
        return -1;
    }
    
    
    return 0;
}

int encode_video(const char *src, const char *dst, const char *codec_name, int *encode_params) {
    
    int fps;
    int width;
    int height;
    int bit_rate;
    
    if (src == NULL || dst == NULL) {
        av_log(NULL, AV_LOG_ERROR, "src or dst is null\n");
        return -1;
    }
    
    if (encode_params != NULL) {
        width = encode_params[0];
        height = encode_params[1];
        fps = encode_params[2];
        bit_rate = encode_params[3];
    } else {
        width = 1920;
        height = 1080;
        fps = 25;
        bit_rate = 4000000;
    }
    encode(src, dst, codec_name, width, height, bit_rate, fps);
    
    return 0;
}
