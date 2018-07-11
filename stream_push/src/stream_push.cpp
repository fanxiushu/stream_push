//// by fanxiushu 2018-06-27 以ffmpeg基础的推流
//// 所属 xdisp_virt 项目,xdisp_virt是远程桌面控制程序，能实现网页方式控制，原生程序方式控制，中转服务器，支持音频等
//// 程序下载： https://github.com/fanxiushu/xdisp_virt , BLOG: https://blog.csdn.net/fanxiushu/article/details/80996391

#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <string>
using namespace std;
#include <MMSystem.h>
#pragma comment(lib,"winmm.lib")
#include "ffmpeg.h"
#include "sps_decode.h"
#include "stream_push.h"


#define STREAM_TIMEOUT  10

#define STREAM_OPEN_TIMEOUT  5

#define STREAM_MAX_WAIT_PACKET    100  //队列中等待太多没发出去,


/////
#define DATAPTR_INC(ptr)  InterlockedIncrement( (volatile LONG*)(ptr)) ;
#define DATAPTR_DEC(ptr)  if(InterlockedDecrement((volatile LONG*)(ptr)) == 0){ free(ptr); /*printf("free raw _ptr \n");*/}

////模拟的SPS,PPS，1920X1080，用于一开始执行只录音不录屏
static unsigned char sps_buf[24] = {
	0x67,
	0x42,0xC0,0x28,0xDA,0x1,0xE0,0x8,0x9F,0x96,0x10,0x0,0x0,0x3,0x0,0x10,0x0,0x0,0x3,0x3,0x28,
	0xF1,0x83,0x2A, };
static unsigned char pps_buf[4] = {
	0x68,
	0xCE,0xF,0xC8, };

struct strm_pkt_t
{
	enum STREAM_TYPE type; //
	///
	union {
		struct {
			byte* sps_buffer;
			int   sps_len;
			byte* pps_buffer;
			int   pps_len;
		}v;
		///
		struct {
			int   channel;
			int   sample_rate_index;
			int   aot;  ///
		}a;
	};

	int64_t  timestamp; /// us 
	/////
	void*   rawptr; ///

	byte*   data;
	int     length;
};

///////
static uint32_t get_bits(unsigned char* buf, uint32_t* s, uint32_t BitCount)
{
	uint32_t dwRet = 0;
	uint32_t& nStartBit = *s;
	for (uint32_t i = 0; i<BitCount; i++)
	{
		dwRet <<= 1;
		if (buf[(nStartBit) / 8] & (0x80 >> ((nStartBit) % 8)))
		{
			dwRet += 1;
		}
		(nStartBit)++;
	}
	return dwRet;
}
static int get_sample_rate(int index)
{
	///
	switch (index) {
	case 0:return 96000;
	case 1:return 88200;
	case 2:return 64000;
	case 3:return 48000;
	case 4:return 44100;
	case 5:return 32000;
	case 6:return 24000;
	case 7:return 22050;
	case 8:return 16000;
	case 9:return 12000;
	case 10:return 11025;
	case 11:return 8000;
	case 12:return 7350;
	}
	///
	return 44100;
}

/////
static bool equal_string(const char* s1, const char* s2)
{
	if (stricmp(s1, s2) == 0)return true;
	return false;
}

class stream_manager
{
protected:
	unsigned int   unique_id;
	//
	CRITICAL_SECTION cs;

	list<stream_url_state_t*>    wait_urls; ///

	list<struct stream_client*>  clients; ///

	///
	unsigned char audio_mute_data[16 * 1024]; ///一帧静音数据
	int audio_mute_size;
	int encode_audio_mute_data(); //编码一帧静音数据

	int64_t  last_audio_timestamp; ///us
	int  audio_channel;
	int  audio_sample_rate; 
	MMRESULT  timerId; 
	static void  CALLBACK audioTimer(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
		stream_manager*uac = (stream_manager*)dwUser;
		uac->audio_timer();
	}
	void audio_timer();

	////
	void post_strm_pkt(strm_pkt_t* pkt);

public:
	////
	stream_manager();
	~stream_manager();

	void destroy(); ///
	void remove_client(stream_client* c) {
		Lock();
		for (list<stream_client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
			if ( *it == c) {
				clients.erase(it);
				break;
			}
		}
		Unlock();
	}
	//
public:
	///
	void inline Lock() { ::EnterCriticalSection(&cs); }
	void inline Unlock() { ::LeaveCriticalSection(&cs); }
	///
	int modify(stream_url_state_t* url);
	int query(bool is_network_order, stream_url_state_t** p_array, int* p_count, int* p_size );
	int query(int* p_all, int* p_video, int*p_audio, int*p_pause, int* p_stop);
	int post(stream_frame_packet_t* frame);

};

struct stream_client 
{
	HANDLE               hThread; ///
	bool                 quit;
	HANDLE               hSemaphore;   ///

	unsigned int         unique_id;
	int                  stream_state;

	///
	static DWORD CALLBACK thread(void* _p) {
		stream_client* s = (stream_client*)_p;
		s->stream_loop();
		////
		s->stream_mgr->remove_client(s); ///
		delete s;
		return 0;
	}
	void stream_loop( );

	///
	stream_manager*      stream_mgr;
	string               stream_url;
	string               stream_proto;

	/////
	AVFormatContext*     ofm_ctx;

	/// param
	/// SPS & PPS
	unsigned char*  sps_buffer;
	unsigned char*  pps_buffer;
	int sps_len, pps_len;
	int video_width;
	int video_height;

	int audio_channel;
	int audio_sample_rate_index;
	int audio_aot; // AAC-LC
	
	///
	stream_client(stream_manager* mgr, const char* url, const char* proto);
	~stream_client();

	//
	bool      is_write_header; ///是否已经写入了头
	time_t    last_write_time; ///
	int       last_write_errcnt; ///

	int64_t   start_time; //毫秒，开始时间
	int64_t   pause_duration; //
	int64_t   last_pause_time; ///

	AVStream* audio_stream;
	AVStream* video_stream;

	AVStream *new_stream(AVFormatContext *oc, enum AVCodecID codec_id);

	static int interrupt_cb(void* _p);

	////
	time_t last_open_time;
	int  open();
	void close();

	////
	list<strm_pkt_t> packets;

	int send_header();
	///
	int send_packet(AVPacket* pkt);
	int parse_packet(strm_pkt_t* frame);
};

