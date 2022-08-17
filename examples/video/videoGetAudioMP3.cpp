//视频转码

#include <stdio.h>
#include <iostream>

extern"C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"

}
#include <iostream>
using namespace std;
//声明相关变量，基本上都是成对出现：输入与输出
    AVFormatContext* srcFmtCtx = NULL, * dstFmtCtx = NULL;
    AVCodecContext* srcAudioCodecCtx = NULL;
    AVCodecContext* dstAudioCodecCtx = NULL;
    AVStream* srcAudioStream = NULL;
    AVStream* dstAudioStream = NULL;
    AVFrame*  srcAudioFrame = NULL;
    AVFrame*  dstAudioFrame = NULL;
    SwrContext* audioSwrCtx = NULL;
    AVPacket* pkt =av_packet_alloc();


    //pts
    int audioPts = 0;
    //执行具体的转码操作
    void doTranscode();
    //释放资源
    void releaseRes();
    //添加流，并设置参数
    int addAudioStream();
    //编解码音频流
    int decodeAudio(AVPacket* pkt);
    int encodeAudio(AVFrame* frame);


//这里与组合视频流与音频流的代码很相似
void doTranscode() {
    int ret;
    do {
        string srcFile= "/testffmpeg/big_buck_bunny.mp4";
        string dstFile= "/testffmpeg/big_buck_bunny.mp3";
        std::cout<<"开始"<<std::endl;
        //打开本地文件
        ret = avformat_open_input(&srcFmtCtx, srcFile.c_str(), NULL, NULL);
        if (ret < 0) {
            cout << "Could not open input" << endl;
            break;
        }
        std::cout<<"1:打开视频文件成功"<<std::endl;
        //获取多媒体文件信息
        ret = avformat_find_stream_info(srcFmtCtx, NULL);
        if (ret < 0) {
            cout << "Could not find stream info" << endl;
            break;
        }
        std::cout<<"2:获取多媒体文件信息成功"<<std::endl;
        av_dump_format(srcFmtCtx, 0, srcFile.c_str(), 0);

        //创建输出结构上下文 AVFormatContext
        ret = avformat_alloc_output_context2(&dstFmtCtx, NULL, NULL, dstFile.c_str());
        if (ret < 0) {
            cout << "Could not create output context" << endl;
            break;
        }
        std::cout<<"3:创建输出结果上下文成功"<<std::endl;
        //打开文件
        ret = avio_open(&dstFmtCtx->pb, dstFile.c_str(), AVIO_FLAG_READ_WRITE);
        if (ret < 0) {
            cout << "Could not open output file" << endl;
            break;
        }
        std::cout<<"4:打开输出文件"<<std::endl;

        //添加一个音频流
        ret = addAudioStream();
        if (ret < 0) {
            cout << "Add audio stream fail" << endl;
            break;
        }
        std::cout<<"5.添加音频流成功"<<std::endl;

        //写入文件头信息
        ret=avformat_write_header(dstFmtCtx,NULL);
        if (ret != AVSTREAM_INIT_IN_WRITE_HEADER) {
            cout << "Write file header fail" << endl;
            break;
        }
        av_dump_format(dstFmtCtx,0,dstFile.c_str(),1);
        std::cout<<"6.输出文件头文件信息写入成功，输出文件信息查看成功"<<std::endl;


        //申请音频流使用的Frame
        srcAudioFrame = av_frame_alloc();
        dstAudioFrame = av_frame_alloc();

        //用于判断是否需要进行转换的参数
        //为av_frame_get_buffer设置必要的参数，nb_samples、channel_layout、format
        dstAudioFrame->nb_samples = dstAudioCodecCtx->frame_size;
        dstAudioFrame->channel_layout = dstAudioCodecCtx->channel_layout;
        dstAudioFrame->format = dstAudioCodecCtx->sample_fmt;

        dstAudioFrame->channels = dstAudioCodecCtx->channels;
        dstAudioFrame->sample_rate = dstAudioCodecCtx->sample_rate;

        //获取Frame的数据缓冲区
        ret = av_frame_get_buffer(dstAudioFrame, 0);
        if (ret < 0) {
            cout << "audio av_frame_get_buffer fail" << endl;
            break;
        }
        std::cout<<"7.获取音频帧Frame数据缓存区成功，开始判断音频流参数是否需要转换"<<std::endl;
        //判断是否需要进行转换
        if (dstAudioCodecCtx->sample_fmt != srcAudioCodecCtx->sample_fmt ||
            dstAudioCodecCtx->channel_layout != srcAudioCodecCtx->channel_layout||
            dstAudioCodecCtx->sample_rate != srcAudioCodecCtx->sample_rate||
            dstAudioCodecCtx->frame_size != srcAudioCodecCtx->frame_size||
            dstAudioCodecCtx->channels != srcAudioCodecCtx->channels) {
            audioSwrCtx = swr_alloc_set_opts(NULL,
                                             dstAudioCodecCtx->channel_layout,dstAudioCodecCtx->sample_fmt,dstAudioCodecCtx->sample_rate,
                                             srcAudioCodecCtx->channel_layout,srcAudioCodecCtx->sample_fmt,srcAudioCodecCtx->sample_rate,
                                             0,NULL);
            if (audioSwrCtx == NULL) {
                cout << "Get SwrContext fail" << endl;
                break;
            }
        }




        std::cout<<"8.源文件读取数据开始转码"<<std::endl;
        //从源文件中读取数据
        while (av_read_frame(srcFmtCtx,pkt)>=0) {
            if (pkt->stream_index == srcAudioStream->index) {
                //解码视频流
                decodeAudio(pkt);
            }
        }
        std::cout<<"9刷新音视频缓冲"<<std::endl;
        avcodec_send_packet(srcAudioCodecCtx, NULL);

        std::cout<<"10.刷新音视频缓冲"<<std::endl;
        if (audioSwrCtx) {
            while (swr_convert_frame(audioSwrCtx, dstAudioFrame, NULL) >= 0) {
                if (dstAudioFrame->nb_samples == 0) break;

                dstAudioFrame->pts = audioPts;
                audioPts += dstAudioFrame->nb_samples;
                encodeAudio(dstAudioFrame);
            }
        }
        encodeAudio(NULL);

        std::cout<<"11.向输出文件写入尾部标识"<<std::endl;
        //向文件中写入文件尾部标识，并释放该文件
        av_write_trailer(dstFmtCtx);
    } while (0);

    std::cout<<"12.释放资源"<<std::endl;
    //释放资源
    releaseRes();
}



