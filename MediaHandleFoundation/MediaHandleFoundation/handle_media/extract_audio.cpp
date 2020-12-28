//
//  get_audio.cpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/17.
//

#include "extract_audio.hpp"
extern "C" {
#include <libavformat/avformat.h>
}

int extract_audio(const char *src, const char *dst) {
    
    int ret;
    AVPacket pkt;
    int audio_stream_index;
    AVFormatContext *fmt_ctx = nullptr;
    
    // 参数中取到源视频的路径和目标文件路径
    if (!src || !dst) {
        av_log(NULL, AV_LOG_ERROR, "src or dst is null\n");
        return -1;
    }
    // 打开源文件
    ret = avformat_open_input(&fmt_ctx, src, nullptr, nullptr);
    if (ret<0) {
        av_log(NULL, AV_LOG_ERROR, "can not open file: %s\n", av_err2str(ret));
        return -1;
    }
    // 打开目标文件
    FILE *dst_fd = fopen(dst, "wb");
    if (!dst_fd) {
        av_log(NULL, AV_LOG_ERROR, "can not open dst file\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    // 输出 media_info
    av_dump_format(fmt_ctx, 0, src, 0);
    // 获取 stream 的 index，此处是音频的索引
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "can not find best stream\n");
        avformat_close_input(&fmt_ctx);
        fclose(dst_fd);
        return -1;
    }
    audio_stream_index = ret;
    av_init_packet(&pkt);
    // 逐个读取源视频的包
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        // 判断是否是音频，并输出音频数据到目标文件
        if (pkt.stream_index == audio_stream_index) {
            fwrite(pkt.data, 1, pkt.size, dst_fd);
        }
        // 释放内存
        av_packet_unref(&pkt);
    }
    // 关闭
    avformat_close_input(&fmt_ctx);
    if (dst_fd) {
        fclose(dst_fd);
    }
    return 0;
}
