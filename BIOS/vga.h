#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cassert>
#include <functional>
#include "memory.h"


struct Layer {
    // pointer/index into image buffer
    int screen_x;
    int screen_y;
    int buffer_x;
    int buffer_y;
    int buffer_width;
    int buffer_height;
};
class ScreenAdapter {
public:
    static constexpr int FLAG_BLINKING = 1;
    static constexpr int FLAG_FONT_PAGE_B = 2;

    virtual void set_mode(bool graphical) {};
    virtual void set_size_text(int cols, int rows) {};
    virtual void set_size_graphical(int w, int h, int vw, int vh) {};
    virtual void put_char(int row, int col, uint8_t chr, int flags,
        int32_t bg, int32_t fg) {};
    virtual void update_cursor(int row, int col) {};
    virtual void update_cursor_scanline(int start, int end, bool visible) {};
    virtual void set_font_bitmap(int height, bool width_9px, bool width_dbl,
        bool copy_8th_col, const uint8_t* plane2,
        bool dirty) {};
    virtual void set_font_page(int page_a, int page_b) {};
    virtual void update_buffer(const std::vector<Layer>& layers) {};
    virtual void clear_screen() {};
};

// Always 64k
static constexpr uint32_t VGA_BANK_SIZE = 64 * 1024;

static constexpr uint32_t MAX_XRES = 2560;
static constexpr uint32_t MAX_YRES = 1600;
static constexpr uint32_t MAX_BPP = 32;

static constexpr uint32_t VGA_LFB_ADDRESS = 0xE0000000;

/**
 * Equals the maximum number of pixels for non-SVGA.
 * 8 pixels per byte.
 */
static constexpr uint32_t VGA_PIXEL_BUFFER_SIZE = 8 * VGA_BANK_SIZE;
static constexpr uint32_t VGA_MIN_MEMORY_SIZE = 4 * VGA_BANK_SIZE;
static constexpr uint32_t VGA_MAX_MEMORY_SIZE = 256 * 1024 * 1024;

/**
 * @see http://www.osdever.net/FreeVGA/vga/graphreg.htm#06
 */
static constexpr uint32_t VGA_HOST_MEMORY_SPACE_START[4] = {
    0xA0000,
    0xA0000,
    0xB0000,
    0xB8000,
};

static constexpr uint32_t VGA_HOST_MEMORY_SPACE_SIZE[4] = {
    0x20000, // 128K
    0x10000, //  64K
    0x8000,  //  32K
    0x8000,  //  32K
};

// Minimal PCI bar descriptor
struct PciBar {
    uint32_t size;
};

class VGAScreen {
public:
    VGAScreen(ScreenAdapter* screen, uint32_t vga_memory_size);
    ~VGAScreen() = default;

    uint32_t svga_allocate_memory(size_t size);
    uint32_t svga_allocate_dest_buffer(size_t size);
    void svga_mark_dirty();
    void svga_fill_pixel_buffer(int bpp, uint32_t svga_dest_offset);

    // Memory-mapped I/O callbacks (registered with CPU)
    uint8_t  vga_memory_read(uint32_t addr);
    void     vga_memory_write(uint32_t addr, uint8_t value);

    // Port I/O handlers
    void     port3C0_write(uint8_t value);
    uint8_t  port3C0_read();
    uint16_t port3C0_read16();
    uint8_t  port3C1_read();
    void     port3C2_write(uint8_t value);

    void     port3C4_write(uint8_t value);
    uint8_t  port3C4_read();
    void     port3C5_write(uint8_t value);
    uint8_t  port3C5_read();

    uint8_t  port3C6_read();
    void     port3C6_write(uint8_t data);
    void     port3C7_write(uint8_t index);
    uint8_t  port3C7_read();
    void     port3C8_write(uint8_t index);
    uint8_t  port3C8_read();
    void     port3C9_write(uint8_t color_byte);
    uint8_t  port3C9_read();

    uint8_t  port3CC_read();

    void     port3CE_write(uint8_t value);
    uint8_t  port3CE_read();
    void     port3CF_write(uint8_t value);
    uint8_t  port3CF_read();

    void     port3D4_write(uint8_t reg);
    void     port3D4_write16(uint16_t reg);
    uint8_t  port3D4_read();
    void     port3D5_write(uint8_t value);
    void     port3D5_write16(uint16_t reg);
    uint8_t  port3D5_read();
    uint16_t port3D5_read16();

    uint8_t  port3DA_read();

    // Bochs VBE (SVGA) extensions
    void     port1CE_write(uint16_t value);
    void     port1CF_write(uint16_t value);
    uint16_t port1CF_read();

    // Called by the render loop
    void screen_fill_buffer();
    void destroy() {}

private:
    void     vga_memory_write_graphical(uint32_t addr, uint8_t value);
    void     vga_memory_write_text_mode(uint32_t addr, uint8_t value);

