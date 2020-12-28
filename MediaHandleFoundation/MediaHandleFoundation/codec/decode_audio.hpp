//
//  aac_decodec.hpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/20.
//

#ifndef decode_audio_hpp
#define decode_audio_hpp

#include <stdio.h>


/// 音频解码，保存为pcm数据
/// @param src 源文件
/// @param dst 目标文件
int decode_audio(const char *src, const char *dst);

#endif /* aac_decodec_hpp */
