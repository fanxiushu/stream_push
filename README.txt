这是从 xdisp_virt项目剥离出来的，
实现 实时的 H264 + AAC 编码 音频和视频的
RTSP, RTMP协议推流，
以及保存到本地MP4，MKV录像文件。

如果要成功编译，
需要下载和编译ffmpeg库，libfdk-aac库，x264库。

编译这些库，非常耗时。如果懒得去编译，
可以直接使用已经编译好的stream_push.dll动态库。

stream_push目录是实现核心推流和保存本地录像工程，
demo目录是简单的调用例子，简单实现了抓取屏幕然后推流到RTSP,RTMP服务器，以及保存到本地。
bin目录是已经编译好的二进制文件.

有兴趣可查看BLOG:
https://blog.csdn.net/fanxiushu/article/details/80996391