/////
stream_client::stream_client(stream_manager* mgr, const char* url, const char* proto):
	stream_mgr(mgr), stream_url(url), stream_proto(proto?proto:"")
{
	quit = false;
	hThread = NULL;
	ofm_ctx = NULL;
	audio_stream = video_stream = NULL;
	hSemaphore = CreateSemaphore(NULL, 0, LONG_MAX, NULL); ///
	unique_id = 0;
	stream_state = STREAM_STATE_RUN_ALL; ///

	////
	if (strnicmp(url, "rtmp", 4) == 0) { // rtmp
		stream_proto = "flv";
	}
	else if (strnicmp(url, "rtsp", 4) == 0) { // rtsp
		stream_proto = "rtsp";
	}

	/////
	sps_buffer = pps_buffer = NULL;
	sps_len = pps_len = 0;
	video_width = 1920;
	video_height = 1080;

	audio_channel = 2;
	audio_sample_rate_index = 3;
	audio_aot = 1;
	////
	last_write_errcnt = 0;
	last_write_time = time(0);
	last_open_time = 0;
	is_write_header = false;
	start_time = timeGetTime();
	pause_duration = 0;
	last_pause_time = start_time;
}

stream_client::~stream_client()
{
	///

	/////
	quit = true;
	ReleaseSemaphore(hSemaphore, 10, NULL);

	close(); ///
	///

	CloseHandle(hSemaphore);
	if (hThread)CloseHandle(hThread);
	if (sps_buffer)free(sps_buffer);
	//
	for (list<strm_pkt_t>::iterator it = packets.begin(); it != packets.end(); ++it) {
		///
		DATAPTR_DEC(it->rawptr);
	}
	packets.clear();
	printf("stream_client::~stream_client()\n");
}

AVStream* stream_client::new_stream(AVFormatContext *oc, enum AVCodecID codec_id)
{
	AVCodecContext *c;
	AVStream *st;
	AVCodec* codec;

	/////
	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		printf("avcodec_find_encoder not found codecID=%d\n", codec_id);
		return NULL;
	}

	st = avformat_new_stream(oc, codec);
	if (!st) {
		fprintf(stderr, "avformat_new_stream err: Could not allocate stream\n");
		return NULL;
	}
	st->id = oc->nb_streams - 1;
	c = st->codec; //printf("new_stream c=%p\n", c);
//	c = avcodec_alloc_context3(codec);

	switch (codec->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt = codec->sample_fmts ?
			codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
		c->channels = audio_channel;
		c->channel_layout = av_get_default_channel_layout(c->channels);
		c->bit_rate = 128*1000;
		c->frame_size = 1024; //
		c->sample_rate = get_sample_rate(audio_sample_rate_index);
		c->time_base = st->time_base = { 1, c->sample_rate };
		break;

	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;
		c->bit_rate = 40 * 1024 * 1024; ////
		c->width = video_width; 
		c->height = video_height;//printf("W=%d,H=%d\n",c->width,c->height);
		c->time_base = st->time_base = { 1, 25 };

		c->gop_size = 600;
		c->pix_fmt = AV_PIX_FMT_YUV420P;

		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B-frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			* This does not happen with normal video, it just happens here as
			* the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}

		break;
	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		printf("*** has set AVFMT_GLOBALHEADER \n");
	}


	/// open codec
/*	int ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		printf("*** avcodec_open2 err=%d\n");
		////
		return NULL;
	}
	avcodec_parameters_from_context(st->codecpar, c);*/

	/////
	return st;
}

int stream_client::interrupt_cb(void* _p)
{
	stream_client* s = (stream_client*)_p;
	///
	time_t cur = time(0);
	if (abs(cur - s->last_write_time) > STREAM_TIMEOUT) {
		//
		s->last_write_time = cur;
		///
		printf("*** interrupt_cb TIMEOUT.\n");
		/////
		return 1;
	}
//	printf("Interrupt Callback.\n");
	///
	return 0;
}

///////
#define STREAM_CLOSE() \
	if(video_stream){ if(video_stream->codec)avcodec_close(video_stream->codec); video_stream=NULL;} \
	if(audio_stream){ if(audio_stream->codec)avcodec_close(audio_stream->codec); audio_stream=NULL;}
///
#define O_CLOSE() if(ofm_ctx){ avformat_free_context(ofm_ctx); ofm_ctx=NULL; } \
                  is_write_header=false; 

int stream_client::open()
{
	if (ofm_ctx) {
		return 0;
	}
//	av_log_set_level(AV_LOG_TRACE);
	////
	int ret = avformat_alloc_output_context2(&ofm_ctx, NULL, 
		stream_proto.length() > 1 ? stream_proto.c_str() : NULL, 
		stream_url.c_str());
	if ( !ofm_ctx ) {
		printf("avformat_alloc_output_context2 err=%d\n", ret );
		return -1;
	}
	AVOutputFormat *fmt = ofm_ctx->oformat;
	if (!fmt) {
		O_CLOSE();
		return -1;
	}

	////固定 H264 + AAC 编码
	fmt->video_codec = AV_CODEC_ID_H264;
	fmt->audio_codec = AV_CODEC_ID_AAC;
	
	///
	video_stream = new_stream(ofm_ctx, fmt->video_codec);
	if (!video_stream) {
		STREAM_CLOSE();
		O_CLOSE();
		printf("*** new_stream video error.\n");
		return -1;
	}
//	printf("--- Begin Audio Stream\n");
	audio_stream = new_stream(ofm_ctx, fmt->audio_codec);
	if (!audio_stream) {
		STREAM_CLOSE();
		O_CLOSE();
		printf("*** new_stream audio error");
		return - 1;
	}
//	printf("*** open a/v strean ok.\n");
	//////
//	av_dump_format(ofm_ctx, 0, stream_url.c_str(), 1);

	///
	ofm_ctx->interrupt_callback.callback = interrupt_cb;
	ofm_ctx->interrupt_callback.opaque = this;

	if (!(fmt->flags & AVFMT_NOFILE)) {
		///
		
		////
		ret = avio_open2(&ofm_ctx->pb, stream_url.c_str(), AVIO_FLAG_WRITE, &ofm_ctx->interrupt_callback, NULL );

		if (ret < 0) {
			///
			STREAM_CLOSE();
			O_CLOSE();

			printf("avio_open2 [%s] err=%d\n", stream_url.c_str(), ret );
			/////
			return -1;
		}

		////
	}

	////
	
	start_time = av_gettime(); // us
	last_write_errcnt = 0;
	pause_duration = 0;
	last_pause_time = start_time;

	printf("*** Open stream OK, video_index=%d, audio_index=%d\n", video_stream->index, audio_stream->index );

	return 0;
}

