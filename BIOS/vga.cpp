#include "vga.h"
#include <cmath>
#include <cstdio>
#include <chrono>

enum device_endian {
#ifndef TARGET_NOT_USING_LEGACY_NATIVE_ENDIAN_API
	DEVICE_NATIVE_ENDIAN = 0,
#endif
	DEVICE_BIG_ENDIAN = 1,
	DEVICE_LITTLE_ENDIAN = 2,
};


static void check_graphics_mode_change(VGACommonState* s)
{
	static int last_graphic_mode = -1;
	static int last_bpp = -1;

	int is_graphic = (s->gr[VGA_GFX_MISC] & 0x01);
	int bpp = s->get_bpp(s);
	if (is_graphic != last_graphic_mode || bpp != last_bpp) {
		last_graphic_mode = is_graphic;
		last_bpp = bpp;

		if (is_graphic) {
			s->adapter->set_mode(true, s->svga_enabled);
		}
		else {
			s->adapter->set_mode(false,false);
		}
	}
}



static void check_resolution_change(VGACommonState* s)
{
	static int last_w = -1;
	static int last_h = -1;

	int w, h;
	s->get_resolution(s, &w, &h);

	if (w != last_w || h != last_h) {
		last_w = w;
		last_h = h;
		s->adapter->set_size_graphical(w, h);
		//printf("[VGA] Resolution changed → %dx%d\n", w, h);
		// ↑ put your logic here
	}
}




/*
 * QEMU VGA Emulator templates
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#define bswap16 _byteswap_ushort
#define bswap32 _byteswap_ulong
#define bswap64 _byteswap_uint64

 // Select correct swap based on size
#define le_bswap(v, size) \
    ((size) == 16 ? bswap16(v) : \
     (size) == 32 ? bswap32(v) : \
     (size) == 64 ? bswap64(v) : (v))

#define be_bswap(v, size) (v)

// 24-bit special case
static inline uint32_t bswap24(uint32_t v)
{
	return ((v & 0x0000FF) << 16) |
		(v & 0x00FF00) |
		((v & 0xFF0000) >> 16);
}

#define le_bswap24(v) bswap24(v)
#define be_bswap24(v) (v)

// pointer swap
#define le_bswaps(p, size) \
    do { *(p) = le_bswap(*(p), size); } while (0)

#define be_bswaps(p, size) do { } while (0)
static uint32_t expand4[256];
static uint16_t expand2[256];
static uint8_t expand4to8[16];
const uint8_t sr_mask[8] = {
	0x03,
	0x3d,
	0x0f,
	0x3f,
	0x0e,
	0x00,
	0x00,
	0xff,
};

const uint8_t gr_mask[16] = {
	0x0f, /* 0x00 */
	0x0f, /* 0x01 */
	0x0f, /* 0x02 */
	0x1f, /* 0x03 */
	0x03, /* 0x04 */
	0x7b, /* 0x05 */
	0x0f, /* 0x06 */
	0x0f, /* 0x07 */
	0xff, /* 0x08 */
	0x00, /* 0x09 */
	0x00, /* 0x0a */
	0x00, /* 0x0b */
	0x00, /* 0x0c */
	0x00, /* 0x0d */
	0x00, /* 0x0e */
	0x00, /* 0x0f */
};


static const uint32_t mask16[16] = {
	uint32_t(0x00000000),
	uint32_t(0x000000ff),
	uint32_t(0x0000ff00),
	uint32_t(0x0000ffff),
	uint32_t(0x00ff0000),
	uint32_t(0x00ff00ff),
	uint32_t(0x00ffff00),
	uint32_t(0x00ffffff),
	uint32_t(0xff000000),
	uint32_t(0xff0000ff),
	uint32_t(0xff00ff00),
	uint32_t(0xff00ffff),
	uint32_t(0xffff0000),
	uint32_t(0xffff00ff),
	uint32_t(0xffffff00),
	uint32_t(0xffffffff),
};
static inline unsigned int rgb_to_pixel32(unsigned int r, unsigned int g,
	unsigned int b)
{
	return (r << 16) | (g << 8) | b;
}
static inline int lduw_he_p(const void* ptr)
{
	uint16_t r;
	memcpy(&r, ptr, sizeof(r));
	return r;
}

static inline int ldsw_he_p(const void* ptr)
{
	int16_t r;
	memcpy(&r, ptr, sizeof(r));
	return r;
}

static inline void stw_he_p(void* ptr, uint16_t v)
{
	memcpy(ptr, &v, sizeof(v));
}

static inline void st24_he_p(void* ptr, uint32_t v)
{
	memcpy(ptr, &v, 3);
}

static inline int ldl_he_p(const void* ptr)
{
	int32_t r;
	memcpy(&r, ptr, sizeof(r));
	return r;
}

static inline void stl_he_p(void* ptr, uint32_t v)
{
	memcpy(ptr, &v, sizeof(v));
}

static inline uint64_t ldq_he_p(const void* ptr)
{
	uint64_t r;
	memcpy(&r, ptr, sizeof(r));
	return r;
}

static inline void stq_he_p(void* ptr, uint64_t v)
{
	memcpy(ptr, &v, sizeof(v));
}

static inline int lduw_le_p(const void* ptr)
{
	return (uint16_t)le_bswap(lduw_he_p(ptr), 16);
}

static inline int ldsw_le_p(const void* ptr)
{
	return (int16_t)le_bswap(lduw_he_p(ptr), 16);
}

static inline int ldl_le_p(const void* ptr)
{
	return le_bswap(ldl_he_p(ptr), 32);
}

static inline uint64_t ldq_le_p(const void* ptr)
{
	return le_bswap(ldq_he_p(ptr), 64);
}

static inline void stw_le_p(void* ptr, uint16_t v)
{
	stw_he_p(ptr, le_bswap(v, 16));
}

static inline void st24_le_p(void* ptr, uint32_t v)
{
	st24_he_p(ptr, le_bswap24(v));
}

static inline void stl_le_p(void* ptr, uint32_t v)
{
	stl_he_p(ptr, le_bswap(v, 32));
}

static inline void stq_le_p(void* ptr, uint64_t v)
{
	stq_he_p(ptr, le_bswap(v, 64));
}

static inline int lduw_be_p(const void* ptr)
{
	return (uint16_t)be_bswap(lduw_he_p(ptr), 16);
}

static inline int ldsw_be_p(const void* ptr)
{
	return (int16_t)be_bswap(lduw_he_p(ptr), 16);
}

static inline int ldl_be_p(const void* ptr)
{
	return be_bswap(ldl_he_p(ptr), 32);
}

static inline uint64_t ldq_be_p(const void* ptr)
{
	return be_bswap(ldq_he_p(ptr), 64);
}

static inline void stw_be_p(void* ptr, uint16_t v)
{
	stw_he_p(ptr, be_bswap(v, 16));
}

static inline void st24_be_p(void* ptr, uint32_t v)
{
	st24_he_p(ptr, be_bswap24(v));
}

static inline void stl_be_p(void* ptr, uint32_t v)
{
	stl_he_p(ptr, be_bswap(v, 32));
}

static inline void stq_be_p(void* ptr, uint64_t v)
{
	stq_he_p(ptr, be_bswap(v, 64));
}
static inline uint8_t vga_read_byte(VGACommonState* vga, uint32_t addr)
{
	return vga->vram_ptr[addr & vga->vbe_size_mask];
}

static inline uint16_t vga_read_word_le(VGACommonState* vga, uint32_t addr)
{
	uint32_t offset = addr & vga->vbe_size_mask & ~1;
	uint16_t* ptr = (uint16_t*)(vga->vram_ptr + offset);
	return lduw_le_p(ptr);
}

static inline uint16_t vga_read_word_be(VGACommonState* vga, uint32_t addr)
{
	uint32_t offset = addr & vga->vbe_size_mask & ~1;
	uint16_t* ptr = (uint16_t*)(vga->vram_ptr + offset);
	return lduw_be_p(ptr);
}

static inline uint32_t vga_read_dword_le(VGACommonState* vga, uint32_t addr)
{
	uint32_t offset = addr & vga->vbe_size_mask & ~3;
	uint32_t* ptr = (uint32_t*)(vga->vram_ptr + offset);
	return ldl_le_p(ptr);
}

#define PUT_PIXEL2(d, n, v) \
((uint32_t *)d)[2*(n)] = ((uint32_t *)d)[2*(n)+1] = (v)


static inline void vga_draw_glyph_line(uint8_t* d, int32_t font_data,
	int32_t xorcol, uint32_t bgcol)
{
	((int32_t*)d)[0] = (-((font_data >> 7)) & xorcol) ^ bgcol;
	((int32_t*)d)[1] = (-((font_data >> 6) & 1) & xorcol) ^ bgcol;
	((int32_t*)d)[2] = (-((font_data >> 5) & 1) & xorcol) ^ bgcol;
	((int32_t*)d)[3] = (-((font_data >> 4) & 1) & xorcol) ^ bgcol;
	((int32_t*)d)[4] = (-((font_data >> 3) & 1) & xorcol) ^ bgcol;
	((int32_t*)d)[5] = (-((font_data >> 2) & 1) & xorcol) ^ bgcol;
	((int32_t*)d)[6] = (-((font_data >> 1) & 1) & xorcol) ^ bgcol;
	((int32_t*)d)[7] = (-((font_data >> 0) & 1) & xorcol) ^ bgcol;
}

static void vga_draw_glyph8(uint8_t* d, int linesize,
	const uint8_t* font_ptr, int h,
	uint32_t fgcol, uint32_t bgcol)
{
	uint32_t font_data, xorcol;

	xorcol = bgcol ^ fgcol;
	do {
		font_data = font_ptr[0];
		vga_draw_glyph_line(d, font_data, xorcol, bgcol);
		font_ptr += 4;
		d += linesize;
	} while (--h);
}

static void vga_draw_glyph16(uint8_t* d, int linesize,
	const uint8_t* font_ptr, int h,
	uint32_t fgcol, uint32_t bgcol)
{
	uint32_t font_data, xorcol;

	xorcol = bgcol ^ fgcol;
	do {
		font_data = font_ptr[0];
		vga_draw_glyph_line(d, expand4to8[font_data >> 4],
			xorcol, bgcol);
		vga_draw_glyph_line(d + 32, expand4to8[font_data & 0x0f],
			xorcol, bgcol);
		font_ptr += 4;
		d += linesize;
	} while (--h);
}

