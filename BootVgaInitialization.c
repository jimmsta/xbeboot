/*
 * video-related stuff
 * 2003-02-02  andy@warmcat.com  Major reshuffle, threw out tons of unnecessary init
                                 Made a good start on separating the video mode from the AV cable
																 Consolidated init tables into a big struct (see boot.h)
 * 2002-12-04  andy@warmcat.com  Video now working :-)
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifdef JUSTVIDEO
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/io.h>
#include <linux/fb.h>
#include <errno.h>
#endif

#include "boot.h"
#include "BootVideo.h"
#include "BootEEPROM.h"
#include "config.h"

const VID_STANDARD vidstda[] = {
	{ 3579545.00, 0.0000053, 0.00000782, 0.0000047, 0.000063555, 0.0000094, 0.000035667, 0.0000015, 243, 262.5, 0.0000092 },
	{ 3579545.00, 0.0000053, 0.00000782, 0.0000047, 0.000064000, 0.0000094, 0.000035667, 0.0000015, 243, 262.5, 0.0000092 },
	{ 4433618.75, 0.0000056, 0.00000785, 0.0000047, 0.000064000, 0.0000105, 0.000036407, 0.0000015, 288, 312.5, 0.0000105 },
	{ 4433618.75, 0.0000056, 0.00000785, 0.0000047, 0.000064000, 0.0000094, 0.000035667, 0.0000015, 288, 312.5, 0.0000092 },
	{ 3582056.25, 0.0000056, 0.00000811, 0.0000047, 0.000064000, 0.0000105, 0.000036407, 0.0000015, 288, 312.5, 0.0000105 },
	{ 3575611.88, 0.0000058, 0.00000832, 0.0000047, 0.000063555, 0.0000094, 0.000035667, 0.0000015, 243, 262.5, 0.0000092 },
	{ 4433619.49, 0.0000053, 0.00000755, 0.0000047, 0.000063555, 0.0000105, 0.000036407, 0.0000015, 243, 262.5, 0.0000092 }
};

#define NVCRTC 0x6013D4

#ifndef JUSTVIDEO
static double fabs(double d) {
	if (d > 0) return d;
	else return -d;
}
#endif

typedef struct {
	long h_blanki;
	long h_blanko;
	long v_blanki;
	long v_blanko;
	long vscale;
} BLANKING_PARAMETER;

typedef struct {
	long xres;
	long crtchdispend;
	long nvhstart;
	long nvhtotal;
	long yres;
	long nvvstart;
	long crtcvstart;
	long crtcvtotal;
	long nvvtotal;
	long pixelDepth;
} GPU_PARAMETER;

// function prototypes
static void SetVGAConexantRegister(BYTE pll_int, BYTE* pbRegs);
static void SetGPURegister(const GPU_PARAMETER* gpu, BYTE* pbRegs);
static EVIDEOSTD xbvDetectVideoStd(void);

static EAVTYPE xbvDetectAvType(void) {
	EAVTYPE avType;
	BYTE b=I2CTransmitByteGetReturn(0x10, 0x04);
	switch (b) {
		case 0: avType = AV_SCART_RGB; break;
		case 1: avType = AV_HDTV; break;
		case 2: avType = AV_VGA; break;
		case 4: avType = AV_SVIDEO; break;
		case 6: avType = AV_COMPOSITE; break;
		default: avType = AV_COMPOSITE; break;
	}

	return avType;
}

void BootVgaInitializationKernelNG(CURRENT_VIDEO_MODE_DETAILS * pcurrentvideomodedetails) {
	EVIDEOSTD videoStd;

	MODE_PARAMETER parameter;
	BYTE b;
	RIVA_HW_INST riva;
	int maxcounter;

	videoStd = xbvDetectVideoStd();

	pcurrentvideomodedetails->m_bAvPack=I2CTransmitByteGetReturn(0x10, 0x04);
	pcurrentvideomodedetails->m_bBPP = 32;

	b=I2CTransmitByteGetReturn(0x54, 0x5A); // the eeprom defines the TV standard for the box

	// The values for hoc and voc are stolen from nvtv small mode

	if(b != 0x40) {
		pcurrentvideomodedetails->hoc = 13.44;
		pcurrentvideomodedetails->voc = 14.24;
		pcurrentvideomodedetails->m_bTvStandard = TV_ENCODING_PAL;
	} else {
		pcurrentvideomodedetails->hoc = 15.11;
		pcurrentvideomodedetails->voc = 14.81;
		pcurrentvideomodedetails->m_bTvStandard = TV_ENCODING_NTSC;
	}
	pcurrentvideomodedetails->hoc /= 100.0;
	pcurrentvideomodedetails->voc /= 100.0;

	mapNvMem(&riva,pcurrentvideomodedetails->m_pbBaseAddressVideo);
	unlockCrtNv(&riva,0);

	MMIO_H_OUT32 (riva.PCRTC, 0, 0x800, pcurrentvideomodedetails->m_dwFrameBufferStart);


	IoOutputByte(0x80d3, 5);  // definitively kill video out using an ACPI control pin

/*	writeCrtNv(&riva, 0, 0x1f, 0x57);
	NVWriteSeq(&riva,0x06,0x57);
	writeCrtNv(&riva, 0, 0x21, 0xff);
*/
//	MMIO_H_OUT32(riva.PRAMDAC,0,0x880,0x21121111);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x880,0);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x884,0x0);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x888,0x0);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x88c,0x10001000);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x890,0x10000000);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x894,0x10000000);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x898,0x10000000);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x89c,0x10000000);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x84c,0x0); // remove magenta borders in RGB mode

	writeCrtNv (&riva, 0, 0x14, 0x00);
	writeCrtNv (&riva, 0, 0x17, 0xe3); // Set CRTC mode register
	writeCrtNv (&riva, 0, 0x19, 0x10); // ?
	writeCrtNv (&riva, 0, 0x1b, 0x05); // arbitration0
	writeCrtNv (&riva, 0, 0x22, 0xff); // ?
	writeCrtNv (&riva, 0, 0x33, 0x11); // ?

	if (xbvDetectAvType() == AV_VGA) {
		GPU_PARAMETER gpu;
		BYTE pll_int;
/*
		// Settings for 640x480@60Hz, 31.5 kHz HSync (should work on every VGA monitor)
		pcurrentvideomodedetails->m_dwWidthInPixels=640;
		pcurrentvideomodedetails->m_dwHeightInLines=480;
		pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=0;
		pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=0;
		gpu.pixelDepth = 4;
		gpu.xres = 640;
		gpu.nvhstart = 737;
		gpu.nvhtotal =  857;
		gpu.yres = 480;
		gpu.nvvstart = 488;
		gpu.crtcvtotal = 524;
		pll_int = 0x0c;
*/
		// Settings for 800x600@56Hz, 35 kHz HSync
		pcurrentvideomodedetails->m_dwWidthInPixels=800;
		pcurrentvideomodedetails->m_dwHeightInLines=600;
		pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=20;
		pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=20;
		gpu.pixelDepth = 4;
		gpu.xres = 800;
		gpu.nvhstart = 900;
		gpu.nvhtotal = 1028;
		gpu.yres = 600;
		gpu.nvvstart = 614;
		gpu.crtcvtotal = 630;
		pll_int = 0x10;
		gpu.crtchdispend = gpu.xres;
		gpu.crtcvstart = gpu.nvvstart;
		gpu.nvvtotal =  gpu.crtcvtotal;
		SetVGAConexantRegister(pll_int, pcurrentvideomodedetails->m_pbBaseAddressVideo);
		SetGPURegister(&gpu, pcurrentvideomodedetails->m_pbBaseAddressVideo);
	}
	else
	{
		switch(pcurrentvideomodedetails->m_nVideoModeIndex) {
			case VIDEO_MODE_640x480:
				pcurrentvideomodedetails->m_dwWidthInPixels=640;
				pcurrentvideomodedetails->m_dwHeightInLines=480;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=0;
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=0;
				break;
			case VIDEO_MODE_640x576:
				pcurrentvideomodedetails->m_dwWidthInPixels=640;
				pcurrentvideomodedetails->m_dwHeightInLines=576;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=40; // pixels
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=40; // lines
				break;
			case VIDEO_MODE_720x576:
				pcurrentvideomodedetails->m_dwWidthInPixels=720;
				pcurrentvideomodedetails->m_dwHeightInLines=576;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=40; // pixels
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=40; // lines
				break;
			case VIDEO_MODE_800x600: // 800x600
				pcurrentvideomodedetails->m_dwWidthInPixels=800;
				pcurrentvideomodedetails->m_dwHeightInLines=600;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=20;
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=20; // lines
				break;
			case VIDEO_MODE_1024x576: // 1024x576
				pcurrentvideomodedetails->m_dwWidthInPixels=1024;
				pcurrentvideomodedetails->m_dwHeightInLines=576;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=20;
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=20; // lines
				I2CTransmitWord(0x45, (0x60<<8)|0xc7);
				I2CTransmitWord(0x45, (0x62<<8)|0x0);
				I2CTransmitWord(0x45, (0x64<<8)|0x0);
				break;
		}
		// Use TV settings
		if(FindOverscanValues(pcurrentvideomodedetails->m_dwWidthInPixels,
			pcurrentvideomodedetails->m_dwHeightInLines,
			pcurrentvideomodedetails->hoc, pcurrentvideomodedetails->voc,
			pcurrentvideomodedetails->m_bBPP,
			videoStd, &parameter)) {
				SetAutoParameter(&parameter, pcurrentvideomodedetails->m_pbBaseAddressVideo);
		}
	}

	NVDisablePalette (&riva, 0);
	writeCrtNv (&riva, 0, 0x44, 0x03);
	NVInitGrSeq(&riva);
	writeCrtNv (&riva, 0, 0x44, 0x00);
	NVInitAttr(&riva,0);

	IoOutputByte(0x80d8, 4);  // ACPI IO thing seen in kernel, set to 4
	IoOutputByte(0x80d6, 5);  // ACPI IO thing seen in kernel, set to 4 or 5

	NVVertIntrEnabled (&riva,0);
	NVSetFBStart (&riva, 0, pcurrentvideomodedetails->m_dwFrameBufferStart);

	IoOutputByte(0x80d3, 4);  // ACPI IO video enable REQUIRED <-- particularly crucial to get composite out

	pcurrentvideomodedetails->m_bFinalConexantA8 = 0x81;
	pcurrentvideomodedetails->m_bFinalConexantAA = 0x49;
	pcurrentvideomodedetails->m_bFinalConexantAC = 0x8c;
	// We dimm the Video OFF
	I2CTransmitWord(0x45, (0xa8<<8)|0);
	I2CTransmitWord(0x45, (0xaa<<8)|0);
	I2CTransmitWord(0x45, (0xac<<8)|0);

	NVWriteSeq(&riva, 0x01, 0x01);  /* reenable display */

