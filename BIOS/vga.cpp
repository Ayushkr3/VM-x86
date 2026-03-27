#include "vga.h"
#include <cmath>
#include <cstdio>




// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

VGAScreen::VGAScreen(ScreenAdapter* screen_,
    uint32_t mem_size)
    : screen(screen_)
{
    cursor_address = 0;
    cursor_scanline_start = 0xE;
    cursor_scanline_end = 0xF;
    max_cols = 80;
    max_rows = 25;
    screen_width = screen_height = 0;
    virtual_width = virtual_height = 0;
    start_address = start_address_latched = 0;

    std::memset(crtc, 0, sizeof(crtc));
    crtc_mode = 0;
    horizontal_display_enable_end = 0;
    horizontal_blank_start = 0;
    vertical_display_enable_end = 0;
    vertical_blank_start = 0;
    underline_location_register = 0;
    preset_row_scan = 0;
    offset_register = 0;
    line_compare = 0;
    graphical_mode = false;

    std::memset(vga256_palette, 0, sizeof(vga256_palette));
    latch_dword = 0;

    svga_version = 0xB0C5;
    svga_width = svga_height = 0;
    svga_enabled = false;
    svga_bpp = 32;
    svga_bank_offset = 0;
    svga_offset = svga_offset_x = svga_offset_y = 0;

    // Clamp / align memory size
    vga_memory_size = mem_size;
    if (vga_memory_size < VGA_MIN_MEMORY_SIZE)
        vga_memory_size = VGA_MIN_MEMORY_SIZE;
    else if (vga_memory_size > VGA_MAX_MEMORY_SIZE)
        vga_memory_size = VGA_MAX_MEMORY_SIZE;
    else
        vga_memory_size = round_up_power2(vga_memory_size);


    index_crtc = 0;

    dac_color_index_write = 0;
    dac_color_index_read = 0;
    dac_state = 0;
    dac_mask = 0xFF;
    std::memset(dac_map, 0, sizeof(dac_map));

    attribute_controller_index = -1;
    palette_source = 0x20;
    attribute_mode = 0;
    color_plane_enable = 0;
    horizontal_panning = 0;
    color_select = 0;

    sequencer_index = -1;
    plane_write_bm = 0xF;
    sequencer_memory_mode = 0;
    clocking_mode = 0;
    graphics_index = -1;
    character_map_select = 0;
    font_page_ab_enabled = false;

    plane_read = 0;
    planar_mode = 0;
    planar_rotate_reg = 0;
    planar_bitmap = 0xFF;
    planar_setreset = 0;
    planar_setreset_enable = 0;
    miscellaneous_graphics_register = 0;
    color_compare = 0;
    color_dont_care = 0;
    max_scan_line = 0;

    miscellaneous_output_register = 0xFF;
    port_3DA_value = 0xFF;

    dispi_index = -1;
    dispi_enable_value = 0;

    // Planar VGA memory
    std::memset(vga_memory_buf, 0, sizeof(vga_memory_buf));
    vga_memory = vga_memory_buf;
    plane0 = vga_memory + 0 * VGA_BANK_SIZE;
    plane1 = vga_memory + 1 * VGA_BANK_SIZE;
    plane2 = vga_memory + 2 * VGA_BANK_SIZE;
    plane3 = vga_memory + 3 * VGA_BANK_SIZE;
    std::memset(pixel_buffer, 0, sizeof(pixel_buffer));

    uint32_t vga_offset = svga_allocate_memory(vga_memory_size);
    svga_memory = (uint8_t*)(SVGA + vga_offset);

    diff_addr_min = vga_memory_size;
    diff_addr_max = 0;
    diff_plot_min = vga_memory_size;
    diff_plot_max = 0;

    image_buffer = nullptr;
    dest_buffer_offset = 0;

}

// ---------------------------------------------------------------------------
// Memory read
// ---------------------------------------------------------------------------

uint8_t VGAScreen::vga_memory_read(uint32_t addr)
{
    if (svga_enabled)
    {
        //return cpu->read8((addr - 0xA0000 | svga_bank_offset) + VGA_LFB_ADDRESS);
    }

    uint32_t memory_space_select = (miscellaneous_graphics_register >> 2) & 0x3;
    addr -= VGA_HOST_MEMORY_SPACE_START[memory_space_select];

    if (addr >= VGA_HOST_MEMORY_SPACE_SIZE[memory_space_select])
        return 0;

    latch_dword = plane0[addr];
    latch_dword |= plane1[addr] << 8;
    latch_dword |= plane2[addr] << 16;
    latch_dword |= (uint32_t)plane3[addr] << 24;

    if (planar_mode & 0x08)
    {
        // Read mode 1 – colour compare
        uint8_t reading = 0xFF;
        if (color_dont_care & 0x1)
            reading &= plane0[addr] ^ ~(color_compare & 0x1 ? 0xFF : 0x00);
        if (color_dont_care & 0x2)
            reading &= plane1[addr] ^ ~(color_compare & 0x2 ? 0xFF : 0x00);
        if (color_dont_care & 0x4)
            reading &= plane2[addr] ^ ~(color_compare & 0x4 ? 0xFF : 0x00);
        if (color_dont_care & 0x8)
            reading &= plane3[addr] ^ ~(color_compare & 0x8 ? 0xFF : 0x00);
        return reading;
    }
    else
    {
        // Read mode 0
        uint32_t plane = plane_read;
        if (!graphical_mode)
        {
            plane &= 0x3;
        }
        else if (sequencer_memory_mode & 0x8)
        {
            // Chain 4
            plane = addr & 0x3;
            addr &= ~0x3u;
        }
        else if (planar_mode & 0x10)
        {
            // Odd/Even host read
            plane = addr & 0x1;
            addr &= ~0x1u;
        }
        return vga_memory[plane << 16 | addr];
    }
}

// ---------------------------------------------------------------------------
// Memory write – dispatcher
// ---------------------------------------------------------------------------

