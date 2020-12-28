//
//  get_video.cpp
//  practice_ffmpeg
//
//  Created by wiley on 2020/12/17.
//

#include "extract_video.hpp"
extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/log.h>
}

/// 读取extradata中的pps/sps数据
/// @param codec_extradata 解码器的extradata
/// @param codec_extradata_size extradata的大小
/// @param out_extradata 输出sps/pps数据的AVPacket包
/// @param padding AV_INPUT_BUFFER_PADDING_SIZE的值(64)
///               是用于解码的输入流的末尾必要的额外字节个数，
///               需要它主要是因为一些优化的流读取器一次读取32或者64比特，
///               可能会读取超过size大小内存的末尾

int video_extradata_read_pps_sps(const uint8_t *codec_extradata, const int codec_extradata_size, AVPacket *out_extradata, int padding) {
    
    int err;

    uint8_t data_count;                     // pps/sps数量
    uint8_t sps_done = 0;                   // sps是否处理完
    uint8_t sps_seen = 0;                   // 是否有sps数据
    uint8_t pps_seen = 0;                   // 是否有pps数据
    uint8_t sps_offset = 0;                 // sps偏移量，0
    uint8_t pps_offset = 0;                 // pps偏移量，即所有的sps的数据+sps特征码的长度
    uint16_t unit_size = 0;                 // sps/pps数据长度
    uint64_t total_size = 0;                // 所有sps/pps数据长度加上其特征码长度后的总长度
    
    uint8_t *out = nullptr;                             // 指向存放所有拷贝的sps/pps数据和其特征码数据
    const uint8_t *extradata = codec_extradata + 4;     // extradata，前4位是站位预留空字节，跳过
    static const uint8_t nal_header[4] = {0, 0, 0, 1};  // sps/pps数据前面的4bit的特征码
    
    sps_offset = pps_offset = -1;
    
    // extradata第5个字节的后2位用于表示编码数据长度所需字节数
    // 不需要，跳过，指向第6个字节
    extradata++;
    
    // 第6个字节后5位是表示接下来的sps的个数，取出然后后指向下一位
    data_count = *extradata++ & 0x1f;
    
    if (!data_count) {
        // 没有sps，处理pps
        goto pps;
    } else {
        // 有sps，开始处理sps
        sps_offset = 0;
        sps_seen = 1;
    }
    
    while (data_count--) {
        err = AVERROR(EINVAL);
        
        // 开始循环处理，基本结构就是 2个存储pps/sps数据长度的字节+pps/sps数据
        // 先取出数据长度
        unit_size = (extradata[0] << 8) | extradata[1];
        total_size += unit_size + 4; // 4 是特征码的长度
        // total_size太大会造成数据溢出
        if (total_size > INT_MAX - padding) {
            av_log(NULL, AV_LOG_ERROR, "extradata too big\n");
            av_free(out);
            return err;
        }
        
        // extradata + 2(存储长度) + unit_size比整个扩展数据都长了表明数据是异常的
        if (extradata + 2 + unit_size > codec_extradata + codec_extradata_size) {
            av_log(NULL, AV_LOG_ERROR, "Packet header is not contained in global extradata\n");
            av_free(out);
            return err;
        }
        
        // 申请存储sps的内存空间
        if ((err = av_reallocp(&out, total_size + padding)) < 0)
            return err;
        
        // 写入到out
        memcpy(out + total_size - unit_size - 4, nal_header, 4);
        memcpy(out + total_size - unit_size, extradata + 2, unit_size); // +2，跳过两个存储长度的字节
        
        // 处理完本次的pps/sps，调到下一个
        extradata += 2 + unit_size;
    pps:
        //处理pps,sps_done++，标记sps处理完成
        if (!unit_size && !sps_done++) {
            // 移动到下个字节，该字节存储pps的长度
            data_count = *extradata++;
            if (data_count) {
                pps_offset = total_size;
                pps_seen = 1;
            }
        }
    }

    // 如果out有数据，那么将out + total_size后面padding(即64)个字节用0替代
    if (out) memset(out + total_size, 0, padding);
        
    if (!sps_seen) av_log(NULL, AV_LOG_INFO, "SPS missing\n");
    if (!pps_seen) av_log(NULL, AV_LOG_INFO, "PPS missing\n");

    out_extradata->data = out;
    out_extradata->size = (int)total_size;
    
    return 0;
}


/// 申请增加内存空间，写入帧前特征码，构建输出AVPacket
/// @param out 输出上下文
/// @param sps_pps sps_pps信息
/// @param sps_pps_size sps_pps长度
/// @param in 需要写入的数据
/// @param in_size 写入的数据的长度
static int alloc_and_copy(AVPacket *out, const uint8_t *sps_pps, uint32_t sps_pps_size, const uint8_t *in, uint32_t in_size) {
    
    uint32_t offset = out->size; // 第一次进来是0
    uint8_t nal_header_size = sps_pps==NULL? 3 : 4; // 4个字节给sps/pps帧的特征码，3个字节给其他帧的特征码
    int err;
    
    // 根据帧的大小，申请内存空间
    err = av_grow_packet(out, sps_pps_size + in_size + nal_header_size);
    if (err < 0) return err;
    
    // 写入sps或pps数据
    if (sps_pps) memcpy(out->data+offset, sps_pps, sps_pps_size);
    
    // 写入特征码，最后一位写1
    for (int i = 0; i < nal_header_size; ++i) {
        (out->data + offset + sps_pps_size)[i] = i==nal_header_size-1 ? 1 : 0;
    }
    
    // 写入帧数据
    memcpy(out->data + nal_header_size + sps_pps_size + offset, in, in_size);
    
    return 0;
}

