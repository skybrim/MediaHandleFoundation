//
//  medie_info.cpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/17.
//

#include "media_info.hpp"
extern "C" {
#include <libavformat/avformat.h>
}

int media_info(const char *src) {
    int ret;
    AVFormatContext *fmt_ctx = nullptr;

    if (src == NULL) return -1;
    ret = avformat_open_input(&fmt_ctx, src, nullptr, nullptr);
    if (ret<0) {
        av_log(NULL, AV_LOG_ERROR, "can not open file: %s\n", av_err2str(ret));
        return -1;
    }
    av_dump_format(fmt_ctx, 0, src, 0);
    avformat_close_input(&fmt_ctx);
    return 0;
}
