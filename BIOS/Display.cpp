#include "display.h"
#include "Global.h"
#include <thread>
#include <string>
#include "SDL3/SDL.h"
#include "SDL3/SDL_opengl.h"
#include "font.h"
using namespace std;

SDL_Window* outWindow;
SDL_GLContext glCtx;
SDL_Renderer* renderer;
SDL_Texture* DisplayTexture;
#define DISPLAY_W 720
#define DISPLAY_H 400
#define CURSOR_H 16
#define CURSOR_W 9
void DisplayInit() {
	SDL_Init(SDL_INIT_VIDEO);
	outWindow = SDL_CreateWindow("Video Output", 720, 400, SDL_WINDOW_OPENGL);
	glCtx = SDL_GL_CreateContext(outWindow);
	renderer = SDL_CreateRenderer(outWindow, NULL);
	
	DisplayTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, DISPLAY_W, DISPLAY_H);
}
static uint32_t framebuffer[DISPLAY_W * DISPLAY_H] = { 0 };
void DisplayLoop() {
	char* videoBuffer = RAM + 0xB8000;
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = DISPLAY_W;
	rect.h = DISPLAY_H;
	while (running.load()) {
		for (int row = 0; row < 25; row++) {
			for (int col = 0; col < 80; col++) {
				int offset = (row * 80 + col) * 2;
				uint8_t character = videoBuffer[offset];
				uint8_t attribute = videoBuffer[offset + 1];

				uint32_t fg = vga_palette[attribute & 0x0F];
				uint32_t bg = vga_palette[(attribute >> 4) & 0x07];

				for (int row_px = 0; row_px < 16; row_px++) {
					uint8_t bits = font8x16[character][row_px];

					for (int col_px = 0; col_px < 9; col_px++) {
						bool pixel_on = bits & (1 << (7 - col_px));

						int px = col * 9 + col_px;
						int py = row * 16 + row_px;

						framebuffer[py * DISPLAY_W + px] =
							pixel_on ? fg : bg;
					}
				}
			}
		}
		bool error;
		error = SDL_UpdateTexture(DisplayTexture, NULL, framebuffer, DISPLAY_W * 4);
		error = SDL_RenderClear(renderer);
		error = SDL_RenderTexture(renderer, DisplayTexture, NULL, NULL);
		error = SDL_RenderPresent(renderer);
	}
}
void DisplayDeInit() {

	SDL_DestroyTexture(DisplayTexture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(outWindow);
	SDL_Quit();
}