#include "dr3.h"
#include "config_file.h"
#include <cstdint>
#include <fstream>
#include <memory>
#include <Windows.h>
#include <Psapi.h>
#include <d3d11.h>

std::ofstream log_file("dr3fix.log");
config_file cfg("dr3fix.cfg");

extern "C" {
	void *addr_stack_cookie;

	void *addr_ExecuteWindowMessage;
	void hook_ExecuteWindowMessage_wrap();

	void *addr_HandlePlayerInput;
	void hook_HandlePlayerInput_wrap();

	void *addr_ProcessMouseInput;
	void hook_ProcessMouseInput_wrap();

	void *addr_UpdateCameraFreeRoam;
	void hook_UpdateCameraFreeRoam();

	void *addr_RestoreD3DDevice;
	void hook_RestoreD3DDevice_wrap();
}

using GetActiveInputDevice_t = InputDevice*(*)();
GetActiveInputDevice_t GetActiveInputDevice;

int *IsInMenu;
SpGraphicServer **GraphicServer;
ID3D11Device **D3DDevice;

auto mouse_x = 0L;
auto mouse_y = 0L;
bool mouse_buttons[5] = { false };

extern "C" float fov = cfg.get_float("fov", 60.F);

void register_mouse(HWND hwnd)
{
	RAWINPUTDEVICE rid;
	rid.usUsagePage = 1;
	rid.usUsage = 2;
	rid.dwFlags = 0;
	rid.hwndTarget = hwnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

void update_mouse(const RAWMOUSE &mouse)
{
	mouse_x += mouse.lLastX;
	mouse_y += mouse.lLastY;

	// Check for RI_MOUSE_BUTTON_1_DOWN etc
	for (auto i = 0; i < 5; i++) {
		if (mouse.usButtonFlags & (1 << (i * 2)))
			mouse_buttons[i] = true;
		else if (mouse.usButtonFlags & (1 << (i * 2 + 1)))
			mouse_buttons[i] = false;
	}
}

void handle_raw_input(HWND hwnd, HRAWINPUT handle)
{
	// Get required buffer size
	UINT buf_size;
	if (GetRawInputData(handle, RID_INPUT, nullptr, &buf_size, sizeof(RAWINPUTHEADER)) != 0) {
		log_file << "Failed to get required size of raw input buffer." << std::endl;
		return;
	}

	auto input_buf = std::make_unique<char[]>(buf_size);
	auto *input = (RAWINPUT*)input_buf.get();

	if (GetRawInputData(handle, RID_INPUT, input, &buf_size, sizeof(RAWINPUTHEADER)) != buf_size) {
		log_file << "Failed to read raw input data." << std::endl;
		return;
	}

	update_mouse(input->data.mouse);
}

extern "C" bool hook_ExecuteWindowMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (!cfg.get_bool("raw_input", false))
		return false;

	// Has to be repeatedly registered because of the Steam overlay
	// unregistering it after shift tab
	register_mouse(hwnd);

	if (msg == WM_ACTIVATE && (wparam & 0xFFFF) != WA_INACTIVE) {
		// Manually handle window focusing because the focusing click
		// isn't received by raw input
		InputDevice *input_device = GetActiveInputDevice();
		if (input_device != nullptr)
			input_device->is_focused = true;
	}

	if (msg != WM_INPUT)
		return false;

	handle_raw_input(hwnd, (HRAWINPUT)lparam);
	return true;
}

extern "C" void hook_HandlePlayerInput(PlayerInput *player_input)
{
	InputDevice *input_device = GetActiveInputDevice();
	input_device->sensitivity = cfg.get_float("sensitivity", 5.F) / 100.F;

	float pitch_limit = cfg.get_float("pitch_limit", 20.F);
	player_input->min_pitch = -pitch_limit;
	player_input->max_pitch = pitch_limit;
}

extern "C" void hook_ProcessMouseInput(SpMouse *mouse, int *buttons, int *new_x, int *new_y)
{
	if (!cfg.get_bool("raw_input", false))
		return;

	Sp2RenderTarget *display_color = (*GraphicServer)->display_color;
	if (display_color != nullptr) {
		// Mouse movement is calculated by comparing new_x/new_y
		// with the old raw_mouse_x/raw_mouse_y
		float center_x = (float)display_color->width / 2;
		float center_y = (float)display_color->height / 2;
		*new_x = (int)(mouse_x + center_x + .5F);
		*new_y = (int)(mouse_y + center_y + .5F);
		mouse->raw_mouse_x = center_x;
		mouse->raw_mouse_y = center_y;
	}

	*buttons = 0;
	for (auto i = 0; i < 5; i++)
		*buttons |= mouse_buttons[i] ? (1 << i) : 0;

	mouse_x = 0;
	mouse_y = 0;
}

extern "C" void hook_RestoreD3DDevice_pre(SpGraphicServer *graphic_server)
{
	graphic_server->refresh_rate = cfg.get_float("refresh_rate", 60.F);
}

extern "C" void hook_RestoreD3DDevice_post()
{
	if (!cfg.get_bool("low_latency", false))
		return;

	log_file << "Setting DXGI max frame latency to 1" << std::endl;
	IDXGIDevice1 *dxgi_device;
	if ((*D3DDevice)->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgi_device) == S_OK)
		dxgi_device->SetMaximumFrameLatency(1);
	else
		log_file << "Failed to get IDXGIDevice" << std::endl;
}

