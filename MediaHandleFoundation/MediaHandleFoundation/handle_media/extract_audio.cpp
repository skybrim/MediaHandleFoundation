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
    FILE *f = nullptr;
    int audio_stream_index;
    AVFormatContext *fmt_ctx = nullptr;

    if (!src || !dst) {
        av_log(NULL, AV_LOG_ERROR, "src or dst is null\n");
        return -1;
    }
    if ((ret = avformat_open_input(&fmt_ctx, src, NULL, NULL))<0) goto __close;
    av_dump_format(fmt_ctx, 0, src, 0);
    
    if ((ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0)) < 0) goto __close;
    audio_stream_index = ret;
    
    av_init_packet(&pkt);
    
    f = fopen(dst, "wb");
    if (!f) {
        av_log(NULL, AV_LOG_ERROR, "can not open dst file\n");
        goto __close;
    }
    
    // 逐个读取源视频的包
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        // 判断是否是音频，并输出音频数据到目标文件
        if (pkt.stream_index == audio_stream_index) {
            fwrite(pkt.data, 1, pkt.size, f);
        }
        av_packet_unref(&pkt);
    }

__close:
    // 关闭
    avformat_close_input(&fmt_ctx);
    fclose(f);
    
    return 0;
}