    uint32_t apply_feed(uint8_t data_byte);
    uint32_t apply_expand(uint8_t data_byte);
    uint8_t  apply_rotate(uint8_t data_byte);
    uint32_t apply_setreset(uint32_t data_dword, uint32_t enable_dword);
    uint32_t apply_logical(uint32_t data_dword, uint32_t latch_dword);
    uint32_t apply_bitmask(uint32_t data_dword, uint32_t bitmask_dword);

    void     text_mode_redraw();
    void     update_cursor();
    void     update_cursor_scanline();
    void     complete_redraw();
    void     complete_replot();
    void     partial_redraw(uint32_t min, uint32_t max);
    void     partial_replot(uint32_t min, uint32_t max);
    void     reset_diffs();

    uint32_t vga_bytes_per_line();
    uint32_t vga_addr_shift_count();
    uint32_t vga_addr_to_pixel(uint32_t addr);
    int      scan_line_to_screen_row(int scan_line);

    void     set_size_text(int cols, int rows);
    void     set_size_graphical(int width, int height, int vwidth, int vheight, int bpp);
    void     update_vga_size();
    void     update_layers();
    void     update_vertical_retrace();

    void     vga_replot();
    void     vga_redraw();

    void     set_font_bitmap(bool font_plane_dirty);
    void     set_font_page();

    uint16_t svga_register_read(uint16_t n);

    // Round up to next power of 2
    static uint32_t round_up_power2(uint32_t v) {
        if (v == 0) return 1;
        v--;
        v |= v >> 1; v |= v >> 2; v |= v >> 4;
        v |= v >> 8; v |= v >> 16;
        return v + 1;
    }

    // ---- external objects ----
    ScreenAdapter* screen;

    // ---- VGA state ----
    uint32_t vga_memory_size;

    uint16_t cursor_address;
    uint8_t  cursor_scanline_start;
    uint8_t  cursor_scanline_end;

    int max_cols;
    int max_rows;

    int screen_width;
    int screen_height;
    int virtual_width;
    int virtual_height;

    std::vector<Layer> layers;

    uint32_t start_address;
    uint32_t start_address_latched;

    uint8_t  crtc[0x19];

    uint8_t  crtc_mode;
    int      horizontal_display_enable_end;
    int      horizontal_blank_start;
    int      vertical_display_enable_end;
    int      vertical_blank_start;
    uint8_t  underline_location_register;
    uint8_t  preset_row_scan;
    uint8_t  offset_register;
    int      line_compare;

    bool     graphical_mode;

    int32_t  vga256_palette[256];

    uint32_t latch_dword;

    // SVGA / Bochs VBE
    uint16_t svga_version;
    int      svga_width;
    int      svga_height;
    bool     svga_enabled;
    int      svga_bpp;
    uint32_t svga_bank_offset;
    uint32_t svga_offset;
    int      svga_offset_x;
    int      svga_offset_y;


    // Indexes
    int      index_crtc;

    // DAC
    uint32_t dac_color_index_write;
    uint32_t dac_color_index_read;
    uint8_t  dac_state;
    uint8_t  dac_mask;
    uint8_t  dac_map[0x10];

    // Attribute controller
    int      attribute_controller_index;
    uint8_t  palette_source;
    uint8_t  attribute_mode;
    uint8_t  color_plane_enable;
    uint8_t  horizontal_panning;
    uint8_t  color_select;

    // Sequencer
    int      sequencer_index;
    uint8_t  plane_write_bm;
    uint8_t  sequencer_memory_mode;
    uint8_t  clocking_mode;
    uint8_t  character_map_select;
    bool     font_page_ab_enabled;

    // Graphics controller
    int      graphics_index;
    uint8_t  plane_read;
    uint8_t  planar_mode;
    uint8_t  planar_rotate_reg;
    uint8_t  planar_bitmap;
    uint8_t  planar_setreset;
    uint8_t  planar_setreset_enable;
    uint8_t  miscellaneous_graphics_register;
    uint8_t  color_compare;
    uint8_t  color_dont_care;
    uint8_t  max_scan_line;

    uint8_t  miscellaneous_output_register;
    uint8_t  port_3DA_value;

    // Bochs VBE
    int16_t  dispi_index;
    uint16_t dispi_enable_value;

    // SVGA memory (allocated via CPU)
    uint8_t* svga_memory;

    // Dirty tracking
    uint32_t diff_addr_min;
    uint32_t diff_addr_max;
    uint32_t diff_plot_min;
    uint32_t diff_plot_max;

    // Destination image buffer (RGBA, allocated via CPU)
    uint32_t* image_buffer;      // points into wasm/CPU memory
    uint32_t  dest_buffer_offset;

    // VGA planar memory: 4 planes × 64k = 256k total
    uint8_t  vga_memory_buf[4 * VGA_BANK_SIZE];
    uint8_t* vga_memory;   // alias for vga_memory_buf
    uint8_t* plane0;       // vga_memory + 0*64k
    uint8_t* plane1;       // vga_memory + 1*64k
    uint8_t* plane2;       // vga_memory + 2*64k
    uint8_t* plane3;       // vga_memory + 3*64k
    uint8_t  pixel_buffer[VGA_PIXEL_BUFFER_SIZE];
};