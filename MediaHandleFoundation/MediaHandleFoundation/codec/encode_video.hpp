//
//  h264_encodec.hpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/20.
//

#ifndef encode_video_hpp
#define encode_video_hpp

#include <stdio.h>

/// 编码视频流
/// @param src 源文件，默认yuv420p
/// @param dst 输出文件
/// @param codec_name 编码器名称
/// @param encode_params 编码参数，按照width/height/bit_rate/fps顺序传入，传NULL使用默认参数
int encode_video(const char *src, const char *dst, const char *codec_name, int *encode_params);

#endif /* h264_encodec_hpp */
