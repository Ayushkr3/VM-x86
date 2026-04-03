#pragma once
#include "CPU.h"
#include "vga.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_opengl.h"



class DisplayAdapter:public ScreenAdapter {
    typedef void (*UpdateDispatchLoop)(void* ctx);
    SDL_GLContext glCtx;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    UpdateDispatchLoop DisplayUpdate;
    CRITICAL_SECTION cs;
public:
    SDL_Window* window;
    VGAController* vgaC = nullptr;
    int text_cols = 80, text_rows = 25;
    int gfx_width = 720, gfx_height = 400;
    bool is_graphical = false;

    // font/palette data
    std::vector<uint32_t> framebuffer;
    void set_mode(bool graphical, bool svga);
    void set_size_text(int cols, int rows);
    void set_size_graphical(int w, int h);
    uint32_t* get_framebuffer();
    uint32_t get_stride();
    void put_char(int row, int col, uint8_t chr, int flags, uint32_t bg, uint32_t fg);
    void update_buffer();
    void update_cursor(int row, int col);
    void update_cursor_scanline(int start, int end, bool visible);
    void set_font_bitmap(int height, bool w9, bool dbl,bool copy8, uint8_t* plane2, bool dirty);
    void set_font_page(int a, int b);
    void clear_screen();

    DisplayAdapter();
    ~DisplayAdapter();
    void DisplayUpdateLoop();
    static void DisplayThunkUpdateLoop(void* ctx);
    static void TextUpdate(void* ctx);
    static void VideoUpdate(void* ctx);
    static void SVGAUpdate(void* ctx);
};