void VGAScreen::vga_memory_write(uint32_t addr, uint8_t value)
{
    if (svga_enabled)
    {
        //cpu->write8((addr - 0xA0000 | svga_bank_offset) + VGA_LFB_ADDRESS, value);
        return;
    }

    uint32_t memory_space_select = (miscellaneous_graphics_register >> 2) & 0x3;
    addr -= VGA_HOST_MEMORY_SPACE_START[memory_space_select];

    if (addr >= VGA_HOST_MEMORY_SPACE_SIZE[memory_space_select])
        return;

    if (graphical_mode)
    {
        vga_memory_write_graphical(addr, value);
    }
    else if (!(plane_write_bm & 0x3))
    {
        if (plane_write_bm & 0x4)
            plane2[addr] = value;
    }
    else
    {
        vga_memory_write_text_mode(addr, value);
    }
}

// ---------------------------------------------------------------------------
// Graphical write
// ---------------------------------------------------------------------------

void VGAScreen::vga_memory_write_graphical(uint32_t addr, uint8_t value)
{
    uint32_t plane_dword;
    uint8_t  write_mode = planar_mode & 3;
    uint32_t bitmask = apply_feed(planar_bitmap);
    uint32_t setreset_dword = apply_expand(planar_setreset);
    uint32_t setreset_en_dword = apply_expand(planar_setreset_enable);

    switch (write_mode)
    {
    case 0:
        value = apply_rotate(value);
        plane_dword = apply_feed(value);
        plane_dword = apply_setreset(plane_dword, setreset_en_dword);
        plane_dword = apply_logical(plane_dword, latch_dword);
        plane_dword = apply_bitmask(plane_dword, bitmask);
        break;
    case 1:
        plane_dword = latch_dword;
        break;
    case 2:
        plane_dword = apply_expand(value);
        plane_dword = apply_logical(plane_dword, latch_dword);
        plane_dword = apply_bitmask(plane_dword, bitmask);
        break;
    case 3:
        value = apply_rotate(value);
        bitmask &= apply_feed(value);
        plane_dword = setreset_dword;
        plane_dword = apply_bitmask(plane_dword, bitmask);
        break;
    default:
        plane_dword = 0;
    }

    uint8_t plane_select = 0xF;

    switch (sequencer_memory_mode & 0xC)
    {
    case 0x0: // Odd/Even
        plane_select = 0x5 << (addr & 0x1);
        addr &= ~0x1u;
        break;
    case 0x8: // Chain 4
    case 0xC:
        plane_select = 1 << (addr & 0x3);
        addr &= ~0x3u;
        break;
    }

    plane_select &= plane_write_bm;

    if (plane_select & 0x1) plane0[addr] = (plane_dword >> 0) & 0xFF;
    if (plane_select & 0x2) plane1[addr] = (plane_dword >> 8) & 0xFF;
    if (plane_select & 0x4) plane2[addr] = (plane_dword >> 16) & 0xFF;
    if (plane_select & 0x8) plane3[addr] = (plane_dword >> 24) & 0xFF;

    uint32_t pixel_addr = vga_addr_to_pixel(addr);
    partial_replot(pixel_addr, pixel_addr + 7);
}

// ---------------------------------------------------------------------------
// Text-mode write
// ---------------------------------------------------------------------------

void VGAScreen::vga_memory_write_text_mode(uint32_t addr, uint8_t value)
{
    vga_memory[addr] = value;

    const int max_c = max(max_cols, (int)offset_register * 2);
    int row, col;

    if ((addr >> 1) >= start_address)
    {
        int mem_start = (int)(addr >> 1) - (int)start_address;
        row = mem_start / max_c;
        col = mem_start % max_c;
    }
    else
    {
        int mem_start = (int)(addr >> 1);
        row = (mem_start / max_c) + scan_line_to_screen_row(line_compare);
        col = mem_start % max_c;
    }

    if (col >= max_cols || row >= max_rows) return;

    uint8_t chr, color;
    if (addr & 1)
    {
        color = value;
        chr = vga_memory[addr & ~1u];
    }
    else
    {
        chr = value;
        color = vga_memory[addr | 1];
    }

    bool blink_en = !!(attribute_mode & (1 << 3));
    bool blinking = blink_en && (color & (1 << 7));
    bool fp_b = font_page_ab_enabled && !(color & (1 << 3));
    int  flags = (blinking ? ScreenAdapter::FLAG_BLINKING : 0)
        | (fp_b ? ScreenAdapter::FLAG_FONT_PAGE_B : 0);
    int  fg_mask = font_page_ab_enabled ? 7 : 0xF;
    int  bg_mask = blink_en ? 7 : 0xF;

    screen->put_char(row, col, chr, flags,
        vga256_palette[dac_mask & dac_map[(color >> 4) & bg_mask]],
        vga256_palette[dac_mask & dac_map[color & fg_mask]]);
}

// ---------------------------------------------------------------------------
// Planar arithmetic helpers
// ---------------------------------------------------------------------------

uint32_t VGAScreen::apply_feed(uint8_t b)
{
    uint32_t d = b;
    d |= d << 8;
    d |= d << 16;
    return d;
}

uint32_t VGAScreen::apply_expand(uint8_t b)
{
    uint32_t d = (b & 0x1) ? 0xFF : 0x00;
    d |= ((b & 0x2) ? 0xFF : 0x00u) << 8;
    d |= ((b & 0x4) ? 0xFF : 0x00u) << 16;
    d |= ((b & 0x8) ? 0xFF : 0x00u) << 24;
    return d;
}

uint8_t VGAScreen::apply_rotate(uint8_t b)
{
    uint16_t wrapped = b | (b << 8);
    uint8_t  count = planar_rotate_reg & 0x7;
    return (wrapped >> count) & 0xFF;
}

uint32_t VGAScreen::apply_setreset(uint32_t data, uint32_t enable)
{
    uint32_t sr = apply_expand(planar_setreset);
    data |= enable & sr;
    data &= ~enable | sr;
    return data;
}

uint32_t VGAScreen::apply_logical(uint32_t data, uint32_t latch)
{
    switch (planar_rotate_reg & 0x18)
    {
    case 0x08: return data & latch;
    case 0x10: return data | latch;
    case 0x18: return data ^ latch;
    }
    return data;
}

uint32_t VGAScreen::apply_bitmask(uint32_t data, uint32_t mask)
{
    return (mask & data) | (~mask & latch_dword);
}

// ---------------------------------------------------------------------------
// Addressing helpers
// ---------------------------------------------------------------------------