/*
	MMIO_H_OUT8(riva.PCIO, 0, 0x3c0, 0x01);
	MMIO_H_OUT8(riva.PVIO, 0, 0x3c2, 0xe3);

	MMIO_H_OUT32 (riva.PCRTC, 0, 0x800, pcurrentvideomodedetails->m_dwFrameBufferStart);

	writeCrtNv (&riva, 0, 0x11, 0x20);
*/

        // We reenable the Video
        I2CTransmitWord(0x45, 0xa800 | pcurrentvideomodedetails->m_bFinalConexantA8);
        I2CTransmitWord(0x45, 0xaa00 | pcurrentvideomodedetails->m_bFinalConexantAA);
        I2CTransmitWord(0x45, 0xac00 | pcurrentvideomodedetails->m_bFinalConexantAC);



}

void NVSetFBStart (RIVA_HW_INST *riva, int head, DWORD dwFBStart) {

	MMIO_H_OUT32 (riva->PCRTC, head, 0x8000, dwFBStart);
	MMIO_H_OUT32 (riva->PMC, head, 0x8000, dwFBStart);

}

void NVVertIntrEnabled (RIVA_HW_INST *riva, int head)
{
	MMIO_H_OUT32 (riva->PCRTC, head, 0x140, 0x1);
	MMIO_H_OUT32 (riva->PCRTC, head, 0x100, 0x1);
	MMIO_H_OUT32 (riva->PCRTC, head, 0x140, 1);
	MMIO_H_OUT32 (riva->PMC, head, 0x140, 0x1);
	MMIO_H_OUT32 (riva->PMC, head, 0x100, 0x1);
	MMIO_H_OUT32 (riva->PMC, head, 0x140, 1);

}

void andOrCrtNv (RIVA_HW_INST *riva, int head, int reg, BYTE and, BYTE or)
{
  register BYTE tmp;

  VGA_WR08(riva->PCIO, CRT_INDEX(head), reg);
  tmp = VGA_RD08(riva->PCIO, CRT_DATA(head));
  VGA_WR08(riva->PCIO, CRT_DATA(head), (tmp & and) | or);
}

BYTE readCrtNv (RIVA_HW_INST *riva, int head, int reg)
{
	VGA_WR08(riva->PCIO, CRT_INDEX(head), reg);
	return VGA_RD08(riva->PCIO, CRT_DATA(head));
}

inline void unlockCrtNv (RIVA_HW_INST *riva, int head)
{
  writeCrtNv (riva, head, 0x1f, 0x57); /* unlock extended registers */
}

inline void lockCrtNv (RIVA_HW_INST *riva, int head)
{
  writeCrtNv (riva, head, 0x1f, 0x99); /* lock extended registers */
}

void andCrtNv (RIVA_HW_INST *riva, int head, int reg, BYTE val)
{
  register BYTE tmp;

  VGA_WR08(riva->PCIO, CRT_INDEX(head), reg);
  tmp = VGA_RD08(riva->PCIO, CRT_DATA(head));
  VGA_WR08(riva->PCIO, CRT_DATA(head), tmp & val);
}

void orCrtNv (RIVA_HW_INST *riva, int head, int reg, BYTE val)
{
  register BYTE tmp;

  VGA_WR08(riva->PCIO, CRT_INDEX(head), reg);
  tmp = VGA_RD08(riva->PCIO, CRT_DATA(head));
  VGA_WR08(riva->PCIO, CRT_DATA(head), tmp | val);
}


void writeCrtNv (RIVA_HW_INST *riva, int head, int reg, BYTE val)
{
	VGA_WR08(riva->PCIO, CRT_INDEX(head), reg);
	VGA_WR08(riva->PCIO, CRT_DATA(head), val);
}

