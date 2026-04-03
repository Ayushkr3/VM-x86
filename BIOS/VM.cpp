#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <Windows.h>
#include "io.h"
#include "memory.h"
#include "CPU.h"
#include <thread>
#include "Display.h"
#include "PIC.h"
#include "PCI.h"
#include "Whpx.h"
#include "SDL3/SDL.h"
WHPX* hypervisior;
PCISystemBus pci;
DisplayAdapter* Display;
static std::vector<uint8_t> load_bios(const char* path) {
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) {
		exit(1);
	}
	size_t sz = f.tellg();
	f.seekg(0);
	std::vector<uint8_t> buf(sz);
	f.read(reinterpret_cast<char*>(buf.data()), sz);
	return buf;
}
void Poll() {
	SDL_Event event;
	while (running.load()) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
				hypervisior->StopVP();
			}
			else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
				SDL_SetWindowRelativeMouseMode(Display->window, true);
			}
			else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
				SDL_SetWindowRelativeMouseMode(Display->window, false);
			}
			else if (event.type == SDL_EVENT_MOUSE_MOTION) {
				ud.kbd->mouse.send_delta(event.motion.xrel, -event.motion.yrel);
				ud.kbd->flush_mouse_output();
			}
			else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
				event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
				bool pressed = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
				switch (event.button.button) {
				case SDL_BUTTON_LEFT:   ud.kbd->mouse.left_btn = pressed; break;
				case SDL_BUTTON_RIGHT:  ud.kbd->mouse.right_btn = pressed; break;
				case SDL_BUTTON_MIDDLE: ud.kbd->mouse.middle_btn = pressed; break;
				}
				ud.kbd->mouse.send_click(
					ud.kbd->mouse.left_btn,
					ud.kbd->mouse.middle_btn,
					ud.kbd->mouse.right_btn
				);
				ud.kbd->flush_mouse_output();
			}
			else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
				event.type == SDL_EVENT_MOUSE_BUTTON_UP) {

				bool pressed = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
				bool left_btn = false;
				bool right_btn = false;
				bool Middle_btn = false;
				switch (event.button.button) {
				case SDL_BUTTON_LEFT:   left_btn = pressed; break;
				case SDL_BUTTON_RIGHT: right_btn = pressed; break;
				case SDL_BUTTON_MIDDLE: Middle_btn = pressed; break;
				}

				ud.kbd->mouse.send_click(left_btn, right_btn, Middle_btn);
				ud.kbd->flush_mouse_output();
			}
			else if (event.type == SDL_EVENT_KEY_DOWN) {
				SDL_Keycode code = event.key.key;
				if (code == SDLK_ESCAPE) {
					SDL_SetWindowRelativeMouseMode(Display->window, false);
				}
				auto it = keymap.find(code);
				if (it == keymap.end()) continue;
				const KeyEntry& entry = it->second;

				if (entry.extended) {
					ud.kbd->send_scancode(Scancode1::Extended::PREFIX);
				}
				ud.kbd->send_scancode(entry.scancode);

			}
			else if (event.type == SDL_EVENT_KEY_UP) {
				SDL_Keycode code = event.key.key;
				auto it = keymap.find(code);
				if (it == keymap.end()) continue;

				const KeyEntry& entry = it->second;

				if (entry.extended) {
					ud.kbd->send_scancode(Scancode1::Extended::PREFIX);
				}
				ud.kbd->send_scancode(Scancode1::BREAK(entry.scancode));
			}
		}
	}
}
void EmulationLoop() {
	hypervisior->RunVP();
}
int main(int argc, char* argv[]) {
	const char* bios_path = "D:/windows nt/bios_nd.bin";
	const char* vgabios_path = "D:/windows nt/vgabios.bin";
	std::string isopath = "D:/windows nt/windows xp32_d.iso";
	auto bios = load_bios(bios_path);
	auto vgabios = load_bios(vgabios_path);
	fw_cfg.init(RAM_SIZE);
	InitMemory();
	Display = new DisplayAdapter;
	InitIO(&pci, WHPX::ThunkRaiseInterrupt, isopath);
	size_t offset = BIOS_SIZE - bios.size();
	memcpy((char*)RAM + BIOS_BASE + offset, bios.data(), bios.size());
	memcpy((char*)VGA_ROM, vgabios.data(), vgabios.size());
	ud.pic = &pic;
	ud.sb = &pci;
	Display->vgaC = ud.sb->vgaC;
	Display->vgaC->vga->adapter = Display;
	hypervisior = new WHPX(&ud);
	ud.whpxCtx = hypervisior;
	std::thread emulation(EmulationLoop);
	std::thread DisplayT(DisplayAdapter::DisplayThunkUpdateLoop, Display);
	std::thread Time(Timers::TimerThread);
	Poll();
	Time.join();
	emulation.join();
	DisplayT.join();
	delete hypervisior;
	delete Display;
	DeinitMemory();
}