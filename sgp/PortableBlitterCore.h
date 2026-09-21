#pragma once

#include "vobject.h"

namespace ja2::blitter
{
enum class Paint
{
	Palette,
	Shadow,
	Intensity,
	Mono,
	Outline,
	OutlineShadow,
	Solid,
	Probe,
};

enum class DepthTest
{
	None,
	Less,
	LessEqual,
	Equal,
};

enum class Pixelation
{
	None,
	Always,
	WhenObscured,
};

enum class ShadowTreatment
{
	AsPaletteColor,
	ShadeDestination,
	Skip,
};

struct BlitPolicy
{
	Paint paint = Paint::Palette;
	DepthTest depthTest = DepthTest::None;
	Pixelation pixelation = Pixelation::None;
	UINT16* zBuffer = nullptr;
	UINT16 zValue = 0;
	const UINT16* palette = nullptr;
	HVOBJECT alphaObject = nullptr;
	SGPRect* clip = nullptr;
	const ZStripInfo* zStrip = nullptr;
	UINT16 zStripInitialDelta = 0;
	UINT16 zStripDelta = 0;
	bool* probeHit = nullptr;
	UINT16 solidColor = 0;
	UINT16 foreground = 0;
	UINT16 background = 0;
	UINT16 shadow = 0;
	INT16 outlineColor = 0;
	bool updateZ = false;
	ShadowTreatment shadowTreatment = ShadowTreatment::AsPaletteColor;
	bool mirror = false;
	bool translucent = false;
	bool outlineEnabled = true;
	bool solidWhenObscured = false;
	bool updateZWhenObscured = false;
	bool drawShadowWhenObscured = false;
	bool suppressEqualShadow = true;
};

BOOLEAN blitEtrle(UINT16* buffer, UINT32 pitchBytes, HVOBJECT object,
	INT32 x, INT32 y, UINT16 index, BlitPolicy policy);
}
