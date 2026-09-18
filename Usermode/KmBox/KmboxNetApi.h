#pragma once

namespace KMBOXNET {
	bool ConnectKMBox(const char* uuid, const char* ip, int port);
}

int kmNet_init(char* ip, char* port, char* mac);
int kmNet_monitor(short port);
int kmNet_mouse_move(short x, short y);
int kmNet_mouse_move_auto(int x, int y, int time_ms);
int kmNet_mouse_move_beizer(int x, int y, int ms, int x1, int y1, int x2, int y2);
int kmNet_mouse_left(int isdown);
int kmNet_mouse_right(int isdown);
int kmNet_mouse_middle(int isdown);
int kmNet_mouse_wheel(int wheel);
int kmNet_monitor_mouse_left();
int kmNet_monitor_mouse_right();
int kmNet_monitor_mouse_middle();
int kmNet_monitor_keyboard(short vkey);