static void vga_draw_glyph9(uint8_t* d, int linesize,
	const uint8_t* font_ptr, int h,
	uint32_t fgcol, uint32_t bgcol, int dup9)
{
	int32_t font_data, xorcol, v;

	xorcol = bgcol ^ fgcol;
	do {
		font_data = font_ptr[0];
		((int32_t*)d)[0] = (-((font_data >> 7)) & xorcol) ^ bgcol;
		((int32_t*)d)[1] = (-((font_data >> 6) & 1) & xorcol) ^ bgcol;
		((int32_t*)d)[2] = (-((font_data >> 5) & 1) & xorcol) ^ bgcol;
		((int32_t*)d)[3] = (-((font_data >> 4) & 1) & xorcol) ^ bgcol;
		((int32_t*)d)[4] = (-((font_data >> 3) & 1) & xorcol) ^ bgcol;
		((int32_t*)d)[5] = (-((font_data >> 2) & 1) & xorcol) ^ bgcol;
		((int32_t*)d)[6] = (-((font_data >> 1) & 1) & xorcol) ^ bgcol;
		v = (-((font_data >> 0) & 1) & xorcol) ^ bgcol;
		((int32_t*)d)[7] = v;
		if (dup9)
			((int32_t*)d)[8] = v;
		else
			((int32_t*)d)[8] = bgcol;
		font_ptr += 4;
		d += linesize;
	} while (--h);
}

/*
 * 4 color mode
 */
static void* vga_draw_line2(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	uint32_t plane_mask, * palette, data, v;
	int x;

	palette = vga->last_palette;
	plane_mask = mask16[vga->ar[VGA_ATC_PLANE_ENABLE] & 0xf];
	hpel &= 7;
	if (hpel) {
		width += 8;
		d = vga->panning_buf;
	}
	width >>= 3;
	for (x = 0; x < width; x++) {
		data = vga_read_dword_le(vga, addr & (VGA_VRAM_SIZE - 1));
		data &= plane_mask;
		v = expand2[GET_PLANE(data, 0)];
		v |= expand2[GET_PLANE(data, 2)] << 2;
		((uint32_t*)d)[0] = palette[v >> 12];
		((uint32_t*)d)[1] = palette[(v >> 8) & 0xf];
		((uint32_t*)d)[2] = palette[(v >> 4) & 0xf];
		((uint32_t*)d)[3] = palette[(v >> 0) & 0xf];

		v = expand2[GET_PLANE(data, 1)];
		v |= expand2[GET_PLANE(data, 3)] << 2;
		((uint32_t*)d)[4] = palette[v >> 12];
		((uint32_t*)d)[5] = palette[(v >> 8) & 0xf];
		((uint32_t*)d)[6] = palette[(v >> 4) & 0xf];
		((uint32_t*)d)[7] = palette[(v >> 0) & 0xf];
		d += 32;
		addr += 4;
	}
	return hpel ? vga->panning_buf + 4 * hpel : NULL;
}



/*
 * 4 color mode, dup2 horizontal
 */
static void* vga_draw_line2d2(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	uint32_t plane_mask, * palette, data, v;
	int x;

	palette = vga->last_palette;
	plane_mask = mask16[vga->ar[VGA_ATC_PLANE_ENABLE] & 0xf];
	hpel &= 7;
	if (hpel) {
		width += 8;
		d = vga->panning_buf;
	}
	width >>= 3;
	for (x = 0; x < width; x++) {
		data = vga_read_dword_le(vga, addr & (VGA_VRAM_SIZE - 1));
		data &= plane_mask;
		v = expand2[GET_PLANE(data, 0)];
		v |= expand2[GET_PLANE(data, 2)] << 2;
		PUT_PIXEL2(d, 0, palette[v >> 12]);
		PUT_PIXEL2(d, 1, palette[(v >> 8) & 0xf]);
		PUT_PIXEL2(d, 2, palette[(v >> 4) & 0xf]);
		PUT_PIXEL2(d, 3, palette[(v >> 0) & 0xf]);

		v = expand2[GET_PLANE(data, 1)];
		v |= expand2[GET_PLANE(data, 3)] << 2;
		PUT_PIXEL2(d, 4, palette[v >> 12]);
		PUT_PIXEL2(d, 5, palette[(v >> 8) & 0xf]);
		PUT_PIXEL2(d, 6, palette[(v >> 4) & 0xf]);
		PUT_PIXEL2(d, 7, palette[(v >> 0) & 0xf]);
		d += 64;
		addr += 4;
	}
	return hpel ? vga->panning_buf + 8 * hpel : NULL;
}

/*
 * 16 color mode
 */
static void* vga_draw_line4(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	uint32_t plane_mask, data, v, * palette;
	int x;

	palette = vga->last_palette;
	plane_mask = mask16[vga->ar[VGA_ATC_PLANE_ENABLE] & 0xf];
	hpel &= 7;
	if (hpel) {
		width += 8;
		d = vga->panning_buf;
	}
	width >>= 3;
	for (x = 0; x < width; x++) {
		data = vga_read_dword_le(vga, addr & (VGA_VRAM_SIZE - 1));
		if (data != 0) {
			std::cout << "something";
		}
		data &= plane_mask;
		v = expand4[GET_PLANE(data, 0)];
		v |= expand4[GET_PLANE(data, 1)] << 1;
		v |= expand4[GET_PLANE(data, 2)] << 2;
		v |= expand4[GET_PLANE(data, 3)] << 3;
		((uint32_t*)d)[0] = palette[v >> 28];
		((uint32_t*)d)[1] = palette[(v >> 24) & 0xf];
		((uint32_t*)d)[2] = palette[(v >> 20) & 0xf];
		((uint32_t*)d)[3] = palette[(v >> 16) & 0xf];
		((uint32_t*)d)[4] = palette[(v >> 12) & 0xf];
		((uint32_t*)d)[5] = palette[(v >> 8) & 0xf];
		((uint32_t*)d)[6] = palette[(v >> 4) & 0xf];
		((uint32_t*)d)[7] = palette[(v >> 0) & 0xf];
		d += 32;
		addr += 4;
	}
	return hpel ? vga->panning_buf + 4 * hpel : NULL;
}

/*
 * 16 color mode, dup2 horizontal
 */
static void* vga_draw_line4d2(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	uint32_t plane_mask, data, v, * palette;
	int x;

	palette = vga->last_palette;
	plane_mask = mask16[vga->ar[VGA_ATC_PLANE_ENABLE] & 0xf];
	hpel &= 7;
	if (hpel) {
		width += 8;
		d = vga->panning_buf;
	}
	width >>= 3;
	for (x = 0; x < width; x++) {
		data = vga_read_dword_le(vga, addr & (VGA_VRAM_SIZE - 1));
		data &= plane_mask;
		v = expand4[GET_PLANE(data, 0)];
		v |= expand4[GET_PLANE(data, 1)] << 1;
		v |= expand4[GET_PLANE(data, 2)] << 2;
		v |= expand4[GET_PLANE(data, 3)] << 3;
		PUT_PIXEL2(d, 0, palette[v >> 28]);
		PUT_PIXEL2(d, 1, palette[(v >> 24) & 0xf]);
		PUT_PIXEL2(d, 2, palette[(v >> 20) & 0xf]);
		PUT_PIXEL2(d, 3, palette[(v >> 16) & 0xf]);
		PUT_PIXEL2(d, 4, palette[(v >> 12) & 0xf]);
		PUT_PIXEL2(d, 5, palette[(v >> 8) & 0xf]);
		PUT_PIXEL2(d, 6, palette[(v >> 4) & 0xf]);
		PUT_PIXEL2(d, 7, palette[(v >> 0) & 0xf]);
		d += 64;
		addr += 4;
	}
	return hpel ? vga->panning_buf + 8 * hpel : NULL;
}

/*
 * 256 color mode, double pixels
 *
 * XXX: add plane_mask support (never used in standard VGA modes)
 */
static void* vga_draw_line8d2(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	uint32_t* palette;
	int x;

	palette = vga->last_palette;
	hpel = (hpel >> 1) & 3;

	/* For 256 color modes, we can adjust the source address and write directly
	 * to the destination, even if horizontal pel panning is active.  However,
	 * the loop below assumes that the address does not wrap in the middle of a
	 * plane.  If that happens...
	 */
	if (addr + (width >> 3) * 4 < VGA_VRAM_SIZE) {
		addr += hpel * 4;
		hpel = 0;
	}

	/* ... use the panning buffer as in planar modes.  */
	if (hpel) {
		width += 8;
		d = vga->panning_buf;
	}
	width >>= 3;
	for (x = 0; x < width; x++) {
		addr &= VGA_VRAM_SIZE - 1;
		PUT_PIXEL2(d, 0, palette[vga_read_byte(vga, addr + 0)]);
		PUT_PIXEL2(d, 1, palette[vga_read_byte(vga, addr + 1)]);
		PUT_PIXEL2(d, 2, palette[vga_read_byte(vga, addr + 2)]);
		PUT_PIXEL2(d, 3, palette[vga_read_byte(vga, addr + 3)]);
		d += 32;
		addr += 4;
	}
	return hpel ? vga->panning_buf + 8 * hpel : NULL;
}

/*
 * standard 256 color mode
 *
 * XXX: add plane_mask support (never used in standard VGA modes)
 */
static void* vga_draw_line8(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	uint32_t* palette;
	int x;

	palette = vga->last_palette;
	hpel = (hpel >> 1) & 3;
	if (hpel) {
		width += 8;
		d = vga->panning_buf;
	}
	width >>= 3;
	for (x = 0; x < width; x++) {
		((uint32_t*)d)[0] = palette[vga_read_byte(vga, addr + 0)];
		((uint32_t*)d)[1] = palette[vga_read_byte(vga, addr + 1)];
		((uint32_t*)d)[2] = palette[vga_read_byte(vga, addr + 2)];
		((uint32_t*)d)[3] = palette[vga_read_byte(vga, addr + 3)];
		((uint32_t*)d)[4] = palette[vga_read_byte(vga, addr + 4)];
		((uint32_t*)d)[5] = palette[vga_read_byte(vga, addr + 5)];
		((uint32_t*)d)[6] = palette[vga_read_byte(vga, addr + 6)];
		((uint32_t*)d)[7] = palette[vga_read_byte(vga, addr + 7)];
		d += 32;
		addr += 8;
	}
	return hpel ? vga->panning_buf + 4 * hpel : NULL;
}

/*
 * 15 bit color
 */
static void* vga_draw_line15_le(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	int w;
	uint32_t v, r, g, b;

	w = width;
	do {
		v = vga_read_word_le(vga, addr);
		r = (v >> 7) & 0xf8;
		g = (v >> 2) & 0xf8;
		b = (v << 3) & 0xf8;
		((uint32_t*)d)[0] = rgb_to_pixel32(r, g, b);
		addr += 2;
		d += 4;
	} while (--w != 0);
	return NULL;
}

static void* vga_draw_line15_be(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	int w;
	uint32_t v, r, g, b;

	w = width;
	do {
		v = vga_read_word_be(vga, addr);
		r = (v >> 7) & 0xf8;
		g = (v >> 2) & 0xf8;
		b = (v << 3) & 0xf8;
		((uint32_t*)d)[0] = rgb_to_pixel32(r, g, b);
		addr += 2;
		d += 4;
	} while (--w != 0);
	return NULL;
}