void mapNvMem (RIVA_HW_INST *riva, BYTE *IOAddress)
{
	riva->PMC     = IOAddress+0x000000;
	riva->PFB     = IOAddress+0x100000;
	riva->PEXTDEV = IOAddress+0x101000;
	riva->PCRTC   = IOAddress+0x600000;
	riva->PCIO    = riva->PCRTC + 0x1000;
	riva->PVIO    = IOAddress+0x0C0000;
	riva->PRAMDAC = IOAddress+0x680000;
	riva->PDIO    = riva->PRAMDAC + 0x1000;
	riva->PVIDEO  = IOAddress+0x008000;
	riva->PTIMER  = IOAddress+0x009000;
}

void NVDisablePalette (RIVA_HW_INST *riva, int head)
{
    volatile CARD8 tmp;

    tmp = VGA_RD08(riva->PCIO + head * HEAD, VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);
    VGA_WR08(riva->PCIO + head * HEAD, VGA_ATTR_INDEX, 0x20);
}

void NVWriteSeq(RIVA_HW_INST *riva, CARD8 index, CARD8 value)
{
	
	VGA_WR08(riva->PVIO, VGA_SEQ_INDEX, index);
	VGA_WR08(riva->PVIO, VGA_SEQ_DATA,  value);

}

CARD8 NVReadSeq(RIVA_HW_INST *riva, CARD8 index)
{
    VGA_WR08(riva->PVIO, VGA_SEQ_INDEX, index);
    return (VGA_RD08(riva->PVIO, VGA_SEQ_DATA));
}

void NVWriteGr(RIVA_HW_INST *riva, CARD8 index, CARD8 value)
{
	VGA_WR08(riva->PVIO, VGA_GRAPH_INDEX, index);
	VGA_WR08(riva->PVIO, VGA_GRAPH_DATA,  value);
}

CARD8 NVReadGr(RIVA_HW_INST *riva, CARD8 index)
{
    VGA_WR08(riva->PVIO, VGA_GRAPH_INDEX, index);
    return (VGA_RD08(riva->PVIO, VGA_GRAPH_DATA));
}

void NVInitGrSeq (RIVA_HW_INST *riva)
{
	NVWriteSeq(riva, 0x00, 0x03);
	NVWriteSeq(riva, 0x01, 0x21);
	NVWriteSeq(riva, 0x02, 0x0f);
	NVWriteSeq(riva, 0x03, 0x00);
	NVWriteSeq(riva, 0x04, 0x06);
	NVWriteGr(riva, 0x00, 0x00);
	NVWriteGr(riva, 0x01, 0x00);
	NVWriteGr(riva, 0x02, 0x00);
	NVWriteGr(riva, 0x03, 0x00);
	NVWriteGr(riva, 0x04, 0x00);    /* depth != 1 */
	NVWriteGr(riva, 0x05, 0x40);    /* depth != 1 && depth != 4 */
	NVWriteGr(riva, 0x06, 0x05);
	NVWriteGr(riva, 0x07, 0x0f);
	NVWriteGr(riva, 0x08, 0xff);
}

void NVWriteAttr(RIVA_HW_INST *riva, int head, CARD8 index, CARD8 value)
{
	MMIO_H_OUT8(riva->PCIO, head, VGA_ATTR_INDEX,  index);
	MMIO_H_OUT8(riva->PCIO, head, VGA_ATTR_DATA_W, value);
}

void NVInitAttr (RIVA_HW_INST *riva, int head)
{
	NVWriteAttr(riva,0, 0, 0x01);
	NVWriteAttr(riva,0, 1, 0x02);
	NVWriteAttr(riva,0, 2, 0x03);
	NVWriteAttr(riva,0, 3, 0x04);
	NVWriteAttr(riva,0, 4, 0x05);
	NVWriteAttr(riva,0, 5, 0x06);
	NVWriteAttr(riva,0, 6, 0x07);
	NVWriteAttr(riva,0, 7, 0x08);
	NVWriteAttr(riva,0, 8, 0x09);
	NVWriteAttr(riva,0, 9, 0x0a);
	NVWriteAttr(riva,0, 10, 0x0b);
	NVWriteAttr(riva,0, 11, 0x0c);
	NVWriteAttr(riva,0, 12, 0x0d);
	NVWriteAttr(riva,0, 13, 0x0e);
	NVWriteAttr(riva,0, 14, 0x0f);
	NVWriteAttr(riva,0, 15, 0x01);
	NVWriteAttr(riva,0, 16, 0x4a);
	NVWriteAttr(riva,0, 17, 0x0f);
	NVWriteAttr(riva,0, 18, 0x00);
	NVWriteAttr(riva,0, 19, 0x00);
}

BYTE NvGetCrtc(BYTE * pbRegs, int nIndex) {
	pbRegs[0x6013d4]=nIndex;
	return pbRegs[0x6013d5];
}

void NvSetCrtc(BYTE * pbRegs, int nIndex, BYTE b) {
	pbRegs[0x6013d4]=nIndex;
	pbRegs[0x6013d5]=b;
}

static EVIDEOSTD xbvDetectVideoStd(void) {
	EVIDEOSTD videoStd;
	BYTE b=I2CTransmitByteGetReturn(0x54, 0x5A); // the eeprom defines the TV standard for the box

	if(b == 0x40) {
		videoStd = NTSC;
	} else {
		videoStd = PALBDGHI;
	}

	return videoStd;
}

static inline DWORD CalcV_ACTIVEO(const MODE_PARAMETER* mode) {
	return (DWORD)(
		(
			(mode->v_activei * vidstda[mode->nVideoStd].m_TotalLinesOut)
			+ mode->v_linesi - 1
		) / mode->v_linesi
	);
}

static inline DWORD CalcH_CLKO(const MODE_PARAMETER* mode) {
	return (DWORD)(
		(
			(mode->v_linesi * mode->h_clki) /
			(vidstda[mode->nVideoStd].m_TotalLinesOut * mode->clk_ratio)
		)
		+ 0.5
	);
}

static void SetVGAConexantRegister(BYTE pll_int, BYTE* pbRegs)
{
	BYTE b;

	I2CTransmitWord(0x45, (0xba<<8) | 0x80); // Conexant reset
	I2CTransmitByteGetReturn(0x45, 0xb8);       // dummy read to wait for completion
	I2CTransmitWord(0x45, (0xa0<<8) | 0x13); // Switch to Pseudo-Master mode
	I2CTransmitWord(0x45, (0x2e<<8) | 0xad); // HDTV_EN = 1, RPR_SYNC_DIS = 1, BPB_SYNC_DIS = 1, HD_SYNC_EDGE = 1, RASTER_SEL = 01
	I2CTransmitWord(0x45, (0x32<<8) | 0x48); // DRVS = 2, IN_MODE[3] = 1;
	I2CTransmitWord(0x45, (0x3c<<8) | 0x80); // MCOMPY
	I2CTransmitWord(0x45, (0x3e<<8) | 0x80); // MCOMPU
	I2CTransmitWord(0x45, (0x40<<8) | 0x80); // MCOMPV
	I2CTransmitWord(0x45, (0x6c<<8) | 0x46); // FLD_MODE = 10, EACTIVE = 1, EN_SCART = 0, EN_REG_RD = 1
	I2CTransmitWord(0x45, (0x9c<<8) | 0x00); // PLL_FRACT
	I2CTransmitWord(0x45, (0x9e<<8) | 0x00); // PLL_FRACT
	I2CTransmitWord(0x45, (0xa0<<8) | pll_int); // PLL_INT
	I2CTransmitWord(0x45, (0xba<<8) | 0x28); // SLAVER = 1, DACDISD = 1
	I2CTransmitWord(0x45, (0xc4<<8) | 0x01); // EN_OUT = 1
	I2CTransmitWord(0x45, (0xc6<<8) | 0x98); // IN_MODE = 24 bit RGB multiplexed
	I2CTransmitWord(0x45, (0xce<<8) | 0xe1); // OUT_MUXA = 01, OUT_MUXB = 00, OUT_MUXC = 10, OUT_MUXD = 11
	I2CTransmitWord(0x45, (0xd6<<8) | 0x0c); // OUT_MODE = 11 (RGB / SCART / HDTV)
	*((DWORD *)&pbRegs[0x680630]) = 0; // switch GPU to RGB

	// Timing Reset
	b=I2CTransmitByteGetReturn(0x45, 0x6c)&(0x7f);
	I2CTransmitWord(0x45, (0x6c<<8)|0x80|b);
}

