//
//  get_video.hpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/17.
//

#ifndef get_video_hpp
#define get_video_hpp

#include <stdio.h>

/// 抽取文件中的视频流
/// @param src 源文件
/// @param dst 输出的视频流文件
int extract_video(const char *src, const char *dst);

#endif /* get_video_hpp */