/*
 * 16 bit color
 */
static void* vga_draw_line16_le(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	int w;
	uint32_t v, r, g, b;

	w = width;
	do {
		v = vga_read_word_le(vga, addr);
		r = (v >> 8) & 0xf8;
		g = (v >> 3) & 0xfc;
		b = (v << 3) & 0xf8;
		((uint32_t*)d)[0] = rgb_to_pixel32(r, g, b);
		addr += 2;
		d += 4;
	} while (--w != 0);
	return NULL;
}

static void* vga_draw_line16_be(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	int w;
	uint32_t v, r, g, b;

	w = width;
	do {
		v = vga_read_word_be(vga, addr);
		r = (v >> 8) & 0xf8;
		g = (v >> 3) & 0xfc;
		b = (v << 3) & 0xf8;
		((uint32_t*)d)[0] = rgb_to_pixel32(r, g, b);
		addr += 2;
		d += 4;
	} while (--w != 0);
	return NULL;
}

/*
 * 24 bit color
 */
static void* vga_draw_line24_le(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	int w;
	uint32_t r, g, b;

	w = width;
	do {
		b = vga_read_byte(vga, addr + 0);
		g = vga_read_byte(vga, addr + 1);
		r = vga_read_byte(vga, addr + 2);
		((uint32_t*)d)[0] = rgb_to_pixel32(r, g, b);
		addr += 3;
		d += 4;
	} while (--w != 0);
	return NULL;
}

static void* vga_draw_line24_be(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	int w;
	uint32_t r, g, b;

	w = width;
	do {
		r = vga_read_byte(vga, addr + 0);
		g = vga_read_byte(vga, addr + 1);
		b = vga_read_byte(vga, addr + 2);
		((uint32_t*)d)[0] = rgb_to_pixel32(r, g, b);
		addr += 3;
		d += 4;
	} while (--w != 0);
	return NULL;
}

/*
 * 32 bit color
 */
static void* vga_draw_line32_le(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	int w;
	uint32_t r, g, b;

	w = width;
	do {
		b = vga_read_byte(vga, addr + 0);
		g = vga_read_byte(vga, addr + 1);
		r = vga_read_byte(vga, addr + 2);
		((uint32_t*)d)[0] = rgb_to_pixel32(r, g, b);
		addr += 4;
		d += 4;
	} while (--w != 0);
	return NULL;
}

static void* vga_draw_line32_be(VGACommonState* vga, uint8_t* d,
	uint32_t addr, int width, int hpel)
{
	int w;
	uint32_t r, g, b;

	w = width;
	do {
		r = vga_read_byte(vga, addr + 1);
		g = vga_read_byte(vga, addr + 2);
		b = vga_read_byte(vga, addr + 3);
		((uint32_t*)d)[0] = rgb_to_pixel32(r, g, b);
		addr += 4;
		d += 4;
	} while (--w != 0);
	return NULL;
}



static inline int clz64(uint64_t val)
{
	return val ? __lzcnt64(val) : 64;
}
static inline uint64_t pow2ceil(uint64_t value)
{
	int n = clz64(value - 1);

	if (!n) {
		/*
		 * @value - 1 has no leading zeroes, thus @value - 1 >= 2^63
		 * Therefore, either @value == 0 or @value > 2^63.
		 * If it's 0, return 1, else return 0.
		 */
		return !value;
	}
	return 0x8000000000000000ull >> (n - 1);
}

/*
 * QEMU VGA Emulator.
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 //#define DEBUG_VGA_MEM
 //#define DEBUG_VGA_REG

bool have_vga = true;

/* 16 state changes per vertical frame @60 Hz */
#define VGA_TEXT_CURSOR_PERIOD_MS       (1000 * 2 * 16 / 60)

/* Address mask for non-VESA modes.  */
#define VGA_VRAM_SIZE                   (256 * KiB)

/* This value corresponds to a shift of zero pixels
 * in 9-dot text mode.  In other modes, bit 3 is undefined;
 * we just ignore it, so that 8 corresponds to zero pixels
 * in all modes.
 */
#define VGA_HPEL_NEUTRAL		8

 /*
  * Video Graphics Array (VGA)
  *
  * Chipset docs for original IBM VGA:
  * http://www.mcamafia.de/pdf/ibm_vgaxga_trm2.pdf
  *
  * FreeVGA site:
  * http://www.osdever.net/FreeVGA/home.htm
  *
  * Standard VGA features and Bochs VBE extensions are implemented.
  */

  /* force some bits to zero */




static void vbe_update_vgaregs(VGACommonState* s);

static inline bool vbe_enabled(VGACommonState* s)
{
	return s->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED;
}

static inline uint8_t sr(VGACommonState* s, int idx)
{
	return vbe_enabled(s) ? s->sr_vbe[idx] : s->sr[idx];
}

static void vga_update_memory_access(VGACommonState* s)
{
	hwaddr base, offset, size;

	if (s->legacy_address_space == NULL) {
		return;
	}

	if (s->has_chain4_alias) {
		//memory_region_del_subregion(s->legacy_address_space, &s->chain4_alias);
		//object_unparent(OBJECT(&s->chain4_alias));
		s->has_chain4_alias = false;
		s->plane_updated = 0xf;
	}
	if ((sr(s, VGA_SEQ_PLANE_WRITE) & VGA_SR02_ALL_PLANES) ==
		VGA_SR02_ALL_PLANES && sr(s, VGA_SEQ_MEMORY_MODE) & VGA_SR04_CHN_4M) {
		offset = 0;
		switch ((s->gr[VGA_GFX_MISC] >> 2) & 3) {
		case 0:
			base = 0xa0000;
			size = 0x20000;
			break;
		case 1:
			base = 0xa0000;
			size = 0x10000;
			offset = s->bank_offset;
			break;
		case 2:
			base = 0xb0000;
			size = 0x8000;
			break;
		case 3:
		default:
			base = 0xb8000;
			size = 0x8000;
			break;
		}
		assert(offset + size <= s->vram_size);
		/*memory_region_init_alias(&s->chain4_alias, memory_region_owner(&s->vram),
			"vga.chain4", &s->vram, offset, size);
		memory_region_add_subregion_overlap(s->legacy_address_space, base,
			&s->chain4_alias, 2);*/
		s->has_chain4_alias = true;
	}
}

static void vga_dumb_update_retrace_info(VGACommonState* s)
{
	(void)s;
}
static uint8_t vga_precise_retrace(VGACommonState* s)
{
	struct vga_precise_retrace* r = &s->retrace_info.precise;
	uint8_t val = s->st01 & ~(ST01_V_RETRACE | ST01_DISP_ENABLE);

	if (r->total_chars) {
		int cur_line, cur_line_char, cur_char;
		int64_t cur_tick;

		cur_tick = 1;

		cur_char = (cur_tick / r->ticks_per_char) % r->total_chars;
		cur_line = cur_char / r->htotal;

		if (cur_line >= r->vstart && cur_line <= r->vend) {
			val |= ST01_V_RETRACE | ST01_DISP_ENABLE;
		}
		else {
			cur_line_char = cur_char % r->htotal;
			if (cur_line_char >= r->hstart && cur_line_char <= r->hend) {
				val |= ST01_DISP_ENABLE;
			}
		}

		return val;
	}
	else {
		return s->st01 ^ (ST01_V_RETRACE | ST01_DISP_ENABLE);
	}
	
}
static void vga_precise_update_retrace_info(VGACommonState* s)
{
	int htotal_chars;
	int hretr_start_char;
	int hretr_skew_chars;
	int hretr_end_char;

	int vtotal_lines;
	int vretr_start_line;
	int vretr_end_line;

	int dots;

	int clocking_mode;
	int clock_sel;
	const int clk_hz[] = { 25175000, 28322000, 25175000, 25175000 };
	int64_t chars_per_sec;
	struct vga_precise_retrace* r = &s->retrace_info.precise;

	htotal_chars = s->cr[VGA_CRTC_H_TOTAL] + 5;
	hretr_start_char = s->cr[VGA_CRTC_H_SYNC_START];
	hretr_skew_chars = (s->cr[VGA_CRTC_H_SYNC_END] >> 5) & 3;
	hretr_end_char = s->cr[VGA_CRTC_H_SYNC_END] & 0x1f;

	vtotal_lines = (s->cr[VGA_CRTC_V_TOTAL] |
		(((s->cr[VGA_CRTC_OVERFLOW] & 1) |
			((s->cr[VGA_CRTC_OVERFLOW] >> 4) & 2)) << 8)) + 2;
	vretr_start_line = s->cr[VGA_CRTC_V_SYNC_START] |
		((((s->cr[VGA_CRTC_OVERFLOW] >> 2) & 1) |
			((s->cr[VGA_CRTC_OVERFLOW] >> 6) & 2)) << 8);
	vretr_end_line = s->cr[VGA_CRTC_V_SYNC_END] & 0xf;

	clocking_mode = (sr(s, VGA_SEQ_CLOCK_MODE) >> 3) & 1;
	clock_sel = (s->msr >> 2) & 3;
	dots = (s->msr & 1) ? 8 : 9;

	chars_per_sec = clk_hz[clock_sel] / dots;

	htotal_chars <<= clocking_mode;

	r->total_chars = vtotal_lines * htotal_chars;
	if (r->freq) {
		r->ticks_per_char = NANOSECONDS_PER_SECOND / (r->total_chars * r->freq);
	}
	else {
		r->ticks_per_char = NANOSECONDS_PER_SECOND / chars_per_sec;
	}

	r->vstart = vretr_start_line;
	r->vend = r->vstart + vretr_end_line + 1;

	r->hstart = hretr_start_char + hretr_skew_chars;
	r->hend = r->hstart + hretr_end_char + 1;
	r->htotal = htotal_chars;

}

static uint8_t vga_dumb_retrace(VGACommonState* s)
{
	return s->st01 ^ (ST01_V_RETRACE | ST01_DISP_ENABLE);
}

int vga_ioport_invalid(VGACommonState* s, uint32_t addr)
{
	if (s->msr & VGA_MIS_COLOR) {
		/* Color */
		return (addr >= 0x3b0 && addr <= 0x3bf);
	}
	else {
		/* Monochrome */
		return (addr >= 0x3d0 && addr <= 0x3df);
	}
}