int h264_video_write(AVFormatContext *fmt_ctx, AVPacket *in, FILE *dst_fd) {
    
    AVPacket *out = NULL;// 输出包指针
    AVPacket sps_pps_pkt;// sps_pps 信息
    long len;
    int ret = 0;
    uint8_t unit_type;
    int32_t nal_size;
    uint32_t cumul_size = 0;
    const uint8_t *buf;
    const uint8_t *buf_end;
    int buf_size;
    int i;
    
    // 初始化输出的AVPackt的指针
    out = av_packet_alloc();
    
    buf = in->data;
    buf_size = in->size;
    buf_end = in->data + in->size;
    
    do {
        ret = AVERROR(EINVAL); // Invalid argument
        
        // 每一帧的前4个字节，是视频帧的长度
        // 如果buf中的数据，小于4字节，跳出处理
        if (buf + 4 > buf_end)
            goto fail;
        
        // 获取帧的长度，即buf的前4个字节的数据
        for (nal_size = 0, i = 0; i < 4; ++i)
            nal_size = (nal_size << 8) | buf[i];
        buf += 4; // 跳过4个字节，指向视频帧的真实数据
        unit_type = *buf & 0x1f; // 视频帧的第一个字节里有 NAL TYPE，是否关键帧
        
        // 每个视频里至少含有一帧或者多帧
        // 视频帧数据大小大于AVPacket中读取的数据，该数据包有问题
        if (nal_size > buf_end - buf || nal_size < 0) {
            goto fail;
        }
        
       // 构建写入的数据
        if (unit_type == 5) {
            // NAL TYPE 值是5，说明是关键帧，帧数据前有 SPS/PPS 信息
            // SPS/PPS 的信息存放在 codec->extradata 中
            // 将他们读取出来
            video_extradata_read_pps_sps(
                    fmt_ctx->streams[in->stream_index]->codecpar->extradata,
                    fmt_ctx->streams[in->stream_index]->codecpar->extradata_size,
                    &sps_pps_pkt,
                    AV_INPUT_BUFFER_PADDING_SIZE);
            // 加上帧前特征码和sps/pps，构建输出上下文
            if ((ret = alloc_and_copy(out, sps_pps_pkt.data, sps_pps_pkt.size, buf, nal_size) < 0))
                goto fail;
        } else {
            // 加上帧前特征码，构建输出上下文
            if ((ret = alloc_and_copy(out, NULL, 0, buf, nal_size)) < 0)
                goto fail;
        }
        
        // 写入dst
        len = fwrite(out->data, 1, out->size, dst_fd);
        if (len != out->size) {
            av_log(NULL, AV_LOG_ERROR, "length of writed data didn't equal pkt.size");
        }
        // 刷新缓冲区
        fflush(dst_fd);

        // 下一帧
        buf += nal_size;
        cumul_size += nal_size + 4;
    } while (cumul_size < buf_size);

fail:
    av_packet_free(&out);
    
    return ret;
}

int extract_video(const char *src, const char *dst) {
    // 声明变量
    int err_code;
    char errors[1024];
    FILE *dst_fd = NULL;
    int video_stream_index = -1;
    AVFormatContext *fmt_ctx = NULL;
    AVPacket pkt;
    
    // 判断参数是否正确
    if (src == NULL || dst == NULL) {
        av_log(NULL, AV_LOG_ERROR, "src or dst is null\n");
        return -1;
    }
    
    // 打开目标文件
    dst_fd = fopen(dst, "wb");
    if (!dst_fd) {
        av_log(NULL, AV_LOG_ERROR, "can not open destination file %s\n", dst);
        return -1;
    }
    
    // 打开输入媒体文件，分配输入上下文
    if ((err_code = avformat_open_input(&fmt_ctx, src, NULL, NULL)) < 0) {
        av_strerror(err_code, errors, 1024);
        av_log(NULL, AV_LOG_ERROR, "can not open src file: %s, %d(%s)\n", src, err_code, errors);
        return -1;
    }
    
    // 输出媒体文件信息
    av_dump_format(fmt_ctx, 0, src, 0);
    
    //初始化 AVPacket
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    // 找到视频流的索引
    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_index < 0) {
        return -1;
    }
    
    // 读取流里面的包
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == video_stream_index) {
            // 抽取视频包，并写入目标文件
            // 每个包里含有一帧或多帧
            h264_video_write(fmt_ctx, &pkt, dst_fd);
        }
        // 释放
        av_packet_unref(&pkt);
    }
    
    // 关闭源文件
    avformat_close_input(&fmt_ctx);
    if (dst_fd) {
        fclose(dst_fd);
    }
    return 0;
}


