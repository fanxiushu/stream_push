////fanxiushu 2018-07-11 调用stream_push.dll例子

#include <Windows.h>
#include <stdio.h>

#include "../bin/stream_push.h"
#pragma comment(lib,"../bin/stream_push.lib")

struct cap_screen_t
{
	HDC memdc;
	HBITMAP hbmp;
	unsigned char* buffer;
	int            length;

	int width;
	int height;
	int bitcount;
};

int init_cap_screen(struct cap_screen_t* sc)
{
	DEVMODE devmode;
	BOOL bRet;
	BITMAPINFOHEADER bi; 

	memset(&devmode, 0, sizeof(DEVMODE));
	devmode.dmSize = sizeof(DEVMODE);
	devmode.dmDriverExtra = 0;
	bRet = EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
	sc->width = devmode.dmPelsWidth;
	sc->height = devmode.dmPelsHeight;
	sc->bitcount = devmode.dmBitsPerPel;
	memset(&bi, 0, sizeof(bi));
	bi.biSize = sizeof(bi);
	bi.biWidth = sc->width;
	bi.biHeight = -sc->height; //从上朝下扫描
	bi.biPlanes = 1;
	bi.biBitCount = sc->bitcount; //RGB
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	HDC hdc = GetDC(NULL); //屏幕DC
	sc->memdc = CreateCompatibleDC(hdc);
	sc->buffer = NULL;
	sc->hbmp = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&sc->buffer, NULL, 0);
	ReleaseDC(NULL, hdc);
	SelectObject(sc->memdc, sc->hbmp); ///
	sc->length = sc->height* (((sc->width*sc->bitcount / 8) + 3) / 4 * 4);
	return 0;
}
int blt_cap_screen(struct cap_screen_t* sc)
{
	HDC hdc = GetDC(NULL);
	BitBlt(sc->memdc, 0, 0, sc->width, sc->height, hdc, 0, 0, SRCCOPY | CAPTUREBLT);
	ReleaseDC(NULL, hdc);
	return 0;
}

BOOL quit = 0;

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
	quit = TRUE;

	return TRUE;
}
int main(int argc, char** argv)
{
	struct cap_screen_t sc;
	void* strm; byte* out;
	
	SetConsoleCtrlHandler(HandlerRoutine, TRUE);

	init_cap_screen(&sc);
	out = (byte*)malloc(sc.length); ///

	strm = stream_push_create();
	
	///
	stream_push_add_url(strm, "rtmp://192.168.88.33/hls/test");
	stream_push_add_url(strm, "rtsp://192.168.88.33/test");
	stream_push_add_url(strm, "d:\\t.mp4");
	/////

	while (!quit) {
		int ret;
		struct stream_frame_packet_t frame;
		
		///
		blt_cap_screen(&sc);
		ret = simple_video_encode(sc.buffer, sc.length, sc.width, sc.height, out, sc.length); ///
		if (ret <= 0) {
			printf("Encoder err\n"); Sleep(1000); continue;
		}

		///
		frame.type = VIDEO_STREAM;
		frame.buffer = out;
		frame.length = ret;

		stream_push_post_encoded_frame(strm, &frame); ///投递

		////
		Sleep(40); /// 每40毫秒采集一次 
		
	}
	printf("Exit...\n");
	///
	stream_push_remove_url(strm, "d:\\t.mp4"); //必须调用，确保写到本地的视频文件能正常使用
	stream_push_destroy(strm);
	/////
	return 0;
}

