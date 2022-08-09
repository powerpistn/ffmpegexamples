# 1.结构体

![]()
## 1.1 AVFormatContext
该结构体存储音视频封装格式中的信息,常用的变量如下
`* struct AVInputFormat *iformat;//输入数据的封装格式
* AVIOContext *pb;//输入数据的缓存
* unsigned int nb_streams;//音视频流的个数
* AVStream **stream;//音视频流
* char filename[1024];//文件名
* int64_t duration;//时长(单位是微秒us,转换为秒需要除以1000000)
* int bit_rate;//比特率(单位是bps,转换为kps需要除以1000)
* AVDictionary *metadata;//元数据`

## 1.2 AVCodecContext
该结构体描述编解码器上下文信息,包含编解码器需要的各种参数,常用的变量如下


## 1.3 AVPacket

If you are new the project, then a good place to start is Gitter:

https://gitter.im/MacDownApp/macdown

Join the room, introduce yourself and find out how you can help out.

## License

MacDown is released under the terms of MIT License. For more details take a look at the [README](https://github.com/MacDownApp/macdown/blob/master/README.md).