uint32_t vga_ioport_read(void* opaque, uint32_t addr)
{
	VGACommonState* s = (VGACommonState*)opaque;
	int val, index;

	if (vga_ioport_invalid(s, addr)) {
		val = 0xff;
	}
	else {
		switch (addr) {
		case VGA_ATT_W:
			if (s->ar_flip_flop == 0) {
				val = s->ar_index;
			}
			else {
				val = 0;
			}
			break;
		case VGA_ATT_R:
			index = s->ar_index & 0x1f;
			if (index < VGA_ATT_C) {
				val = s->ar[index];
			}
			else {
				val = 0;
			}
			break;
		case VGA_MIS_W:
			val = s->st00;
			break;
		case VGA_SEQ_I:
			val = s->sr_index;
			break;
		case VGA_SEQ_D:
			val = s->sr[s->sr_index];
#ifdef DEBUG_VGA_REG
			printf("vga: read SR%x = 0x%02x\n", s->sr_index, val);
#endif
			break;
		case VGA_PEL_IR:
			val = s->dac_state;
			break;
		case VGA_PEL_IW:
			val = s->dac_write_index;
			break;
		case VGA_PEL_D:
			val = s->palette[s->dac_read_index * 3 + s->dac_sub_index];
			if (++s->dac_sub_index == 3) {
				s->dac_sub_index = 0;
				s->dac_read_index++;
			}
			break;
		case VGA_FTC_R:
			val = s->fcr;
			break;
		case VGA_MIS_R:
			val = s->msr;
			break;
		case VGA_GFX_I:
			val = s->gr_index;
			break;
		case VGA_GFX_D:
			val = s->gr[s->gr_index];
#ifdef DEBUG_VGA_REG
			printf("vga: read GR%x = 0x%02x\n", s->gr_index, val);
#endif
			break;
		case VGA_CRT_IM:
		case VGA_CRT_IC:
			val = s->cr_index;
			break;
		case VGA_CRT_DM:
		case VGA_CRT_DC:
			val = s->cr[s->cr_index];
#ifdef DEBUG_VGA_REG
			printf("vga: read CR%x = 0x%02x\n", s->cr_index, val);
#endif
			break;
		case VGA_IS1_RM:
		case VGA_IS1_RC:
			/* just toggle to fool polling */
			val = s->st01 = s->retrace(s);
			s->ar_flip_flop = 0;
			break;
		default:
			val = 0x00;
			break;
		}
	}
	return val;
}

void vga_ioport_write(void* opaque, uint32_t addr, uint32_t val)
{
	VGACommonState* s = (VGACommonState*)opaque;
	int index;

	/* check port range access depending on color/monochrome mode */
	if (vga_ioport_invalid(s, addr)) {
		return;
	}

	switch (addr) {
	case VGA_ATT_W:
		if (s->ar_flip_flop == 0) {
			val &= 0x3f;
			s->ar_index = val;
		}
		else {
			index = s->ar_index & 0x1f;
			switch (index) {
			case VGA_ATC_PALETTE0:
			case VGA_ATC_PALETTE1:
			case VGA_ATC_PALETTE2:
			case VGA_ATC_PALETTE3:
			case VGA_ATC_PALETTE4:
			case VGA_ATC_PALETTE5:
			case VGA_ATC_PALETTE6:
			case VGA_ATC_PALETTE7:
			case VGA_ATC_PALETTE8:
			case VGA_ATC_PALETTE9:
			case VGA_ATC_PALETTEA:
			case VGA_ATC_PALETTEB:
			case VGA_ATC_PALETTEC:
			case VGA_ATC_PALETTED:
			case VGA_ATC_PALETTEE:
			case VGA_ATC_PALETTEF:
				s->ar[index] = val & 0x3f;
				break;
			case VGA_ATC_MODE:
				s->ar[index] = val & ~0x10;
				break;
			case VGA_ATC_OVERSCAN:
				s->ar[index] = val;
				break;
			case VGA_ATC_PLANE_ENABLE:
				s->ar[index] = val & ~0xc0;
				break;
			case VGA_ATC_PEL:
				s->ar[index] = val & ~0xf0;
				break;
			case VGA_ATC_COLOR_PAGE:
				s->ar[index] = val & ~0xf0;
				break;
			default:
				break;
			}
		}
		s->ar_flip_flop ^= 1;
		break;
	case VGA_MIS_W:
		s->msr = val & ~0x10;
		s->update_retrace_info(s);
		break;
	case VGA_SEQ_I:
		s->sr_index = val & 7;
		break;
	case VGA_SEQ_D:
#ifdef DEBUG_VGA_REG
		printf("vga: write SR%x = 0x%02x\n", s->sr_index, val);
#endif
		s->sr[s->sr_index] = val & sr_mask[s->sr_index];
		if (s->sr_index == VGA_SEQ_CLOCK_MODE) {
			s->update_retrace_info(s);
		}
		vga_update_memory_access(s);
		check_graphics_mode_change(s);
		break;
	case VGA_PEL_IR:
		s->dac_read_index = val;
		s->dac_sub_index = 0;
		s->dac_state = 3;
		break;
	case VGA_PEL_IW:
		s->dac_write_index = val;
		s->dac_sub_index = 0;
		s->dac_state = 0;
		break;
	case VGA_PEL_D:
		s->dac_cache[s->dac_sub_index] = val;
		if (++s->dac_sub_index == 3) {
			memcpy(&s->palette[s->dac_write_index * 3], s->dac_cache, 3);
			s->dac_sub_index = 0;
			s->dac_write_index++;
		}
		break;
	case VGA_GFX_I:
		s->gr_index = val & 0x0f;
		break;
	case VGA_GFX_D:
#ifdef DEBUG_VGA_REG
		printf("vga: write GR%x = 0x%02x\n", s->gr_index, val);
#endif
		s->gr[s->gr_index] = val & gr_mask[s->gr_index];
		vbe_update_vgaregs(s);
		vga_update_memory_access(s);
		check_graphics_mode_change(s);
		break;
	case VGA_CRT_IM:
	case VGA_CRT_IC:
		s->cr_index = val;
		break;
	case VGA_CRT_DM:
	case VGA_CRT_DC:
#ifdef DEBUG_VGA_REG
		printf("vga: write CR%x = 0x%02x\n", s->cr_index, val);
#endif
		/* handle CR0-7 protection */
		if ((s->cr[VGA_CRTC_V_SYNC_END] & VGA_CR11_LOCK_CR0_CR7) &&
			s->cr_index <= VGA_CRTC_OVERFLOW) {
			/* can always write bit 4 of CR7 */
			if (s->cr_index == VGA_CRTC_OVERFLOW) {
				s->cr[VGA_CRTC_OVERFLOW] = (s->cr[VGA_CRTC_OVERFLOW] & ~0x10) |
					(val & 0x10);
				vbe_update_vgaregs(s);
			}
			return;
		}
		s->cr[s->cr_index] = val;
		vbe_update_vgaregs(s);

		switch (s->cr_index) {
		case VGA_CRTC_H_TOTAL:
		case VGA_CRTC_H_SYNC_START:
		case VGA_CRTC_H_SYNC_END:
		case VGA_CRTC_V_TOTAL:
		case VGA_CRTC_OVERFLOW:
		case VGA_CRTC_V_SYNC_END:
		case VGA_CRTC_MODE:
			s->update_retrace_info(s);
			check_resolution_change(s);
			break;
		}
		break;
	case VGA_IS1_RM:
	case VGA_IS1_RC:
		s->fcr = val & 0x10;
		break;
	}
}

/*
 * Sanity check vbe register writes.
 *
 * As we don't have a way to signal errors to the guest in the bochs
 * dispi interface we'll go adjust the registers to the closest valid
 * value.
 */
static void vbe_fixup_regs(VGACommonState* s)
{
	uint16_t* r = s->vbe_regs;
	uint32_t bits, linelength, maxy, offset;

	if (!vbe_enabled(s)) {
		/* vbe is turned off -- nothing to do */
		return;
	}

	/* check depth */
	switch (r[VBE_DISPI_INDEX_BPP]) {
	case 4:
	case 8:
	case 16:
	case 24:
	case 32:
		bits = r[VBE_DISPI_INDEX_BPP];
		break;
	case 15:
		bits = 16;
		break;
	default:
		bits = r[VBE_DISPI_INDEX_BPP] = 8;
		break;
	}

	/* check width */
	r[VBE_DISPI_INDEX_XRES] &= ~7u;
	if (r[VBE_DISPI_INDEX_XRES] == 0) {
		r[VBE_DISPI_INDEX_XRES] = 8;
	}
	if (r[VBE_DISPI_INDEX_XRES] > VBE_DISPI_MAX_XRES) {
		r[VBE_DISPI_INDEX_XRES] = VBE_DISPI_MAX_XRES;
	}
	r[VBE_DISPI_INDEX_VIRT_WIDTH] &= ~7u;
	if (r[VBE_DISPI_INDEX_VIRT_WIDTH] > VBE_DISPI_MAX_XRES) {
		r[VBE_DISPI_INDEX_VIRT_WIDTH] = VBE_DISPI_MAX_XRES;
	}
	if (r[VBE_DISPI_INDEX_VIRT_WIDTH] < r[VBE_DISPI_INDEX_XRES]) {
		r[VBE_DISPI_INDEX_VIRT_WIDTH] = r[VBE_DISPI_INDEX_XRES];
	}

	/* check height */
	linelength = r[VBE_DISPI_INDEX_VIRT_WIDTH] * bits / 8;
	maxy = s->vbe_size / linelength;
	if (r[VBE_DISPI_INDEX_YRES] == 0) {
		r[VBE_DISPI_INDEX_YRES] = 1;
	}
	if (r[VBE_DISPI_INDEX_YRES] > VBE_DISPI_MAX_YRES) {
		r[VBE_DISPI_INDEX_YRES] = VBE_DISPI_MAX_YRES;
	}
	if (r[VBE_DISPI_INDEX_YRES] > maxy) {
		r[VBE_DISPI_INDEX_YRES] = maxy;
	}

	/* check offset */
	if (r[VBE_DISPI_INDEX_X_OFFSET] > VBE_DISPI_MAX_XRES) {
		r[VBE_DISPI_INDEX_X_OFFSET] = VBE_DISPI_MAX_XRES;
	}
	if (r[VBE_DISPI_INDEX_Y_OFFSET] > VBE_DISPI_MAX_YRES) {
		r[VBE_DISPI_INDEX_Y_OFFSET] = VBE_DISPI_MAX_YRES;
	}
	offset = r[VBE_DISPI_INDEX_X_OFFSET] * bits / 8;
	offset += r[VBE_DISPI_INDEX_Y_OFFSET] * linelength;
	if (offset + r[VBE_DISPI_INDEX_YRES] * linelength > s->vbe_size) {
		r[VBE_DISPI_INDEX_Y_OFFSET] = 0;
		offset = r[VBE_DISPI_INDEX_X_OFFSET] * bits / 8;
		if (offset + r[VBE_DISPI_INDEX_YRES] * linelength > s->vbe_size) {
			r[VBE_DISPI_INDEX_X_OFFSET] = 0;
			offset = 0;
		}
	}

	/* update vga state */
	r[VBE_DISPI_INDEX_VIRT_HEIGHT] = maxy;
	s->vbe_line_offset = linelength;
	s->vbe_start_addr = offset / 4;
}