static void SetTVConexantRegister(const MODE_PARAMETER* mode, const BLANKING_PARAMETER* blanks, BYTE* pbRegs)
{
	BYTE b;
	DWORD m = 0;
	ULONG h_clko = CalcH_CLKO(mode);
	ULONG v_activeo = CalcV_ACTIVEO(mode);
	double dPllOutputFrequency;

	I2CTransmitWord(0x45, (0xb8<<8) | 0x07); // Set autoconfig, 800x600, PAL, YCrCb in
	I2CTransmitByteGetReturn(0x45, 0xb8);    // dummy read to wait for completion
	I2CTransmitWord(0x45, (0xa0<<8) | 0x13); // Switch to Pseudo-Master mode
	I2CTransmitWord(0x45, (0x32<<8) | 0x28); // DRVS = 1, IN_MODE[3] = 1;

	// H_CLKI
	b=I2CTransmitByteGetReturn(0x45, 0x8e)&(~0x07);
	I2CTransmitWord(0x45, (0x8e<<8)|((mode->h_clki>>8)&0x07)|b);
	I2CTransmitWord(0x45, (0x8a<<8)|((mode->h_clki)&0xff));
	// H_CLKO
	b=I2CTransmitByteGetReturn(0x45, 0x86)&(~0x0f);
	I2CTransmitWord(0x45, (0x86<<8)|((h_clko>>8)&0x0f)|b);
	I2CTransmitWord(0x45, (0x76<<8)|((h_clko)&0xff));
	// V_LINESI
	b=I2CTransmitByteGetReturn(0x45, 0x38)&(~0x02);
	I2CTransmitWord(0x45, (0x38<<8)|((mode->v_linesi>>9)&0x02)|b);
	b=I2CTransmitByteGetReturn(0x45, 0x96)&(~0x03);
	I2CTransmitWord(0x45, (0x96<<8)|((mode->v_linesi>>8)&0x03)|b);
	I2CTransmitWord(0x45, (0x90<<8)|((mode->v_linesi)&0xff));
	// V_ACTIVEO
	/* TODO: Absolutely not sure about other modes than plain NTSC / PAL */
	switch(mode->nVideoStd) {
		case NTSC:
		case NTSC60:
		case PALM:
		case PAL60:
			m=v_activeo + 1;
			break;
		case PALBDGHI:
			m=v_activeo + 2;
			break;
		default:
			m=v_activeo + 2;
			break;
	}
	b=I2CTransmitByteGetReturn(0x45, 0x86)&(~0x80);
	I2CTransmitWord(0x45, (0x86<<8)|((m>>1)&0x80)|b);
	I2CTransmitWord(0x45, (0x84<<8)|((m)&0xff));
	// H_ACTIVE
	b=I2CTransmitByteGetReturn(0x45, 0x86)&(~0x70);
	I2CTransmitWord(0x45, (0x86<<8)|(((mode->h_active + 5)>>4)&0x70)|b);
	I2CTransmitWord(0x45, (0x78<<8)|((mode->h_active + 5)&0xff));
	// V_ACTIVEI
	b=I2CTransmitByteGetReturn(0x45, 0x96)&(~0x0c);
	I2CTransmitWord(0x45, (0x96<<8)|((mode->v_activei>>6)&0x0c)|b);
	I2CTransmitWord(0x45, (0x94<<8)|((mode->v_activei)&0xff));
	// H_BLANKI
	b=I2CTransmitByteGetReturn(0x45, 0x38)&(~0x01);
	I2CTransmitWord(0x45, (0x38<<8)|((blanks->h_blanki>>9)&0x01)|b);
	b=I2CTransmitByteGetReturn(0x45, 0x8e)&(~0x08);
	I2CTransmitWord(0x45, (0x8e<<8)|((blanks->h_blanki>>5)&0x08)|b);
	I2CTransmitWord(0x45, (0x8c<<8)|((blanks->h_blanki)&0xff));
	// H_BLANKO
	b=I2CTransmitByteGetReturn(0x45, 0x9a)&(~0xc0);
	I2CTransmitWord(0x45, (0x9a<<8)|((blanks->h_blanko>>2)&0xc0)|b);
	I2CTransmitWord(0x45, (0x80<<8)|((blanks->h_blanko)&0xff));

	// V_SCALE
#ifdef JUSTVIDEO
	printf("Computed vertical scaling coeff=%ld\n", blanks->vscale);
#endif
	b=I2CTransmitByteGetReturn(0x45, 0x9a)&(~0x3f);
	I2CTransmitWord(0x45, (0x9a<<8)|((blanks->vscale>>8)&0x3f)|b);
	I2CTransmitWord(0x45, (0x98<<8)|((blanks->vscale)&0xff));
	// V_BLANKO
	I2CTransmitWord(0x45, (0x82<<8)|((blanks->v_blanko)&0xff));
	// V_BLANKI
	I2CTransmitWord(0x45, (0x92<<8)|((blanks->v_blanki)&0xff));
	{
		DWORD dwPllRatio, dwFract, dwInt;
		// adjust PLL
		dwPllRatio = (long)(6.0 * ((double)h_clko / vidstda[mode->nVideoStd].m_dSecHsyncPeriod) *
			mode->clk_ratio * dPllBasePeriod * 0x10000 + 0.5);
		dwInt = dwPllRatio / 0x10000;
		dwFract = dwPllRatio - (dwInt * 0x10000);
		b=I2CTransmitByteGetReturn(0x45, 0xa0)&(~0x3f);
		I2CTransmitWord(0x45, (0xa0<<8)|((dwInt)&0x3f)|b);
		I2CTransmitWord(0x45, (0x9e<<8)|((dwFract>>8)&0xff));
		I2CTransmitWord(0x45, (0x9c<<8)|((dwFract)&0xff));
		// recalc value
		dPllOutputFrequency = ((double)dwInt + ((double)dwFract)/65536.0)/(6 * dPllBasePeriod * mode->clk_ratio);
#ifdef JUSTVIDEO
		printf("pll clock frequency=%.3f MHz\n", dPllOutputFrequency / 1e6);
#endif
		// enable 3:2 clocking mode
		b=I2CTransmitByteGetReturn(0x45, 0x38)&(~0x20);
		if (mode->clk_ratio > 1.1) {
			b |= 0x20;
		}
		I2CTransmitWord(0x45, (0x38<<8)|b);

		// update burst start position
		m=(vidstda[mode->nVideoStd].m_dSecBurstStart) * dPllOutputFrequency + 0.5;
		b=I2CTransmitByteGetReturn(0x45, 0x38)&(~0x04);
		I2CTransmitWord(0x45, (0x38<<8)|((m>>6)&0x04)|b);
		I2CTransmitWord(0x45, (0x7c<<8)|(m&0xff));
		// update burst end position (note +128 is in hardware)
		m=(vidstda[mode->nVideoStd].m_dSecBurstEnd) * dPllOutputFrequency + 0.5;
		if(m<128) m=128;
		b=I2CTransmitByteGetReturn(0x45, 0x38)&(~0x08);
		I2CTransmitWord(0x45, (0x38<<8)|(((m-128)>>5)&0x08)|b);
		I2CTransmitWord(0x45, (0x7e<<8)|((m-128)&0xff));
		// update HSYNC width
		m=(vidstda[mode->nVideoStd].m_dSecHsyncWidth) * dPllOutputFrequency + 0.5;
		I2CTransmitWord(0x45, (0x7a<<8)|((m)&0xff));
	}
	// adjust Subcarrier generation increment
	{
		DWORD dwSubcarrierIncrement = (DWORD) (
			(65536.0 * 65536.0) * (
				vidstda[mode->nVideoStd].m_dHzBurstFrequency
				* vidstda[mode->nVideoStd].m_dSecHsyncPeriod
				/ (double)h_clko
			) + 0.5
		);
		I2CTransmitWord(0x45, (0xae<<8)|(dwSubcarrierIncrement&0xff));
		I2CTransmitWord(0x45, (0xb0<<8)|((dwSubcarrierIncrement>>8)&0xff));
		I2CTransmitWord(0x45, (0xb2<<8)|((dwSubcarrierIncrement>>16)&0xff));
		I2CTransmitWord(0x45, (0xb4<<8)|((dwSubcarrierIncrement>>24)&0xff));
	}
	// adjust WSS increment
	{
		DWORD dwWssIncrement = 0;

		switch(mode->nVideoStd) {
			case NTSC:
			case NTSC60:
				dwWssIncrement=(DWORD) ((1048576.0 / ( 0.000002234 * dPllOutputFrequency))+0.5);
				break;
			case PALBDGHI:
			case PALN:
			case PALNC:
			case PALM:
			case PAL60:
				dwWssIncrement=(DWORD) ((1048576.0 / ( 0.0000002 * dPllOutputFrequency))+0.5);
				break;
			default:
				break;
			}

		I2CTransmitWord(0x45, (0x66<<8)|(dwWssIncrement&0xff));
		I2CTransmitWord(0x45, (0x68<<8)|((dwWssIncrement>>8)&0xff));
		I2CTransmitWord(0x45, (0x6a<<8)|((dwWssIncrement>>16)&0xf));
	}
	// set mode register
	b=I2CTransmitByteGetReturn(0x45, 0xa2)&(0x41);
	switch(mode->nVideoStd) {
			case NTSC:
				b |= 0x0a; // SETUP + VSYNC_DUR
				break;
			case NTSC60:
				b |= 0x08; // VSYNC_DUR
				break;
			case PALBDGHI:
			case PALNC:
    				b |= 0x24; // PAL_MD + 625LINE
				break;
			case PALN:
				b |= 0x2e; // PAL_MD + SETUP + 625LINE + VSYNC_DUR
				break;
			case PALM:
				b |= 0x2a; // PAL_MD + SETUP + VSYNC_DUR
				break;
			case PAL60:
				b |= 0x28; // PAL_MD + VSYNC_DUR
				break;
			default:
				break;
	}
	I2CTransmitWord(0x45, (0xa2<<8)|b);
	switch(xbvDetectAvType()) {
		case AV_COMPOSITE:
		case AV_SVIDEO:
			I2CTransmitWord(0x45, (0x6c<<8) | 0x46); // FLD_MODE = 10, EACTIVE = 1, EN_SCART = 0, EN_REG_RD = 1
			I2CTransmitWord(0x45, (0x5a<<8) | 0x00); // Y_OFF (Brightness)
			I2CTransmitWord(0x45, (0xa4<<8) | 0xe5); // SYNC_AMP
			I2CTransmitWord(0x45, (0xa6<<8) | 0x74); // BST_AMP
			I2CTransmitWord(0x45, (0xba<<8) | 0x24); // SLAVER = 1, DACDISC = 1
			I2CTransmitWord(0x45, (0xc6<<8) | 0x9c); // IN_MODE = 24 bit YCrCb multiplexed
			I2CTransmitWord(0x45, (0xce<<8) | 0x19); // OUT_MUXA = 01, OUT_MUXB = 10, OUT_MUXC = 10, OUT_MUXD = 00
			I2CTransmitWord(0x45, (0xd6<<8) | 0x00); // OUT_MODE = 00 (CVBS)
			*((DWORD *)&pbRegs[0x680630]) = 2; // switch GPU to YCrCb
			*((DWORD *)&pbRegs[0x68084c]) =0x801080;
			break;
		case AV_SCART_RGB:
			I2CTransmitWord(0x45, (0x6c<<8) | 0x4e); // FLD_MODE = 10, EACTIVE = 1, EN_SCART = 1, EN_REG_RD = 1
			I2CTransmitWord(0x45, (0x5a<<8) | 0xff); // Y_OFF (Brightness)
			I2CTransmitWord(0x45, (0xa4<<8) | 0xe7); // SYNC_AMP
			I2CTransmitWord(0x45, (0xa6<<8) | 0x77); // BST_AMP
			I2CTransmitWord(0x45, (0xba<<8) | 0x20); // SLAVER = 1, enable all DACs
			I2CTransmitWord(0x45, (0xc6<<8) | 0x98); // IN_MODE = 24 bit RGB multiplexed
			I2CTransmitWord(0x45, (0xce<<8) | 0xe1); // OUT_MUXA = 01, OUT_MUXB = 00, OUT_MUXC = 10, OUT_MUXD = 11
			I2CTransmitWord(0x45, (0xd6<<8) | 0x0c); // OUT_MODE = 11 (RGB / SCART / HDTV)
			*((DWORD *)&pbRegs[0x680630]) = 0; // switch GPU to RGB
			*((DWORD *)&pbRegs[0x68084c]) =0;
			break;
		case AV_HDTV:
			I2CTransmitWord(0x45, (0x6c<<8) | 0x46); // FLD_MODE = 10, EACTIVE = 1, EN_SCART = 0, EN_REG_RD = 1
			I2CTransmitWord(0x45, (0x5a<<8) | 0x00); // Y_OFF (Brightness)
			I2CTransmitWord(0x45, (0xa4<<8) | 0xe5); // SYNC_AMP
			I2CTransmitWord(0x45, (0xa6<<8) | 0x74); // BST_AMP
			I2CTransmitWord(0x45, (0xba<<8) | 0x20); // SLAVER = 1, enable all DACs
			I2CTransmitWord(0x45, (0xc6<<8) | 0x9c); // IN_MODE = 24 bit YCrCb multiplexed
			I2CTransmitWord(0x45, (0xce<<8) | 0x21); // OUT_MUXA = 01, OUT_MUXB = 00, OUT_MUXC = 10, OUT_MUXD = 00
			I2CTransmitWord(0x45, (0xd6<<8) | 0x08); // OUT_MODE = 10 (VYU)
			*((DWORD *)&pbRegs[0x680630]) = 2; // switch GPU to YCrCb
			*((DWORD *)&pbRegs[0x68084c]) =0x801080;
			break;
		default:
			break;
	}
	I2CTransmitWord(0x45, (0xc4<<8) | 0x01); // EN_OUT = 1
	// Timing Reset
	b=I2CTransmitByteGetReturn(0x45, 0x6c)&(0x7f);
	I2CTransmitWord(0x45, (0x6c<<8)|0x80|b);
}