void stream_client::close()
{
	if (!ofm_ctx) {
		return;
	}

	/* Write the trailer, if any. The trailer must be written before you
	* close the CodecContexts open when you wrote the header; otherwise
	* av_write_trailer() may try to use memory that was freed on
	* av_codec_close(). */
	if (is_write_header) {
		av_write_trailer(ofm_ctx);
		//
		is_write_header = false;
	}

	////
	STREAM_CLOSE(); ///
	if (!(ofm_ctx->oformat->flags & AVFMT_NOFILE))avio_closep(&ofm_ctx->pb);
	O_CLOSE();
	/////
	pause_duration = 0; ///
}
int stream_client::send_header()
{
	if (!ofm_ctx) {
		time_t cur = time(0);
		if (abs(cur - last_open_time) >= STREAM_OPEN_TIMEOUT) {
			last_open_time = cur;
			////
			open();
			if (!ofm_ctx) {
               printf("Can not OutputStream for send Header.\n");
			}
		}
	}
	////
	if (!ofm_ctx) {
		
		return -1;
	}
	if (is_write_header) {
		return 0;
	}

	if (!sps_buffer || !pps_buffer)return -1;

	/////
	AVCodecContext* c = video_stream->codec;
	int extradata_len = 8 + sps_len + pps_len + 2 + 1;
	c->extradata_size = extradata_len;
	c->extradata = (byte*)av_mallocz(extradata_len);

	int i = 0;
	byte* body = (byte*)c->extradata;

	//H264 AVCC 格式的extradata头，用来存储 SPS，PPS
	body[i++] = 0x01;
	body[i++] = sps_buffer[1];
	body[i++] = sps_buffer[2];
	body[i++] = sps_buffer[3];
	body[i++] = 0xff;

	//// SPS 
	body[i++] = 0xe1;
	body[i++] = (sps_len >> 8) & 0xff;
	body[i++] = sps_len & 0xff;
	memcpy(&body[i], sps_buffer, sps_len);
	i += sps_len;

	/// PPS
	body[i++] = 0x01;
	body[i++] = (pps_len >> 8) & 0xff;
	body[i++] = (pps_len) & 0xff;
	memcpy(&body[i], pps_buffer, pps_len);
	i += pps_len;

	////// audio, ADTS头转换为MPEG-4 AudioSpecficConfig
	c = audio_stream->codec;
	c->extradata_size = 2;
	c->extradata = (byte*)av_malloc(2);
	byte dsi[2];
	dsi[0] = ((audio_aot+1) << 3) | (audio_sample_rate_index >> 1);
	dsi[1] = ((audio_sample_rate_index & 1) << 7) | (audio_channel << 3);
	memcpy(c->extradata, dsi, 2);

	last_write_time = time(0); ///
	
	//////
	AVDictionary* options = NULL;
	if (strnicmp(stream_url.c_str(), "rtsp", 4) == 0) {// RTSP protocol , use TCP 
		///
		av_dict_set(&options, "rtsp_transport", "tcp", 0);
	}
	int ret = avformat_write_header(ofm_ctx, options ? &options : NULL); ////
	if (options)av_dict_free(&options);

	if (ret < 0) {

		close();
		////
		char err[512] = ""; av_strerror(ret, err, 512);
		printf("avformat_write_header [%s] err=%d [%s]\n", stream_url.c_str(), ret, err);

		return -1;
	}

	is_write_header = true;
	///
	start_time = av_gettime(); ///
	pause_duration = 0;
	last_pause_time = start_time;

	//////
	return 0;
}

int stream_client::send_packet(AVPacket* pkt)
{
	time_t cur = time(0);
	if (!ofm_ctx) {
		//
		if (abs(cur - last_open_time) >= STREAM_OPEN_TIMEOUT) {
			last_open_time = cur;
			////
			close();
			open();
			////
			printf("*** reconnect [%s]\n", stream_url.c_str() );
		}
	}

	/////
	int ret = 0;
	if (ofm_ctx) {
		///
		last_write_time = cur; ///
		//
//		printf("stream_index=%d, PTS=%lld\n", pkt->stream_index, pkt->pts);
		ret = av_interleaved_write_frame(ofm_ctx, pkt);
//		ret = av_write_frame(ofm_ctx, pkt);
		if (ret < 0) {
			last_write_errcnt++;

			char err[512] = ""; av_strerror(ret, err, 512);
			printf("av_interleaved_write_frame err=%d, [%s]\n", ret , err );
			////
			if (last_write_errcnt > 100) {
				last_write_errcnt = 0;
				///
				close(); // 下次重新连接
			}
			///////
		}
		else {
			last_write_errcnt = 0; ///
		}
	}
	else {
		ret = -1;
	}

	/////
	return ret;
}
///

