#include "font.h"
#include "Global.h"
#include "Display.h"
#include <iomanip>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
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
    window = SDL_CreateWindow("Video Output", 720, 400, SDL_WINDOW_RESIZABLE);
    glCtx = SDL_GL_CreateContext(window);
    renderer = SDL_CreateRenderer(window, NULL);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, DISPLAY_W, DISPLAY_H);
    DisplayUpdate = TextUpdate;
    framebuffer.resize(1024 * 768);
    InitializeCriticalSection(&cs);
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);



    //ok(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf())));
    //D3D12_FEATURE_DATA_ARCHITECTURE arch = {};
    //D3D12_COMMAND_QUEUE_DESC qdesc = {};
    //qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    //ok(device->CreateCommandQueue(&qdesc,IID_PPV_ARGS(cq.GetAddressOf())));
    //device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch));

    //DXGI_SWAP_CHAIN_DESC1 desc = {};
    //desc.BufferCount = 2;
    //desc.Width = 1024;
    //desc.Height = 768;
    //desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    //desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    //desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    //desc.SampleDesc.Count = 1;
    //ok(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())));
    ////ok(factory->CreateSwapChainForHwnd(cq.Get(), hwnd,&desc,nullptr,nullptr,swapchain.GetAddressOf()));

    //D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    //rtvDesc.NumDescriptors = 2;
    //rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    //device->CreateDescriptorHeap(&rtvDesc,IID_PPV_ARGS(rtvHeap.GetAddressOf()));
    //D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    //srvDesc.NumDescriptors = 1;
    //srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    //srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    //device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(srvHeap.GetAddressOf()));

    //for (int i = 0; i < 2; i++) {
    //    ok(swapchain->GetBuffer(i, IID_PPV_ARGS(renderTargets[i].GetAddressOf())));
    //    device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, handle);
    //}
    //D3D12_HEAP_PROPERTIES heap = {};
    //heap.Type = D3D12_HEAP_TYPE_CUSTOM;
    //heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    //heap.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    //D3D12_RESOURCE_DESC rdesc = {};
    //rdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    //rdesc.Width = gfx_width * gfx_height * 4;
    //rdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    //ok(device->CreateCommittedResource(
    //    &heap,
    //    D3D12_HEAP_FLAG_NONE,
    //    &rdesc,
    //    D3D12_RESOURCE_STATE_GENERIC_READ,
    //    nullptr,
    //    IID_PPV_ARGS(frameBuffer.GetAddressOf())
    //));
    //frameBuffer->Map(0, nullptr, (void**)&SVGA);

    //D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    //srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    //srv.Format = DXGI_FORMAT_UNKNOWN;
    //srv.Buffer.NumElements = gfx_width * gfx_height;
    //srv.Buffer.StructureByteStride = 4;

    //device->CreateShaderResourceView(frameBuffer.Get(), &srv, handle);
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
    if(w!=-1)
    gfx_width = w;
    if(h!=-1)
    gfx_height = h;

    SDL_SetWindowSize(window, gfx_width, gfx_height);
    SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
        SDL_TEXTUREACCESS_STREAMING, gfx_width, gfx_height);
    
    framebuffer.assign((size_t)gfx_width* gfx_height, 0);

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