uint32_t VGAScreen::vga_bytes_per_line()
{
    uint32_t bpl = offset_register << 2;
    if (underline_location_register & 0x40) bpl <<= 1;
    else if (crtc_mode & 0x40)              bpl >>= 1;
    return bpl;
}

uint32_t VGAScreen::vga_addr_shift_count()
{
    uint32_t sc = 0x80;
    sc += (~underline_location_register & crtc_mode & 0x40);
    sc -= (underline_location_register & 0x40);
    sc -= (attribute_mode & 0x40);
    return sc >> 6;
}

uint32_t VGAScreen::vga_addr_to_pixel(uint32_t addr)
{
    uint32_t shift_count = vga_addr_shift_count();

    if (~crtc_mode & 0x3)
    {
        int pixel_addr = (int)addr - (int)start_address;
        pixel_addr &= (crtc_mode << 13) | ~0x6000;
        pixel_addr <<= shift_count;

        int row = pixel_addr / (int)virtual_width;
        int col = pixel_addr % (int)virtual_width;

        switch (crtc_mode & 0x3)
        {
        case 0x2: row = (row << 1) | ((addr >> 13) & 0x1); break;
        case 0x1: row = (row << 1) | ((addr >> 14) & 0x1); break;
        case 0x0: row = (row << 2) | ((addr >> 13) & 0x3); break;
        }

        return (uint32_t)(row * (int)virtual_width + col)
            + (start_address << shift_count);
    }
    else
    {
        return addr << shift_count;
    }
}

int VGAScreen::scan_line_to_screen_row(int scan_line)
{
    if (max_scan_line & 0x80) scan_line >>= 1;

    int repeat_factor = 1 + (max_scan_line & 0x1F);
    scan_line = (int)std::ceil((double)scan_line / repeat_factor);

    if (!(crtc_mode & 0x1)) scan_line <<= 1;
    if (!(crtc_mode & 0x2)) scan_line <<= 1;

    return scan_line;
}

// ---------------------------------------------------------------------------
// Size / layout
// ---------------------------------------------------------------------------

void VGAScreen::set_size_text(int cols, int rows)
{
    max_cols = cols;
    max_rows = rows;
    screen->set_size_text(cols, rows);
}

void VGAScreen::set_size_graphical(int w, int h, int vw, int vh, int bpp)
{
    vw = max(vw, 1);
    vh = max(vh, 1);

    bool needs_update = (screen_width != w || screen_height != h ||
        virtual_width != vw || virtual_height != vh);
    if (needs_update)
    {
        screen_width = w;
        screen_height = h;
        virtual_width = vw;
        virtual_height = vh;

        uint32_t size = (uint32_t)vw * vh;
        dest_buffer_offset = svga_allocate_dest_buffer(size);
        //image_buffer = reinterpret_cast<uint32_t*>(
            //cpu->wasm_memory_base + dest_buffer_offset);

        svga_mark_dirty();

        screen->set_size_graphical(w, h, vw, vh);
    }
}

void VGAScreen::update_vga_size()
{
    if (svga_enabled) return;

    int h_chars = min(1 + horizontal_display_enable_end,
        horizontal_blank_start);
    int v_scans = min(1 + vertical_display_enable_end,
        vertical_blank_start);

    if (!h_chars || !v_scans) return;

    if (graphical_mode)
    {
        int sw = h_chars << 3;
        int vw = offset_register << 4;
        int bpp = 4;

        if (attribute_mode & 0x40)
        {
            sw >>= 1; vw >>= 1; bpp = 8;
        }
        else if (attribute_mode & 0x2)
        {
            bpp = 1;
        }

        int sh = scan_line_to_screen_row(v_scans);

        uint32_t available = VGA_HOST_MEMORY_SPACE_SIZE[0];
        uint32_t bpl = vga_bytes_per_line();
        int vh = bpl ? (int)std::ceil((double)available / bpl) : sh;

        set_size_graphical(sw, sh, vw, vh, bpp);
        update_vertical_retrace();
        update_layers();
    }
    else
    {
        if (max_scan_line & 0x80) v_scans >>= 1;
        int height = v_scans / (1 + (max_scan_line & 0x1F));
        if (h_chars && height) set_size_text(h_chars, height);
    }
}

// ---------------------------------------------------------------------------
// Layers / panning
// ---------------------------------------------------------------------------

void VGAScreen::update_layers()
{
    if (!graphical_mode)
    {
        text_mode_redraw();
    }

    if (svga_enabled)
    {
        layers.clear();
        return;
    }

    if (!virtual_width || !screen_width) return;

    if (!palette_source || (clocking_mode & 0x20))
    {
        layers.clear();
        screen->clear_screen();
        return;
    }

    int pixel_panning = horizontal_panning;
    if (attribute_mode & 0x40) pixel_panning >>= 1;

    int byte_panning = (preset_row_scan >> 5) & 0x3;
    int pixel_addr_start = (int)vga_addr_to_pixel(start_address_latched + byte_panning);

    int start_buf_row = pixel_addr_start / virtual_width;
    int start_buf_col = pixel_addr_start % virtual_width + pixel_panning;

    int split_screen_row = scan_line_to_screen_row(1 + line_compare);
    split_screen_row = min(split_screen_row, screen_height);

    int split_buf_height = screen_height - split_screen_row;

    layers.clear();

    for (int x = -start_buf_col, y = 0; x < screen_width;
        x += virtual_width, y++)
    {
        layers.push_back({ x, 0, 0, start_buf_row + y,
                          virtual_width, split_screen_row });
    }

    int start_split_col = 0;
    if (!(attribute_mode & 0x20))
        start_split_col = (int)vga_addr_to_pixel(byte_panning) + pixel_panning;

    for (int x = -start_split_col, y = 0; x < screen_width;
        x += virtual_width, y++)
    {
        layers.push_back({ x, split_screen_row, 0, y,
                          virtual_width, split_buf_height });
    }
}

void VGAScreen::update_vertical_retrace()
{
    port_3DA_value |= 0x8;
    if (start_address_latched != start_address)
    {
        start_address_latched = start_address;
        update_layers();
    }
}

// ---------------------------------------------------------------------------
// Redraw helpers
// ---------------------------------------------------------------------------