int stream_client::parse_packet(strm_pkt_t* frame)
{
	////
	if (frame->type == VIDEO_STREAM) { /// video
		///
		if (frame->v.sps_buffer && frame->v.pps_buffer) { // have SPS, PPS
			///
			bool is_change = false;

			if (sps_len == frame->v.sps_len && pps_len == frame->v.pps_len && sps_buffer && pps_buffer) {
				if (memcmp(sps_buffer, frame->v.sps_buffer, sps_len) == 0 &&
					memcmp(pps_buffer, frame->v.pps_buffer, pps_len) == 0) 
				{

				}
				else {
					is_change = true; printf("SPS & PPS context changed.\n");////
				}
			}
			else {
				is_change = true; printf("First Use SPS & PPS.\n");
			}

			////
			int len = frame->v.sps_len + frame->v.pps_len;
			if (pps_len + sps_len < len || !sps_buffer ) {
				if (sps_buffer)free(sps_buffer);
				sps_buffer = (byte*)malloc(len);
				is_change = true;
				///
			}
			
			/// copy SPS PPS
			if (is_change) {
				sps_len = frame->v.sps_len;
				pps_len = frame->v.pps_len;
				pps_buffer = sps_buffer + sps_len;
				memcpy(sps_buffer, frame->v.sps_buffer, sps_len);
				memcpy(pps_buffer, frame->v.pps_buffer, pps_len);
			}
			////
			

			int width, height;
			SPS sps;
			h264_sps_parse(&sps, sps_buffer, sps_len);
		//	printf("VideoWidth=%d,VideoHeight=%d, crop=[%d,%d,%d,%d]\n", sps.pic_width, sps.pic_height,
		//		sps.frame_crop_left_offset, sps.frame_crop_top_offset, sps.frame_crop_right_offset, sps.frame_crop_bottom_offset);
			width = sps.pic_width;
			height = sps.pic_height;

			////
			if (width != video_width || height != video_height) {
				video_width = width; video_height = height;
				is_change = true;
			}
			/////
			if (is_change) {
				close();
				printf("*** Video: PPS & SPS Changed.\n");
			}

			///
			if (!is_write_header) {
				///
				send_header();
			}
		}

		if (!is_write_header && sps_buffer && pps_buffer && !ofm_ctx ) {// 没写头 
			time_t cur = time(0);
			///
			send_header(); /// 
			////
		}
		////
		if (is_write_header && frame->length > 0) {//只有写了头，才能写frame
			///
			AVPacket pkt = { 0 };
			av_init_packet(&pkt);
			pkt.stream_index = video_stream->index;
			pkt.flags = 0;
			byte type = (frame->data[4] & 0x1F);
			if (type == 5 || type==6 ) { // IDR or SEI
				pkt.flags = AV_PKT_FLAG_KEY; /// keyframe
			}
			
			int64_t pts = (frame->timestamp - start_time - pause_duration )*1.0/1000000.0/av_q2d(video_stream->time_base);
			pkt.pts = pts;
			pkt.dts = pkt.pts;

			pkt.data = frame->data;
			pkt.size = frame->length;
			////
			send_packet(&pkt);
			///
			av_packet_unref(&pkt);
		}

		/////
	}
	else if (frame->type == AUDIO_STREAM) { // audio
		///
		bool is_change = false;
		//////
		if (frame->rawptr) {
			if (frame->a.channel != audio_channel ||
				frame->a.aot != audio_aot ||
				frame->a.sample_rate_index != audio_sample_rate_index) {
				is_change = true;
				printf("*** Audio Format Changed.\n");
				////
				audio_channel = frame->a.channel;
				audio_aot = frame->a.aot;
				audio_sample_rate_index = frame->a.sample_rate_index;

				////音频格式发生变化，重新打开
				close();
				printf("*** Audio: Format CHanged.\n");
				send_header();
				
				////
			}
			/////
		}
		/////

		if (is_write_header) { //只有写了头，才能写frame
			AVPacket pkt = { 0 };
			av_init_packet(&pkt);
			pkt.stream_index = audio_stream->index;
			pkt.flags = 0;

			int64_t pts = (frame->timestamp - start_time - pause_duration )*1.0 / 1000000.0 / av_q2d(audio_stream->time_base);
			pkt.pts = pts;
			pkt.dts = pkt.pts;
			pkt.duration = 1024; /// AAC 固定值

			pkt.data = frame->data;
			pkt.size = frame->length;

			send_packet(&pkt);
			///
			av_packet_unref(&pkt);
			///////
		}
	}
	return 0;
}

void stream_client::stream_loop()
{
	///
	while (!quit) {
		///
		::WaitForSingleObject(this->hSemaphore, INFINITE);
		if (this->quit)break;
		/////
		bool find = false;
		strm_pkt_t frame;

		stream_mgr->Lock();
		if (packets.size() > 0) {
			frame = packets.front();
			packets.pop_front();
			find = true;
		}
		stream_mgr->Unlock();
		////
		if (!find)continue;
		
		if (stream_state == STREAM_STATE_RUN_AUDIO && !this->is_write_header) { //还没写头
			printf("** Only Record Audio ,But not Write Header.\n");
			if (!this->sps_buffer) {
				printf("** only Record Audio : no SPS,PPS ,use default.\n");
				strm_pkt_t pkt; memset(&pkt, 0, sizeof(pkt));
				pkt.type = VIDEO_STREAM; 
				pkt.v.sps_buffer = sps_buf; pkt.v.sps_len = sizeof(sps_buf);
				pkt.v.pps_buffer = pps_buf; pkt.v.pps_len = sizeof(pps_buf);
				///
				parse_packet(&pkt);
			}
			/////
			send_header();
		}
		///////
		parse_packet(&frame);
		
		/////
		if (frame.rawptr) {
			DATAPTR_DEC(frame.rawptr);
		}

		////
	}

	/////
}

////
static unsigned char* find_start_code(unsigned char* s, unsigned char* end, int* p_hdr_len)
{
	unsigned char* e = s;
	*p_hdr_len = 0;
	while (e < end) {

		if (e + 2 >= end)break; ///
								////
		if (*e == 0x00 && *(e + 1) == 0x00) {
			if (*(e + 2) == 0x01) { //三个字节
				*p_hdr_len = 3;
				return e;
			}
			else if (*(e + 2) == 0x00 && (e + 3) < end && *(e + 3) == 0x01) {//四个字节
				*p_hdr_len = 4;
				return e;
			}
			////
		}
		//////
		++e;
	}
	////
	return NULL;
}

static int parse_h264_strm_pkt(unsigned char* buffer, int length, strm_pkt_t* pkt)
{
	memset(pkt, 0, sizeof(strm_pkt_t));

	pkt->type = VIDEO_STREAM;
	////
	unsigned char* end = buffer + length;
	unsigned char* s, *e;
	////
	int alloc_size = 4 + length + 4096; ///
	byte* rawptr = (byte*)malloc(alloc_size);
	byte* data = rawptr + 4;
	int size = 0;
	int pos = 4 ;

	////
	e = s = buffer;
	int hlen = 0;

	while (e && e < end) {
		////
		if (hlen == 0) { // first
			s = find_start_code(e, end, &hlen);
			if (!s)break;
		}
		else {
			s = e;
		}
		//	printf("hlen=%d\n",hlen);
		s += hlen; ///去除开始码
		////
		int nalu_len = 0;

		e = find_start_code(s, end, &hlen);
		if (e) {
			nalu_len = e - s;
			////
		}
		else {
			nalu_len = end - s;
		}
		if (nalu_len <= 0)continue; ///

		/////NALU data, NALU size
		byte* nalu = s; int nalu_size = nalu_len;
		byte type = nalu[0] & 0x1F;

		if (type == 7) { // SPS
			memcpy(rawptr + pos, nalu, nalu_size);
			pkt->v.sps_buffer = rawptr + pos;
			pkt->v.sps_len = nalu_size;
			///
			pos += nalu_size;
			data = NULL; size = 0;
		}
		else if (type == 8) { // PPS
			memcpy(rawptr + pos, nalu, nalu_size);
			pkt->v.pps_buffer = rawptr + pos;
			pkt->v.pps_len = nalu_size;
			////
			pos += nalu_size;
			data = rawptr + pos;
			size = 0;
		}
		else { ///normal frame
			if (!data)continue;
			////
			*(uint32_t*)(rawptr + pos) = htonl(nalu_size);
			pos += 4;
			memcpy(rawptr + pos, nalu, nalu_size);
			pos += nalu_size;

			size += 4 + nalu_size;
			////
		}
	}
	///
	if (!data) {
		printf("parse h264 data err\n");
		free(rawptr);
		return -1;
	}

	*(int*)rawptr = 1; /// refcount==1
	pkt->data = data;
	pkt->length = size;
	pkt->rawptr = rawptr;

	return 0;
}