static void SetGPURegister(const GPU_PARAMETER* gpu, BYTE *pbRegs) {
	BYTE b;
	DWORD m = 0;

	// NVHDISPEND
	*((DWORD *)&pbRegs[0x680820]) = gpu->crtchdispend - 1;
	// NVHTOTAL
	*((DWORD *)&pbRegs[0x680824]) = gpu->nvhtotal;
	// NVHCRTC
	*((DWORD *)&pbRegs[0x680828]) = gpu->xres - 1;
	// NVHVALIDSTART
	*((DWORD *)&pbRegs[0x680834]) = 0;
	// NVHSYNCSTART
	*((DWORD *)&pbRegs[0x68082c]) = gpu->nvhstart;
	// NVHSYNCEND = NVHSYNCSTART + 32
	*((DWORD *)&pbRegs[0x680830]) = gpu->nvhstart+32;
	// NVHVALIDEND
	*((DWORD *)&pbRegs[0x680838]) = gpu->xres - 1;
	// CRTC_HSYNCSTART = h_total - 32 (heuristic)
	m = gpu->nvhtotal - 32;
	NvSetCrtc(pbRegs, 4, m/8);
	// CRTC_HSYNCEND = CRTC_HSYNCSTART + 16
	NvSetCrtc(pbRegs, 5, (NvGetCrtc(pbRegs, 5)&0xe0) | ((((m + 16)/8)-1)&0x1f) );
	// CRTC_HTOTAL = nvh_total (heuristic)
	NvSetCrtc(pbRegs, 0, (gpu->nvhtotal/8)-5);
	// CRTC_HBLANKSTART = crtchdispend
	NvSetCrtc(pbRegs, 2, ((gpu->crtchdispend)/8)-1);
	// CRTC_HBLANKEND = CRTC_HTOTAL = nvh_total
	NvSetCrtc(pbRegs, 3, (NvGetCrtc(pbRegs, 3)&0xe0) |(((gpu->nvhtotal/8)-1)&0x1f));
	NvSetCrtc(pbRegs, 5, (NvGetCrtc(pbRegs, 5)&(~0x80)) | ((((gpu->nvhtotal/8)-1)&0x20)<<2) );
	// CRTC_HDISPEND
	NvSetCrtc(pbRegs, 0x17, (NvGetCrtc(pbRegs, 0x17)&0x7f));
	NvSetCrtc(pbRegs, 1, ((gpu->crtchdispend)/8)-1);
	NvSetCrtc(pbRegs, 2, ((gpu->crtchdispend)/8)-1);
	NvSetCrtc(pbRegs, 0x17, (NvGetCrtc(pbRegs, 0x17)&0x7f)|0x80);
	// CRTC_LINESTRIDE = (xres / 8) * pixelDepth
	m=(gpu->xres / 8) * gpu->pixelDepth;
	NvSetCrtc(pbRegs, 0x19, (NvGetCrtc(pbRegs, 0x19)&0x1f) | ((m >> 3) & 0xe0));
	NvSetCrtc(pbRegs, 0x13, (m & 0xff));
	// NVVDISPEND
	*((DWORD *)&pbRegs[0x680800]) = gpu->yres - 1;
	// NVVTOTAL
	*((DWORD *)&pbRegs[0x680804]) = gpu->nvvtotal;
	// NVVCRTC
	*((DWORD *)&pbRegs[0x680808]) = gpu->yres - 1;
	// NVVVALIDSTART
	*((DWORD *)&pbRegs[0x680814]) = 0;
	// NVVSYNCSTART
	*((DWORD *)&pbRegs[0x68080c])=gpu->nvvstart;
	// NVVSYNCEND = NVVSYNCSTART + 3
	*((DWORD *)&pbRegs[0x680810])=(gpu->nvvstart+3);
	// NVVVALIDEND
	*((DWORD *)&pbRegs[0x680818]) = gpu->yres - 1;
	// CRTC_VSYNCSTART
	b = NvGetCrtc(pbRegs, 7) & 0x7b;
	NvSetCrtc(pbRegs, 7, b | ((gpu->crtcvstart >> 2) & 0x80) | ((gpu->crtcvstart >> 6) & 0x04));
	NvSetCrtc(pbRegs, 0x10, (gpu->crtcvstart & 0xff));
	// CRTC_VTOTAL
	b = NvGetCrtc(pbRegs, 7) & 0xde;
	NvSetCrtc(pbRegs, 7, b | ((gpu->crtcvtotal >> 4) & 0x20) | ((gpu->crtcvtotal >> 8) & 0x01));
	NvSetCrtc(pbRegs, 6, (gpu->crtcvtotal & 0xff));
	// CRTC_VBLANKEND = CRTC_VTOTAL
	b = NvGetCrtc(pbRegs, 0x16) & 0x80;
	NvSetCrtc(pbRegs, 0x16, b |(gpu->crtcvtotal & 0x7f));
	// CRTC_VDISPEND = yres
	b = NvGetCrtc(pbRegs, 7) & 0xbd;
	NvSetCrtc(pbRegs, 7, b | (((gpu->yres - 1) >> 3) & 0x40) | (((gpu->yres - 1) >> 7) & 0x02));
	NvSetCrtc(pbRegs, 0x12, ((gpu->yres - 1) & 0xff));
	// CRTC_VBLANKSTART
	b = NvGetCrtc(pbRegs, 9) & 0xdf;
	NvSetCrtc(pbRegs, 9, b | (((gpu->yres - 1)>> 4) & 0x20));
	b = NvGetCrtc(pbRegs, 7) & 0xf7;
	NvSetCrtc(pbRegs, 7, b | (((gpu->yres - 1) >> 5) & 0x08));
	NvSetCrtc(pbRegs, 0x15, ((gpu->yres - 1) & 0xff));
	// CRTC_LINECOMP
	m = 0x3ff; // 0x3ff = disable
	b = NvGetCrtc(pbRegs, 7) & 0xef;
	NvSetCrtc(pbRegs, 7, b | ((m>> 4) & 0x10));
	b = NvGetCrtc(pbRegs, 9) & 0xbf;
	NvSetCrtc(pbRegs, 9, b | ((m >> 3) & 0x40));
	NvSetCrtc(pbRegs, 0x18, (m & 0xff));
	// CRTC_REPAINT1
	if (gpu->xres < 1280) {
		b = 0x04;
	}
	else {
		b = 0x00;
	}
	NvSetCrtc(pbRegs, 0x1a, b);
	// Overflow bits
	/*
	b = ((hTotal   & 0x040) >> 2)
		| ((vDisplay & 0x400) >> 7)
		| ((vStart   & 0x400) >> 8)
		| ((vDisplay & 0x400) >> 9)
		| ((vTotal   & 0x400) >> 10);
	*/
	b = (((gpu->nvhtotal / 8 - 5) & 0x040) >> 2)
		| (((gpu->yres - 1) & 0x400) >> 7)
		| ((gpu->crtcvstart & 0x400) >> 8)
		| (((gpu->yres - 1) & 0x400) >> 9)
		| ((gpu->crtcvtotal & 0x400) >> 10);
	NvSetCrtc(pbRegs, 0x25, b);

	b = gpu->pixelDepth;
	if (b >= 3) b = 3;
	/* switch pixel mode to TV */
	b  |= 0x80;
	NvSetCrtc(pbRegs, 0x28, b);

	b = NvGetCrtc(pbRegs, 0x2d) & 0xe0;
	if ((gpu->nvhtotal / 8 - 1) >= 260) {
		b |= 0x01;
	}
	NvSetCrtc(pbRegs, 0x2d, b);
}