void VGAScreen::complete_redraw()
{
    if (graphical_mode)
    {
        if (svga_enabled)
            svga_mark_dirty();
        else
        {
            diff_addr_min = 0;
            diff_addr_max = VGA_PIXEL_BUFFER_SIZE;
        }
    }
    else
    {
        text_mode_redraw();
    }
}

void VGAScreen::complete_replot()
{
    if (!graphical_mode || svga_enabled) return;
    diff_plot_min = 0;
    diff_plot_max = VGA_PIXEL_BUFFER_SIZE;
    complete_redraw();
}

void VGAScreen::partial_redraw(uint32_t min_, uint32_t max_)
{
    if (min_ < diff_addr_min) diff_addr_min = min_;
    if (max_ > diff_addr_max) diff_addr_max = max_;
}

void VGAScreen::partial_replot(uint32_t min_, uint32_t max_)
{
    if (min_ < diff_plot_min) diff_plot_min = min_;
    if (max_ > diff_plot_max) diff_plot_max = max_;
    partial_redraw(min_, max_);
}

void VGAScreen::reset_diffs()
{
    diff_addr_min = vga_memory_size;
    diff_addr_max = 0;
    diff_plot_min = vga_memory_size;
    diff_plot_max = 0;
}

// ---------------------------------------------------------------------------
// Text mode full redraw
// ---------------------------------------------------------------------------

void VGAScreen::text_mode_redraw()
{
    int split_row = scan_line_to_screen_row(line_compare);
    int row_offset = max(0, (offset_register * 2 - max_cols) * 2);
    bool blink_en = !!(attribute_mode & (1 << 3));
    int fg_mask = font_page_ab_enabled ? 7 : 0xF;
    int bg_mask = blink_en ? 7 : 0xF;

    uint32_t addr = start_address << 1;

    for (int row = 0; row < max_rows; row++)
    {
        if (row == split_row) addr = 0;

        for (int col = 0; col < max_cols; col++)
        {
            uint8_t chr = vga_memory[addr];
            uint8_t color = vga_memory[addr | 1];

            bool blinking = blink_en && (color & (1 << 7));
            bool fp_b = font_page_ab_enabled && !(color & (1 << 3));
            int  flags = (blinking ? ScreenAdapter::FLAG_BLINKING : 0)
                | (fp_b ? ScreenAdapter::FLAG_FONT_PAGE_B : 0);

            screen->put_char(row, col, chr, flags,
                vga256_palette[dac_mask & dac_map[(color >> 4) & bg_mask]],
                vga256_palette[dac_mask & dac_map[color & fg_mask]]);

            addr += 2;
        }
        addr += row_offset;
    }
}

void VGAScreen::update_cursor()
{
    int max_c = max(max_cols, (int)offset_register * 2);
    int row, col;

    if (cursor_address >= start_address)
    {
        row = (cursor_address - start_address) / max_c;
        col = (cursor_address - start_address) % max_c;
    }
    else
    {
        row = (cursor_address / max_c) + scan_line_to_screen_row(line_compare);
        col = cursor_address % max_c;
    }

    screen->update_cursor(row, col);
}

void VGAScreen::update_cursor_scanline()
{
    bool disabled = !!(cursor_scanline_start & 0x20);
    int  mx = max_scan_line & 0x1F;
    int  start =min(mx, (int)(cursor_scanline_start & 0x1F));
    int  end = min(mx, (int)(cursor_scanline_end & 0x1F));
    bool visible = !disabled && start < end;
    screen->update_cursor_scanline(start, end, visible);
}

// ---------------------------------------------------------------------------
// Font helpers
// ---------------------------------------------------------------------------

void VGAScreen::set_font_bitmap(bool font_plane_dirty)
{
    int height = max_scan_line & 0x1F;
    if (height && !graphical_mode)
    {
        bool width_dbl = !!(clocking_mode & 0x08);
        bool width_9px = !width_dbl && !(clocking_mode & 0x01);
        bool copy_8th_col = !!(attribute_mode & 0x04);
        screen->set_font_bitmap(height + 1, width_9px, width_dbl,
            copy_8th_col, plane2, font_plane_dirty);
    }
}

void VGAScreen::set_font_page()
{
    static const int linear_index_map[8] = { 0, 2, 4, 6, 1, 3, 5, 7 };
    int vga_A = ((character_map_select & 0b1100) >> 2)
        | ((character_map_select & 0b100000) >> 3);
    int vga_B = (character_map_select & 0b11)
        | ((character_map_select & 0b10000) >> 2);
    font_page_ab_enabled = (vga_A != vga_B);
    screen->set_font_page(linear_index_map[vga_A], linear_index_map[vga_B]);
    complete_redraw();
}

// ---------------------------------------------------------------------------
// VGA replot (planes → pixel buffer)
// ---------------------------------------------------------------------------