static int parse_aac_strm_pkt(unsigned char* buffer, int length, strm_pkt_t* pkt)
{
	memset(pkt, 0, sizeof(strm_pkt_t));

	pkt->type = AUDIO_STREAM;
	////
	if (length < 7) { //ADTS 头长度必须大于7，
		return -1;
	}
	int aac_header_size = 7; //

	/////以下代码基本是从ffmpeg复制
	int size, rdb, ch, sr;
	int aot, crc_abs;

	uint32_t curpos = 0;
	int logo = get_bits(buffer, &curpos, 12);
	if ( logo != 0xfff) {
		printf("Not AAC Header. logo=0x%X, [%.2X%.2X]\n",logo, buffer[0], buffer[1]);
		return -1;
	}
	get_bits(buffer, &curpos, 1);            /* id */
	get_bits(buffer, &curpos, 2);            /* layer */
	crc_abs = get_bits(buffer, &curpos, 1);    /* protection_absent */
	aot = get_bits(buffer, &curpos, 2);   /* profile_objecttype */
	sr = get_bits(buffer, &curpos, 4);   /* sample_frequency_index */

	get_bits(buffer, &curpos, 1);             /* private_bit */
	ch = get_bits(buffer, &curpos, 3);       /* channel_configuration */

	get_bits(buffer, &curpos, 1);              /* original/copy */
	get_bits(buffer, &curpos, 1);              /* home */

	/* adts_variable_header */
	get_bits(buffer, &curpos, 1);              /* copyright_identification_bit */
	get_bits(buffer, &curpos, 1);              /* copyright_identification_start */
	size = get_bits(buffer, &curpos, 13);     /* aac_frame_length */
	if (size < 7) {
		return -1;
	}

	get_bits(buffer, &curpos, 11);           /* adts_buffer_fullness */
	rdb = get_bits(buffer, &curpos, 2);       /* number_of_raw_data_blocks_in_frame */

	////
	if (!crc_abs) {
		aac_header_size = 9; //CRC 
		if (length < 9) {
			return -1;
		}
		///
	}

	//////
	byte* rawptr = (byte*)malloc(length + 4 );
	int len = length - aac_header_size;
	byte* data = rawptr + 4;
	memcpy(data, buffer + aac_header_size, len);

	pkt->a.channel = ch;
	pkt->a.aot = aot;
	pkt->a.sample_rate_index = sr;

	*(int*)rawptr = 1; /// refcount==1
	pkt->data = data;
	pkt->length = len;
	pkt->rawptr = rawptr;

	return 0;
}

stream_manager::stream_manager()
{
	::InitializeCriticalSection(&cs);
	timerId = 0;
	audio_channel = 2; ///
	audio_sample_rate = 48000; /// 48KHZ
	audio_mute_size = 0;
	memset(audio_mute_data, 0, sizeof(audio_mute_data));
	last_audio_timestamp = 0;
	////
	unique_id = 0; ///

}
stream_manager::~stream_manager()
{
	
	////
	::DeleteCriticalSection(&cs);
}
void stream_manager::destroy()
{
	if (timerId > 0) {
		timeKillEvent(timerId);
		timerId = 0;//
	}
	/////
	Lock();
	for (list<stream_client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
		stream_client*c = *it;
		c->quit = true;
		ReleaseSemaphore(c->hSemaphore, 100, NULL);
	}

	for (list<stream_url_state_t*>::iterator it = wait_urls.begin(); it != wait_urls.end(); ++it) {
		free(*it);
	}
	wait_urls.clear();
	Unlock();

	////
	int cc = 0;
	while (cc++ < 10) {
		if (clients.size() == 0) {
			break;
		}
		Sleep(1000);
	}
	////
	if (clients.size() > 0) {
		printf("*** Warning: clients not complete quit.\n");
	}
}

static AVFrame *alloc_silence_frame(int channels, int samplerate, AVSampleFormat format)
{
	AVFrame *frame;
	int32_t ret;
	frame = av_frame_alloc();
	if (!frame){
		return NULL;
	}
	frame->sample_rate = samplerate;
	frame->format = format; //默认的format:AV_SAMPLE_FMT_FLTP
	frame->channel_layout = av_get_default_channel_layout(channels);
	frame->channels = channels;
	frame->nb_samples = 1024; // AAC :1024
	ret = av_frame_get_buffer(frame, 0);
	if (ret < 0){
		av_frame_free(&frame);
		return NULL;
	}

	av_samples_set_silence(frame->data, 0, frame->nb_samples, frame->channels, format);
	return frame;
}
int stream_manager::encode_audio_mute_data()
{
	AVCodec* id = avcodec_find_encoder(AV_CODEC_ID_AAC); if (!id)return -1;
	AVCodecContext* ctx = avcodec_alloc_context3(id); if (!ctx)return -1;
	ctx->bit_rate = 16*1000;
	ctx->sample_rate = audio_sample_rate;
	ctx->sample_fmt = AV_SAMPLE_FMT_S16;//固定16位
	ctx->channels = audio_channel; /// 双声道
	ctx->channel_layout = av_get_default_channel_layout(ctx->channels);
	ctx->profile = FF_PROFILE_AAC_LOW; /// AAC-LC
	///
	ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; /// Not use ADTS.
//	printf("ctx->flags=0x%X\n", ctx->flags);
	////
	int ret = avcodec_open2(ctx, id, NULL);
	if (ret < 0) {
		printf("audio: avcodec_open2 err=%d\n", ret);
		avcodec_free_context(&ctx);
		return -1;
	}
	///
	AVFrame* frame = alloc_silence_frame(audio_channel, audio_sample_rate, AV_SAMPLE_FMT_S16); ////
	if (!frame) {
		avcodec_close(ctx);
		avcodec_free_context(&ctx);
		///
		return -1;
	}

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = audio_mute_data;
	pkt.size = sizeof(audio_mute_data);

	
	int got_frame; //
	ret = avcodec_encode_audio2( ctx, &pkt,  frame, &got_frame);
	if (ret < 0) {
		printf("avcodec_encode_audio2 err=%d\n", ret);
		av_packet_unref(&pkt);

	}
	else {
		if (got_frame) {
			audio_mute_size = pkt.size; printf("------- [%.2X%.2X]\n", audio_mute_data[0],audio_mute_data[1] );
		}
		else {
			ret = -1;
		}
	}
	////
	avcodec_close(ctx);
	avcodec_free_context(&ctx);
	av_frame_free(&frame);
	return ret;
}