/* we initialize the VGA graphic mode */
static void vbe_update_vgaregs(VGACommonState* s)
{
	int h, shift_control;

	if (!vbe_enabled(s)) {
		/* vbe is turned off -- nothing to do */
		return;
	}

	/* graphic mode + memory map 1 */
	s->gr[VGA_GFX_MISC] = (s->gr[VGA_GFX_MISC] & ~0x0c) | 0x04 |
		VGA_GR06_GRAPHICS_MODE;
	s->cr[VGA_CRTC_MODE] |= 3; /* no CGA modes */
	s->cr[VGA_CRTC_OFFSET] = s->vbe_line_offset >> 3;
	/* width */
	s->cr[VGA_CRTC_H_DISP] =
		(s->vbe_regs[VBE_DISPI_INDEX_XRES] >> 3) - 1;
	/* height (only meaningful if < 1024) */
	h = s->vbe_regs[VBE_DISPI_INDEX_YRES] - 1;
	s->cr[VGA_CRTC_V_DISP_END] = h;
	s->cr[VGA_CRTC_OVERFLOW] = (s->cr[VGA_CRTC_OVERFLOW] & ~0x42) |
		((h >> 7) & 0x02) | ((h >> 3) & 0x40);
	/* line compare to 1023 */
	s->cr[VGA_CRTC_LINE_COMPARE] = 0xff;
	s->cr[VGA_CRTC_OVERFLOW] |= 0x10;
	s->cr[VGA_CRTC_MAX_SCAN] |= 0x40;

	if (s->vbe_regs[VBE_DISPI_INDEX_BPP] == 4) {
		shift_control = 0;
		s->sr_vbe[VGA_SEQ_CLOCK_MODE] &= ~8; /* no double line */
	}
	else {
		shift_control = 2;
		/* set chain 4 mode */
		s->sr_vbe[VGA_SEQ_MEMORY_MODE] |= VGA_SR04_CHN_4M;
		/* activate all planes */
		s->sr_vbe[VGA_SEQ_PLANE_WRITE] |= VGA_SR02_ALL_PLANES;
	}
	s->gr[VGA_GFX_MODE] = (s->gr[VGA_GFX_MODE] & ~0x60) |
		(shift_control << 5);
	s->cr[VGA_CRTC_MAX_SCAN] &= ~0x9f; /* no double scan */
}

static uint32_t vbe_ioport_read_index(void* opaque, uint32_t addr)
{
	VGACommonState* s = (VGACommonState*)opaque;
	return s->vbe_index;
}

uint32_t vbe_ioport_read_data(void* opaque, uint32_t addr)
{
	VGACommonState* s = (VGACommonState*)opaque;
	uint32_t val;

	if (s->vbe_index < VBE_DISPI_INDEX_NB) {
		if (s->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_GETCAPS) {
			switch (s->vbe_index) {
				/* XXX: do not hardcode ? */
			case VBE_DISPI_INDEX_XRES:
				val = VBE_DISPI_MAX_XRES;
				break;
			case VBE_DISPI_INDEX_YRES:
				val = VBE_DISPI_MAX_YRES;
				break;
			case VBE_DISPI_INDEX_BPP:
				val = VBE_DISPI_MAX_BPP;
				break;
			default:
				val = s->vbe_regs[s->vbe_index];
				break;
			}
		}
		else {
			val = s->vbe_regs[s->vbe_index];
		}
	}
	else if (s->vbe_index == VBE_DISPI_INDEX_VIDEO_MEMORY_64K) {
		val = s->vbe_size / (64 * KiB);
	}
	else {
		val = 0;
	}
	return val;
}

void vbe_ioport_write_index(void* opaque, uint32_t addr, uint32_t val)
{
	VGACommonState* s = (VGACommonState*)opaque;
	s->vbe_index = val;
}

void vbe_ioport_write_data(void* opaque, uint32_t addr, uint32_t val)
{

	VGACommonState* s = (VGACommonState*)opaque;

	if (s->vbe_index <= VBE_DISPI_INDEX_NB) {
		switch (s->vbe_index) {
		case VBE_DISPI_INDEX_ID:
			if (val == VBE_DISPI_ID0 ||
				val == VBE_DISPI_ID1 ||
				val == VBE_DISPI_ID2 ||
				val == VBE_DISPI_ID3 ||
				val == VBE_DISPI_ID4 ||
				val == VBE_DISPI_ID5) {
				s->vbe_regs[s->vbe_index] = val;
			}
			break;
		case VBE_DISPI_INDEX_XRES:
		case VBE_DISPI_INDEX_YRES:
		case VBE_DISPI_INDEX_BPP:
		case VBE_DISPI_INDEX_VIRT_WIDTH:
		case VBE_DISPI_INDEX_X_OFFSET:
		case VBE_DISPI_INDEX_Y_OFFSET:
			s->vbe_regs[s->vbe_index] = val;
			vbe_fixup_regs(s);
			vbe_update_vgaregs(s);
			break;
		case VBE_DISPI_INDEX_BANK:
			val &= s->vbe_bank_mask;
			s->vbe_regs[s->vbe_index] = val;
			s->bank_offset = (val << 16);
			vga_update_memory_access(s);
			break;
		case VBE_DISPI_INDEX_ENABLE:
			if ((val & VBE_DISPI_ENABLED) &&
				!(s->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED)) {

				s->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH] = 0;
				s->vbe_regs[VBE_DISPI_INDEX_X_OFFSET] = 0;
				s->vbe_regs[VBE_DISPI_INDEX_Y_OFFSET] = 0;
				s->vbe_regs[VBE_DISPI_INDEX_ENABLE] |= VBE_DISPI_ENABLED;
				vbe_fixup_regs(s);
				vbe_update_vgaregs(s);
				/* clear the screen */
				s->svga_enabled = true;
				if (!(val & VBE_DISPI_NOCLEARMEM)) {
					memset(s->vram_ptr, 0,
						s->vbe_regs[VBE_DISPI_INDEX_YRES] * s->vbe_line_offset);
				}
			}
			else {
				s->bank_offset = 0;
			}
			s->dac_8bit = (val & VBE_DISPI_8BIT_DAC) > 0;
			s->vbe_regs[s->vbe_index] = val;
			vga_update_memory_access(s);
			break;
		default:
			break;
		}
	}
}

/* called for accesses between 0xa0000 and 0xc0000 */
uint32_t vga_mem_readb(VGACommonState* s, hwaddr addr)
{
	int memory_map_mode, plane;
	uint32_t ret;

	/* convert to VGA memory offset */
	memory_map_mode = (s->gr[VGA_GFX_MISC] >> 2) & 3;
	addr &= 0x1ffff;
	switch (memory_map_mode) {
	case 0:
		break;
	case 1:
		if (addr >= 0x10000)
			return 0xff;
		addr += s->bank_offset;
		break;
	case 2:
		addr -= 0x10000;
		if (addr >= 0x8000)
			return 0xff;
		break;
	default:
	case 3:
		addr -= 0x18000;
		if (addr >= 0x8000)
			return 0xff;
		break;
	}

	if (sr(s, VGA_SEQ_MEMORY_MODE) & VGA_SR04_CHN_4M) {
		/* chain4 mode */
		plane = addr & 3;
		addr &= ~3;
	}
	else if (s->gr[VGA_GFX_MODE] & VGA_GR05_HOST_ODD_EVEN) {
		/* odd/even mode (aka text mode mapping) */
		plane = (s->gr[VGA_GFX_PLANE_READ] & 2) | (addr & 1);
	}
	else {
		/* standard VGA latched access */
		plane = s->gr[VGA_GFX_PLANE_READ];
	}

	if (s->gr[VGA_GFX_MISC] & VGA_GR06_CHAIN_ODD_EVEN) {
		addr &= ~1;
	}

	/* Doubleword/word mode.  See comment in vga_mem_writeb */
	if (s->cr[VGA_CRTC_UNDERLINE] & VGA_CR14_DW) {
		addr >>= 2;
	}
	else if ((s->gr[VGA_GFX_MODE] & VGA_GR05_HOST_ODD_EVEN) &&
		(s->cr[VGA_CRTC_MODE] & VGA_CR17_WORD_BYTE) == 0) {
		addr >>= 1;
	}

	if (addr * sizeof(uint32_t) >= s->vram_size) {
		return 0xff;
	}

	if (s->sr[VGA_SEQ_MEMORY_MODE] & VGA_SR04_CHN_4M) {
		/* chain 4 mode: simplified access (but it should use the same
		 * algorithms as below, see e.g. vga_mem_writeb's plane mask check).
		 */
		return s->vram_ptr[(addr << 2) | plane];
	}

	s->latch = ((uint32_t*)s->vram_ptr)[addr];
	if (!(s->gr[VGA_GFX_MODE] & 0x08)) {
		/* read mode 0 */
		ret = GET_PLANE(s->latch, plane);
	}
	else {
		/* read mode 1 */
		ret = (s->latch ^ mask16[s->gr[VGA_GFX_COMPARE_VALUE]]) &
			mask16[s->gr[VGA_GFX_COMPARE_MASK]];
		ret |= ret >> 16;
		ret |= ret >> 8;
		ret = (~ret) & 0xff;
	}

	return ret;
}

