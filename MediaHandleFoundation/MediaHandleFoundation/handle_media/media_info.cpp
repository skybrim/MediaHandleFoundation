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
    if ((ret = avformat_open_input(&fmt_ctx, src, nullptr, nullptr))<0) goto __close;
    av_dump_format(fmt_ctx, 0, src, 0);
    
__close:
    avformat_close_input(&fmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "error: %s\n", av_err2str(ret));
        return -1;
    }
    
    return 0;
}