////
static void __drop_strm_pkts(list<strm_pkt_t>& packets, const char* url)
{
	int drop_a_cnt = 0, drop_v_cnt = 0;
	int idr_cnt = 0;
	list<strm_pkt_t>::iterator it, cur;
	for (it = packets.begin(); it != packets.end(); ) {
		cur = it; ++it;
		////
		if (cur->type == VIDEO_STREAM) {
			byte type = cur->data[4] & 0x1F;
			if (type == 5 || type == 6 || (cur->v.sps_buffer && cur->v.pps_buffer) ) { // IDR
				idr_cnt++;
			}
			else {
				drop_v_cnt++;
				DATAPTR_DEC(cur->rawptr);
				packets.erase(cur);
			}
			/////
		}
		else { // audio
			drop_a_cnt++;
			if (cur->rawptr) { DATAPTR_DEC(cur->rawptr); }
			packets.erase(cur);
		}
	}
	//////
	if (idr_cnt > 1) {//不止一个关键帧，只保留最后一个
		for (it = packets.begin(); it != packets.end(); ) {
			cur = it; ++it;
			///
			if (cur->type == VIDEO_STREAM) {
				byte type = cur->data[4] & 0x1F;
				if (type == 5 || type == 6 || (cur->v.sps_buffer && cur->v.pps_buffer)) { // IDR
					idr_cnt--;
					if (idr_cnt == 0) {//最后一个
						break;
					}
					////
				}
				else {
					drop_v_cnt++;
					DATAPTR_DEC(cur->rawptr);
					packets.erase(cur);
				}
				///////
			}
			////
		}
	}
	//////
	printf("StreamPush Drop Frame: VideoCnt=%d, AudioCnt=%d; [%s]\n", drop_v_cnt,drop_a_cnt, url );
}

void stream_manager::post_strm_pkt(strm_pkt_t* pkt)
{
	for (list<stream_client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
		stream_client* c = *it;
		if (c->quit)continue;

		///网络太慢，判断是否可以丢弃一些帧
		if (c->packets.size() > STREAM_MAX_WAIT_PACKET) {
			__drop_strm_pkts(c->packets, c->stream_url.c_str());
		}
		
		///判断入队哪些帧
		if (c->stream_state == STREAM_STATE_PAUSE)continue;

		strm_pkt_t tmp_pkt;
		if (pkt->rawptr) { //除去timer发送的静音帧
			if (pkt->type == VIDEO_STREAM) {
				if (c->stream_state != STREAM_STATE_RUN_ALL && c->stream_state != STREAM_STATE_RUN_VIDEO) {
					///
					continue;
				}
			}
			else { // audio 
				if (c->stream_state != STREAM_STATE_RUN_ALL && c->stream_state != STREAM_STATE_RUN_AUDIO) {//改成静音
					///
					if (audio_mute_size > 0) {
						memset(&tmp_pkt, 0, sizeof(tmp_pkt)); ///
						tmp_pkt.timestamp = pkt->timestamp;
						//
						pkt = &tmp_pkt;
						pkt->type = AUDIO_STREAM;
						pkt->data = audio_mute_data;
						pkt->length = audio_mute_size;
						pkt->rawptr = NULL;
					}
					////
				}
			}
			/////
		}

		/////
		if (c->packets.size() < STREAM_MAX_WAIT_PACKET * 2 / 3) {//如果丢弃了足够多的帧
			if (pkt->rawptr) { DATAPTR_INC(pkt->rawptr); }
			c->packets.push_back(*pkt);
			///
			ReleaseSemaphore(c->hSemaphore, 1, NULL);
		}
		else {
			printf("Drop PerFrame Queue Too Much Packets. [%s] \n", c->stream_url.c_str());
		}
		//
	}
	//////
}

void stream_manager::audio_timer()
{
	///
	if (audio_mute_size <= 0)return;

	int64_t cur = av_gettime(); ///
	////
	int max_cnt = 4;
	Lock();
	int64_t step = (int64_t)1024 * 1000 * 1000 / audio_sample_rate; // us，每个AAC压缩帧播放的时间
	int64_t last = max( cur - max_cnt*step, last_audio_timestamp + step );
	///
	strm_pkt_t pkt; memset(&pkt, 0, sizeof(pkt));
	pkt.type = AUDIO_STREAM;
	pkt.data = audio_mute_data;
	pkt.length = audio_mute_size;
	pkt.rawptr = NULL;
	/////
	while (last <= cur -step ) { ////
		///
		pkt.timestamp = last;
		last_audio_timestamp = last;

		////
		post_strm_pkt(&pkt); ////
		/////
		last += step; 
	}
	Unlock();
}

