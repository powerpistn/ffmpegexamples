#include <iostream>
#include <map>
extern "C"
{
#include <libavcodec/avcodec.h>//处理原始音频和视频流的解码
#include <libavformat/avformat.h>//处理解析视频文件并将包含在其中的流分离出来
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libswscale/swscale.h>
}

bool change_pix_format(AVFrame* src_avframe, AVFrame*& dst_avframe, AVPixelFormat format)
{
    if (src_avframe->format == format)
    {
        AVFrame* temp = av_frame_clone(src_avframe);
        dst_avframe = temp;
        return true;
    }
    AVFrame* dec_avframe = av_frame_alloc();
    dec_avframe->format = format;
    dec_avframe->width = src_avframe->width;
    dec_avframe->height = src_avframe->height;

    int ret = 0;
    if ((ret = av_frame_get_buffer(dec_avframe, 0)) < 0) {
        return false;
    }

    struct SwsContext* swsContext = sws_getContext(src_avframe->width, src_avframe->height, (AVPixelFormat)src_avframe->format, dec_avframe->width, dec_avframe->height, format, SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale(swsContext, src_avframe->data, src_avframe->linesize, 0, src_avframe->height, dec_avframe->data, dec_avframe->linesize);

    av_frame_copy_props(dec_avframe, src_avframe);
    dst_avframe = dec_avframe;
    sws_freeContext(swsContext);
    return true;
}

void ProcessFrame(std::string output,AVFrame * frame)
{
    AVFormatContext* output_ctx = nullptr;

    AVCodecContext * codec_ctx = nullptr;
    try
    {
        if (int ret = avformat_alloc_output_context2(&output_ctx, nullptr, nullptr, output.c_str()) < 0)
        {
            std::cout << "avformat_alloc_output_context2 error! url: " << output << " ret: " << ret << std::endl;
            throw ret;
        }
        //获取封装格式
        const AVOutputFormat* fmt = av_guess_format(nullptr, output.c_str(), nullptr);
        if (fmt)
            output_ctx->oformat = fmt;

        //根据输入的解码器类型获取输出封装格式对应的编码器id
        AVCodecID codecId = av_guess_codec(output_ctx->oformat, nullptr, output.c_str(), nullptr, AVMEDIA_TYPE_VIDEO);

        //根据编码器id获取编码器
        const AVCodec* codec = avcodec_find_encoder(codecId);
        if (codec == nullptr)
        {
            std::cout << "find encodec is null! " << std::endl;
            throw -1;
        }
        //初始化编码器上下文参数
        AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
        if (codec_ctx == nullptr)
        {
            std::cout << "codec_ctx is null!" << std::endl;
            throw - 2;
        }
        AVFrame* frame_temp = nullptr;
        if (frame->format!= codec->pix_fmts[0])
        {

            if(change_pix_format(frame,frame_temp, codec->pix_fmts[0]))
            {
                av_frame_free(&frame);
                frame = frame_temp;
            }
        }

        //配置编码器
        codec_ctx->codec_id = codecId;
        codec_ctx->pix_fmt = codec->pix_fmts[0];
        codec_ctx->width = frame->width;
        codec_ctx->height = frame->height;
        codec_ctx->time_base = { 1,25 };


        //判断输出的封装格式是否有编解码器参数信息，没有则添加上
        if (output_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        {
            codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        //打开编码器
        int ret = avcodec_open2(codec_ctx, codec, nullptr);
        if (ret < 0)
        {
            std::cout << "open encodec is error!" << std::endl;
            throw ret;
        }

        //初始化输出流
        AVStream* output_stream = avformat_new_stream(output_ctx, nullptr);
        if (int ret = avcodec_parameters_from_context(output_stream->codecpar, codec_ctx) < 0)
        {
            std::cout << "avcodec_parameters_from_context error! ret: " << ret << std::endl;
            throw - 2;
        }

        //打开输出流
        if (!(output_ctx->flags & AVFMT_NOFILE))//是文件才打开输出流
        {
            if (int ret = avio_open2(&output_ctx->pb, output.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr) < 0)
            {
                std::cout << "avio_open2 error! ret: " << ret << std::endl;
                throw - 2;
            }
        }

        // 写输出文件头
        if (int ret = avformat_write_header(output_ctx, nullptr) < 0)
        {
            std::cout << "avformat_write_header error! ret: " << ret << std::endl;
            throw - 2;
        }

        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0)
        {
            std::cout << "send frame error! ret:" << ret << std::endl;
            throw ret;
        }

        ret = avcodec_send_frame(codec_ctx, nullptr);
        if (ret < 0)
        {
            std::cout << "send frame error! ret:" << ret << std::endl;
            throw ret;
        }

        while (1)
        {
            AVPacket new_packet;
            av_init_packet(&new_packet);
            new_packet.data = nullptr;
            new_packet.size = 0;
            //从编码器中获取编码后的数据包
            ret = avcodec_receive_packet(codec_ctx, &new_packet);
            if (ret < 0)
                throw ret;

            //将数据包写入到打开的输出源中
            ret = av_interleaved_write_frame(output_ctx, &new_packet);//写文件
            if (ret < 0)
            {
                std::cout << "write packet error! ret:" << ret << std::endl;
                throw ret;
            }
            av_packet_unref(&new_packet);
        }
        av_frame_free(&frame);
    }
    catch (int e)
    {
        std::cout << "Transcoding exception: " << e << std::endl;

        //写文件尾
        av_write_trailer(output_ctx);

        //打印输出源信息
        av_dump_format(output_ctx, 0, output.c_str(), 1);
        if (output_ctx != nullptr)
        {
            if (!(output_ctx->flags & AVFMT_NOFILE))
            {
                avio_closep(&output_ctx->pb);
            }
            avformat_free_context(output_ctx);
            output_ctx = nullptr;
        }

        if (codec_ctx != nullptr)
        {
            avcodec_free_context(&codec_ctx);
            codec_ctx = nullptr;
        }
    }
}

AVFilterGraph* filter_graph = nullptr;
AVFilterContext* buffersrc_ctx = nullptr;
AVFilterContext* buffersink_ctx = nullptr;
const AVFilter* buffersrc = nullptr;
const AVFilter* buffersink = nullptr;
AVFilterInOut* outputs = nullptr;
AVFilterInOut* inputs = nullptr;
bool Init(AVStream* stream, const std::string command)
{
    std::cout<<"command="<<command<<std::endl;
    buffersrc = avfilter_get_by_name("abuffer");
    buffersink = avfilter_get_by_name("buffersink");
    filter_graph = avfilter_graph_alloc();

    char args[512];
    std::cout<<"command="<<command<<std::endl;


/*
    snprintf(args, sizeof(args), "time_base=%d:sample_rate=%d:sample_fmt=%d:channel_layout=%PRId64",
             stream->time_base, stream->codecpar->sample_rate,
             stream->codecpar->format, stream->codecpar->channel_layout);
             */


    snprintf(args, sizeof(args), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%lld",
             stream->time_base.num,stream->time_base.den, stream->codecpar->sample_rate,
             av_get_sample_fmt_name((enum AVSampleFormat)stream->codecpar->format), stream->codecpar->channel_layout);

    try
    {

        int ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
        if (ret < 0)
        {
            printf("Cannot create buffer source\n");
            throw ret;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
        if (ret < 0)
        {
            std::cout << "Cannot create buffer sink: " << ret << std::endl;
            throw ret;
        }

        outputs = avfilter_inout_alloc();
        inputs = avfilter_inout_alloc();

        outputs->name = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx;
        outputs->pad_idx = 0;
        outputs->next = NULL;

        inputs->name = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx;
        inputs->pad_idx = 0;
        inputs->next = NULL;

        if ((ret = avfilter_graph_parse_ptr(filter_graph, command.c_str(), &inputs, &outputs, NULL)) < 0)
            throw ret;

        if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
            throw ret;
    }
    catch (int e)
    {
        if (inputs != nullptr)
        {
            avfilter_inout_free(&inputs);
            inputs = nullptr;
        }

        if (outputs != nullptr)
        {
            avfilter_inout_free(&outputs);
            outputs = nullptr;
        }

        if (filter_graph != nullptr)
        {
            avfilter_graph_free(&filter_graph);
            filter_graph = nullptr;
        }

        return false;
    }

    return true;
}

void Run(AVFrame* avframe, AVFrame*& filt_frame)
{

    int ret = av_buffersrc_add_frame_flags(buffersrc_ctx, avframe, 1);
    if (ret < 0)
    {
        std::cout << "Error while feeding the filtergraph\n" << std::endl;
        return ;
    }

    ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
    if (ret < 0)
    {
        av_frame_free(&filt_frame);
        filt_frame = nullptr;
        return ;
    }

}



bool func()
{
    std::cout << "learn ffmpeg" << std::endl;
    std::string path = "/testffmpeg/153149-movers-sample-listening-test-vol2.mp3";
    std::string output_path = "/testffmpeg/tuiliu.jpg";


    AVFormatContext* input_ctx = nullptr;
    std::map<int, AVCodecContext*> input_codec_ctx_map;
    int video_streamIndex = -1;
    int audio_streamIndex = -1;
    //AudioFilter* audio_filter = nullptr;

        if (int ret = avformat_open_input(&input_ctx, path.c_str(), NULL, NULL) < 0)
        {
            std::cout << "avformat_open_input error! url:" << path << " ret:" << ret << std::endl;
            throw ret;
        }
        std::cout<<"1.打开视频文件"<<std::endl;

        //获取流信息
        if (int ret = avformat_find_stream_info(input_ctx, NULL) < 0)
        {
            std::cout << "avformat_find_stream_info failed! ret: " << ret << std::endl;
            throw ret;
        }
        std::cout<<"2.获取流信息成功，打印流信息"<<std::endl;
        //打印输入源信息
        av_dump_format(input_ctx, 0, path.c_str(), 0);

        /*****************************配置解码器参数**********************************/
        for (int i = 0; i < input_ctx->nb_streams; i++)
        {
            AVStream* stream = input_ctx->streams[i];
            //根据流信息中的解码id找到解码器
            const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
            if (codec == nullptr)
            {
                std::cout << "find decodec is null!" << std::endl;
                continue;
            }
            //使用解码器对象初始化解码器上下文参数
            AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
            if (codec_ctx == nullptr)
            {
                std::cout << "codec_ctx is null!" << std::endl;
                throw -1;
            }
            //视频流中解码器参数来初始化解码器上下文成员
            int ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
            if (ret < 0)
            {
                std::cout << "init codec_ctx parameters is error!" << std::endl;
                throw ret;
            }

            //打开解码器
            ret = avcodec_open2(codec_ctx, codec, nullptr);
            if (ret < 0)
            {
                std::cout << "open decodec error!" << std::endl;
                throw ret;
            }

            //获取音视频流的索引id
            if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                std::cout<<"3.找到视频流"<<std::endl;
                audio_streamIndex = i;
                std::string command = "[in]showwavespic=s=720x180[out]";
                Init(stream, command);
                std::cout<<"3.找到视频流"<<std::endl;
            }

            input_codec_ctx_map.insert(std::make_pair(i, codec_ctx));
        }
        /*****************************配置解码器参数**********************************/

    //初始化一个AVPacket来获取数据包
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = nullptr;
    packet.size = 0;
    //初始化一个解码后的帧
    AVFrame* frame = av_frame_alloc();

    //从源上下文中读取数据包
    std::cout<<"4.开始读取数据包"<<std::endl;
    while (av_read_frame(input_ctx, &packet) >= 0)
    {
        if (packet.stream_index != audio_streamIndex)//不是音频流
        {
            av_packet_unref(&packet);
            continue;
        }

        if (input_codec_ctx_map.end() == input_codec_ctx_map.find(packet.stream_index))
        {
            av_packet_unref(&packet);
            continue;
        }

        //将读取到的音视频包送到解码器解码
        int ret = avcodec_send_packet(input_codec_ctx_map.at(packet.stream_index), &packet);
        if (ret < 0)
        {
            std::cout << "send packet error! ret:" << ret << std::endl;
            av_packet_unref(&packet);
            continue;
        }
        std::cout<<"4.1解码器解码音频数据成功"<<std::endl;
        while (1)
        {
            //从解码器中获取解码后的音视频数据
            int ret = avcodec_receive_frame(input_codec_ctx_map.at(packet.stream_index), frame);
            if (ret < 0)
            {
                break;
            }
            std::cout<<"4.1获取音频数据成功，开始获取帧"<<std::endl;
            AVFrame* filter = av_frame_alloc();
            Run(frame, filter);
            if (filter != nullptr)
            {
                av_frame_free(&filter);
            }
        }
        std::cout<<"音频解码成功"<<std::endl;
        av_packet_unref(&packet);
    }
    AVFrame* filter = av_frame_alloc();
    Run(nullptr, filter);
    if (filter != nullptr)
    {
        std::string output_path = "/testffmpeg/tuiliu.jpg";
        ProcessFrame(output_path,filter);
    }

    std::cout<<"释放资源"<<std::endl;
    if (input_ctx != nullptr)
    {
        avformat_close_input(&input_ctx);
        input_ctx = nullptr;
    }
    for (auto& it : input_codec_ctx_map)
    {
        avcodec_free_context(&it.second);
    }
    input_codec_ctx_map.clear();

}
int main()
{
    func();
    if (inputs != nullptr)
    {
        avfilter_inout_free(&inputs);
        inputs = nullptr;
    }

    if (outputs != nullptr)
    {
        avfilter_inout_free(&outputs);
        outputs = nullptr;
    }

    if (filter_graph != nullptr)
    {
        avfilter_graph_free(&filter_graph);
        filter_graph = nullptr;
    }
    return 0;
}

