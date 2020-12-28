//
//  aac_decodec.cpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/20.
//

#include "decode_audio.hpp"
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#define MAX_AUDIO_FRAME_SIZE 192000

int decode_audio(const char *src, const char *dst) {
    int ret;
    FILE *f = nullptr;
    int audio_stream_index = -1;
    AVFormatContext *fmt_ctx = nullptr;

    AVPacket pkt;                        // 音频压缩数据包
    AVFrame *frame = nullptr;            // 音频帧
    const AVCodec *codec = nullptr;      // 音频解码器
    AVCodecContext  *avctx = nullptr;    // 音频解码器上下文

    int in_sample_rate;                  // 输入采样率
    int out_sample_rate;                 // 输出采样率
    uint64_t in_channel_layout;          // 输入声道
    uint64_t out_channel_layout;         // 输出声道
    AVSampleFormat in_sample_fmt;        // 输入采样精度
    AVSampleFormat out_sample_fmt;       // 输出采样精度
    SwrContext *audio_convert_ctx;       // 音频采样上下文

    int out_buffer_size;                 // 音频解码缓存实际大小
    uint8_t *out_buffer = nullptr;       // 音频解码缓存空间

    
    if (src == NULL || dst == NULL) {
        av_log(NULL, AV_LOG_ERROR, "src or dst is null\n");
        return -1;
    }
    if ((ret = avformat_open_input(&fmt_ctx, src, NULL, NULL)) < 0) goto __close;
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) goto __close;
    av_dump_format(fmt_ctx, 0, src, 0);
    // 找到音频流index
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (ret < 0) goto __close;
    audio_stream_index = ret;
    // 构建解码上下文和解码器
    avctx = avcodec_alloc_context3(NULL);
    if (!avctx) {
        av_log(NULL, AV_LOG_ERROR, "AVCodecContext alloc error\n");
        ret = AVERROR_UNKNOWN;
        goto __close;
    }
    ret = avcodec_parameters_to_context(avctx, fmt_ctx->streams[audio_stream_index]->codecpar);
    if (ret < 0) goto __close;
    codec = avcodec_find_decoder(fmt_ctx->streams[audio_stream_index]->codecpar->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "AVCodec can not find\n");
        ret = AVERROR_UNKNOWN;
        goto __close;
    }
    if ((ret = avcodec_open2(avctx, codec, NULL)) < 0) goto __close;
    
    // 设置重采样参数
    // 输入信息
    in_sample_fmt = avctx->sample_fmt;
    in_channel_layout = avctx->channel_layout;
    in_sample_rate = avctx->sample_rate;
    
    // 输出信息
    out_channel_layout = AV_CH_LAYOUT_STEREO;
    out_sample_rate = in_sample_rate;
    out_sample_fmt = AV_SAMPLE_FMT_S16;
    
    // 申请输出缓存
    out_buffer = (uint8_t *)malloc(MAX_AUDIO_FRAME_SIZE * 2);
    
    // 重采样上下文
    audio_convert_ctx = swr_alloc();
    swr_alloc_set_opts(audio_convert_ctx,
                       out_channel_layout, out_sample_fmt, out_sample_rate,
                       in_channel_layout, in_sample_fmt, in_sample_rate,
                       0, NULL);
    swr_init(audio_convert_ctx);

    // 循环解码
    frame = av_frame_alloc();
    if (!frame) {
        ret = AVERROR_UNKNOWN;
        av_log(NULL, AV_LOG_ERROR, "AVFrame can not alloc\n");
        goto __close;
    }
    
    av_init_packet(&pkt);
    f = fopen(dst, "wb");
    
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == audio_stream_index) {
            // 发送 packet
            ret = avcodec_send_packet(avctx, &pkt);
            if (ret < 0) goto __close;
            // 接收 frame，可能一次send，多次receive
            while (avcodec_receive_frame(avctx, frame) == 0) {
                swr_convert(audio_convert_ctx,
                            &out_buffer, frame->nb_samples,
                            (const uint8_t **)frame->data, frame->nb_samples);
                out_buffer_size = av_samples_get_buffer_size(NULL,
                                                             av_get_channel_layout_nb_channels(out_channel_layout),
                                                             frame->nb_samples,
                                                             AV_SAMPLE_FMT_S16,
                                                             1);
                fwrite(out_buffer, 1, out_buffer_size, f);
            }
        }
        av_packet_unref(&pkt);
    }
    
__close:
    
    free(out_buffer);
    av_frame_free(&frame);
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&avctx);
    fclose(f);
    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "error: %s\n", av_err2str(ret));
        return -1;
    }
    
    return 0;
}
