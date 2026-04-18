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
			s->adapter->set_mode(true,false);
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
	}
}
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
static inline int clz64(uint64_t val)
{
	return val ? __lzcnt64(val) : 64;
}
static inline uint64_t pow2ceil(uint64_t value)
{
	int n = clz64(value - 1);

	if (!n) {
		return !value;
	}
	return 0x8000000000000000ull >> (n - 1);
}
bool have_vga = true;
#define VGA_TEXT_CURSOR_PERIOD_MS       (1000 * 2 * 16 / 60)

/* Address mask for non-VESA modes.  */
#define VGA_VRAM_SIZE                   (256 * KiB)
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
		s->has_chain4_alias = true;
	}
}

static void vga_dumb_update_retrace_info(VGACommonState* s)
{
	(void)s;
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
			val = 0;
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
			if (s->vbe_index == VBE_DISPI_INDEX_XRES) {
				s->adapter->set_size_graphical(val, -1);
			}
			if (s->vbe_index == VBE_DISPI_INDEX_YRES) {
				s->adapter->set_size_graphical(-1, val);
			}
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

static void vga_draw_graphic(VGACommonState* s, int full_update)
{
	int y, width, height, linesize;
	uint32_t addr1, addr;
	uint32_t* d;
	s->get_resolution(s, &width, &height);
	full_update |= update_basic_params(s);
	full_update |= update_palette16(s);
	s->adapter->set_size_graphical(width, height);
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
			uint32_t rgb = s->last_palette[color_index];

			d[y * width + t] = rgb;
		}
	}
	memset(s->invalidated_y_table, 0, sizeof(s->invalidated_y_table));
}

#define GMODE_TEXT     0
#define GMODE_GRAPH    1
#define GMODE_BLANK 2

void vga_update_display(void* opaque)
{
	VGACommonState* s = (VGACommonState*)opaque;
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
		full_update = 1;
	}
	switch (graphic_mode) {
	case GMODE_TEXT:
		//vga_draw_text(s, full_update);
		break;
	case GMODE_GRAPH:
		vga_draw_graphic(s, full_update);
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
	s->vram_ptr = (uint8_t*)SVGA;
	s->get_bpp = vga_get_bpp;
	s->get_params = vga_get_params;
	s->get_resolution = vga_get_resolution;
	return true;
}



void vga_init(VGACommonState* s)
{
	vga_mem_ops.read = vga_mem_read;
	vga_mem_ops.write = vga_mem_write;
	vga_mem_ops.endianness = DEVICE_LITTLE_ENDIAN;
	vga_mem_ops.impl.min_access_size = 1;
	vga_mem_ops.impl.max_access_size = 1;
	vbe_update_vgaregs(s);
	vga_common_init(s);
}
