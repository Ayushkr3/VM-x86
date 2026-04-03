#include "font.h"
#include "Global.h"
#include "Display.h"
#include <iomanip>
#define DISPLAY_W 720
#define DISPLAY_H 400
#define CURSOR_H 16
#define CURSOR_W 9

bool DumpFramebuffer = false;
void DisplayAdapter::set_mode(bool graphical, bool svga) {
    EnterCriticalSection(&cs);

    if (graphical && svga) {
        DisplayUpdate = SVGAUpdate;
    }
    else if (graphical) {
        DisplayUpdate = VideoUpdate;
    }
    else {
        DisplayUpdate = TextUpdate;
    }
    std::fill(framebuffer.begin(), framebuffer.end(), 0);
    LeaveCriticalSection(&cs);
    is_graphical = graphical;
}

void DisplayAdapter::set_size_text(int cols, int rows) {
    
}


void DisplayAdapter::put_char(int row, int col, uint8_t chr, int flags,
    uint32_t bg, uint32_t fg) {
    
}
void DisplayAdapter::update_cursor(int row, int col) {}
void DisplayAdapter::update_cursor_scanline(int start, int end, bool visible) {}
void DisplayAdapter::set_font_bitmap(int height, bool w9, bool dbl,
    bool copy8, uint8_t* plane2, bool dirty) {}
void DisplayAdapter::set_font_page(int a, int b) {}
void DisplayAdapter::clear_screen() {
    std::fill(framebuffer.begin(), framebuffer.end(), 0);
}
DisplayAdapter::DisplayAdapter() {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Video Output", 720, 400, SDL_WINDOW_OPENGL);
    glCtx = SDL_GL_CreateContext(window);
    renderer = SDL_CreateRenderer(window, NULL);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, DISPLAY_W, DISPLAY_H);
    DisplayUpdate = TextUpdate;
    framebuffer.resize(1024 * 768);
    InitializeCriticalSection(&cs);
}
DisplayAdapter::~DisplayAdapter() {
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
void DisplayAdapter::DisplayUpdateLoop() {
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = DISPLAY_W;
    rect.h = DISPLAY_H;
    while (running.load()) {
        EnterCriticalSection(&cs);
        DisplayUpdate(this);
        LeaveCriticalSection(&cs);
        Sleep(16);
    }
}
void DisplayAdapter::TextUpdate(void* ctx) {
    DisplayAdapter* adapter = (DisplayAdapter*)ctx;
    char* videoBuffer = RAM + 0xB8000;
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

                    uint32_t col = pixel_on ? fg : bg;
                    adapter->framebuffer[py * DISPLAY_W + px] =
                        pixel_on ? fg : bg;
                }
            }
        }
    }
    SDL_UpdateTexture(adapter->texture, NULL, adapter->framebuffer.data(), DISPLAY_W * 4);
    SDL_RenderClear(adapter->renderer);
    SDL_RenderTexture(adapter->renderer, adapter->texture, NULL, NULL);
    SDL_RenderPresent(adapter->renderer);
}
void DisplayAdapter::VideoUpdate(void* ctx) {
   DisplayAdapter* adapter = ((DisplayAdapter*)(ctx));
   vga_update_display(adapter->vgaC->vga);
   adapter->update_buffer();
}
void DisplayAdapter::DisplayThunkUpdateLoop(void* ctx) {
    ((DisplayAdapter*)(ctx))->DisplayUpdateLoop();
}

void DisplayAdapter::set_size_graphical(int w, int h)
{
    if (!is_graphical)
        return;
    EnterCriticalSection(&cs);
    gfx_width = w;
    gfx_height = h;

    SDL_SetWindowSize(window, w, h);
    SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
        SDL_TEXTUREACCESS_STREAMING, w, h);
    
    framebuffer.assign((size_t)w * h, 0);

    LeaveCriticalSection(&cs);
}
uint32_t* DisplayAdapter::get_framebuffer() {
    return framebuffer.data();
}
uint32_t DisplayAdapter::get_stride() {
    return gfx_width * 4;
}

void DisplayAdapter::update_buffer(){
    SDL_UpdateTexture(texture, nullptr, framebuffer.data(), gfx_width * 4);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}
void DisplayAdapter::SVGAUpdate(void* ctx) {

    DisplayAdapter* adapter = (DisplayAdapter*)ctx;
    SDL_UpdateTexture(adapter->texture, nullptr, SVGA, adapter->gfx_width * 4);
    SDL_RenderClear(adapter->renderer);
    SDL_RenderTexture(adapter->renderer, adapter->texture, nullptr, nullptr);
    SDL_RenderPresent(adapter->renderer);
}