/* called for accesses between 0xa0000 and 0xc0000 */
void vga_mem_writeb(VGACommonState* s, hwaddr addr, uint32_t val)
{
	int memory_map_mode, write_mode, b, func_select, mask;
	uint32_t write_mask, bit_mask, set_mask;
	int plane = 0;

#ifdef DEBUG_VGA_MEM
	printf("vga: [0x" HWADDR_FMT_plx "] = 0x%02x\n", addr, val);
#endif
	/* convert to VGA memory offset */
	memory_map_mode = (s->gr[VGA_GFX_MISC] >> 2) & 3;
	addr &= 0x1ffff;
	switch (memory_map_mode) {
	case 0:
		break;
	case 1:
		if (addr >= 0x10000)
			return;
		addr += s->bank_offset;
		break;
	case 2:
		addr -= 0x10000;
		if (addr >= 0x8000)
			return;
		break;
	default:
	case 3:
		addr -= 0x18000;
		if (addr >= 0x8000)
			return;
		break;
	}

	mask = sr(s, VGA_SEQ_PLANE_WRITE);
	if (sr(s, VGA_SEQ_MEMORY_MODE) & VGA_SR04_CHN_4M) {
		/* chain 4 mode : simplest access */
		plane = addr & 3;
		mask &= (1 << plane);
		addr &= ~3;
	}
	else {
		if ((sr(s, VGA_SEQ_MEMORY_MODE) & VGA_SR04_SEQ_MODE) == 0) {
			mask &= (addr & 1) ? 0x0a : 0x05;
		}
		if (s->gr[VGA_GFX_MISC] & VGA_GR06_CHAIN_ODD_EVEN) {
			addr &= ~1;
		}
	}

	/* Doubleword/word mode.  These should be honored when displaying,
	 * not when reading/writing to memory!  For example, chain4 modes
	 * use double-word mode and, on real hardware, would fetch bytes
	 * 0,1,2,3, 16,17,18,19, 32,33,34,35, etc.  Text modes use word
	 * mode and, on real hardware, would fetch bytes 0,1, 8,9, etc.
	 *
	 * QEMU instead shifted addresses on memory accesses because it
	 * allows more optimizations (e.g. chain4_alias) and simplifies
	 * the draw_line handlers. Unfortunately, there is one case where
	 * the difference shows.  When fetching font data, accesses are
	 * always in consecutive bytes, even if the text/attribute pairs
	 * are done in word mode.  Hence, doing a right shift when operating
	 * on font data is wrong.  So check the odd/even mode bits together with
	 * word mode bit.  The odd/even read bit is 0 when reading font data,
	 * and the odd/even write bit is 1 when writing it.
	 */
	if (s->cr[VGA_CRTC_UNDERLINE] & VGA_CR14_DW) {
		addr >>= 2;
	}
	else if ((sr(s, VGA_SEQ_MEMORY_MODE) & VGA_SR04_SEQ_MODE) == 0 &&
		(s->cr[VGA_CRTC_MODE] & VGA_CR17_WORD_BYTE) == 0) {
		addr >>= 1;
	}

	if (addr * sizeof(uint32_t) >= s->vram_size) {
		return;
	}

	if (sr(s, VGA_SEQ_MEMORY_MODE) & VGA_SR04_CHN_4M) {
		if (mask) {
			s->vram_ptr[(addr << 2) | plane] = val;
#ifdef DEBUG_VGA_MEM
			printf("vga: chain4: [0x" HWADDR_FMT_plx "]\n", addr);
#endif
			s->plane_updated |= mask; /* only used to detect font change */
			memory_region_set_dirty(&s->vram, addr, 1);
		}
		return;
	}

	/* standard VGA latched access */
	write_mode = s->gr[VGA_GFX_MODE] & 3;
	switch (write_mode) {
	default:
	case 0:
		/* rotate */
		b = s->gr[VGA_GFX_DATA_ROTATE] & 7;
		val = ((val >> b) | (val << (8 - b))) & 0xff;
		val |= val << 8;
		val |= val << 16;

		/* apply set/reset mask */
		set_mask = mask16[s->gr[VGA_GFX_SR_ENABLE]];
		val = (val & ~set_mask) |
			(mask16[s->gr[VGA_GFX_SR_VALUE]] & set_mask);
		bit_mask = s->gr[VGA_GFX_BIT_MASK];
		break;
	case 1:
		val = s->latch;
		goto do_write;
	case 2:
		val = mask16[val & 0x0f];
		bit_mask = s->gr[VGA_GFX_BIT_MASK];
		break;
	case 3:
		/* rotate */
		b = s->gr[VGA_GFX_DATA_ROTATE] & 7;
		val = (val >> b) | (val << (8 - b));

		bit_mask = s->gr[VGA_GFX_BIT_MASK] & val;
		val = mask16[s->gr[VGA_GFX_SR_VALUE]];
		break;
	}

	/* apply logical operation */
	func_select = s->gr[VGA_GFX_DATA_ROTATE] >> 3;
	switch (func_select) {
	case 0:
	default:
		/* nothing to do */
		break;
	case 1:
		/* and */
		val &= s->latch;
		break;
	case 2:
		/* or */
		val |= s->latch;
		break;
	case 3:
		/* xor */
		val ^= s->latch;
		break;
	}

	/* apply bit mask */
	bit_mask |= bit_mask << 8;
	bit_mask |= bit_mask << 16;
	val = (val & bit_mask) | (s->latch & ~bit_mask);
	if (mask & 0x1) s->plane0[addr] = (val >> 0) & 0xFF;
	if (mask & 0x2) s->plane1[addr] = (val >> 8) & 0xFF;
	if (mask & 0x4) s->plane2[addr] = (val >> 16) & 0xFF;
	if (mask & 0x8) s->plane3[addr] = (val >> 24) & 0xFF;

do_write:
	/* mask data according to sr[2] */
	s->plane_updated |= mask; /* only used to detect font change */
	write_mask = mask16[mask];
	((uint32_t*)s->vram_ptr)[addr] =
		(((uint32_t*)s->vram_ptr)[addr] & ~write_mask) |
		(val & write_mask);
#ifdef DEBUG_VGA_MEM
	printf("vga: latch: [0x" HWADDR_FMT_plx "] mask=0x%08x val=0x%08x\n",
		addr * 4, write_mask, val);
#endif
	//memory_region_set_dirty(&s->vram, addr << 2, sizeof(uint32_t));
}

typedef void* vga_draw_line_func(VGACommonState* s1, uint8_t* d,
	uint32_t srcaddr, int width, int hpel);


/* return true if the palette was modified */
static int update_palette16(VGACommonState* s)
{
	int full_update, i;
	uint32_t v, col, * palette;

	full_update = 0;
	palette = s->last_palette;
	for (i = 0; i < 16; i++) {
		v = s->ar[i];
		if (s->ar[VGA_ATC_MODE] & 0x80) {
			v = ((s->ar[VGA_ATC_COLOR_PAGE] & 0xf) << 4) | (v & 0xf);
		}
		else {
			v = ((s->ar[VGA_ATC_COLOR_PAGE] & 0xc) << 4) | (v & 0x3f);
		}
		v = v * 3;
		col = rgb_to_pixel32(c6_to_8(s->palette[v]),
			c6_to_8(s->palette[v + 1]),
			c6_to_8(s->palette[v + 2]));
		if (col != palette[i]) {
			full_update = 1;
			palette[i] = col;
		}
	}
	return full_update;
}

/* return true if the palette was modified */
static int update_palette256(VGACommonState* s)
{
	int full_update, i;
	uint32_t v, col, * palette;

	full_update = 0;
	palette = s->last_palette;
	v = 0;
	for (i = 0; i < 256; i++) {
		if (s->dac_8bit) {
			col = rgb_to_pixel32(s->palette[v],
				s->palette[v + 1],
				s->palette[v + 2]);
		}
		else {
			col = rgb_to_pixel32(c6_to_8(s->palette[v]),
				c6_to_8(s->palette[v + 1]),
				c6_to_8(s->palette[v + 2]));
		}
		if (col != palette[i]) {
			full_update = 1;
			palette[i] = col;
		}
		v += 3;
	}
	return full_update;
}

static void vga_get_params(VGACommonState* s,
	VGADisplayParams* params)
{
	if (vbe_enabled(s)) {
		params->line_offset = s->vbe_line_offset;
		params->start_addr = s->vbe_start_addr;
		params->line_compare = 65535;
		params->hpel = VGA_HPEL_NEUTRAL;
		params->hpel_split = false;
	}
	else {
		/* compute line_offset in bytes */
		params->line_offset = s->cr[VGA_CRTC_OFFSET] << 3;

		/* starting address */
		params->start_addr = s->cr[VGA_CRTC_START_LO] |
			(s->cr[VGA_CRTC_START_HI] << 8);

		/* line compare */
		params->line_compare = s->cr[VGA_CRTC_LINE_COMPARE] |
			((s->cr[VGA_CRTC_OVERFLOW] & 0x10) << 4) |
			((s->cr[VGA_CRTC_MAX_SCAN] & 0x40) << 3);

		params->hpel = s->ar[VGA_ATC_PEL];
		params->hpel_split = s->ar[VGA_ATC_MODE] & 0x20;
	}
}

/* update start_addr and line_offset. Return TRUE if modified */
static int update_basic_params(VGACommonState* s)
{
	int full_update;
	VGADisplayParams current;

	full_update = 0;

	s->get_params(s, &current);

	if (memcmp(&current, &s->params, sizeof(current))) {
		s->params = current;
		full_update = 1;
	}
	return full_update;
}


static const uint8_t cursor_glyph[32 * 4] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static void vga_get_text_resolution(VGACommonState* s, int* pwidth, int* pheight,
	int* pcwidth, int* pcheight)
{
	int width, cwidth, height, cheight;

	/* total width & height */
	cheight = (s->cr[VGA_CRTC_MAX_SCAN] & 0x1f) + 1;
	cwidth = 8;
	if (!(sr(s, VGA_SEQ_CLOCK_MODE) & VGA_SR01_CHAR_CLK_8DOTS)) {
		cwidth = 9;
	}
	if (sr(s, VGA_SEQ_CLOCK_MODE) & 0x08) {
		cwidth = 16; /* NOTE: no 18 pixel wide */
	}
	width = (s->cr[VGA_CRTC_H_DISP] + 1);
	if (s->cr[VGA_CRTC_V_TOTAL] == 100) {
		/* ugly hack for CGA 160x100x16 - explain me the logic */
		height = 100;
	}
	else {
		height = s->cr[VGA_CRTC_V_DISP_END] |
			((s->cr[VGA_CRTC_OVERFLOW] & 0x02) << 7) |
			((s->cr[VGA_CRTC_OVERFLOW] & 0x40) << 3);
		height = (height + 1) / cheight;
	}

	*pwidth = width;
	*pheight = height;
	*pcwidth = cwidth;
	*pcheight = cheight;
}

/*
 * Text mode update
 * Missing:
 * - double scan
 * - double width
 * - underline
 * - flashing
 */
static void vga_draw_text(VGACommonState* s, int full_update)
{
	
}

enum {
	VGA_DRAW_LINE2,
	VGA_DRAW_LINE2D2,
	VGA_DRAW_LINE4,
	VGA_DRAW_LINE4D2,
	VGA_DRAW_LINE8D2,
	VGA_DRAW_LINE8,
	VGA_DRAW_LINE15_LE,
	VGA_DRAW_LINE16_LE,
	VGA_DRAW_LINE24_LE,
	VGA_DRAW_LINE32_LE,
	VGA_DRAW_LINE15_BE,
	VGA_DRAW_LINE16_BE,
	VGA_DRAW_LINE24_BE,
	VGA_DRAW_LINE32_BE,
	VGA_DRAW_LINE_NB,
};

static vga_draw_line_func* const vga_draw_line_table[VGA_DRAW_LINE_NB] = {
	vga_draw_line2,
	vga_draw_line2d2,
	vga_draw_line4,
	vga_draw_line4d2,
	vga_draw_line8d2,
	vga_draw_line8,
	vga_draw_line15_le,
	vga_draw_line16_le,
	vga_draw_line24_le,
	vga_draw_line32_le,
	vga_draw_line15_be,
	vga_draw_line16_be,
	vga_draw_line24_be,
	vga_draw_line32_be,
};