void VGAScreen::vga_replot()
{
    uint32_t start = diff_plot_min & ~0xFu;
    uint32_t end = min(diff_plot_max | 0xFu,
        (uint32_t)VGA_PIXEL_BUFFER_SIZE - 1);

    uint32_t addr_shift = vga_addr_shift_count();
    uint32_t addr_substitution = ~crtc_mode & 0x3;
    uint8_t  shift_mode = planar_mode & 0x60;
    bool     pel_width = !!(attribute_mode & 0x40);

    for (uint32_t pixel_addr = start; pixel_addr <= end; )
    {
        uint32_t addr = pixel_addr >> addr_shift;

        if (addr_substitution)
        {
            int row = (int)pixel_addr / virtual_width;
            int col = (int)pixel_addr - virtual_width * row;
            uint32_t sub = 0;

            switch (addr_substitution)
            {
            case 0x1:
                sub = (uint32_t)(row & 1) << 13;
                row >>= 1;
                break;
            case 0x2:
                sub = (uint32_t)(row & 1) << 14;
                row >>= 1;
                break;
            case 0x3:
                sub = (uint32_t)(row & 3) << 13;
                row >>= 2;
                break;
            }
            addr = sub | (((uint32_t)(row * virtual_width + col)) >> addr_shift)
                + start_address;
        }

        uint8_t b0 = plane0[addr], b1 = plane1[addr];
        uint8_t b2 = plane2[addr], b3 = plane3[addr];
        uint8_t sl[8];

        switch (shift_mode)
        {
        case 0x00: // Planar
            b0 <<= 0; b1 <<= 1; b2 <<= 2; b3 <<= 3;
            for (int i = 7; i >= 0; i--)
                sl[7 - i] = ((b0 >> i) & 1) | ((b1 >> i) & 2)
                | ((b2 >> i) & 4) | ((b3 >> i) & 8);
            break;
        case 0x20: // Packed (CGA modes 4/5)
            sl[0] = ((b0 >> 6) & 0x3) | ((b2 >> 4) & 0xC);
            sl[1] = ((b0 >> 4) & 0x3) | ((b2 >> 2) & 0xC);
            sl[2] = ((b0 >> 2) & 0x3) | ((b2) & 0xC);
            sl[3] = ((b0) & 0x3) | ((b2 << 2) & 0xC);
            sl[4] = ((b1 >> 6) & 0x3) | ((b3 >> 4) & 0xC);
            sl[5] = ((b1 >> 4) & 0x3) | ((b3 >> 2) & 0xC);
            sl[6] = ((b1 >> 2) & 0x3) | ((b3) & 0xC);
            sl[7] = ((b1) & 0x3) | ((b3 << 2) & 0xC);
            break;
        default:   // 256-colour (mode 13h)
            sl[0] = (b0 >> 4) & 0xF; sl[1] = b0 & 0xF;
            sl[2] = (b1 >> 4) & 0xF; sl[3] = b1 & 0xF;
            sl[4] = (b2 >> 4) & 0xF; sl[5] = b2 & 0xF;
            sl[6] = (b3 >> 4) & 0xF; sl[7] = b3 & 0xF;
            break;
        }

        if (pel_width)
        {
            for (int i = 0, j = 0; i < 4; i++, pixel_addr++, j += 2)
                pixel_buffer[pixel_addr] = (sl[j] << 4) | sl[j + 1];
        }
        else
        {
            for (int i = 0; i < 8; i++, pixel_addr++)
                pixel_buffer[pixel_addr] = sl[i];
        }
    }
}

// ---------------------------------------------------------------------------
// VGA redraw (pixel buffer → RGBA image)
// ---------------------------------------------------------------------------

void VGAScreen::vga_redraw()
{
    uint32_t start = diff_addr_min;
    uint32_t end = (diff_addr_max,
        (uint32_t)VGA_PIXEL_BUFFER_SIZE - 1);

    auto* buffer = reinterpret_cast<int32_t*>(image_buffer);

    uint8_t mask = 0xFF;
    uint8_t colorset = 0x00;

    if (attribute_mode & 0x80)
    {
        mask &= 0xCF;
        colorset |= (color_select << 4) & 0x30;
    }

    if (attribute_mode & 0x40)
    {
        // 8-bit mode
        for (uint32_t p = start; p <= end; p++)
        {
            uint8_t  c256 = (pixel_buffer[p] & mask) | colorset;
            int32_t  col = vga256_palette[c256];
            buffer[p] = (col & 0xFF00) | (col << 16) | (col >> 16)
                | (int32_t)0xFF000000;
        }
    }
    else
    {
        // 4-bit mode
        mask &= 0x3F;
        colorset |= (color_select << 4) & 0xC0;

        for (uint32_t p = start; p <= end; p++)
        {
            uint8_t  c16 = pixel_buffer[p] & color_plane_enable;
            uint8_t  c256 = (dac_map[c16] & mask) | colorset;
            int32_t  col = vga256_palette[c256];
            buffer[p] = (col & 0xFF00) | (col << 16) | (col >> 16)
                | (int32_t)0xFF000000;
        }
    }
}

// ---------------------------------------------------------------------------
// screen_fill_buffer (main render tick)
// ---------------------------------------------------------------------------

void VGAScreen::screen_fill_buffer()
{
    if (!graphical_mode)
    {
        update_vertical_retrace();
        return;
    }

    if (svga_enabled)
    {
        int min_y = 0, max_y = svga_height;

        if (svga_bpp == 8)
        {
            // Slow 8-bit path (palette lookup)
            auto* buf = reinterpret_cast<int32_t*>(image_buffer);
            auto* smem = svga_memory;
            int   len = screen_width * screen_height;
            for (int i = 0; i < len; i++)
            {
                int32_t col = vga256_palette[smem[i]];
                buf[i] = (col & 0xFF00) | (col << 16) | (col >> 16)
                    | (int32_t)0xFF000000;
            }
        }
        else
        {
            svga_fill_pixel_buffer(svga_bpp, svga_offset);
            int bpp_bytes = (svga_bpp == 15) ? 2 : svga_bpp / 8;
            //min_y = (((int)(svga_dirty_bitmap_min_offset[0] / bpp_bytes)- (int)svga_offset) / svga_width);
            //max_y = (((int)(svga_dirty_bitmap_max_offset[0] / bpp_bytes)- (int)svga_offset) / svga_width) + 1;
        }

        if (min_y < max_y)
        {
            min_y = max(min_y, 0);
            max_y = min(max_y, svga_height);
            screen->update_buffer({ {0, min_y, 0, min_y,
                                    svga_width, max_y - min_y} });
        }
    }
    else
    {
        vga_replot();
        vga_redraw();
        screen->update_buffer(layers);
    }

    reset_diffs();
    update_vertical_retrace();
}

uint32_t VGAScreen::svga_allocate_memory(size_t size) {
    //uint32_t offset = next_alloc_offset;
    //next_alloc_offset += size;
    //return offset;           // offset into wasm_memory_base
    return 0;
}

// Allocate the RGBA destination buffer (one uint32 per pixel)
uint32_t VGAScreen::svga_allocate_dest_buffer(size_t pixel_count) {
    //uint32_t offset = next_alloc_offset;
    //next_alloc_offset += pixel_count * 4;
    //return offset;
    return 0;
}

void VGAScreen::svga_mark_dirty() {
    // flag that the SVGA buffer needs to be re-blitted
    //svga_dirty = true;
}

// Convert SVGA memory to RGBA pixels (implement per bpp)
void VGAScreen::svga_fill_pixel_buffer(int bpp, uint32_t offset) {
    // bpp can be 8, 15, 16, 24, or 32
    // source: svga_memory (wasm_memory_base + svga_offset_in_wasm)
    // dest:   dest_buffer (wasm_memory_base + dest_buffer_offset)
    // offset: pixel offset for panning/page flipping
}
