static void CalcBlankings(const MODE_PARAMETER* m, BLANKING_PARAMETER* blanks)
{
	/* algorithm shamelessly ripped from nvtv/calc_bt.c */
	double dTotalHBlankI;
	double dFrontPorchIn;
	double dFrontPorchOut;
	double dMinFrontPorchIn;
	double dBackPorchIn;
	double dBackPorchOut;
	double dTotalHBlankO;
	double dHeadRoom;
	double dMaxHsyncDrift;
	double dFifoMargin;
	double vsrq;
	double dMaxHR;
	double tlo =  vidstda[m->nVideoStd].m_TotalLinesOut;
	const int MFP = 14; // Minimum front porch
	const int MBP = 4;  // Minimum back porch
	const int FIFO_SIZE = 1024;
	double vsr = (double)m->v_linesi / vidstda[m->nVideoStd].m_TotalLinesOut;
	DWORD v_activeo = CalcV_ACTIVEO(m);
	DWORD h_clko = CalcH_CLKO(m);

	// H_BLANKO
	blanks->h_blanko = 2 * (long)(
		vidstda[m->nVideoStd].m_dSecImageCentre / (2 * vidstda[m->nVideoStd].m_dSecHsyncPeriod) *
		h_clko
		+ 0.5
	) - m->h_active + 15;

	// V_BLANKO
	switch (m->nVideoStd) {
		case NTSC:
		case NTSC60:
		case PAL60:
		case PALM:
			blanks->v_blanko = (long)( 140 - ( v_activeo / 2.0 ) + 0.5 );
			break;
		default:
			blanks->v_blanko = (long)( 167 - ( v_activeo / 2.0 ) + 0.5 );
			break;
	}

	// V_BLANKI
	vsrq = ( (long)( vsr * 4096.0 + .5 ) ) / 4096.0;
	blanks->vscale = (long)( ( vsr - 1 ) * 4096 + 0.5 );
	if( vsrq < vsr )
	{
	// These calculations are in units of dHCLKO
		dMaxHsyncDrift = ( vsrq - vsr ) * tlo / vsr * h_clko;
		dMinFrontPorchIn = MFP / ( (double)m->h_clki * vsr ) * h_clko;
		dFrontPorchOut = h_clko - blanks->h_blanko - m->h_active * 2;
		dFifoMargin = ( FIFO_SIZE - m->h_active ) * 2;

		// Check for fifo overflow
		if( dFrontPorchOut + dFifoMargin < -dMaxHsyncDrift + dMinFrontPorchIn )
		{
			dTotalHBlankO = h_clko - m->h_active * 2;
			dTotalHBlankI = ( (double)m->h_clki - (double)m->h_active ) / m->h_clki / vsr * h_clko;

			// Try forcing the Hsync drift the opposite direction
			dMaxHsyncDrift = ( vsrq + 1.0 / 4096 - vsr ) * tlo / vsr * h_clko;

			// Check that fifo overflow and underflow can be avoided
			if( dTotalHBlankO + dFifoMargin >= dTotalHBlankI + dMaxHsyncDrift )
			{
				vsrq = vsrq + 1.0 / 4096;
				blanks->vscale = (long)( ( vsrq - 1 ) * 4096 );
			}

			// NOTE: If fifo overflow and underflow can't be avoided,
			//       alternative overscan compensation ratios should
			//       be selected and all calculations repeated.  If
			//       that is not feasible, the calculations for
			//       H_BLANKI below will delay the overflow or under-
			//       flow as much as possible, to minimize the visible
		//       artifacts.
		}
	}

	blanks->v_blanki = (long)( ( blanks->v_blanko - 1 ) * vsrq );

	// H_BLANKI

	// These calculations are in units of dHCLKI
	dTotalHBlankI = m->h_clki - m->h_active;
	dFrontPorchIn = max( MFP, min( dTotalHBlankI / 8.0, dTotalHBlankI - MBP ) );
	dBackPorchIn = dTotalHBlankI - dFrontPorchIn;
	dMaxHsyncDrift = ( vsrq - vsr ) * tlo * m->h_clki;
	dTotalHBlankO = ( h_clko - m->h_active * 2.0 ) / h_clko * vsr * m->h_clki;
	dBackPorchOut = ((double)blanks->h_blanko) / (double)h_clko * vsr * m->h_clki;
	dFrontPorchOut = dTotalHBlankO - dBackPorchOut;
	dFifoMargin = ( FIFO_SIZE - m->h_active ) * 2.0 / h_clko * vsr * m->h_clki;
	// This may be excessive, but is adjusted by the code.
	dHeadRoom = 32.0;

	// Check that fifo overflow and underflow can be avoided
	if( ( dTotalHBlankO + dFifoMargin ) >= ( dTotalHBlankI + fabs( dMaxHsyncDrift ) ) )
	{
		dMaxHR = ( dTotalHBlankO + dFifoMargin ) - ( dTotalHBlankI - fabs( dMaxHsyncDrift ) );
		if( dMaxHR < ( dHeadRoom * 2.0 ) )
		{
			dHeadRoom = (long)( dMaxHR / 2.0);
		}

		// Check for overflow
		if( ( ( dFrontPorchOut + dFifoMargin ) - dHeadRoom ) < ( dFrontPorchIn - min( dMaxHsyncDrift, 0 ) ) )
		{
			dFrontPorchIn = max( MFP, ( dFrontPorchOut + dFifoMargin + min( dMaxHsyncDrift, 0 ) - dHeadRoom ) );
			dBackPorchIn = dTotalHBlankI - dFrontPorchIn;
		}

		// Check for underflow
		if( dBackPorchOut - dHeadRoom < dBackPorchIn + max( dMaxHsyncDrift, 0 ) )
		{
			dBackPorchIn = max( MBP, ( dBackPorchOut - max( dMaxHsyncDrift, 0 ) - dHeadRoom ) );
			dFrontPorchIn = dTotalHBlankI - dBackPorchIn;
		}
	}
	else if( dMaxHsyncDrift < 0 )
	{
#ifdef JUSTVIDEO
		printf("Overflow expected\n");
#endif
		// Delay the overflow as long as possible
		dBackPorchIn = min( ( dBackPorchOut - 1 ), ( dTotalHBlankI - MFP ) );
		dFrontPorchIn = dTotalHBlankI - dBackPorchIn;
	}
	else
	{
#ifdef JUSTVIDEO
		printf("Underflow expected\n");
#endif
		// Delay the underflow as long as possible
		dFrontPorchIn = min( ( dFrontPorchOut + dFifoMargin - 1 ), ( dTotalHBlankI - MBP ) );
		dBackPorchIn = dTotalHBlankI - dFrontPorchIn;
	}

	blanks->h_blanki = (long)( dBackPorchIn );
}

