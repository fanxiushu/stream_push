////by fanxiushu 2018-07-11 简单的编码

#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <string>

#include "ffmpeg.h"
#include "stream_push.h"

struct vid_enc
{
	AVCodecContext* avctx;
	AVFrame* avframe;
	//
	int width;
	int height;
	///
	vid_enc() {

	}
	~vid_enc() {
		if (avctx) {
			avcodec_close(avctx);
			avcodec_free_context(&avctx);
		}
		if (avframe) {
			av_free(avframe->data[0]); // 之前是 av_free(&avframe->data[0]) BUG 2017-12-11
			av_frame_free(&avframe);
		}
		width = height = 0;
	}
	int init(int w, int h) {
		width = w;
		height = h;
		///
		AVCodec*id = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (!id) {
			printf("avcodec_find_encoder not found H264");
			return -1;
		}
		AVCodecContext* ctx = avcodec_alloc_context3(id);
		if (!ctx)return -1;

		ctx->max_b_frames = 0; //不要B针
		ctx->gop_size = 30;
		ctx->keyint_min = 1;// axp->keyint_max;
		ctx->width = w;
		ctx->height = h;
		ctx->codec = id;
		ctx->pix_fmt = AV_PIX_FMT_YUV420P;//*id->pix_fmts; // 
		ctx->time_base.den = 25;
		ctx->time_base.num = 1;

		int bit_rate = 4 * 1024 * 1024; //// 4Mbps
		ctx->bit_rate = bit_rate; 
		ctx->rc_max_rate = bit_rate;
		ctx->bit_rate_tolerance = bit_rate;
		ctx->rc_buffer_size = bit_rate;
		ctx->rc_initial_buffer_occupancy = bit_rate * 3 / 4;

		AVDictionary *options = NULL;
		av_dict_set(&options, "tune", "zerolatency", 0); /////

		int ret = avcodec_open2(ctx, id, &options);

		if (ret < 0) {
			printf("*** avcodec_open2 err=%d, not open hardware accel.\n", ret);
			avcodec_free_context(&ctx);
			return -1;
		}
		AVFrame* frame = av_frame_alloc();
		if (!frame) {
			return -1;
		}
		frame->width = w;
		frame->height = h;
		frame->format = ctx->pix_fmt;
		frame->pts = 0;

		ret = av_image_alloc(frame->data, frame->linesize, ctx->width, ctx->height,
			ctx->pix_fmt, 32);
		if (ret < 0) {
			printf("*** av_image_alloc err=%d.\n", ret);
			av_frame_free(&frame);
			return -1;
		}
		///
		this->avctx = ctx;
		this->avframe = frame;

		return 0;
	}

	int encode(byte* rgb32, int rgb32_len, unsigned char* out, int out_len) {
		SwsContext* sws_ctx = NULL;
		AVPixelFormat fmt = AV_PIX_FMT_BGRA, fmt2 = AV_PIX_FMT_YUV420P; ///
		sws_ctx = sws_getContext(width, height, fmt, width, height, fmt2, SWS_BILINEAR, NULL, NULL, NULL);
		if (!sws_ctx)return -1;
		AVPicture spic ;
		int r1 = avpicture_fill(&spic, (uint8_t*)rgb32, fmt, width, height);
		int ret = sws_scale(sws_ctx, spic.data, spic.linesize, 0, height, this->avframe->data , this->avframe->linesize);
		sws_freeContext(sws_ctx);
		////
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = out;
		pkt.size = out_len;

		int got_frame; int data_len = 0;
		ret = avcodec_encode_video2(this->avctx, &pkt, this->avframe, &got_frame);
		if (ret < 0) {
			printf("*** avcodec_encode_video2 err=%d\n", ret);
			av_packet_unref(&pkt);
			return -1;
		}
		if (got_frame) {
			data_len = pkt.size; //printf("OK size=%d\n", data_len);
		}

		this->avframe->pts++; if (this->avframe->pts < 0) this->avframe->pts = 0;//
		av_packet_unref(&pkt);

		return data_len;

	}
};
extern "C" int simple_video_encode(unsigned char* rgb32, int rgb32_len, 
	int width, int height, 
	unsigned char* out, int out_len)
{
	static vid_enc* enc = NULL;
	if (!enc || enc->width != width || enc->height != height) {
		if (enc)delete enc;
		enc = new vid_enc;
		int r = enc->init(width, height);
		if (r < 0) {
			printf("init encoder err\n");
			delete enc;
			enc = NULL;
			return -1;
		}
	}
	/////
	int len = enc->encode(rgb32, rgb32_len, out, out_len);
	///////
	return len;
}


