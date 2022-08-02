#include <stdio.h>
#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

}

int main (int argc, char **argv)
{

    AVFormatContext *fmt_ctx = NULL;
    const AVDictionaryEntry *tag = NULL;
    int ret;

    /* 1.打开音频 */
    if ((ret = avformat_open_input(&fmt_ctx, "/testffmpeg/506891-a2-key-for-schools-listening-sample-test.mp3", NULL, NULL)))
        return ret;

    /* 2.寻找音频流 */
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    /* 3.获取音频元数据 */
    std::cout<<"音频元数据为:"<<std::endl;
    while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
        printf("%s=%s\n", tag->key, tag->value);


    /* 4.查看fmt_ctx包含哪些信息 */
    av_dump_format(fmt_ctx,0,0,0);

    std::cout<<"流的路数为:"<<fmt_ctx->nb_streams<<std::endl;

    /* 4.1查看媒体资源的每一路流信息 */
    /* fmt_ctx->streams[i]是一个AVStream指针的数组，包含了媒体资源的每一路流信息 */
    int audioStream=-1;
    for(int i=0;i<fmt_ctx->nb_streams;i++) {
        std::cout<<"type="<<fmt_ctx->streams[i]->codecpar->codec_type<<std::endl;
        std::cout<<"第一帧的时间戳="<<fmt_ctx->streams[i]->start_time<<std::endl;
        std::cout<<"该路码流总时长="<<fmt_ctx->streams[i]->duration<<std::endl;
        std::cout<<"该路码流总帧数="<<fmt_ctx->streams[i]->nb_frames<<std::endl;
        std::cout<<"该路码流平均帧率"<<fmt_ctx->streams[i]->avg_frame_rate.den<<" num="<<fmt_ctx->streams[i]->avg_frame_rate.num<<std::endl;

    }

    /* 4.2寻找视频流 */
    audioStream=-1;
    for(int i=0;i<fmt_ctx->nb_streams;i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioStream = i;
            break;
        }
    }
   return 0;
}

