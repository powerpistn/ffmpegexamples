#include <stdio.h>
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

using namespace std;


#define CHANNEL 2
#define OSR 44100
int pcm_to_mp3(const char *pcm_file_path, const char *mp3_file_path)
{
    FILE *pcm_file = NULL;
    FILE *mp3_file = NULL;
    int result;

    // 获取mp3编码器
    std::cout << "获取mp3编码器" << std::endl;
    AVFormatContext* fmtctx=NULL;
    result=avformat_alloc_output_context2(&fmtctx, NULL, NULL, mp3_file_path);
    if (result < 0) {
        cout << "Cannot alloc output file context" << endl;
        return -1;
    }
    if(fmtctx==NULL)
        std::cout<<"fmtctx==NULL"<<std::endl;


    //const AVCodec *avCodec = avcodec_find_encoder(fmtctx->oformat->audio_codec);
    //const AVCodec *avCodec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    const AVCodec* avCodec = avcodec_find_encoder_by_name("libmp3lame");

    if (avCodec==NULL)
    {
        std::cout << "1.初始化mp3 编码器失败" << std::endl;
        return -1;
    }
    else
        std::cout << "1.初始化mp3 编码器成功" << std::endl;

    // 创建编码器上下文
    AVCodecContext *avCodecContext = avcodec_alloc_context3(avCodec);
    if (!avCodecContext)
    {
        std::cout << "2.avcodec_alloc_context3 失败" << avCodecContext << std::endl;
        return -1;
    }
    else
        std::cout << "2.avcodec_alloc_context3 成功" << avCodecContext << std::endl;
    avCodecContext->bit_rate = 64000;
    avCodecContext->channels = CHANNEL;



    avCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
    avCodecContext->sample_rate = OSR;
    avCodecContext->sample_fmt = AV_SAMPLE_FMT_S16P;
    avCodecContext->time_base = av_get_time_base_q();

    // 打开编码器
    std::cout << "打开mp3编码器" << std::endl;
    result = avcodec_open2(avCodecContext, avCodec, NULL);
    if (result < 0)
    {
        std::cout << "avcodec_open2失败: " << result << std::endl;
        return result;
    }

    std::cout << "打开mp3文件" << mp3_file_path << std::endl;
    // 打开输出文件
    mp3_file = fopen(mp3_file_path, "wb");
    if (!mp3_file)
    {
        std::cout << "打开mp3文件失败" << std::endl;
        return -1;
    }

    // AVFrame 接受重采样的每一帧的音频数据 每帧的样本大小为1152
    AVFrame *avFrame = av_frame_alloc();
    if (!avFrame)
    {
        std::cout << "分配avFrame帧失败" << std::endl;
        return -1;
    }
    // mp3一帧的样本数为1152
    avFrame->nb_samples = 1152;
    avFrame->channels = CHANNEL;
    avFrame->channel_layout = AV_CH_LAYOUT_STEREO;
    avFrame->format = AV_SAMPLE_FMT_S16P;

    // 给帧分配内存空间
    result = av_frame_get_buffer(avFrame, 0);
    if (result < 0)
    {
        std::cout << "分配帧内存失败" << std::endl;
        return result;
    }

    // 重采样  创建音频重采样上下文
    std::cout << "配置重采样器上下文" << std::endl;
    SwrContext *swrContext = swr_alloc();
    if (!swrContext)
    {
        std::cout << "配置重采样上下文失败" << std::endl;
        return -1;
    }

    // 设置重采样输入pcm参数:通道布局:立体声 采样率:44100  样本格式 s16交错存储
    av_opt_set_int(swrContext, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(swrContext, "in_sample_rate", OSR, 0);
    av_opt_set_sample_fmt(swrContext, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    // 设置重采样输出mp3参数:通道布局:立体声 采样率:44100  样本格式 s16平面存储
    av_opt_set_int(swrContext, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(swrContext, "out_sample_rate", OSR, 0);
    av_opt_set_sample_fmt(swrContext, "out_sample_fmt", AV_SAMPLE_FMT_S16P, 0);

    // 重采样初始化
    result = swr_init(swrContext);
    if (result < 0)
    {
        std::cout << "重采样器初始化失败,error=" << result << std::endl;
        return result;
    }

    uint8_t **input_data = NULL;
    uint8_t **output_data = NULL;
    int input_linesize, output_linesize;

    // 打开pcm文件
    std::cout << "打开源 pcm 文件 " << pcm_file_path << std::endl;
    pcm_file = fopen(pcm_file_path, "rb");
    if (!pcm_file)
    {
        std::cout << "打开 pcm 文件失败" << std::endl;
        return -1;
    }

    std::cout << "开始编码转换" << std::endl;

    // 给pcm文件数据分配空间
    result = av_samples_alloc_array_and_samples(&input_data, &input_linesize, 2, avFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
    if (result < 0)
    {
        std::cout << "给pcm文件分配空间失败, result = " << result << std::endl;
        return result;
    }

    // 缓存重采样数据的空间分配
    result = av_samples_alloc_array_and_samples(&output_data, &output_linesize, 2, avFrame->nb_samples, AV_SAMPLE_FMT_S16P, 0);
    if (result < 0)
    {
        std::cout << "获取mp3 重采样数据失败, result=" << result << std::endl;
        return result;
    }

    // 存放编码后的数据
    AVPacket *avPacket = av_packet_alloc();
    if (!avPacket)
    {
        std::cout << "分配 avPacket 内存失败" << std::endl;
        return -1;
    }

    std::cout << "==========循环读入帧==========" << std::endl;
    long total_size = 0;
    while (!feof(pcm_file))
    {
        long read_size = (long)fread(input_data[0], 1, avFrame->nb_samples * 4, pcm_file);
        total_size += read_size;
        if ((total_size / read_size) % 50 == 0)
        {
            std::cout << "读取数据:" << read_size << "字节; 累计:" << total_size << " 字节 " << std::endl;
        }

        if (read_size <= 0)
        {
            break;
        }

        // 重采样
        result = swr_convert(swrContext, output_data, avFrame->nb_samples, (const uint8_t **)input_data, avFrame->nb_samples);
        if (result < 0)
        {
            std::cout << "音频编码失败,错误信息" << result << std::endl;
            return result;
        }
        // 将重采样后的数据存入frame，MP3是s16p 先存放左声道的数据 后存放右声道的数据， data[0]是左声道，1是右声道
        avFrame->data[0] = output_data[0];
        avFrame->data[1] = output_data[1];

        // 编码，写入mp3文件，实际上是对frame这个结构体里面的数据进行编码操作,发送到编码线程:使用编码器  和 存储数据的frame
        result = avcodec_send_frame(avCodecContext, avFrame);
        if (result < 0)
        {
            std::cout << "mp3编码失败,错误信息:" << result << std::endl;
            return result;
        }

        while (result >= 0)
        {
            // 接收编码后的数据，使用编码器 和 存储编码数据的pkt, 有可能需要多次才能接收完成
            result = avcodec_receive_packet(avCodecContext, avPacket);

            // AVERROR_EOF表示没有数据了 这两个错误不影响继续接收数据
            if (result == AVERROR_EOF || result == AVERROR(EAGAIN))
            {
                continue;
            }
            else if (result < 0)
            {
                break;
            }
            fwrite(avPacket->data, 1, avPacket->size, mp3_file);
            av_packet_unref(avPacket);
        }
    }
    // 告诉解码器没有帧了,如果没有这几行的逻辑，在关闭 avCodecContext 可能会提示 * fames left in the queu on closing
    avcodec_send_frame(avCodecContext, __null);
    while(avcodec_receive_packet(avCodecContext, avPacket)!=AVERROR_EOF);

    // 关闭缓存
    if (input_data)
    {
        av_free(input_data);
    }
    if (output_data)
    {
        av_free(output_data);
    }
    std::cout << "关闭文件" << std::endl;
    fclose(pcm_file);
    fclose(mp3_file);
    std::cout << "释放资源" << std::endl;
    // s释放 frame pkt
    av_frame_free(&avFrame);
    av_packet_free(&avPacket);
    // 释放重采样上下文
    swr_free(&swrContext);
    // 释放编码器上下文
    avcodec_free_context(&avCodecContext);

    std::cout << "转码完成" << std::endl;

    return 0;
}

int main (int argc, char **argv)
{
    pcm_to_mp3("/testffmpeg/big_buck_bunny2.pcm","/testffmpeg/big_buck_bunny2.mp3");
    return 0;
}
