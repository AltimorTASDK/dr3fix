#pragma once

class PlayerInput {
private:
	char pad0000[0x90];
public:
	int can_move;
private:
	char pad0094[0xB8 - 0x94];
public:
	float min_pitch;
	float max_pitch;
private:
	char pad00C0[0x21C - 0xC0];
public:
	int is_in_control;
	int current_mode;
};

class InputDevice {
private:
	char pad0000[0x8];
public:
	float sensitivity;
private:
	char pad000C[0x1C - 0x0C];
public:
	int is_focused;
private:
	char pad0020[0x2C - 0x20];
public:
	float yaw_input;
	float pitch_input;
private:
	char pad009C[0x9C - 0x28];
public:
	int showing_cursor;
};

class CameraValues {
private:
	char pad0000[0xDC];
public:
	float fov;
};

class SpMouse {
private:
	char pad0000[0xC8];
public:
	float raw_mouse_x;
	float raw_mouse_y;
};

class Sp2RenderTarget {
private:
	char pad0000[0x38];
public:
	int width;
	int height;
};

class SpGraphicServer {
private:
	char pad0000[0x68];
public:
	Sp2RenderTarget *display_color;
private:
	char pad0070[0x88 - 0x70];
public:
	Sp2RenderTarget *back_buffer;
private:
	char pad0090[0xBB0 - 0x90];
public:
	float refresh_rate;
};