void patch_code(void *address, const void *src, size_t size)
{
	DWORD old_protect;
	VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_protect);
	memcpy(address, src, size);
	VirtualProtect(address, size, old_protect, &old_protect);
}

template<size_t N>
void patch_code(void *address, const char (&src)[N])
{
	patch_code(address, src, N - 1);
}

void jmp_hook(void *address, const void *target)
{
	// mov rax, target
	// jmp rax
	char bytes[] = "\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0";
	*(const void**)(bytes + 2) = target;
	patch_code(address, bytes);
}

void get_addresses(uintptr_t base)
{
	addr_stack_cookie = (void*)(base + 0x907F60);
	addr_ExecuteWindowMessage = (void*)(base + 0x66D340);
	addr_HandlePlayerInput = (void*)(base + 0x40A120);
	addr_ProcessMouseInput = (void*)(base + 0x57B490);
	addr_UpdateCameraFreeRoam = (void*)(base + 0x409010);
	addr_RestoreD3DDevice = (void*)(base + 0x5B0600);

	GetActiveInputDevice = (GetActiveInputDevice_t)(base + 0x3C19D0);

	IsInMenu = (int*)(base + 0xC24FF0);
	GraphicServer = (SpGraphicServer**)(base + 0xD20AA8);
	D3DDevice = (ID3D11Device**)(base + 0xD22B30);
}

void disable_look_smoothing(uintptr_t base)
{
	// stop pitch drift with lerping disabled
	patch_code((void*)(base + 0x40A56E), "\xEB");

	// stop yaw drift with lerping disabled
	patch_code((void*)(base + 0x40A1E8), "\xEB");
	patch_code((void*)(base + 0x40A1F8), "\xEB");

	// disable yaw lerping
	patch_code((void*)(base + 0x40A236),
		"\x90\x90\x90\x90"
		"\x90\x90\x90\x90\x90"
		"\x90\x90\x90\x90");

	// disable pitch lerping
	patch_code((void*)(base + 0x40A5A4),
		"\x90\x90\x90\x90"
		"\x90\x90\x90\x90\x90"
		"\x90\x90\x90\x90");

	// disable yaw negative acceleration
	patch_code((void*)(base + 0x3C303E), "\xEB");
	patch_code((void*)(base + 0x3C3048), "\xEB");

	// disable pitch negative acceleration
	patch_code((void*)(base + 0x3C321D), "\xEB");
	patch_code((void*)(base + 0x3C3227), "\xEB");
}

void apply_patches(uintptr_t base)
{
	if (!cfg.get_bool("auto_center", true)) {
		// disable auto centering
		patch_code((void*)(base + 0x40A5EE), "\xEB");
	}

	if (!cfg.get_bool("look_smoothing", true)) {
		// disable mouse smoothing
		disable_look_smoothing(base);
	}

	if (cfg.get_bool("fix_x_sensitivity", false)) {
		// 1:1 pitch/yaw sensitivity
		patch_code((void*)(base + 0x40A1EC), "\x90\x90\x90\x90\x90\x90\x90\x90");
	}

	if (!cfg.get_bool("look_fps_scaling", true)) {
		// disable framerate affecting mouse input
		patch_code((void*)(base + 0x3C9465),
			"\x90\x90\x90\x90"
			"\x90\x90\x90\x90\x90\x90\x90\x90");
	}

	if (cfg.get_bool("low_latency", false)) {
		// use swap chain BufferCount of 1
		patch_code((void*)(base + 0x5B08F9), "\x01");
	}
}

uintptr_t get_base_address()
{
	const auto module = GetModuleHandle(nullptr);
	if (module == nullptr)
		return 0;

	MODULEINFO info;
	GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info));
	return (uintptr_t)info.lpBaseOfDll;
}

extern "C" {
	extern FARPROC orig_SteamAPI_Init;
}

extern "C" bool SteamAPI_Init()
{
	log_file << "Raw input:             " << cfg.get_bool("raw_input", false) << std::endl;
	log_file << "Sensitivity:           " << cfg.get_float("sensitivity", 5.F) << std::endl;
	log_file << "Pitch limit:           " << cfg.get_float("pitch_limit", -1.F) << std::endl;
	log_file << "Camera auto centering: " << cfg.get_bool("auto_center", true) << std::endl;
	log_file << "Look smoothing:        " << cfg.get_bool("look_smoothing", true) << std::endl;
	log_file << "Fix X sensitivity:     " << cfg.get_bool("fix_x_sensitivity", false) << std::endl;
	log_file << "Look FPS scaling:      " << cfg.get_bool("look_fps_scaling", true) << std::endl;
	log_file << "Field of view:         " << cfg.get_float("fov", 60.F) << std::endl;
	log_file << "Low latency mode:      " << cfg.get_bool("low_latency", false) << std::endl;
	log_file << "Refresh rate:          " << cfg.get_float("refresh_rate", 60.F) << std::endl;

	const auto base = get_base_address();
	get_addresses(base);
	apply_patches(base);

	jmp_hook(addr_ExecuteWindowMessage, hook_ExecuteWindowMessage_wrap);
	jmp_hook(addr_HandlePlayerInput, hook_HandlePlayerInput_wrap);
	jmp_hook(addr_ProcessMouseInput, hook_ProcessMouseInput_wrap);
	jmp_hook(addr_UpdateCameraFreeRoam, hook_UpdateCameraFreeRoam);
	jmp_hook(addr_RestoreD3DDevice, hook_RestoreD3DDevice_wrap);

	log_file << "Initialized" << std::endl;

	return orig_SteamAPI_Init();
}