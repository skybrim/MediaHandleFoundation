//
//  cut_video.cpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/20.
//

#include "cut_video.hpp"
#include <stdlib.h>
extern "C" {
#ifdef __cplusplus
 #define __STDC_CONSTANT_MACROS
 #ifdef _STDINT_H
  #undef _STDINT_H
 #endif
 #include <stdint.h>
#endif
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
}

int cut_video(double from_second, double end_second, const char *src, const char *dst) {

    int ret;
    AVPacket pkt;
    int64_t *dts_start_from;
    int64_t *pts_start_from;
    AVOutputFormat *out_fmt = nullptr;
    AVFormatContext *in_fmt_ctx = nullptr;
    AVFormatContext *out_fmt_ctx = nullptr;
    
    // 构建输入上下文
    if ((ret = avformat_open_input(&in_fmt_ctx, src, 0, 0)) < 0) goto end;

    if ((ret = avformat_find_stream_info(in_fmt_ctx, 0)) < 0) goto end;
    av_dump_format(in_fmt_ctx, 0, src, 0);
    
    // 构建输出上下文
    avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, dst);
    if (!out_fmt_ctx) {
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    
    // 创建流及参数拷贝
    for (int i = 0; i < in_fmt_ctx->nb_streams; ++i) {
        AVStream *in_stream = in_fmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(out_fmt_ctx, NULL);
        if (!out_stream) {
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        if ((ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar)) < 0)
            goto end;
        out_stream->codecpar->codec_tag = 0;
    }
    av_dump_format(out_fmt_ctx, 0, dst, 1);
    
    // 打开目标文件
    out_fmt = out_fmt_ctx->oformat;
    if (!(out_fmt->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&out_fmt_ctx->pb, dst, AVIO_FLAG_WRITE)) < 0) goto end;
    }
    
    // 写入媒体头信息
    ret = avformat_write_header(out_fmt_ctx, NULL);
    if (ret < 0) goto end;
    
    //跳转到指定帧
    ret = av_seek_frame(in_fmt_ctx, -1, from_second * AV_TIME_BASE, AVSEEK_FLAG_ANY);
    if (ret < 0) goto end;
    
    dts_start_from = (int64_t *)malloc(sizeof(int64_t) * in_fmt_ctx->nb_streams);
    memset(dts_start_from, 0, sizeof(int64_t) * in_fmt_ctx->nb_streams);
    pts_start_from = (int64_t *)malloc(sizeof(int64_t) * in_fmt_ctx->nb_streams);
    memset(pts_start_from, 0, sizeof(int64_t) * in_fmt_ctx->nb_streams);
    
    while (true) {
        AVStream *in_stream, *out_stream;
        
        // 读取包
        ret = av_read_frame(in_fmt_ctx, &pkt);
        if (ret < 0) goto end;
        
        in_stream = in_fmt_ctx->streams[pkt.stream_index];
        out_stream = out_fmt_ctx->streams[pkt.stream_index];
        
        // 如果时间大于结束时间，跳出
        if (av_q2d(in_stream->time_base) * pkt.pts > end_second) {
            av_packet_unref(&pkt);
            break;
        }
        
        // 将截取后的每个流的起始dts 、pts记录下来，作为开始时间，用来做后面的时间基转换
        if (dts_start_from[pkt.stream_index] == 0)
            dts_start_from[pkt.stream_index] = pkt.dts;
        if (pts_start_from[pkt.stream_index] == 0)
            pts_start_from[pkt.stream_index] = pkt.pts;
        
        // 转换时间基
        pkt.dts = av_rescale_q(pkt.dts-dts_start_from[pkt.stream_index], in_stream->time_base, out_stream->time_base);
        pkt.pts = av_rescale_q(pkt.pts-pts_start_from[pkt.stream_index], in_stream->time_base, out_stream->time_base);
        pkt.dts = pkt.dts < 0 ? 0 : pkt.dts;
        pkt.pts = pkt.pts < 0 ? 0 : pkt.pts;
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        
        // 写数据
        ret = av_interleaved_write_frame(out_fmt_ctx, &pkt);
        if (ret < 0) break;
        
        av_packet_unref(&pkt);
    }
    
    free(dts_start_from);
    free(pts_start_from);
    
    av_write_trailer(out_fmt_ctx);
    
end:
    avformat_close_input(&in_fmt_ctx);
    if (out_fmt && !(out_fmt->flags & AVFMT_NOFILE)) avio_closep(&out_fmt_ctx->pb);
    avformat_free_context(out_fmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "error: %s\n", av_err2str(ret));
        return -1;
    }
    return 0;
}
