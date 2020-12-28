//
//  remuxing.cpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/19.
//

#include "remuxing.hpp"
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


int remuxing(const char *src, const char *dst) {
    
    int ret;
    int stream_index = 0;
    
    int stream_index_map_size = 0;
    int *stream_index_map = nullptr;        // 用来记录视频流、音频流、字幕流的数组
    
    AVPacket pkt;
    AVOutputFormat *out_fmt = nullptr;
    AVFormatContext *in_fmt_ctx = nullptr;
    AVFormatContext *out_fmt_ctx = nullptr;
    
    if (src == NULL || dst == NULL) return -1;
    if ((ret = avformat_open_input(&in_fmt_ctx, src, 0, 0)) < 0)  goto __close;
    
    // 获取流的信息
    if ((ret = avformat_find_stream_info(in_fmt_ctx, 0)) < 0)  goto __close;
    av_dump_format(in_fmt_ctx, 0, src, 0);
    
    // 创建输出文件上下文
    avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, dst);
    if (!out_fmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "can not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto __close;
    }
    // 赋值输出文件格式
    out_fmt = out_fmt_ctx->oformat;
    // 只统计 audio video subtitle 的流
    stream_index_map_size = in_fmt_ctx->nb_streams;
    stream_index_map = (int *)malloc(sizeof(int) * stream_index_map_size);
    memset(stream_index_map, 0, stream_index_map_size);
    for (int i = 0; i < in_fmt_ctx->nb_streams; ++i) {
        AVStream *out_stream;
        AVStream *in_stream = in_fmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE ){
            stream_index_map[i] = -1;
            continue;
        }
        stream_index_map[i] = stream_index++;
        out_stream = avformat_new_stream(out_fmt_ctx, NULL);
        if (!out_stream) {
            ret = AVERROR_UNKNOWN;
            goto __close;
        }
        // 赋值源文件的编解码器
        if ((ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar)) < 0)  goto __close;
        // 标记
        out_stream->codecpar->codec_tag = 0;
    }
    av_dump_format(out_fmt_ctx, 0, dst, 1);
    
    // 打开目标文件
    if (!(out_fmt->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&out_fmt_ctx->pb, dst, AVIO_FLAG_WRITE)) < 0) goto __close;
    }
    
    // 写入媒体文件头信息
    if ((ret = avformat_write_header(out_fmt_ctx, NULL)) < 0) goto __close;
    
    // 循环写入数据
    while (true) {
        AVStream *in_stream, *out_stream;
        
        // 读取源文件包信息
        ret = av_read_frame(in_fmt_ctx, &pkt);
        if (ret < 0) break;
        
        // 拿到输入文件的流
        in_stream = in_fmt_ctx->streams[pkt.stream_index];
        
        // 如果不是 audio video subtitle ，丢弃
        if (stream_index_map[pkt.stream_index] < 0) {
            av_packet_unref(&pkt);
            continue;
        }
        
        // 构建输出流
        pkt.stream_index = stream_index_map[pkt.stream_index];
        out_stream = out_fmt_ctx->streams[pkt.stream_index];

        // 复制
        // dts 解码时间戳，这个时间戳的意义在于告诉播放器该在什么时候解码这一帧的数据
        // pts 显示时间戳，这个时间戳用来告诉播放器该在什么时候显示这一帧的数据。
        pkt.dts = av_rescale_q(pkt.dts, in_stream->time_base, out_stream->time_base);
        pkt.pts = av_rescale_q(pkt.pts, in_stream->time_base, out_stream->time_base);
        //根据时间基转换duration
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        
        // 写入数据
        ret = av_interleaved_write_frame(out_fmt_ctx, &pkt);
        if (ret < 0) break;
        
        av_packet_unref(&pkt);
    }
    free(stream_index_map);
    // 写入媒体文件尾信息
    av_write_trailer(out_fmt_ctx);
    
__close:
    avformat_close_input(&in_fmt_ctx);
    if (out_fmt_ctx && !(out_fmt_ctx->flags & AVFMT_NOFILE))
        avio_closep(&out_fmt_ctx->pb);
    avformat_free_context(out_fmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "error: %s\n", av_err2str(ret));
        return -1;
    }
    
    return 0;
}