static int vga_get_bpp(VGACommonState* s)
{
	int ret;

	if (vbe_enabled(s)) {
		ret = s->vbe_regs[VBE_DISPI_INDEX_BPP];
	}
	else {
		ret = 0;
	}
	return ret;
}

static void vga_get_resolution(VGACommonState* s, int* pwidth, int* pheight)
{
	int width, height;

	if (vbe_enabled(s)) {
		width = s->vbe_regs[VBE_DISPI_INDEX_XRES];
		height = s->vbe_regs[VBE_DISPI_INDEX_YRES];
	}
	else {
		width = (s->cr[VGA_CRTC_H_DISP] + 1) * 8;
		height = s->cr[VGA_CRTC_V_DISP_END] |
			((s->cr[VGA_CRTC_OVERFLOW] & 0x02) << 7) |
			((s->cr[VGA_CRTC_OVERFLOW] & 0x40) << 3);
		height = (height + 1);
	}
	*pwidth = width;
	*pheight = height;
}

void vga_invalidate_scanlines(VGACommonState* s, int y1, int y2)
{
	int y;
	if (y1 >= VGA_MAX_HEIGHT)
		return;
	if (y2 >= VGA_MAX_HEIGHT)
		y2 = VGA_MAX_HEIGHT;
	for (y = y1; y < y2; y++) {
		s->invalidated_y_table[y >> 5] |= 1 << (y & 0x1f);
	}
}

static bool vga_scanline_invalidated(VGACommonState* s, int y)
{
	if (y >= VGA_MAX_HEIGHT) {
		return false;
	}
	return s->invalidated_y_table[y >> 5] & (1 << (y & 0x1f));
}

void vga_dirty_log_start(VGACommonState* s)
{
	//memory_region_set_log(&s->vram, true, DIRTY_MEMORY_VGA);
}

void vga_dirty_log_stop(VGACommonState* s)
{
	//memory_region_set_log(&s->vram, false, DIRTY_MEMORY_VGA);
}

/*
 * graphic modes
 */
//static void vga_draw_graphic(VGACommonState* s, int full_update)
//{
//	int y1, y, update, linesize, y_start, double_scan, mask, depth;
//	int width, height, shift_control, bwidth, bits;
//	uint64_t page0, page1, region_start, region_end;
//	int disp_width, multi_scan, multi_run;
//	int hpel;
//	uint8_t* d;
//	uint32_t v, addr1, addr;
//	vga_draw_line_func* vga_draw_line = NULL;
//	bool allocate_surface, force_shadow = false;
//	uint16_t format;
//#if HOST_BIG_ENDIAN
//	bool byteswap = !s->big_endian_fb;
//#else
//	bool byteswap = s->big_endian_fb;
//#endif
//
//	full_update |= update_basic_params(s);
//
//	s->get_resolution(s, &width, &height);
//	disp_width = width;
//	depth = s->get_bpp(s);
//
//	/* bits 5-6: 0 = 16-color mode, 1 = 4-color mode, 2 = 256-color mode.  */
//	shift_control = (s->gr[VGA_GFX_MODE] >> 5) & 3;
//	double_scan = (s->cr[VGA_CRTC_MAX_SCAN] >> 7);
//	if (s->cr[VGA_CRTC_MODE] & 1) {
//		multi_scan = (((s->cr[VGA_CRTC_MAX_SCAN] & 0x1f) + 1) << double_scan)
//			- 1;
//	}
//	else {
//		/* in CGA modes, multi_scan is ignored */
//		/* XXX: is it correct ? */
//		multi_scan = double_scan;
//	}
//	multi_run = multi_scan;
//	if (shift_control != s->shift_control ||
//		double_scan != s->double_scan) {
//		full_update = 1;
//		s->shift_control = shift_control;
//		s->double_scan = double_scan;
//	}
//
//	if (shift_control == 0) {
//		full_update |= update_palette16(s);
//		if (sr(s, VGA_SEQ_CLOCK_MODE) & 8) {
//			disp_width <<= 1;
//			v = VGA_DRAW_LINE4D2;
//		}
//		else {
//			v = VGA_DRAW_LINE4;
//		}
//		bits = 4;
//
//	}
//	else if (shift_control == 1) {
//		full_update |= update_palette16(s);
//		if (sr(s, VGA_SEQ_CLOCK_MODE) & 8) {
//			disp_width <<= 1;
//			v = VGA_DRAW_LINE2D2;
//		}
//		else {
//			v = VGA_DRAW_LINE2;
//		}
//		bits = 4;
//
//	}
//	else {
//		switch (depth) {
//		default:
//		case 0:
//			full_update |= update_palette256(s);
//			v = VGA_DRAW_LINE8D2;
//			bits = 4;
//			break;
//		case 8:
//			full_update |= update_palette256(s);
//			v = VGA_DRAW_LINE8;
//			bits = 8;
//			break;
//		case 15:
//			v = s->big_endian_fb ? VGA_DRAW_LINE15_BE : VGA_DRAW_LINE15_LE;
//			bits = 16;
//			break;
//		case 16:
//			v = s->big_endian_fb ? VGA_DRAW_LINE16_BE : VGA_DRAW_LINE16_LE;
//			bits = 16;
//			break;
//		case 24:
//			v = s->big_endian_fb ? VGA_DRAW_LINE24_BE : VGA_DRAW_LINE24_LE;
//			bits = 24;
//			break;
//		case 32:
//			v = s->big_endian_fb ? VGA_DRAW_LINE32_BE : VGA_DRAW_LINE32_LE;
//			bits = 32;
//			break;
//		}
//	}
//
//	/* Horizontal pel panning bit 3 is only used in text mode.  */
//	hpel = bits <= 8 ? s->params.hpel & 7 : 0;
//#define DIV_ROUND_UP(n, d)  (((n) + (d) - 1) / (d))
//	bwidth = DIV_ROUND_UP(width * bits, 8); /* scanline length */
//	if (hpel) {
//		bwidth += 4;
//	}
//
//	region_start = (s->params.start_addr * 4);
//	region_end = region_start + (uint64_t)s->params.line_offset * (height - 1) + bwidth;
//	if (region_end > s->vbe_size) {
//		/*
//		 * On wrap around take the safe and slow route:
//		 *   - create a dirty bitmap snapshot for all vga memory.
//		 *   - force shadowing (so all vga memory access goes
//		 *     through vga_read_*() helpers).
//		 *
//		 * Given this affects only vga features which are pretty much
//		 * unused by modern guests there should be no performance
//		 * impact.
//		 */
//		region_start = 0;
//		region_end = s->vbe_size;
//		force_shadow = true;
//	}
//	if (s->params.line_compare < height) {
//		/* split screen mode */
//		region_start = 0;
//	}
//
//	/*
//	 * Check whether we can share the surface with the backend
//	 * or whether we need a shadow surface. We share native
//	 * endian surfaces for 15bpp and above and byteswapped
//	 * surfaces for 24bpp and above.
//	 */
//	format = 4;//qemu_default_pixman_format(depth, !byteswap);
//	if (format) {
//		allocate_surface = true;
//	}
//	else {
//		allocate_surface = true;
//	}
//
//	if (s->params.line_offset != s->last_line_offset ||
//		disp_width != s->last_width ||
//		height != s->last_height ||
//		s->last_depth != depth ||
//		s->last_byteswap != byteswap ||
//		allocate_surface != true) {
//		/* display parameters changed -> need new display surface */
//		s->last_scr_width = disp_width;
//		s->last_scr_height = height;
//		s->last_width = disp_width;
//		s->last_height = height;
//		s->last_line_offset = s->params.line_offset;
//		s->last_depth = depth;
//		s->last_byteswap = byteswap;
//		/* 16 extra pixels are needed for double-width planar modes.  */
//		s->panning_buf = (uint8_t*)malloc(
//			(disp_width + 16) * sizeof(uint32_t));
//		full_update = 1;
//	}
//	if (s->adapter->get_framebuffer() != s->vram_ptr + (s->params.start_addr * 4)
//		&& true) {
//		/* base address changed (page flip) -> shared display surfaces
//		 * must be updated with the new base address */
//		full_update = 1;
//	}
//
//	if (full_update) {
//		if (!allocate_surface) {
//			/*surface = qemu_create_displaysurface_from(disp_width,
//				height, format, s->params.line_offset,
//				s->vram_ptr + (s->params.start_addr * 4));*/
//				//dpy_gfx_replace_surface(s->con, surface);
//		}
//		else {
//			/*qemu_console_resize(s->con, disp_width, height);
//			surface = qemu_console_surface(s->con);*/
//		}
//	}
//
//	vga_draw_line = vga_draw_line_table[v];
//
//	if (true && s->cursor_invalidate) {
//		//s->cursor_invalidate(s);
//	}
//
//#if 0
//	printf("w=%d h=%d v=%d cr[0x09]=0x%02x cr[0x17]=0x%02x linecmp=%d sr[0x01]=0x%02x\n",
//		width, height, v, s->cr[9], s->cr[VGA_CRTC_MODE],
//		s->params.line_compare, sr(s, VGA_SEQ_CLOCK_MODE));
//#endif
//	addr1 = (s->params.start_addr * 4);
//	y_start = -1;
//	d = s->adapter->get_framebuffer();
//	linesize = s->adapter->get_stride();
//	y1 = 0;
//
//
//	for (y = 0; y < height; y++) {
//
//		addr = addr1;
//		if (!(s->cr[VGA_CRTC_MODE] & 1)) {
//			int shift;
//			/* CGA compatibility handling */
//			shift = 14 + ((s->cr[VGA_CRTC_MODE] >> 6) & 1);
//			addr = (addr & ~(1 << shift)) | ((y1 & 1) << shift);
//		}
//		if (!(s->cr[VGA_CRTC_MODE] & 2)) {
//			addr = (addr & ~0x8000) | ((y1 & 2) << 14);
//		}
//		page0 = addr & s->vbe_size_mask;
//		page1 = (addr + bwidth - 1) & s->vbe_size_mask;
//		if (full_update) {
//			update = 1;
//		}
//		else if (page1 < page0) {
//			/* scanline wraps from end of video memory to the start */
//			assert(force_shadow);
//			/*update = memory_region_snapshot_get_dirty(&s->vram, snap,
//				page0, s->vbe_size - page0);
//			update |= memory_region_snapshot_get_dirty(&s->vram, snap,
//				0, page1);*/
//		}
//		else {
//			/*update = memory_region_snapshot_get_dirty(&s->vram, snap,
//				page0, page1 - page0);*/
//		}
//		/* explicit invalidation for the hardware cursor (cirrus only) */
//		update = 1;
//		update |= vga_scanline_invalidated(s, y);
//		if (update) {
//			if (y_start < 0)
//				y_start = y;
//			if (true) {
//				uint8_t* p;
//				p = (uint8_t*)vga_draw_line(s, d, addr, width, hpel);
//				if (p) {
//					memcpy(d, p, disp_width * sizeof(uint32_t));
//				}
//				/*if (s->cursor_draw_line)
//					s->cursor_draw_line(s, d, y);*/
//			}
//		}
//		else {
//			if (y_start >= 0) {
//				/* flush to display */
//				//dpy_gfx_update(s->con, 0, y_start,disp_width, y - y_start);
//				y_start = -1;
//			}
//		}
//		if (!multi_run) {
//			mask = (s->cr[VGA_CRTC_MODE] & 3) ^ 3;
//			if ((y1 & mask) == mask)
//				addr1 += s->params.line_offset;
//			y1++;
//			multi_run = multi_scan;
//		}
//		else {
//			multi_run--;
//		}
//		/* line compare acts on the displayed lines */
//		if (y == s->params.line_compare) {
//			if (s->params.hpel_split) {
//				hpel = VGA_HPEL_NEUTRAL;
//			}
//			addr1 = 0;
//		}
//		d += linesize;
//	}
//	if (y_start >= 0) {
//		/* flush to display */
//		//dpy_gfx_update(s->con, 0, y_start,disp_width, y - y_start);
//	}
//	memset(s->invalidated_y_table, 0, sizeof(s->invalidated_y_table));
//}
static void vga_draw_graphic(VGACommonState* s, int full_update)
{
	int y, width, height, linesize;
	uint32_t addr1, addr;
	uint32_t* d;

	// get resolution
	s->get_resolution(s, &width, &height);

	// update params (line_offset, start_addr etc)
	full_update |= update_basic_params(s);

	// update palette
	full_update |= update_palette16(s);

	// notify adapter of size
	s->adapter->set_size_graphical(width, height);

	// get output buffer
	d = s->adapter->get_framebuffer();
	linesize = s->adapter->get_stride(); // width * 4

	if (!d) {
		printf("no framebuffer\n");
		return;
	}

	// Mode 12h: planar 16 color
	// each scanline in VRAM is line_offset bytes (320 for 640px)
	// addr advances by line_offset each row
	// vga_draw_line4 reads 4 planes and outputs 8 pixels per iteration
	// output is 32bpp XRGB, 8 pixels * 4 bytes = 32 bytes per iteration
	// 640 pixels / 8 = 80 iterations per scanline
	// output per scanline = 80 * 32 = 2560 bytes = width * 4 ✓

	addr1 = s->params.start_addr * 4;
	int bytes_per_scanline = width / 8; // 640 / 8 = 80
	for (y = 0; y < height; y++) {
		for (int t = 0; t < width; t++) {
			int byte_index = y * bytes_per_scanline + (t >> 3);
			int bit = 7 - (t & 7);

			uint8_t p0 = s->plane0[byte_index];
			uint8_t p1 = s->plane1[byte_index];
			uint8_t p2 = s->plane2[byte_index];
			uint8_t p3 = s->plane3[byte_index];

			uint8_t color_index =
				((p0 >> bit) & 1) |
				((p1 >> bit) & 1) << 1 |
				((p2 >> bit) & 1) << 2 |
				((p3 >> bit) & 1) << 3;

			// temporary: direct 16-color palette
			uint32_t rgb = s->last_palette[color_index];

			d[y * width + t] = rgb;
		}
	}

	// clear invalidation table
	memset(s->invalidated_y_table, 0, sizeof(s->invalidated_y_table));
}

