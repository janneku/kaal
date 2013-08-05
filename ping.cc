/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#include "ping.h"
#include <string>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
	#include <netinet/in.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <netdb.h>
	#include <errno.h>
	#define closesocket	close
	#define OS "Linux"
#else
	/* windows */
	#include <winsock.h>
	#undef errno
	#undef EAGAIN
	#define errno	WSAGetLastError()
	#define EAGAIN	WSAEWOULDBLOCK
	#define EINPROGRESS WSAEWOULDBLOCK
	#define OS "Windows"
#endif

extern "C" {
extern const char *version;
}

namespace {

/* Where to send usage statistics to */
const char SERVER[] = "localhost";

#ifdef _WIN32

/* fake window proc */
LRESULT CALLBACK hwnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	return DefWindowProc(hwnd, msg, wparam, lparam);
}
#endif

int set_nonblock(int fd, bool enabled)
{
#ifndef _WIN32
	int flags = fcntl(fd, F_GETFL) & ~O_NONBLOCK;
	if (enabled) {
		flags |= O_NONBLOCK;
	}
	return fcntl(fd, F_SETFL, flags);
#else
	/* windows */
	u_long arg = enabled ? 1 : 0;
	return ioctlsocket(fd, FIONBIO, &arg);
#endif
}

/* Non-blocking resolver for Windows */
hostent *resolve_name(const char *name)
{
#ifndef _WIN32
	return gethostbyname(name);
#else
	/* create dummy window */
	WNDCLASS wc;
	memset(&wc, 0, sizeof wc);
	wc.lpfnWndProc = hwnd_proc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = "DUMMYWINDOW";
	RegisterClass(&wc);

	HWND hwnd = CreateWindow(wc.lpszClassName, "foo", WS_POPUP,
		    0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL);
	assert(hwnd != NULL);

	const UINT WM_RESOLVE_COMPLETE = WM_USER + 1337;

	/* is returned to the caller */
	static char buffer[MAXGETHOSTSTRUCT];

	HANDLE handle = WSAAsyncGetHostByName(hwnd, WM_RESOLVE_COMPLETE,
					      name, buffer, sizeof buffer);
	if (handle == 0) {
		DestroyWindow(hwnd);
		return NULL;
	}
	for (int i = 0; i < 50; ++i) {
		MSG msg;
		if (PeekMessage(&msg, hwnd, WM_RESOLVE_COMPLETE,
				WM_RESOLVE_COMPLETE, PM_REMOVE)) {
			assert(msg.message == WM_RESOLVE_COMPLETE);
			DestroyWindow(hwnd);
			printf("resolve took %d ms: error %d\n", i * 100,
				WSAGETASYNCERROR(msg.lParam));
			if (WSAGETASYNCERROR(msg.lParam) != 0) {
				return NULL;
			}
			return (hostent *) buffer;
		}
		Sleep(100);
	}
	printf("resolve timeout\n");
	WSACancelAsyncRequest(handle);
	DestroyWindow(hwnd);
	return NULL;
#endif
}

}

void ping(int time, int level)
{
#ifdef _WIN32
	static bool wsa_initialized;
	if (!wsa_initialized) {
		WSADATA wsadata;
		if (WSAStartup(MAKEWORD(2, 2), &wsadata)) {
			printf("Unable to initialize winsock\n");
			return;
		}
		wsa_initialized = true;
	}
#endif

	hostent *hp = resolve_name(SERVER);
	if (hp == NULL) {
		printf("Unable to resolve the server\n");
		return;
	}

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr = *(in_addr *) hp->h_addr_list[0];
	sin.sin_port = htons(80);

	set_nonblock(fd, true);

	if (connect(fd, (sockaddr *) &sin, sizeof sin)) {
		if (errno != EINPROGRESS) {
			closesocket(fd);
			printf("Can not connect!\n");
			return;
		}
	}

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	timeval tv = {2, 0};
	if (select(fd + 1, NULL, &fds, NULL, &tv) < 1) {
		closesocket(fd);
		printf("connection timed out\n");
		return;
	}

	char req[256];
	sprintf(req, "POST /kaal?version=%s&os=" OS "&time=%d&level=%d HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n\r\n", version, time, level, SERVER);

	if (send(fd, req, strlen(req), 0) < (int) strlen(req)) {
		closesocket(fd);
		printf("Unable to write request\n");
		return;
	}

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = 2;
	if (select(fd + 1, &fds, NULL, NULL, &tv) < 1) {
		closesocket(fd);
		printf("Reply timeout\n");
		return;
	}
	closesocket(fd);
}
