//
//  cut_video.hpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/20.
//

#ifndef cut_video_hpp
#define cut_video_hpp

#include <stdio.h>

/// 剪切视频
/// @param from_second 起始时间
/// @param end_second 结束时间
/// @param src 源文件
/// @param dst 目标文件
int cut_video(double from_second, double end_second, const char *src, const char *dst);

#endif /* cut_video_hpp */