bool FindOverscanValues(
	long h_active,
	long v_activei,
	double hoc,
	double voc,
	long bpp,
	EVIDEOSTD nVideoStd,
	MODE_PARAMETER* result
){
	const double  dMinHBT = 2.5e-6; // 2.5uSec time for horizontal syncing

	/* algorithm shamelessly ripped from nvtv/calc_bt.c */
	double dTempVOC = 0;
	double dTempHOC = 0;
	DWORD  dMinTLI = 0;
	DWORD  dMaxTLI = 0;
	DWORD  dTempTLI = 0;
	DWORD  dBestTLI = 0;
	DWORD  dMinHCLKO = 0;
	DWORD  dMaxHCLKO = 0;
	double dBestMetric = 1000;
	double dTempVSR = 0;
	DWORD  dMinHCLKI = 0;
	DWORD  dTempHCLKI = 0;
	DWORD  dBestHCLKI = 0;
	double dBestVSR = 0;
	double dTempCLKRATIO = 1;
	double dBestCLKRATIO = 1;
	int    actCLKRATIO;
	DWORD  dTempHCLKO = 0;
	double dTempVACTIVEO = 0;
	double dDelta = 0;
	double dMetric = 0;
	double alo =  vidstda[nVideoStd].m_dwALO;
	double tlo =  vidstda[nVideoStd].m_TotalLinesOut;
	double tto = vidstda[nVideoStd].m_dSecHsyncPeriod;
	double ato = tto - (vidstda[nVideoStd].m_dSecBlankBeginToHsync + vidstda[nVideoStd].m_dSecActiveBegin);

	/* Range to search */
	double dMinHOC = hoc - 0.02;
	double dMaxHOC = hoc + 0.02;
	double dMinVOC = voc - 0.02;
	double dMaxVOC = voc + 0.02;

	if (dMinHOC < 0) dMinHOC = 0;
	if (dMinVOC < 0) dMinVOC = 0;

	result->nVideoStd = nVideoStd;
	result->h_active = h_active;
	result->v_activei = v_activei;
	result->bpp = bpp;

	dMinTLI= (DWORD)(v_activei / ((1 - dMinVOC) * alo) * tlo);
	dMaxTLI = (DWORD)(v_activei / ((1 - dMaxVOC) * alo) * tlo);
	dMinHCLKO = (DWORD) ((h_active * 2) /
				((1 - dMinHOC) * (ato / tto)));
	dMaxHCLKO = (DWORD) ((h_active * 2) /
				((1 - dMaxHOC) * (ato / tto)));
	for (actCLKRATIO = 0; actCLKRATIO <= 1; actCLKRATIO++)
	{
		dTempCLKRATIO = 1.0;
		if (actCLKRATIO) dTempCLKRATIO = 3.0/2.0;
		for(dTempTLI = dMinTLI; dTempTLI <= dMaxTLI; dTempTLI++)
		{
			dTempVSR = (double)dTempTLI / tlo;
			dTempVACTIVEO = (long)((((double)v_activei * tlo) +
						(dTempTLI - 1)) / dTempTLI);
			dTempVOC = 1 - dTempVACTIVEO / alo;

			for(dTempHCLKO = dMinHCLKO; dTempHCLKO <= dMaxHCLKO; dTempHCLKO++)
			{
				dTempHCLKI = (DWORD)((dTempHCLKO * dTempCLKRATIO) * (tlo / dTempTLI) + 0.5);
				dMinHCLKI = ((dMinHBT / tto) * dTempHCLKI) + h_active;
				// check if solution is valid
				if ((fabs((double)(dTempTLI * dTempHCLKI) - (tlo * dTempHCLKO * dTempCLKRATIO)) < 1e-3) &&
					(dTempHCLKI >= dMinHCLKI))
				{
					dTempHOC = 1 - (((double)h_active / ((double)dTempHCLKO / 2)) /
						(ato / tto));
					dDelta = fabs(dTempHOC - hoc) + fabs(dTempVOC - voc);
					dMetric = ((dTempHOC - hoc) * (dTempHOC - hoc)) +
						((dTempVOC - voc) * (dTempVOC - voc)) +
						(2 * dDelta * dDelta);
					if(dMetric < dBestMetric)
					{
						dBestVSR = dTempVSR;
						dBestMetric = dMetric;
						dBestTLI = dTempTLI;
						dBestHCLKI = dTempHCLKI;
						dBestCLKRATIO = dTempCLKRATIO;
					}
				} /* valid solution */
			} /* dTempHCLKO loop */
		} /* dTempTLI loop */
	} /* CLKRATIO loop */

	if(dBestMetric == 1000)
	{
		return FALSE;
	}
	result->v_linesi = dBestTLI;
	result->h_clki = dBestHCLKI;
	result->clk_ratio = dBestCLKRATIO;

	return TRUE;
}

