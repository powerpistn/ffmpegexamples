#include <stdio.h>
#include <iostream>
extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <unistd.h>

}
bool decode_test(const char *inputPath,const char* outputPath)
{

    FILE* fp_YUV = fopen("/testffmpeg/big_buck_bunny.yuv", "wb+");
    char buf[256];

    if (!fp_YUV){
        std::cout<<"open out.yuv failed"<<std::endl;
        return false;
    }

    AVFormatContext *avFormatContext = NULL;    //获取上下文
    //2.打开视频地址并获取里面的内容(解封装)
    if (avformat_open_input(&avFormatContext, "/testffmpeg/big_buck_bunny.mp4", NULL, NULL) < 0) {
        std::cout<<"打开视频失败"<<std::endl;
        return false;
    }
    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        std::cout<<"获取内容失败"<<std::endl;
        return false;
    }
    av_dump_format(avFormatContext, 0, 0, 0);

    //3.获取视频的编码信息
    AVCodecParameters *origin_par = NULL;
    int mVideoStreamIdx = -1;
    mVideoStreamIdx = av_find_best_stream(avFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (mVideoStreamIdx < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
        return false;
    }
    std::cout<<"成功找到视频流"<<std::endl;
    origin_par = avFormatContext->streams[mVideoStreamIdx]->codecpar;

    std::cout<<"type===="<<origin_par->codec_id<<std::endl;

    // 寻找解码器 {start
    const AVCodec *mVcodec = NULL;
    AVCodecContext *mAvContext = NULL;
    mVcodec = avcodec_find_decoder(origin_par->codec_id);
    mAvContext = avcodec_alloc_context3(mVcodec);
    if (!mVcodec || !mAvContext) {
        return false;
    }


    //4.不初始化解码器context会导致MP4封装的mpeg4码流解码失败
    int ret = avcodec_parameters_to_context(mAvContext, origin_par);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error initializing the decoder context.\n");
    }

    // 打开解码器
    if (avcodec_open2(mAvContext, mVcodec, NULL) != 0){
        std::cout<<"打开解码器失败"<<std::endl;
        return false;
    }
    std::cout<<"打开解码器成功"<<std::endl;
    // 寻找解码器 end}


    //5.申请AVPacket
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    av_init_packet(packet);
    //申请AVFrame
    AVFrame *frame = av_frame_alloc();//分配一个AVFrame结构体,AVFrame结构体一般用于存储原始数据，指向解码后的原始帧

    //6.申请存放解码后YUV格式数据的相关buf
    uint8_t *byte_buffer = NULL;

    int byte_buffer_size = av_image_get_buffer_size(mAvContext->pix_fmt, mAvContext->width, mAvContext->height, 32);
    std::cout<<"width = "<<mAvContext->width<<" height="<<mAvContext->height<<std::endl;
    byte_buffer = (uint8_t*)av_malloc(byte_buffer_size);
    if (!byte_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate buffer\n");
        return AVERROR(ENOMEM);
    }

    //7.开始解码
    while(1)
    {
        int ret = av_read_frame(avFormatContext, packet);
        if (ret != 0){
            av_strerror(ret,buf,sizeof(buf));
            std::cout<<"buf="<<buf<<std::endl;
            av_packet_unref(packet);
            break;
        }

        if (ret >= 0 && packet->stream_index != mVideoStreamIdx) {
            av_packet_unref(packet);
            continue;
        }

        {
            // 发送待解码包
            int result = avcodec_send_packet(mAvContext, packet);
            av_packet_unref(packet);
            if (result < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error submitting a packet for decoding\n");
                continue;
            }

            // 接收解码数据
            while (result >= 0){
                result = avcodec_receive_frame(mAvContext, frame);
                if (result == AVERROR_EOF)
                    break;
                else if (result == AVERROR(EAGAIN)) {
                    result = 0;
                    break;
                } else if (result < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                    av_frame_unref(frame);
                    break;
                }

                int number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,
                                                                      (const uint8_t* const *)frame->data, (const int*) frame->linesize,
                                                                      mAvContext->pix_fmt, mAvContext->width, mAvContext->height, 1);
                if (number_of_written_bytes < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Can't copy image to buffer\n");
                    av_frame_unref(frame);
                    continue;
                }

                // 写文件保存视频数据
                fwrite(byte_buffer, number_of_written_bytes, 1, fp_YUV);
                fflush(fp_YUV);

                av_frame_unref(frame);
            }
        }

    }
    //8.释放资源
    fclose(fp_YUV);
    av_frame_free(&frame);
    avcodec_close(mAvContext);
    avformat_free_context(avFormatContext);
    return true;
}
int main ()
{

    const char *input="/testffmpeg/big_buck_bunny.mp4";
    const char *output="/testffmpeg/big_buck_bunny.yuv";
    decode_test(input,output);
    return 0;
}