int stream_manager::post(stream_frame_packet_t* frame)
{
	if (clients.size() == 0) { // no clients 
		return -1;
	}
	/////
	strm_pkt_t pkt;
	pkt.rawptr = NULL;

	if (frame->type == VIDEO_STREAM) {
		parse_h264_strm_pkt(frame->buffer, frame->length, &pkt);
		if (pkt.v.sps_buffer) {
		//	printf("Video getSPS,PPS: SPS=%d, PPS=%d\n", pkt.v.sps_len,pkt.v.pps_len );
		    ///
		}
	}
	else if (frame->type == AUDIO_STREAM) {
		parse_aac_strm_pkt(frame->buffer, frame->length, &pkt);
	}
	if (!pkt.rawptr) {
		return -1;
	}

	pkt.timestamp = av_gettime(); ///
	/////
	
	Lock();
	///
	if (pkt.type == VIDEO_STREAM) {
		if (pkt.v.sps_buffer&& pkt.v.pps_buffer) { // SPS PPS

			///设置定时器，目的是为了不间断的发送音频帧，这是为了同步音视频.
			if (!timerId) { //
				///
				last_audio_timestamp = av_gettime() - 10 * 1000; //// us

				int r = encode_audio_mute_data();
				if (r == 0) {
					printf("-- Encode Slience Data size=%d\n", audio_mute_size);
				}
				/////
				timerId = timeSetEvent( 30 , 10, audioTimer, (DWORD_PTR)this,
					TIME_PERIODIC | TIME_KILL_SYNCHRONOUS | TIME_CALLBACK_FUNCTION);
				////
			}
		}
	}
	else if (pkt.type == AUDIO_STREAM) {
		if (audio_mute_size <= 0 || pkt.a.channel != audio_channel || get_sample_rate(pkt.a.sample_rate_index) != audio_sample_rate) {
			audio_channel = pkt.a.channel;
			audio_sample_rate = get_sample_rate(pkt.a.sample_rate_index);
			////
			int r = encode_audio_mute_data();
			if (r == 0) {
				printf("Encode Slience Data size=%d\n", audio_mute_size );
			}
			/////
		}
		////
		last_audio_timestamp = pkt.timestamp;
		//////
	}
	
	/////
	post_strm_pkt(&pkt); ///

	////
	Unlock();

	////
	DATAPTR_DEC(pkt.rawptr);

	return 0;
}


int stream_manager::modify(stream_url_state_t* url)
{
	if (!url)return -1;

	int ret = 0; ///
	///
	Lock();
	
	list<stream_url_state_t*>::iterator itr_state = wait_urls.end();
	list<stream_client*>::iterator itr_client = clients.end();

	bool is_url = true;
	if (url->unique_id > 0 && url->total_size == STREAM_URL_STATE_HEADEZR_SIZE) {
		is_url = false;
	}

	///
	for (list<stream_client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
		stream_client* c = *it;
		///
		if (is_url ) {/// use URL
			//////
			if (equal_string(c->stream_url.c_str(), url->url)) { ///
				itr_client = it;
				break;
			}
			////
		}
		else { 
			if (c->unique_id == url->unique_id) {
				itr_client = it;
				break;
			}
		}
		///////
	}
	if (itr_client == clients.end()) { // not found
		for (list<stream_url_state_t*>::iterator it = wait_urls.begin(); it != wait_urls.end(); ++it) {

			if (is_url) {
				if (equal_string((*it)->url, url->url)) {
					itr_state = it;
					break;
				}
			}
			else {
				////
				if ((*it)->unique_id == url->unique_id) {
					itr_state = it;
					break;
				}
			}
			/////
		}
	}

	////
	///////////////////////////////////
	if (itr_client != clients.end()) {// in clients
		///
		stream_client* c = *itr_client;

		if (url->stream_state >= STREAM_STATE_RUN_ALL && url->stream_state <= STREAM_STATE_PAUSE) {
			///
			if (c->stream_state == STREAM_STATE_PAUSE && url->stream_state != c->stream_state) {
				int64_t cur = av_gettime();
				c->pause_duration += (cur - c->last_pause_time); //
				c->last_pause_time = cur; ///
			}
			c->stream_state = url->stream_state;
			if (c->stream_state == STREAM_STATE_PAUSE) {
				c->last_pause_time = av_gettime(); 
			}
			///
		}
		else { /// 删除或者停止
			///
			if (url->stream_state == STREAM_STATE_STOP) {
				///
				int sz = STREAM_URL_STATE_HEADEZR_SIZE + c->stream_url.length() + 1; //
				stream_url_state_t* u = (stream_url_state_t*)malloc(sz);
				u->total_size = sz;
				u->stream_state = STREAM_STATE_STOP;
				u->unique_id = c->unique_id; ///
				strcpy(u->url, c->stream_url.c_str());
				///
				wait_urls.push_back(u); 

			}

			////
			HANDLE hSem = c->hSemaphore;
			c->quit = true;
			ReleaseSemaphore(hSem, 100, NULL); ///
			printf("** Close Running PushStream [%s]\n", c->stream_url.c_str());
			clients.erase(itr_client ); ///

			/////

		}
		/////////
	}
	else if (itr_state != wait_urls.end()) { // in wait_urls
		if (url->stream_state >= STREAM_STATE_RUN_ALL && url->stream_state <= STREAM_STATE_PAUSE) {//重新创建
			///
			stream_url_state_t* u = *itr_state;

			stream_client* n = new stream_client(this, u->url, NULL);
			n->unique_id = u->unique_id;
			n->stream_state = url->stream_state;
			if (n->stream_state == STREAM_STATE_PAUSE) {
				n->last_pause_time = av_gettime(); ///
			}

			////
			DWORD tid;
			n->hThread = CreateThread(NULL, 0, stream_client::thread, n, 0, &tid);

			clients.push_back(n); ////
			printf("*** Recreate Run PushStream [%s]\n", u->url );
			
			////
			wait_urls.erase(itr_state);
			free(u);

			//////
		}
		else { /// 删除或者停止
			///
			if (url->stream_state != STREAM_STATE_STOP) {
				stream_url_state_t* u = *itr_state;
				wait_urls.erase(itr_state);
				free(u);
				////
			}
			else {
				printf("alread stopped.\n");
			}
			//////
		}
	}
	else if (is_url) { // new 
		///
		if (wait_urls.size() + clients.size() > STREAM_PUSH_MAX_URLS ) { //队列中推流不宜过多，可以手动调节这个值
			Unlock();
			printf("StreamPush Queue Have Much Too.\n");
			return -1;
		}
		///
		while (1) {
			++this->unique_id; if (this->unique_id < 1) this->unique_id = 1;
			///
			bool is_find = false;
			for (list<stream_client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
				stream_client*c = *it;
				if (c->unique_id == this->unique_id) {
					is_find = true;
					break;
				}
			}
			for (list<stream_url_state_t*>::iterator it = wait_urls.begin(); it != wait_urls.end(); ++it) {
				if ((*it)->unique_id == this->unique_id) {
					is_find = true;
					break;
				}
			}
			///
			if (!is_find)break;
		}

		/////
		if (url->stream_state >= STREAM_STATE_RUN_ALL && url->stream_state <= STREAM_STATE_PAUSE) {//重新创建
			///
			stream_client* n = new stream_client(this, url->url, NULL);
			n->unique_id = this->unique_id;
			n->stream_state = url->stream_state;
			if (n->stream_state == STREAM_STATE_PAUSE) {
				n->last_pause_time = av_gettime(); ///
			}

			////
			DWORD tid;
			n->hThread = CreateThread(NULL, 0, stream_client::thread, n, 0, &tid);

			clients.push_back(n); ////
			printf("*** New Run PushStream [%s]\n", url->url);

			/////
		}
		else if (url->stream_state == STREAM_STATE_STOP) {
			////
			int sz = strlen(url->url) + 1 + STREAM_URL_STATE_HEADEZR_SIZE;
			stream_url_state_t* u = (stream_url_state_t*)malloc(sz);
			u->stream_state = url->stream_state;
			u->total_size = sz;
			u->unique_id = this->unique_id;
			strcpy(u->url, url->url);
			/////
			wait_urls.push_back(u);
			printf("*** new push wait Url queue [%s]\n", url->url );
		}
		else {

		}
		/////
	}
	else {
		////
		printf("Not Found URL for state.\n");
		ret = -2; 
	}
	//

    /////
	Unlock();

	return ret;
}
int stream_manager::query( bool is_network_order, stream_url_state_t** p_array, int* p_count, int* p_size )
{
	if (!p_array || !p_count || !p_size )return -1;
	int count = 0;
	*p_array = NULL;
	*p_count = *p_size = 0;
	///
	Lock();
	int size = 0;
	list<stream_client*>::iterator m; 
	list<stream_url_state_t*>::iterator k;
	for (m = clients.begin(); m != clients.end(); ++m) {
		stream_client*c = *m;
		size += STREAM_URL_STATE_HEADEZR_SIZE + c->stream_url.length() + 1;
	}
	for (k = wait_urls.begin(); k != wait_urls.end(); ++k) {
		stream_url_state_t*u = *k;
		size += STREAM_URL_STATE_HEADEZR_SIZE + strlen(u->url) + 1;
	}
	if (size == 0) {
		Unlock();
		return -1;
	}
	/////
	char* ptr = (char*)malloc(size);
	stream_url_state_t* sus = (stream_url_state_t*)ptr;
	for (m = clients.begin(); m != clients.end(); ++m) {
		stream_client*c = *m;
		int sz = STREAM_URL_STATE_HEADEZR_SIZE + c->stream_url.length() + 1;
		sus->total_size = is_network_order ? htonl(sz):sz;
		sus->stream_state = is_network_order ? htonl(c->stream_state):c->stream_state;
		sus->unique_id = is_network_order ? htonl(c->unique_id):c->unique_id;
		strcpy(sus->url, c->stream_url.c_str());
		////
		sus = (stream_url_state_t*)( (char*)sus + sz );
		count++;
	}
	for (k = wait_urls.begin(); k != wait_urls.end(); ++k) {
		stream_url_state_t*u = *k;
		int sz = STREAM_URL_STATE_HEADEZR_SIZE + strlen(u->url) + 1;
		sus->total_size = is_network_order ? htonl(sz):sz;
		sus->stream_state = is_network_order ?htonl(u->stream_state):u->stream_state;
		sus->unique_id = is_network_order ? htonl(u->unique_id):u->unique_id;
		strcpy(sus->url, u->url);
		/////
		sus = (stream_url_state_t*)((char*)sus + sz);
		count++;
	}

	Unlock();

	*p_array = (stream_url_state_t*)ptr;
	*p_count = count;
	*p_size = size;

	return 0;
}
int stream_manager::query(int* p_all, int* p_video, int*p_audio, int*p_pause, int* p_stop)
{
	int all = 0, vid = 0, aud = 0, pause = 0, stop = 0;
	Lock();
	for (list<stream_client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
		stream_client*c = *it;
		switch (c->stream_state) {
		case STREAM_STATE_RUN_ALL:all++; break;
		case STREAM_STATE_RUN_VIDEO:vid++; break;
		case STREAM_STATE_RUN_AUDIO:aud++; break;
		case STREAM_STATE_PAUSE:pause++; break;
			///
		}
	}
	stop = wait_urls.size();
	Unlock();
	*p_all = all;
	*p_video = vid;
	*p_audio = aud;
	*p_pause = pause;
	*p_stop = stop;
	return 0;
}
///////////////////

