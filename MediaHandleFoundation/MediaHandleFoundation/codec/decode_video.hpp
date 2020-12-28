//
//  h264_decodec.hpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/20.
//

#ifndef decode_video_hpp
#define decode_video_hpp

#include <stdio.h>

/// 视频解码为yuv420p
/// @param src 源文件
/// @param dst 目标文件
int decode_video(const char *src, const char *dst);

#endif /* h264_decodec_hpp */