//释放相关的资源
void releaseRes() {
    if (srcFmtCtx) avformat_close_input(&srcFmtCtx);
    if (dstFmtCtx) {
        avformat_free_context(dstFmtCtx);
        avio_close(dstFmtCtx->pb);
        dstFmtCtx = NULL;
    }

    if (srcAudioCodecCtx) avcodec_free_context(&srcAudioCodecCtx);
    if (dstAudioCodecCtx) avcodec_free_context(&dstAudioCodecCtx);
    if (srcAudioFrame) av_frame_free(&srcAudioFrame);
    if (dstAudioFrame) av_frame_free(&dstAudioFrame);
    if (pkt)av_packet_free(&pkt);
    if (audioSwrCtx) swr_free(&audioSwrCtx);
}


//返回0表示成功，返回一个负数表示失败
int addAudioStream(){
    int ret;
    const AVCodec* srcCodec = NULL, * dstCodec = NULL;
    do {
        //查找音频流，并根据音频流格式给srcCodec解码器赋值
        ret = av_find_best_stream(srcFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &srcCodec, 0);
        if (ret < 0) {
            cout << "Could not find audio stream" << endl;
            break;
        }

        //ret返回的就是找到的视频流的索引
        srcAudioStream = srcFmtCtx->streams[ret];
        AVCodecParameters* srcParam = srcAudioStream->codecpar;

        //申请编码器上下文结构体
        srcAudioCodecCtx = avcodec_alloc_context3(srcCodec);
        if (srcAudioCodecCtx == NULL) {
            ret = -1;
            cout << "Could not alloc audio context" << endl;
            break;
        }

        //将参数传给解码器上下文
        ret = avcodec_parameters_to_context(srcAudioCodecCtx, srcParam);
        if (ret < 0) {
            cout << "Fail copy audio parameters to context" << endl;
            break;
        }

        //打开解码器
        ret = avcodec_open2(srcAudioCodecCtx, srcCodec, NULL);
        if (ret < 0) {
            printf("cannot open audio decoder\n");
            break;
        }

        //--------------------------------------------------

        //查找编码器
        dstCodec = avcodec_find_encoder(dstFmtCtx->oformat->audio_codec);
        if (dstCodec == NULL) {
            ret = -1;
            cout << "Cannot find any audio endcoder" << endl;
            break;
        }

        //申请编码器上下文结构体
        dstAudioCodecCtx = avcodec_alloc_context3(dstCodec);
        if (dstAudioCodecCtx == NULL) {
            ret = -1;
            cout << "Cannot alloc audio dst context" << endl;
            break;
        }

        //创建音频流
        dstAudioStream = avformat_new_stream(dstFmtCtx, dstCodec);
        if (dstAudioStream == NULL) {
            ret = -1;
            cout << "Could not new audio stream" << endl;
            break;
        }

        AVCodecParameters* dstParam = dstAudioStream->codecpar;
        //拷贝输入文件中音频流的参数
        ret = avcodec_parameters_copy(dstParam, srcParam);
        if (ret < 0) {
            cout << "Fail copy audio parameters" << endl;
            break;
        }

        //可以在这里更改需要修改的参数
        dstParam->codec_id = dstCodec->id;
        dstParam->codec_tag = 0;
        dstParam->sample_rate=44100;//修改音频采样率

        //将参数传给解码器上下文
        ret = avcodec_parameters_to_context(dstAudioCodecCtx, dstParam);
        if (ret < 0) {
            cout << "Fail copy video parameters to dst context" << endl;
            break;
        }

        //打开解码器
        ret = avcodec_open2(dstAudioCodecCtx, dstCodec, NULL);
        if (ret < 0) {
            printf("cannot open audio encoder\n");
            break;
        }

        //再将dstVideoCodecCtx设置的参数传给dstParam，用于写入头文件信息
        ret = avcodec_parameters_from_context(dstParam, dstAudioCodecCtx);
        if (ret < 0) {
            cout << "Fail copy video parameters from dst context" << endl;
            break;
        }
    } while (0);

    return ret;
}


