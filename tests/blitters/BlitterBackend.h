#pragma once

#include "vobject_blitters.h"

struct BlitterBackend
{
	const char* name;
	const char* compiler;
	void (*initialize)(UINT16 width, UINT16 height);
	void (*registerBuffer)(UINT16* buffer, UINT32 width, UINT32 height);
	void (*releaseBuffer)(UINT16* buffer);
	decltype(&Blt8BPPDataTo16BPPBufferTransparent) transparent;
	decltype(&Blt8BPPDataTo16BPPBufferTransparentClip) transparentClip;
	decltype(&Blt8BPPDataTo16BPPBufferTransZ) transZ;
	decltype(&Blt8BPPDataTo16BPPBufferTransZNB) transZNB;
	decltype(&Blt8BPPDataTo16BPPBufferTransZClip) transZClip;
	decltype(&Blt8BPPDataTo16BPPBufferTransZNBClip) transZNBClip;
	decltype(&Blt8BPPDataTo16BPPBufferTransShadow) transShadow;
	decltype(&Blt8BPPDataTo16BPPBufferTransShadowClip) transShadowClip;
	decltype(&Blt8BPPDataTo16BPPBufferTransShadowZ) transShadowZ;
	decltype(&Blt8BPPDataTo16BPPBufferTransShadowZNB) transShadowZNB;
	decltype(&Blt8BPPDataTo16BPPBufferTransShadowZNBObscured) obscuredShadow;
	decltype(&Blt8BPPDataTo16BPPBufferTransShadowAlpha) shadowAlpha;
	decltype(&Blt8BPPDataTo16BPPBufferShadow) shadow;
	decltype(&Blt8BPPDataTo16BPPBufferShadowClip) shadowClip;
	decltype(&Blt8BPPDataTo16BPPBufferShadowZ) shadowZ;
	decltype(&Blt8BPPDataTo16BPPBufferIntensity) intensity;
	decltype(&Blt8BPPDataTo16BPPBufferIntensityZ) intensityZ;
	decltype(&Blt8BPPDataTo16BPPBufferOutline) outline;
	decltype(&Blt8BPPDataTo16BPPBufferOutlineClip) outlineClip;
	decltype(&Blt8BPPDataTo16BPPBufferOutlineZ) outlineZ;
	decltype(&Blt8BPPDataTo16BPPBufferOutlineShadow) outlineShadow;
	decltype(&Blt8BPPDataTo16BPPBufferTransZPixelate) pixelate;
	decltype(&Blt8BPPDataTo16BPPBufferTransZPixelateObscured) pixelateObscured;
	decltype(&Blt8BPPDataTo16BPPBufferTransZTranslucent) translucent;
	decltype(&Blt8BPPDataTo16BPPBufferMonoShadowClip) monoClip;
	decltype(&Blt16BPPTo16BPP) copy16;
	decltype(&FillRect16BPP) fill16;
	decltype(&Blt16BPPBufferHatchRectWithColor) hatch;
	decltype(&Blt16BPPBufferShadowRect) shadeRect;
};

extern "C" const BlitterBackend* GetBlitterBackend();