extern "C" void* stream_push_create()
{
	////
	return new stream_manager();
}
extern "C" void stream_push_destroy(void* handle)
{
	stream_manager* h = (stream_manager*)handle;
	if (!h)return;
	///
	h->destroy();
	delete h;
}

extern "C" int stream_push_post_encoded_frame(void* handle, stream_frame_packet_t* frame)
{
	stream_manager* h = (stream_manager*)handle;
	if (!h)return -1;
	///
	return h->post(frame);
}
extern "C" int stream_push_modify_url(void* handle, stream_url_state_t* u)
{
	stream_manager* h = (stream_manager*)handle;
	if (!h)return -1;
	////
	return h->modify(u);
}

extern "C" int stream_push_add_url(void* handle, const char* url)
{
	stream_manager* h = (stream_manager*)handle;
	if (!h)return -1;
	////
	char buf[16 * 1024];
	stream_url_state_t* u = (stream_url_state_t*)buf;
	u->total_size = STREAM_URL_STATE_HEADEZR_SIZE + strlen(url) + 1;
	u->unique_id = 0;
	u->stream_state = STREAM_STATE_RUN_ALL;
	strcpy(u->url, url);
	////
	return h->modify(u);
}
extern "C" int stream_push_remove_url(void* handle, const char* url)
{
	stream_manager* h = (stream_manager*)handle;
	if (!h)return -1;
	////
	char buf[16 * 1024];
	stream_url_state_t* u = (stream_url_state_t*)buf;
	u->total_size = STREAM_URL_STATE_HEADEZR_SIZE + strlen(url) + 1;
	u->unique_id = 0;
	u->stream_state = STREAM_STATE_DELETE;
	strcpy(u->url, url);
	///
	return h->modify(u);
}

extern "C" int stream_push_query_url(void* handle, int is_network_order, 
	stream_url_state_t** p_array, int* p_count, int* p_size )
{
	stream_manager* h = (stream_manager*)handle;
	if (!h)return -1;
	if (!p_array || !p_count)return -1;
	///
	int ret = h->query(is_network_order, p_array, p_count, p_size );

	return ret;
}
extern "C" int stream_push_query_state(void* handle, 
	int* p_all, int* p_video, int*p_audio, int*p_pause, int* p_stop)
{
	stream_manager* h = (stream_manager*)handle;
	if (!h)return -1;
    ///
	return h->query(p_all,p_video,p_audio,p_pause,p_stop);
}
extern "C" void stream_push_free_memory(void* ptr)
{
	if(ptr)free(ptr);
}


///
///////////////////
