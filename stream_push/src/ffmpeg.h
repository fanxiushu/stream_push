/// by fanxiushu 2018-07-01 
///把ffmpeg所有初始化集中到一起， 所属 xdisp_virt 项目

#pragma once

/// ffmpeg
extern "C" {
#include <libswscale\swscale.h>
#include <libavcodec\avcodec.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>

	//audio
	extern AVCodec ff_flac_encoder;
	extern AVCodec ff_mp2_encoder;
	extern AVCodec ff_ac3_encoder;
	extern AVCodec ff_libfdk_aac_encoder;

	//video
	extern AVCodec ff_libx264_encoder;
	extern AVCodec ff_mpeg4_encoder;
	extern AVCodec ff_mpeg2video_encoder;
	extern AVCodec ff_mpeg1video_encoder;
	
	////muxer
	extern AVOutputFormat ff_flv_muxer;       // RTMP
	extern AVOutputFormat ff_rtsp_muxer;      // RTSP
	extern AVOutputFormat ff_rtp_muxer;
	extern AVOutputFormat ff_mp4_muxer;       // MP4
	extern AVOutputFormat ff_matroska_muxer;  // MKV
}

#pragma comment(lib,"libswscale\\libswscale.a")
#pragma comment(lib,"libavutil\\libavutil.a")
#pragma comment(lib,"libavcodec\\libavcodec.a")
#pragma comment(lib,"libavformat\\libavformat.a")
#pragma comment(lib,"libswresample\\libswresample.a")
#pragma comment(lib,"Secur32.lib")
#pragma comment(lib,"ws2_32.lib")

//////fdk-aac
#pragma comment(lib,"../fdk-aac/fdk-aac.lib")

///x264
#pragma comment(lib,"../x264/libx264.lib")

///这么初始化，是为了尽量减少静态编译的文件体积
static void ffmpeg_init()
{
//	av_register_all();
//	avdevice_register_all();
	
    ////audio
//	avcodec_register(&ff_flac_encoder);
//	avcodec_register(&ff_mp2_encoder);
//	avcodec_register(&ff_ac3_encoder);
	avcodec_register(&ff_libfdk_aac_encoder);

	/// video
	avcodec_register(&ff_libx264_encoder);
//	avcodec_register(&ff_mpeg4_encoder);
//	avcodec_register(&ff_mpeg2video_encoder);
//	avcodec_register(&ff_mpeg1video_encoder);
	
	//// stream container
	av_register_output_format(&ff_flv_muxer); // RTMP, flv
	av_register_output_format(&ff_rtp_muxer); // RTSP
	av_register_output_format(&ff_rtsp_muxer); // RTSP
	av_register_output_format(&ff_mp4_muxer);      /// MP4
	av_register_output_format(&ff_matroska_muxer); /// MKV

	avformat_network_init(); //

}