// vga_ports.cpp  –  I/O port handlers for VGAScreen
// Include after vga_screen.cpp (or merge into one translation unit)

// ---------------------------------------------------------------------------
// 0x3C0 – Attribute Controller index/data (shared port)
// ---------------------------------------------------------------------------

void VGAScreen::port3C0_write(uint8_t value)
{
    if (attribute_controller_index == -1)
    {
        attribute_controller_index = value & 0x1F;

        uint8_t src = value & 0x20;
        if (palette_source != src)
        {
            palette_source = src;
            update_layers();
        }
    }
    else
    {
        if (attribute_controller_index < 0x10)
        {
            dac_map[attribute_controller_index] = value;
            if (!(attribute_mode & 0x40)) complete_redraw();
        }
        else
        {
            switch (attribute_controller_index)
            {
            case 0x10: // Attribute Mode Control
                if (attribute_mode != value)
                {
                    uint8_t prev = attribute_mode;
                    attribute_mode = value;

                    bool is_graphical = !!(value & 0x1);
                    if (!svga_enabled && graphical_mode != is_graphical)
                    {
                        graphical_mode = is_graphical;
                        screen->set_mode(graphical_mode);
                    }

                    if ((prev ^ value) & 0x40) complete_replot();

                    update_vga_size();
                    complete_redraw();
                    set_font_bitmap(false);
                }
                break;

            case 0x12: // Color Plane Enable
                if (color_plane_enable != value)
                {
                    color_plane_enable = value;
                    complete_redraw();
                }
                break;

            case 0x13: // Horizontal Panning
                if (horizontal_panning != (value & 0xF))
                {
                    horizontal_panning = value & 0xF;
                    update_layers();
                }
                break;

            case 0x14: // Color Select
                if (color_select != value)
                {
                    color_select = value;
                    complete_redraw();
                }
                break;

            default:
                break;
            }
        }
        attribute_controller_index = -1;
    }
}

uint8_t VGAScreen::port3C0_read()
{
    return (attribute_controller_index | palette_source) & 0xFF;
}

uint16_t VGAScreen::port3C0_read16()
{
    return port3C0_read() | ((uint16_t)port3C1_read() << 8);
}

uint8_t VGAScreen::port3C1_read()
{
    if (attribute_controller_index < 0x10)
        return dac_map[attribute_controller_index] & 0xFF;

    switch (attribute_controller_index)
    {
    case 0x10: return attribute_mode;
    case 0x12: return color_plane_enable;
    case 0x13: return horizontal_panning;
    case 0x14: return color_select;
    default:   return 0xFF;
    }
}

// ---------------------------------------------------------------------------
// 0x3C2 – Miscellaneous Output Register
// ---------------------------------------------------------------------------

void VGAScreen::port3C2_write(uint8_t value)
{
    miscellaneous_output_register = value;
}

// ---------------------------------------------------------------------------
// 0x3C4 / 0x3C5 – Sequencer
// ---------------------------------------------------------------------------

void VGAScreen::port3C4_write(uint8_t value) { sequencer_index = value; }
uint8_t VGAScreen::port3C4_read() { return sequencer_index; }

void VGAScreen::port3C5_write(uint8_t value)
{
    switch (sequencer_index)
    {
    case 0x01: // Clocking Mode
    {
        uint8_t prev = clocking_mode;
        clocking_mode = value;
        if ((prev ^ value) & 0x20) update_layers();
        set_font_bitmap(false);
        break;
    }
    case 0x02: // Map Mask (plane write bitmask)
    {
        uint8_t prev = plane_write_bm;
        plane_write_bm = value;
        if (!graphical_mode && (prev & 0x4) && !(plane_write_bm & 0x4))
            set_font_bitmap(true);
        break;
    }
    case 0x03: // Character Map Select
    {
        uint8_t prev = character_map_select;
        character_map_select = value;
        if (!graphical_mode && prev != value) set_font_page();
        break;
    }
    case 0x04: // Memory Mode
        sequencer_memory_mode = value;
        break;

    default:
        break;
    }
}

uint8_t VGAScreen::port3C5_read()
{
    switch (sequencer_index)
    {
    case 0x01: return clocking_mode;
    case 0x02: return plane_write_bm;
    case 0x03: return character_map_select;
    case 0x04: return sequencer_memory_mode;
    case 0x06: return 0x12;  // unlock extended regs
    default:   return 0;
    }
}

// ---------------------------------------------------------------------------
// 0x3C6–0x3C9 – DAC / Palette
// ---------------------------------------------------------------------------

void VGAScreen::port3C6_write(uint8_t data)
{
    if (dac_mask != data) { dac_mask = data; complete_redraw(); }
}
uint8_t VGAScreen::port3C6_read() { return dac_mask; }

void VGAScreen::port3C7_write(uint8_t index)
{
    dac_color_index_read = (uint32_t)index * 3;
    dac_state &= 0x0;
}
uint8_t VGAScreen::port3C7_read() { return dac_state; }

void VGAScreen::port3C8_write(uint8_t index)
{
    dac_color_index_write = (uint32_t)index * 3;
    dac_state |= 0x3;
}
uint8_t VGAScreen::port3C8_read()
{
    return (uint8_t)(dac_color_index_write / 3) & 0xFF;
}

void VGAScreen::port3C9_write(uint8_t color_byte)
{
    uint32_t index = dac_color_index_write / 3;
    uint32_t offset = dac_color_index_write % 3;
    int32_t  color = vga256_palette[index];

    if (!(dispi_enable_value & 0x20))
    {
        color_byte &= 0x3F;
        uint8_t b = color_byte & 1;
        color_byte = (color_byte << 2) | (b << 1) | b;
    }

    if (offset == 0)
        color = (color & ~0xFF0000) | ((int32_t)color_byte << 16);
    else if (offset == 1)
        color = (color & ~0xFF00) | ((int32_t)color_byte << 8);
    else
        color = (color & ~0xFF) | color_byte;

    if (vga256_palette[index] != color)
    {
        vga256_palette[index] = color;
        complete_redraw();
    }
    dac_color_index_write++;
}