static void vga_draw_blank(VGACommonState* s, int full_update)
{
	
}

#define GMODE_TEXT     0
#define GMODE_GRAPH    1
#define GMODE_BLANK 2

void vga_update_display(void* opaque)
{
	VGACommonState* s = (VGACommonState*)opaque;
	//DisplaySurface* surface = qemu_console_surface(s->con);
	int full_update, graphic_mode;

	full_update = 1;
	if (!(s->ar_index & 0x20)) {
		graphic_mode = GMODE_BLANK;
	}
	else {
		graphic_mode = s->gr[VGA_GFX_MISC] & VGA_GR06_GRAPHICS_MODE;
	}
	if (graphic_mode != s->graphic_mode) {
		s->graphic_mode = graphic_mode;
		s->cursor_blink_time = 10;
		full_update = 1;
	}
	switch (graphic_mode) {
	case GMODE_TEXT:
		//vga_draw_text(s, full_update);
		break;
	case GMODE_GRAPH:
		vga_draw_graphic(s, full_update);
		break;
	case GMODE_BLANK:
	default:
		vga_draw_blank(s, full_update);
		break;
	}
}

/* force a full display refresh */
static void vga_invalidate_display(void* opaque)
{
	VGACommonState* s = (VGACommonState*)opaque;

	s->last_width = -1;
	s->last_height = -1;
}

void vga_common_reset(VGACommonState* s)
{
	s->sr_index = 0;
	memset(s->sr, '\0', sizeof(s->sr));
	memset(s->sr_vbe, '\0', sizeof(s->sr_vbe));
	s->gr_index = 0;
	memset(s->gr, '\0', sizeof(s->gr));
	s->ar_index = 0;
	memset(s->ar, '\0', sizeof(s->ar));
	s->ar_flip_flop = 0;
	s->cr_index = 0;
	memset(s->cr, '\0', sizeof(s->cr));
	s->msr = 0;
	s->fcr = 0;
	s->st00 = 0;
	s->st01 = 0;
	s->dac_state = 0;
	s->dac_sub_index = 0;
	s->dac_read_index = 0;
	s->dac_write_index = 0;
	memset(s->dac_cache, '\0', sizeof(s->dac_cache));
	s->dac_8bit = 0;
	memset(s->palette, '\0', sizeof(s->palette));
	s->bank_offset = 0;
	s->vbe_index = 0;
	memset(s->vbe_regs, '\0', sizeof(s->vbe_regs));
	s->vbe_regs[VBE_DISPI_INDEX_ID] = VBE_DISPI_ID5;
	s->vbe_start_addr = 0;
	s->vbe_line_offset = 0;
	s->vbe_bank_mask = (s->vram_size >> 16) - 1;
	s->graphic_mode = -1; /* force full update */
	s->shift_control = 0;
	s->double_scan = 0;
	memset(&s->params, '\0', sizeof(s->params));
	s->plane_updated = 0;
	s->last_cw = 0;
	s->last_ch = 0;
	s->last_width = 0;
	s->last_height = 0;
	s->last_scr_width = 0;
	s->last_scr_height = 0;
	s->cursor_start = 0;
	s->cursor_end = 0;
	s->cursor_offset = 0;
	memset(s->invalidated_y_table, '\0', sizeof(s->invalidated_y_table));
	memset(s->last_palette, '\0', sizeof(s->last_palette));
	memset(s->last_ch_attr, '\0', sizeof(s->last_ch_attr));
	vga_update_memory_access(s);
}

static void vga_reset(void* opaque)
{
	VGACommonState* s = (VGACommonState*)opaque;
	vga_common_reset(s);
}

#define TEXTMODE_X(x)	((x) % width)
#define TEXTMODE_Y(x)	((x) / width)
#define VMEM2CHTYPE(v)	((v & 0xff0007ff) | \
        ((v & 0x00000800) << 10) | ((v & 0x00007000) >> 1))


static uint64_t vga_mem_read(void* opaque, hwaddr addr,
	unsigned size)
{
	VGACommonState* s = (VGACommonState*)opaque;

	return vga_mem_readb(s, addr);
}

static void vga_mem_write(void* opaque, hwaddr addr,
	uint64_t data, unsigned size)
{
	VGACommonState* s = (VGACommonState*)opaque;

	vga_mem_writeb(s, addr, data);
}

MemoryRegionOps vga_mem_ops;


static int vga_common_post_load(void* opaque, int version_id)
{
	VGACommonState* s = (VGACommonState*)opaque;

	/* force refresh */
	s->graphic_mode = -1;
	vbe_update_vgaregs(s);
	vga_update_memory_access(s);
	return 0;
}

static bool vga_endian_state_needed(void* opaque)
{
	VGACommonState* s = (VGACommonState*)opaque;

	/*
	 * Only send the endian state if it's different from the
	 * default one, thus ensuring backward compatibility for
	 * migration of the common case
	 */
	return s->default_endian_fb != s->big_endian_fb;
}



static GraphicHwOps vga_ops;

static inline uint32_t uint_clamp(uint32_t val, uint32_t vmin, uint32_t vmax)
{
	if (val < vmin) {
		return vmin;
	}
	if (val > vmax) {
		return vmax;
	}
	return val;
}

bool vga_common_init(VGACommonState* s)
{
	int i, j, v, b;

	for (i = 0; i < 256; i++) {
		v = 0;
		for (j = 0; j < 8; j++) {
			v |= ((i >> j) & 1) << (j * 4);
		}
		expand4[i] = v;

		v = 0;
		for (j = 0; j < 4; j++) {
			v |= ((i >> (2 * j)) & 3) << (j * 4);
		}
		expand2[i] = v;
	}
	for (i = 0; i < 16; i++) {
		v = 0;
		for (j = 0; j < 4; j++) {
			b = ((i >> j) & 1);
			v |= b << (2 * j);
			v |= b << (2 * j + 1);
		}
		expand4to8[i] = v;
	}

	s->vram_size_mb = uint_clamp(s->vram_size_mb, 1, 512);
	s->vram_size_mb = pow2ceil(s->vram_size_mb);
	s->vram_size = s->vram_size_mb * MiB;
	s->vbe_size = SVGA_SIZE;
	s->vbe_size_mask = s->vbe_size - 1;

	s->is_vbe_vmstate = 1;
	s->svga_enabled = false;
	//s->vram_ptr = (uint8_t*)SVGA;//memory_region_get_ram_ptr(&s->vram);
	s->vram_ptr = (uint8_t*)SVGA;
	s->get_bpp = vga_get_bpp;
	s->get_params = vga_get_params;
	s->get_resolution = vga_get_resolution;
	s->hw_ops = &vga_ops;
	s->retrace = vga_dumb_retrace;
	s->update_retrace_info = vga_dumb_update_retrace_info;
	return true;
}



void vga_init(VGACommonState* s)
{
	vga_mem_ops.read = vga_mem_read;
	vga_mem_ops.write = vga_mem_write;
	vga_mem_ops.endianness = DEVICE_LITTLE_ENDIAN;
	vga_mem_ops.impl.min_access_size = 1;
	vga_mem_ops.impl.max_access_size = 1;
	vga_ops.invalidate = vga_invalidate_display;
	vga_ops.gfx_update = vga_update_display;
	vbe_update_vgaregs(s);
	vga_common_init(s);
}
void memory_region_set_dirty(void* mr, hwaddr addr, hwaddr size) {

}
