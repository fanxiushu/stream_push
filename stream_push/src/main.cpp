/////fanxiushu 2018-07-11
#include <WinSock2.h>
#include "ffmpeg.h"

BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpReserved)
{

	if (fdwReason == DLL_PROCESS_ATTACH) {
		ffmpeg_init();
	}
	else if (fdwReason == DLL_PROCESS_DETACH) { ///
	}

	///
	return TRUE;
}