void SetAutoParameter(const MODE_PARAMETER* mode, BYTE *pbRegs) {
	ULONG v_activeo = 0;
	ULONG h_clko = 0;
	double hoc = 0;
	double voc = 0;
	double  tto = vidstda[mode->nVideoStd].m_dSecHsyncPeriod;
	double  ato = tto - (vidstda[mode->nVideoStd].m_dSecBlankBeginToHsync + vidstda[mode->nVideoStd].m_dSecActiveBegin);
	BLANKING_PARAMETER blanks;
	GPU_PARAMETER gpu;

	// Let's go
	CalcBlankings (mode, &blanks);
	v_activeo = CalcV_ACTIVEO(mode);
	h_clko = CalcH_CLKO(mode);
	hoc = 1 - ((2* (double)mode->h_active / (double)h_clko) / (ato / tto));
	voc = 1- ((double)v_activeo / vidstda[mode->nVideoStd].m_dwALO);
#ifdef JUSTVIDEO
	printf("Computed horizontal overscan factor=%.2f%%\n", hoc * 100);
	printf("Computed vertical overscan factor=%.2f%%\n", voc * 100);
	printf("H_CLKO: %ld\n", h_clko/2);
	printf("V_ACTIVEO: %ld\n", v_activeo);
	printf("H_BLANKI: %ld\n", blanks.h_blanki);
	printf("H_BLANKO: %ld\n", blanks.h_blanko);
	printf("V_BLANKI: %ld\n", blanks.v_blanki);
	printf("V_BLANKO: %ld\n", blanks.v_blanko);
#endif
	gpu.xres = mode->h_active;
	gpu.crtchdispend = mode->h_active + 8;
	gpu.nvhstart = mode->h_clki - blanks.h_blanki - 7;
	gpu.nvhtotal =  mode->h_clki - 1;
	gpu.yres = mode->v_activei;
	gpu.nvvstart = mode->v_linesi - blanks.v_blanki + 1;
	gpu.crtcvstart = mode->v_activei + 32;
	// CRTC_VTOTAL = v_linesi - yres + CRTC_VSYNCSTART = v_linesi + 32
	gpu.crtcvtotal = mode->v_linesi + 32;
	gpu.nvvtotal =  mode->v_linesi - 1;
	gpu.pixelDepth = (mode->bpp+1) / 8;

	SetTVConexantRegister(mode, &blanks, pbRegs);
	SetGPURegister(&gpu, pbRegs);
}