uint8_t VGAScreen::port3C9_read()
{
    uint32_t index = dac_color_index_read / 3;
    uint32_t offset = dac_color_index_read % 3;
    int32_t  color = vga256_palette[index];
    uint8_t  c8 = (uint8_t)((color >> ((2 - offset) * 8)) & 0xFF);

    dac_color_index_read++;
    return (dispi_enable_value & 0x20) ? c8 : (c8 >> 2);
}

uint8_t VGAScreen::port3CC_read() { return miscellaneous_output_register; }

// ---------------------------------------------------------------------------
// 0x3CE / 0x3CF – Graphics Controller
// ---------------------------------------------------------------------------

void VGAScreen::port3CE_write(uint8_t value) { graphics_index = value; }
uint8_t VGAScreen::port3CE_read() { return graphics_index; }

void VGAScreen::port3CF_write(uint8_t value)
{
    switch (graphics_index)
    {
    case 0: planar_setreset = value; break;
    case 1: planar_setreset_enable = value; break;
    case 2: color_compare = value; break;
    case 3: planar_rotate_reg = value; break;
    case 4: plane_read = value; break;
    case 5:
    {
        uint8_t prev = planar_mode;
        planar_mode = value;
        if ((prev ^ value) & 0x60) complete_replot();
        break;
    }
    case 6:
        if (miscellaneous_graphics_register != value)
        {
            miscellaneous_graphics_register = value;
            update_vga_size();
        }
        break;
    case 7: color_dont_care = value; break;
    case 8: planar_bitmap = value; break;
    default: break;
    }
}

uint8_t VGAScreen::port3CF_read()
{
    switch (graphics_index)
    {
    case 0: return planar_setreset;
    case 1: return planar_setreset_enable;
    case 2: return color_compare;
    case 3: return planar_rotate_reg;
    case 4: return plane_read;
    case 5: return planar_mode;
    case 6: return miscellaneous_graphics_register;
    case 7: return color_dont_care;
    case 8: return planar_bitmap;
    default: return 0;
    }
}

// ---------------------------------------------------------------------------
// 0x3D4 / 0x3D5 – CRT Controller
// ---------------------------------------------------------------------------

void VGAScreen::port3D4_write(uint8_t reg) { index_crtc = reg; }
uint8_t VGAScreen::port3D4_read() { return index_crtc; }

void VGAScreen::port3D4_write16(uint16_t reg)
{
    port3D4_write(reg & 0xFF);
    port3D5_write((reg >> 8) & 0xFF);
}

void VGAScreen::port3D5_write(uint8_t value)
{
    switch (index_crtc)
    {
    case 0x01: // Horizontal Display Enable End
        if (horizontal_display_enable_end != value)
        {
            horizontal_display_enable_end = value;
            update_vga_size();
        }
        break;

    case 0x02: // Start Horizontal Blanking
        if (horizontal_blank_start != value)
        {
            horizontal_blank_start = value;
            update_vga_size();
        }
        break;

    case 0x07: // Overflow Register (MSBs for various counters)
    {
        int prev_vde = vertical_display_enable_end;
        vertical_display_enable_end &= 0xFF;
        vertical_display_enable_end |= (value << 3 & 0x200) | (value << 7 & 0x100);
        if (prev_vde != vertical_display_enable_end) update_vga_size();

        line_compare = (line_compare & 0x2FF) | ((value << 4) & 0x100);

        int prev_vbs = vertical_blank_start;
        vertical_blank_start = (vertical_blank_start & 0x2FF) | ((value << 5) & 0x100);
        if (prev_vbs != vertical_blank_start) update_vga_size();
        update_layers();
        break;
    }

    case 0x08: // Preset Row Scan
        preset_row_scan = value;
        update_layers();
        break;

    case 0x09: // Maximum Scan Line
    {
        uint8_t prev_msl = max_scan_line;
        max_scan_line = value;
        line_compare = (line_compare & 0x1FF) | ((value << 3) & 0x200);

        int prev_vbs = vertical_blank_start;
        vertical_blank_start = (vertical_blank_start & 0x1FF) | ((value << 4) & 0x200);

        if (((prev_msl ^ max_scan_line) & 0x9F) || prev_vbs != vertical_blank_start)
            update_vga_size();

        update_cursor_scanline();
        update_layers();
        set_font_bitmap(false);
        break;
    }

    case 0x0A: // Cursor Start
        cursor_scanline_start = value;
        update_cursor_scanline();
        break;

    case 0x0B: // Cursor End
        cursor_scanline_end = value;
        update_cursor_scanline();
        break;

    case 0x0C: // Start Address High
        if ((start_address >> 8 & 0xFF) != value)
        {
            start_address = (start_address & 0xFF) | ((uint32_t)value << 8);
            update_layers();
            if (~crtc_mode & 0x3) complete_replot();
        }
        break;

    case 0x0D: // Start Address Low
        if ((start_address & 0xFF) != value)
        {
            start_address = (start_address & 0xFF00) | value;
            update_layers();
            if (~crtc_mode & 0x3) complete_replot();
        }
        break;

    case 0x0E: // Cursor Location High
        cursor_address = (cursor_address & 0x00FF) | ((uint16_t)value << 8);
        update_cursor();
        break;

    case 0x0F: // Cursor Location Low
        cursor_address = (cursor_address & 0xFF00) | value;
        update_cursor();
        break;

    case 0x12: // Vertical Display Enable End (low 8 bits)
        if ((vertical_display_enable_end & 0xFF) != value)
        {
            vertical_display_enable_end = (vertical_display_enable_end & 0x300) | value;
            update_vga_size();
        }
        break;

    case 0x13: // Offset Register (stride)
        if (offset_register != value)
        {
            offset_register = value;
            update_vga_size();
            if (~crtc_mode & 0x3) complete_replot();
        }
        break;

    case 0x14: // Underline Location
        if (underline_location_register != value)
        {
            uint8_t prev = underline_location_register;
            underline_location_register = value;
            update_vga_size();
            if ((prev ^ value) & 0x40) complete_replot();
        }
        break;

    case 0x15: // Vertical Blank Start (low 8 bits)
        if ((vertical_blank_start & 0xFF) != value)
        {
            vertical_blank_start = (vertical_blank_start & 0x300) | value;
            update_vga_size();
        }
        break;

    case 0x17: // CRTC Mode Control
        if (crtc_mode != value)
        {
            uint8_t prev = crtc_mode;
            crtc_mode = value;
            update_vga_size();
            if ((prev ^ value) & 0x43) complete_replot();
        }
        break;

    case 0x18: // Line Compare (low 8 bits)
        line_compare = (line_compare & 0x300) | value;
        update_layers();
        break;

    default:
        if (index_crtc < (int)sizeof(crtc))
            crtc[index_crtc] = value;
        break;
    }
}

