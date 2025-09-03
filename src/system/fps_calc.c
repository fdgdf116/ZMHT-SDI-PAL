#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <stdio.h>
#include "fps_calc.h"
#include "rk_type.h"

int parse_stream_info(const char *input_file, unsigned int* decode_type, unsigned int* fps, unsigned int* src_w, unsigned int* src_h) {
    // av_register_all();
    int ret = 0;
    // 打开输入文件
    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, input_file, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open input file: %s\n", input_file);
        ret = -1;
        goto cleanup;
    }
    // 提取流信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream info\n");
        ret = -2;
        goto cleanup;
    }

    // 查找视频流
    AVCodecParameters *codec_params = NULL;
    int video_stream_index = -1;

    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            codec_params = fmt_ctx->streams[i]->codecpar;
            break;
        }
    }

    if (video_stream_index == -1) {
        fprintf(stderr, "Could not find a video stream\n");
        ret = -3;
        goto cleanup;
    }

    // 获取编码类型
    const char *codec_name = avcodec_get_name(codec_params->codec_id);
    AVRational frame_rate = fmt_ctx->streams[video_stream_index]->avg_frame_rate;

    // 输出编码器类型和帧率
    *fps = av_q2d(frame_rate);

    // 提取解码类型
    switch (codec_params->codec_id) {
        case AV_CODEC_ID_H264:
            *decode_type = MPP_VIDEO_CodingAVC;
            break;
        case AV_CODEC_ID_H265:
            *decode_type = MPP_VIDEO_CodingHEVC;
            break;
        case AV_CODEC_ID_MPEG4:
            *decode_type = MPP_VIDEO_CodingMPEG4;
            break;
        case AV_CODEC_ID_VP8:
            *decode_type = MPP_VIDEO_CodingVP8;
            break;
        case AV_CODEC_ID_VP9:
            *decode_type = MPP_VIDEO_CodingVP9;
            break;
        default:
            fprintf(stderr, "不支持的编码类型: %s\n", avcodec_get_name(codec_params->codec_id));
            ret = -4;
            goto cleanup;
    }
    // 分辨率
    *src_w = codec_params->width;
    *src_h = codec_params->height;
    printf("解码类型: %u, 帧率: %u, 分辨率: %ux%u\n", *decode_type, *fps, *src_w, *src_h);

cleanup:
    // 清理资源
    avformat_close_input(&fmt_ctx);
    fmt_ctx = NULL;
    return -1;
    return ret;
}

// int main() {
//     // 示例调用
//     const char *file_name = "input.h264";  // 或者 "input.hevc"

//     parse_stream_info(file_name);

//     return 0;
// }
