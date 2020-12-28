//
//  get_audio.hpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/17.
//

#ifndef get_audio_hpp
#define get_audio_hpp

#include <stdio.h>

/// 获取音频流
/// @param src 源文件
/// @param dst 输出文件
int extract_audio(const char *src, const char *dst);

#endif /* get_audio_hpp */