void VGAScreen::port3D5_write16(uint16_t reg)
{
    port3D5_write(reg & 0xFF);
}

uint8_t VGAScreen::port3D5_read()
{
    switch (index_crtc)
    {
    case 0x01: return (uint8_t)horizontal_display_enable_end;
    case 0x02: return (uint8_t)horizontal_blank_start;
    case 0x07:
        return (uint8_t)(
            ((vertical_display_enable_end >> 7) & 0x2) |
            ((vertical_blank_start >> 5) & 0x8) |
            ((line_compare >> 4) & 0x10) |
            ((vertical_display_enable_end >> 3) & 0x40));
    case 0x08: return preset_row_scan;
    case 0x09: return max_scan_line;
    case 0x0A: return cursor_scanline_start;
    case 0x0B: return cursor_scanline_end;
    case 0x0C: return (uint8_t)(start_address & 0xFF);
    case 0x0D: return (uint8_t)(start_address >> 8);
    case 0x0E: return (uint8_t)(cursor_address >> 8);
    case 0x0F: return (uint8_t)(cursor_address & 0xFF);
    case 0x12: return (uint8_t)(vertical_display_enable_end & 0xFF);
    case 0x13: return offset_register;
    case 0x14: return underline_location_register;
    case 0x15: return (uint8_t)(vertical_blank_start & 0xFF);
    case 0x17: return crtc_mode;
    case 0x18: return (uint8_t)(line_compare & 0xFF);
    default:
        if (index_crtc < (int)sizeof(crtc)) return crtc[index_crtc];
        return 0;
    }
}

uint16_t VGAScreen::port3D5_read16() { return port3D5_read(); }

// ---------------------------------------------------------------------------
// 0x3DA – Input Status Register #1  (also resets ATC flip-flop)
// ---------------------------------------------------------------------------

uint8_t VGAScreen::port3DA_read()
{
    uint8_t value = port_3DA_value;

    if (!graphical_mode)
    {
        if (port_3DA_value & 1) port_3DA_value ^= 8;
        port_3DA_value ^= 1;
    }
    else
    {
        port_3DA_value ^= 1;
        port_3DA_value &= 1;
    }

    attribute_controller_index = -1;   // reset ATC flip-flop
    return value;
}

// ---------------------------------------------------------------------------
// 0x1CE / 0x1CF – Bochs VBE (SVGA) extensions
// ---------------------------------------------------------------------------

void VGAScreen::port1CE_write(uint16_t value) { dispi_index = (int16_t)value; }

void VGAScreen::port1CF_write(uint16_t value)
{
    bool was_enabled = svga_enabled;

    switch (dispi_index)
    {
    case 0: // Version
        if (value >= 0xB0C0 && value <= 0xB0C5) svga_version = value;
        break;
    case 1: // Width
        svga_width = (int)value;
        if (svga_width > (int)MAX_XRES) svga_width = MAX_XRES;
        break;
    case 2: // Height
        svga_height = (int)value;
        if (svga_height > (int)MAX_YRES) svga_height = MAX_YRES;
        break;
    case 3: // BPP
        svga_bpp = value;
        break;
    case 4: // Enable / options
        svga_enabled = (value & 1) == 1;
        if (svga_enabled && !(value & 0x80))
            std::memset(svga_memory, 0, vga_memory_size);
        dispi_enable_value = value;
        break;
    case 5: // Bank offset
        svga_bank_offset = (uint32_t)value << 16;
        break;
    case 8: // X offset
        if (svga_offset_x != (int)value)
        {
            svga_offset_x = value;
            svga_offset = (uint32_t)(svga_offset_y * svga_width + svga_offset_x);
            complete_redraw();
        }
        break;
    case 9: // Y offset
        if (svga_offset_y != (int)value)
        {
            svga_offset_y = value;
            svga_offset = (uint32_t)(svga_offset_y * svga_width + svga_offset_x);
            complete_redraw();
        }
        break;
    default:
        break;
    }

    // Guard against zero dimensions
    if (svga_enabled && (!svga_width || !svga_height))
        svga_enabled = false;

    if (svga_enabled && !was_enabled)
    {
        svga_offset = svga_offset_x = svga_offset_y = 0;
        graphical_mode = true;
        screen->set_mode(true);
        set_size_graphical(svga_width, svga_height,
            svga_width, svga_height, svga_bpp);
    }

    if (was_enabled && !svga_enabled)
    {
        bool is_graphical = !!(attribute_mode & 0x1);
        graphical_mode = is_graphical;
        screen->set_mode(is_graphical);
        update_vga_size();
        set_font_bitmap(false);
        complete_redraw();
    }

    if (!svga_enabled) svga_bank_offset = 0;

    update_layers();
}

uint16_t VGAScreen::port1CF_read() { return svga_register_read(dispi_index); }

uint16_t VGAScreen::svga_register_read(uint16_t n)
{
    switch (n)
    {
    case 0: return svga_version;
    case 1: return (uint16_t)(dispi_enable_value & 2 ? MAX_XRES : svga_width);
    case 2: return (uint16_t)(dispi_enable_value & 2 ? MAX_YRES : svga_height);
    case 3: return (uint16_t)(dispi_enable_value & 2 ? MAX_BPP : svga_bpp);
    case 4: return dispi_enable_value;
    case 5: return (uint16_t)(svga_bank_offset >> 16);
    case 6: return (uint16_t)(screen_width ? screen_width : 1);
    case 8: return (uint16_t)svga_offset_x;
    case 9: return (uint16_t)svga_offset_y;
    case 0x0A: return (uint16_t)(vga_memory_size / VGA_BANK_SIZE);
    default: return 0xFF;
    }
}