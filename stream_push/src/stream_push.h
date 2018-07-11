//// by fanxiushu 2018-06-27 以ffmpeg为基础的推流
//// 所属 xdisp_virt 项目,xdisp_virt是远程桌面控制程序，能实现网页方式控制，原生程序方式控制，中转服务器，支持音频等
//// 程序下载： https://github.com/fanxiushu/xdisp_virt BLOG: https://blog.csdn.net/fanxiushu/article/details/80996391

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define STREAM_PUSH_MAX_URLS    200 //能同时多路推流的最大值

///////////////////////////////
enum STREAM_TYPE
{
	VIDEO_STREAM = 1,
	AUDIO_STREAM = 2,
};
////
struct stream_frame_packet_t
{
	enum STREAM_TYPE type;   ////
	
	///buffer包含已经编码好的H264或AAC一帧数据， H264是Annex-B格式存储的（就是以00 00 01 或00 00 00 01开始码分隔）， AAC是包含ADTS头的格式
	unsigned char*   buffer;
	int              length;
	
	////
};

/////
#define STREAM_STATE_RUN_ALL      1 //同时推流视频和音频
#define STREAM_STATE_RUN_VIDEO    2 //推流视频
#define STREAM_STATE_RUN_AUDIO    3 //推流音频
#define STREAM_STATE_PAUSE        4 //暂停
#define STREAM_STATE_STOP         5 //停止
#define STREAM_STATE_DELETE       6 //删除

/////
#define STREAM_URL_STATE_HEADEZR_SIZE  (sizeof(int)*3)
#pragma pack(1)
struct stream_url_state_t
{
	int            total_size;    ///整个包大小
	int            stream_state;  ///对应上面 STREAM_STATE_XXX
	unsigned int   unique_id;     ///如果>0 且 total_size == STREAM_URL_STATE_HEADEZR_SIZE，则根据 unique_id 判断，否则使用 url 进行判断
	char           url[0];        /// 
};
#pragma pack()


/// function

void* stream_push_create();
void  stream_push_destroy(void* handle);

int stream_push_post_encoded_frame(void* handle, struct stream_frame_packet_t* frame); //按照实时流方式投递已经编码好的H264或AAC-LC数据帧

int stream_push_modify_url(void* handle, struct stream_url_state_t* u);

int stream_push_query_url(void* handle, int is_network_order,
	      struct stream_url_state_t** p_array, int* p_count, int* p_size);

int stream_push_query_state(void* handle,
	int* p_all, int* p_video, int*p_audio, int*p_pause, int* p_stop);

void stream_push_free_memory(void* ptr);//释放 stream_push_query_url 分配的内存

////
int stream_push_add_url(void* handle, const char* url);
int stream_push_remove_url(void* handle, const char* url);

/// simple video encoder
int simple_video_encode(unsigned char* rgb32, int rgb32_len,
	int width, int height,
	unsigned char* out, int out_len);


#ifdef __cplusplus
}
#endif