/*
* 解码输入文件
* @param pkt:包含数据的Packet，传入NULL表示刷新缓存
* @return 返回0表示成功，返回一个负数表示失败
*/
int decodeAudio(AVPacket* pkt) {
    int ret = 0;
    //发送包数据去进行解析获得帧数据
    if (avcodec_send_packet(srcAudioCodecCtx,pkt)>=0) {
        //获取解码器解析后产生的Frame
        while (avcodec_receive_frame(srcAudioCodecCtx,srcAudioFrame)>=0) {
            //是否需要进行转换操作
            if (audioSwrCtx) {
                ret = swr_convert_frame(audioSwrCtx, dstAudioFrame, srcAudioFrame);
                if (ret<0) {
                    cout << "Cannot convert frame" << endl;
                    break;
                }
            }else {
                //不需要转换直接拷贝Frame中的数据即可
                ret = av_frame_copy(dstAudioFrame,srcAudioFrame);
                if (ret<0) {
                    cout << "Not copy audio frame" << endl;
                    break;
                }
            }
            dstAudioFrame->pts = audioPts;
            audioPts += dstAudioFrame->nb_samples;

            //执行编码操作
            ret=encodeAudio(dstAudioFrame);
            if (ret < 0) {
                cout << "Do encodeAudio fail" << endl;
                break;
            }
        }
        av_packet_unref(pkt);
    }

    return ret;
}


/*
* 编码输出文件
* @param frame:包含数据的Frame，传入NULL表示刷新缓存
* @return 返回0表示成功，返回一个负数表示失败
*/
int encodeAudio(AVFrame* frame){
    int ret = 0;
    //将frame发送至编码器进行编码，codecCtx中保存了codec
    //当frame为NULL时，表示将缓冲区中的数据读取出来
    if (avcodec_send_frame(dstAudioCodecCtx,frame)>=0) {
        //接收编码后形成的packet
        //查看源码后，会发现该方法会先调用 av_packet_unref，在执行接收操作
        while (avcodec_receive_packet(dstAudioCodecCtx,pkt)>=0) {
            //设置对应的流索引
            pkt->stream_index = dstAudioStream->index;
            pkt->pos = -1;

            //转换pts至基于时间基的pts
            av_packet_rescale_ts(pkt,dstAudioCodecCtx->time_base,dstAudioStream->time_base);

            cout << "encoder audio success pts:" << pkt->pts << endl;

            //将包数据写入文件中，该方法不用使用 av_packet_unref
            ret = av_interleaved_write_frame(dstFmtCtx, pkt);
            if (ret < 0) {
                char errStr[256];
                av_strerror(ret, errStr, 256);
                cout << "error is:" << errStr << endl;
                break;
            }
        }
    }

    return ret;
}

int main()
{
    std::cout<<"指针成功"<<std::endl;
    doTranscode();
    return 0;
}
