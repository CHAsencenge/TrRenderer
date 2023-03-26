#pragma once
#include <Windows.h>

typedef struct window
{
	HWND h_window;
	HDC mem_dc;
	HBITMAP bm_old;
	HBITMAP bm_dib;
	unsigned char* window_fb;
	int width;
	int height;
	char keys[512];
	char buttons[2];	//left button¡ª0£¬ right button¡ª1
	int is_close;
	// mouse_t mouse_info;
}window_t;

extern window_t* window;

int window_init(int width, int height, const wchar_t* title);
int window_destroy();
void window_draw(unsigned char* framebuffer);
void msg_dispatch();
// vec2 get_mouse_pos();
float platform_get_time(void);