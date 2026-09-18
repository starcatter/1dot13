// Portable replacement for the original MSVC/x86 assembly blitter collection.
//
// The public API is intentionally unchanged.  The old implementation spells out
// every combination of clipping, depth testing, shadowing and pixelation as a
// separate assembly routine.  This implementation decodes ETRLE once and applies
// those differences as policy, following the behaviour of the original routines
// and the portable ETRLE walkers in JA2 Stracciatella.

#include "vobject_blitters.h"

#include "MemMan.h"
#include "PortableBlitterCore.h"
#include "ScreenGeometry.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>

SGPRect ClippingRect{};
UINT32 guiTranslucentMask = 0x3def;
std::map<UINT32, ClipRectangle> g_SurfaceRectangle;

namespace ja2::blitter
{
struct PaintedPixel
{
	UINT16 color;
	bool draw;
	bool applyAlpha;
};

SGPRect* effectiveClip(SGPRect* clip)
{
	return clip ? clip : &ClippingRect;
}

bool passesDepth(UINT16 current, UINT16 incoming, const BlitPolicy& policy)
{
	switch (policy.depthTest)
	{
		case DepthTest::None: return true;
		case DepthTest::Less: return current < incoming;
		case DepthTest::LessEqual: return current <= incoming;
		case DepthTest::Equal: return current == incoming;
	}
	return true;
}

struct ZStripCursor
{
	explicit ZStripCursor(const BlitPolicy& policy)
		: info(policy.zStrip), delta(policy.zStripDelta),
		  level(static_cast<UINT16>(static_cast<INT16>(policy.zValue) +
			  static_cast<INT16>(info ? info->bInitialZChange : 0) * policy.zStripInitialDelta)),
		  remaining(info ? info->ubFirstZStripWidth : 0)
	{
	}

	void advance()
	{
		if (!info) return;
		if (remaining > 0) --remaining;
		if (remaining != 0) return;
		if (changeIndex < info->ubNumberOfZChanges)
		{
			const INT8 change = info->pbZChange[changeIndex++];
			if (change < 0) level = static_cast<UINT16>(level - delta);
			else if (change > 0) level = static_cast<UINT16>(level + delta);
			remaining = 20;
		}
		else remaining = 0xffff;
	}

	const ZStripInfo* info;
	UINT16 delta;
	UINT16 level;
	UINT16 remaining;
	UINT16 changeIndex = 0;
};

bool checkerPixel(INT32 x, INT32 y)
{
	return (x & 1) == (y & 1);
}

UINT16 alphaBlend565(UINT16 foreground, UINT16 background, UINT8 alpha)
{
	const UINT32 inverse = 255 - alpha;
	const UINT32 r = ((((foreground >> 11) & 0x1f) * alpha) + (((background >> 11) & 0x1f) * inverse) + 127) / 255;
	const UINT32 g = ((((foreground >> 5) & 0x3f) * alpha) + (((background >> 5) & 0x3f) * inverse) + 127) / 255;
	const UINT32 b = (((foreground & 0x1f) * alpha) + ((background & 0x1f) * inverse) + 127) / 255;
	return static_cast<UINT16>((r << 11) | (g << 5) | b);
}

PaintedPixel paintPixel(UINT8 source, UINT16 destination, const BlitPolicy& policy)
{
	PaintedPixel result{destination, true, true};
	switch (policy.paint)
	{
		case Paint::Shadow:
			result.color = ShadeTable[destination];
			result.applyAlpha = false;
			break;
		case Paint::Intensity:
			result.color = IntensityTable[destination];
			result.applyAlpha = false;
			break;
		case Paint::Mono:
			if (source == 1)
			{
				result.color = policy.shadow;
				result.draw = policy.shadow != 0;
			}
			else if (source == 0)
			{
				result.color = policy.background;
				result.draw = policy.background != 0;
			}
			else result.color = policy.foreground;
			result.applyAlpha = false;
			break;
		case Paint::Outline:
			if (source == 254)
			{
				result.draw = policy.outlineEnabled;
				result.color = static_cast<UINT16>(policy.outlineColor);
			}
			else result.color = policy.palette[source];
			break;
		case Paint::OutlineShadow:
			result.color = source == 254 ? ShadeTable[destination] : policy.palette[source];
			result.applyAlpha = source != 254;
			break;
		case Paint::Solid:
			result.color = policy.solidColor;
			result.applyAlpha = false;
			break;
		case Paint::Probe:
			result.draw = false;
			break;
		case Paint::Palette:
			if (source == 254 && policy.shadowTreatment != ShadowTreatment::AsPaletteColor)
			{
				result.draw = policy.shadowTreatment == ShadowTreatment::ShadeDestination;
				result.color = ShadeTable[destination];
				result.applyAlpha = false;
			}
			else result.color = policy.palette[source];
			break;
	}
	return result;
}

BOOLEAN blitEtrle(UINT16* buffer, UINT32 pitchBytes, HVOBJECT object, INT32 x, INT32 y,
	UINT16 index, BlitPolicy policy)
{
	if ((!buffer && policy.paint != Paint::Probe) || !object || !object->pETRLEObject ||
		!object->pPixData || index >= object->usNumberOfObjects)
		return FALSE;

	const ETRLEObject& frame = object->pETRLEObject[index];
	const UINT8* source = static_cast<const UINT8*>(object->pPixData) + frame.uiDataOffset;
	const UINT8* const sourceEnd = source + frame.uiDataLength;
	const UINT8* alpha = nullptr;
	const UINT8* alphaEnd = nullptr;
	if (policy.alphaObject)
	{
		if (!policy.alphaObject->pETRLEObject || !policy.alphaObject->pPixData ||
			index >= policy.alphaObject->usNumberOfObjects) return FALSE;
		const ETRLEObject& alphaFrame = policy.alphaObject->pETRLEObject[index];
		if (alphaFrame.usWidth != frame.usWidth || alphaFrame.usHeight != frame.usHeight) return FALSE;
		alpha = static_cast<const UINT8*>(policy.alphaObject->pPixData) + alphaFrame.uiDataOffset;
		alphaEnd = alpha + alphaFrame.uiDataLength;
	}

	const INT32 originX = x + frame.sOffsetX;
	const INT32 originY = y + frame.sOffsetY;
	if (!policy.clip && (originX < 0 || originY < 0)) return FALSE;
	if (!policy.palette) policy.palette = object->pShadeCurrent;
	if ((policy.paint == Paint::Palette || policy.paint == Paint::Outline || policy.paint == Paint::OutlineShadow) && !policy.palette)
		return FALSE;

	const SGPRect* clip = policy.clip;
	const UINT32 pitch = pitchBytes / sizeof(UINT16);
	for (UINT16 sourceY = 0; sourceY < frame.usHeight; ++sourceY)
	{
		ZStripCursor zStrip(policy);
		const INT32 destinationY = originY + sourceY;
		UINT32 sourceX = 0;
		bool endOfLine = false;
		while (!endOfLine)
		{
			if (source >= sourceEnd || (alpha && alpha >= alphaEnd)) return FALSE;
			const UINT8 code = *source++;
			if (alpha) ++alpha; // Alpha ETRLE uses the same control-byte layout as the source.
			if (code == 0)
			{
				endOfLine = true;
				continue;
			}

			if (code & 0x80)
			{
				const UINT32 count = code & 0x7f;
				if (sourceX + count > frame.usWidth) return FALSE;
				if (policy.paint == Paint::Mono && policy.background != 0 && destinationY >= 0)
				{
					for (UINT32 pixel = 0; pixel < count; ++pixel)
					{
						const UINT32 logicalX = sourceX + pixel;
						const INT32 destinationX = originX + (policy.mirror ? frame.usWidth - 1 - logicalX : logicalX);
						if (destinationX < 0 || (clip && (destinationX < clip->iLeft || destinationX >= clip->iRight ||
							destinationY < clip->iTop || destinationY >= clip->iBottom))) continue;
						buffer[static_cast<std::size_t>(destinationY) * pitch + destinationX] = policy.background;
					}
				}
				for (UINT32 pixel = 0; pixel < count; ++pixel) zStrip.advance();
				sourceX += count;
				continue;
			}

			const UINT32 count = code;
			if (sourceX + count > frame.usWidth || static_cast<std::size_t>(sourceEnd - source) < count ||
				(alpha && static_cast<std::size_t>(alphaEnd - alpha) < count)) return FALSE;
			for (UINT32 pixel = 0; pixel < count; ++pixel)
			{
				const UINT8 sourcePixel = source[pixel];
				const UINT32 logicalX = sourceX + pixel;
				const UINT16 incomingZ = policy.zStrip ? zStrip.level : policy.zValue;
				zStrip.advance();
				const INT32 destinationX = originX + (policy.mirror ? frame.usWidth - 1 - logicalX : logicalX);
				if (destinationX < 0 || destinationY < 0) continue;
				if (clip && (destinationX < clip->iLeft || destinationX >= clip->iRight ||
					destinationY < clip->iTop || destinationY >= clip->iBottom)) continue;

				UINT16* destination = buffer
					? buffer + static_cast<std::size_t>(destinationY) * pitch + destinationX : nullptr;
				UINT16* depth = policy.zBuffer
					? policy.zBuffer + static_cast<std::size_t>(destinationY) * pitch + destinationX : nullptr;
				const bool visible = !depth || passesDepth(*depth, incomingZ, policy);
				if (policy.paint == Paint::Probe)
				{
					if (visible)
					{
						if (policy.probeHit) *policy.probeHit = true;
						return TRUE;
					}
					continue;
				}
				bool draw = visible || (policy.solidWhenObscured && depth);
				if (policy.pixelation == Pixelation::Always) draw = visible && checkerPixel(destinationX, destinationY);
				else if (policy.pixelation == Pixelation::WhenObscured && !visible)
					draw = checkerPixel(destinationX, destinationY);
				if (!draw) continue;

				// Obscured trans-shadow blitters never render the shadow marker through foreground geometry.
				if (!visible && sourcePixel == 254 &&
					policy.shadowTreatment != ShadowTreatment::AsPaletteColor &&
					!policy.drawShadowWhenObscured) continue;
				// Their equal-Z shadow marker is also suppressed, while ordinary pixels use <=.
				if (depth && sourcePixel == 254 && policy.pixelation == Pixelation::WhenObscured &&
					*depth == incomingZ && policy.shadowTreatment == ShadowTreatment::ShadeDestination &&
					policy.suppressEqualObscuredShadow) continue;

				if (depth && policy.updateZ && (visible || policy.updateZWhenObscured) &&
					!(policy.paint == Paint::Outline && sourcePixel == 254 &&
						policy.pixelation == Pixelation::None)) *depth = incomingZ;
				PaintedPixel painted = !visible && policy.solidWhenObscured
					? PaintedPixel{policy.solidColor, true, false}
					: paintPixel(sourcePixel, *destination, policy);
				if (!painted.draw) continue;
				if (policy.translucent)
					painted.color = static_cast<UINT16>(((painted.color >> 1) & guiTranslucentMask) +
						((*destination >> 1) & guiTranslucentMask));
				if (alpha && painted.applyAlpha)
					painted.color = alphaBlend565(painted.color, *destination, alpha[pixel]);
				*destination = painted.color;
			}
			source += count;
			if (alpha) alpha += count;
			sourceX += count;
		}
	}
	return TRUE;
}

BlitPolicy palettePolicy(UINT16* z, UINT16 value, bool update, SGPRect* clip = nullptr)
{
	BlitPolicy policy;
	policy.zBuffer = z;
	policy.zValue = value;
	policy.updateZ = update;
	policy.clip = clip;
	policy.depthTest = z ? DepthTest::LessEqual : DepthTest::None;
	return policy;
}

BlitPolicy shadowPolicy(UINT16* z, UINT16 value, bool update, SGPRect* clip,
	const UINT16* palette, BOOLEAN ignoreShadows)
{
	BlitPolicy policy = palettePolicy(z, value, update, clip);
	policy.depthTest = z ? DepthTest::Less : DepthTest::None;
	policy.palette = palette;
	policy.shadowTreatment = ignoreShadows ? ShadowTreatment::Skip : ShadowTreatment::ShadeDestination;
	return policy;
}
}

using namespace ja2::blitter;

unsigned short blendWithAlpha(unsigned int rgb565New, unsigned int rgb565Old, unsigned int alpha)
{
	return alphaBlend565(static_cast<UINT16>(rgb565New), static_cast<UINT16>(rgb565Old),
		static_cast<UINT8>(std::min(alpha, 255u)));
}

void SetClippingRect(SGPRect* clip)
{
	if (clip) ClippingRect = *clip;
}

void GetClippingRect(SGPRect* clip)
{
	if (clip) *clip = ClippingRect;
}

BOOLEAN BltIsClipped(HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	if (!object || index >= object->usNumberOfObjects) return TRUE;
	if (!clip) clip = &ClippingRect;
	const ETRLEObject& frame = object->pETRLEObject[index];
	const INT32 left = x + frame.sOffsetX;
	const INT32 top = y + frame.sOffsetY;
	return left < clip->iLeft || top < clip->iTop || left + frame.usWidth > clip->iRight || top + frame.usHeight > clip->iBottom;
}

CHAR8 BltIsClippedOrOffScreen(HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	if (!object || index >= object->usNumberOfObjects) return -1;
	if (!clip) clip = &ClippingRect;
	const ETRLEObject& frame = object->pETRLEObject[index];
	const INT32 left = x + frame.sOffsetX;
	const INT32 top = y + frame.sOffsetY;
	if (left >= clip->iRight || top >= clip->iBottom || left + frame.usWidth <= clip->iLeft || top + frame.usHeight <= clip->iTop)
		return -1;
	return BltIsClipped(object, x, y, index, clip) ? 1 : 0;
}

UINT16* InitZBuffer(UINT32 pitch, UINT32 height)
{
	ClippingRect.iLeft = 0;
	ClippingRect.iTop = 0;
	ClippingRect.iRight = SCREEN_WIDTH;
	ClippingRect.iBottom = SCREEN_HEIGHT;
	UINT16* buffer = static_cast<UINT16*>(MemAlloc(static_cast<std::size_t>(pitch) * height));
	if (buffer) std::memset(buffer, 0, static_cast<std::size_t>(pitch) * height);
	return buffer;
}

BOOLEAN ShutdownZBuffer(UINT16* buffer)
{
	MemFree(buffer);
	return TRUE;
}

BOOLEAN BlitZRect(UINT16* buffer, UINT32 pitchBytes, INT16 left, INT16 top, INT16 right, INT16 bottom, UINT16 value)
{
	if (!buffer) return FALSE;
	left = static_cast<INT16>(std::max<INT32>(left, ClippingRect.iLeft));
	top = static_cast<INT16>(std::max<INT32>(top, ClippingRect.iTop));
	right = static_cast<INT16>(std::min<INT32>(right, ClippingRect.iRight));
	bottom = static_cast<INT16>(std::min<INT32>(bottom, ClippingRect.iBottom));
	if (left >= right || top >= bottom) return FALSE;
	const UINT32 pitch = pitchBytes / 2;
	for (INT32 y = top; y < bottom; ++y)
		std::fill_n(buffer + static_cast<std::size_t>(y) * pitch + left, right - left, value);
	return TRUE;
}

#define SIMPLE_Z_BLITTER(name, update, cliparg) \
	BOOLEAN name(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i cliparg) \
	{ return blitEtrle(b, p, o, x, y, i, palettePolicy(z, zv, update, nullptr)); }

SIMPLE_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZ, true, )
SIMPLE_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZNB, false, )
#undef SIMPLE_Z_BLITTER

BOOLEAN Blt8BPPDataTo16BPPBufferTransZClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ return blitEtrle(b, p, o, x, y, i, palettePolicy(z, zv, true, effectiveClip(c))); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ return blitEtrle(b, p, o, x, y, i, palettePolicy(z, zv, false, effectiveClip(c))); }

BOOLEAN Blt8BPPDataTo16BPPBufferTransparent(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ return blitEtrle(b, p, o, x, y, i, {}); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransparentClip(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ BlitPolicy q; q.clip = effectiveClip(c); return blitEtrle(b, p, o, x, y, i, q); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransMirror(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ BlitPolicy q; q.mirror = true; return blitEtrle(b, p, o, x, y, i, q); }

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBColor(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, UINT16 color)
{ BlitPolicy q = palettePolicy(z, zv, false); q.solidWhenObscured = true; q.solidColor = color; return blitEtrle(b, p, o, x, y, i, q); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipColor(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16 color)
{ BlitPolicy q = palettePolicy(z, zv, false, effectiveClip(c)); q.solidWhenObscured = true; q.solidColor = color; return blitEtrle(b, p, o, x, y, i, q); }

#define PIXEL_Z_BLITTER(name, update, clipped, mode) \
	BOOLEAN name(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i clipped) \
	{ BlitPolicy q = palettePolicy(z, zv, update, nullptr); q.pixelation = mode; return blitEtrle(b, p, o, x, y, i, q); }
PIXEL_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZPixelate, true, , Pixelation::Always)
PIXEL_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZNBPixelate, false, , Pixelation::Always)
#undef PIXEL_Z_BLITTER

BOOLEAN Blt8BPPDataTo16BPPBufferTransZPixelateObscured(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ BlitPolicy q = palettePolicy(z, zv, true); q.depthTest = DepthTest::Less; q.pixelation = Pixelation::WhenObscured; return blitEtrle(b, p, o, x, y, i, q); }

#define CLIPPED_PIXEL_Z_BLITTER(name, update, mode) \
	BOOLEAN name(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c) \
	{ BlitPolicy q = palettePolicy(z, zv, update, effectiveClip(c)); q.pixelation = mode; return blitEtrle(b, p, o, x, y, i, q); }
CLIPPED_PIXEL_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZClipPixelate, true, Pixelation::Always)
CLIPPED_PIXEL_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZNBClipPixelate, false, Pixelation::Always)
#undef CLIPPED_PIXEL_Z_BLITTER

BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipPixelateObscured(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ BlitPolicy q = palettePolicy(z, zv, true, effectiveClip(c)); q.depthTest = DepthTest::Less; q.pixelation = Pixelation::WhenObscured; return blitEtrle(b, p, o, x, y, i, q); }

#define TRANSLUCENT_Z_BLITTER(name, update) \
	BOOLEAN name(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i) \
	{ BlitPolicy q = palettePolicy(z, zv, update); q.translucent = true; return blitEtrle(b, p, o, x, y, i, q); }
TRANSLUCENT_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZTranslucent, true)
TRANSLUCENT_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZNBTranslucent, false)
#undef TRANSLUCENT_Z_BLITTER

#define CLIPPED_TRANSLUCENT_Z_BLITTER(name, update) \
	BOOLEAN name(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c) \
	{ BlitPolicy q = palettePolicy(z, zv, update, effectiveClip(c)); q.translucent = true; return blitEtrle(b, p, o, x, y, i, q); }
CLIPPED_TRANSLUCENT_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZClipTranslucent, true)
CLIPPED_TRANSLUCENT_Z_BLITTER(Blt8BPPDataTo16BPPBufferTransZNBClipTranslucent, false)
#undef CLIPPED_TRANSLUCENT_Z_BLITTER

BOOLEAN Blt8BPPDataTo16BPPBufferMonoShadowClip(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16 fg, UINT16 bg, UINT16 sh)
{ BlitPolicy q; q.paint = Paint::Mono; q.clip = effectiveClip(c); q.foreground = fg; q.background = bg; q.shadow = sh; return blitEtrle(b, p, o, x, y, i, q); }

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadow(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, UINT16* pal, BOOLEAN ignore)
{ return blitEtrle(b, p, o, x, y, i, shadowPolicy(nullptr, 0, false, nullptr, pal, ignore)); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowAlpha(UINT16* b, UINT32 p, HVOBJECT o, HVOBJECT a, INT32 x, INT32 y, UINT16 i, UINT16* pal, BOOLEAN ignore)
{ BlitPolicy q = shadowPolicy(nullptr, 0, false, nullptr, pal, ignore); q.alphaObject = a; return blitEtrle(b, p, o, x, y, i, q); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowClip(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16* pal, BOOLEAN ignore)
{ return blitEtrle(b, p, o, x, y, i, shadowPolicy(nullptr, 0, false, effectiveClip(c), pal, ignore)); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowClipAlpha(UINT16* b, UINT32 p, HVOBJECT o, HVOBJECT a, INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16* pal, BOOLEAN ignore)
{ BlitPolicy q = shadowPolicy(nullptr, 0, false, effectiveClip(c), pal, ignore); q.alphaObject = a; return blitEtrle(b, p, o, x, y, i, q); }

#define TRANSSHADOW_Z(name, update) \
	BOOLEAN name(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, UINT16* pal, BOOLEAN ignore) \
	{ return blitEtrle(b, p, o, x, y, i, shadowPolicy(z, zv, update, nullptr, pal, ignore)); }
TRANSSHADOW_Z(Blt8BPPDataTo16BPPBufferTransShadowZ, true)
TRANSSHADOW_Z(Blt8BPPDataTo16BPPBufferTransShadowZNB, false)
#undef TRANSSHADOW_Z
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZAlpha(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, HVOBJECT a, INT32 x, INT32 y, UINT16 i, UINT16* pal, BOOLEAN ignore)
{ BlitPolicy q = shadowPolicy(z, zv, true, nullptr, pal, ignore); q.alphaObject = a; return blitEtrle(b, p, o, x, y, i, q); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBAlpha(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, HVOBJECT a, INT32 x, INT32 y, UINT16 i, UINT16* pal, BOOLEAN ignore)
{ BlitPolicy q = shadowPolicy(z, zv, false, nullptr, pal, ignore); q.alphaObject = a; return blitEtrle(b, p, o, x, y, i, q); }

#define TRANSSHADOW_Z_CLIP(name, update) \
	BOOLEAN name(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16* pal, BOOLEAN ignore) \
	{ return blitEtrle(b, p, o, x, y, i, shadowPolicy(z, zv, update, effectiveClip(c), pal, ignore)); }
TRANSSHADOW_Z_CLIP(Blt8BPPDataTo16BPPBufferTransShadowZClip, true)
TRANSSHADOW_Z_CLIP(Blt8BPPDataTo16BPPBufferTransShadowZNBClip, false)
#undef TRANSSHADOW_Z_CLIP
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZClipAlpha(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, HVOBJECT a, INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16* pal, BOOLEAN ignore)
{ BlitPolicy q = shadowPolicy(z, zv, true, effectiveClip(c), pal, ignore); q.alphaObject = a; return blitEtrle(b, p, o, x, y, i, q); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBClipAlpha(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, HVOBJECT a, INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16* pal, BOOLEAN ignore)
{ BlitPolicy q = shadowPolicy(z, zv, false, effectiveClip(c), pal, ignore); q.alphaObject = a; return blitEtrle(b, p, o, x, y, i, q); }

BOOLEAN shadowZ(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c, bool update)
{ BlitPolicy q; q.paint = Paint::Shadow; q.zBuffer = z; q.zValue = zv; q.depthTest = DepthTest::Less; q.updateZ = update; q.clip = c; return blitEtrle(b, p, o, x, y, i, q); }
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZ(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ return shadowZ(b, p, z, zv, o, x, y, i, nullptr, true); }
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZNB(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ return shadowZ(b, p, z, zv, o, x, y, i, nullptr, false); }
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ return shadowZ(b, p, z, zv, o, x, y, i, effectiveClip(c), true); }
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZNBClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ return shadowZ(b, p, z, zv, o, x, y, i, effectiveClip(c), false); }

BOOLEAN Blt8BPPDataTo16BPPBufferShadow(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ BlitPolicy q; q.paint = Paint::Shadow; return blitEtrle(b, p, o, x, y, i, q); }
BOOLEAN Blt8BPPDataTo16BPPBufferShadowClip(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ BlitPolicy q; q.paint = Paint::Shadow; q.clip = effectiveClip(c); return blitEtrle(b, p, o, x, y, i, q); }

BOOLEAN intensity(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c, bool update)
{ BlitPolicy q; q.paint=Paint::Intensity; q.zBuffer=z; q.zValue=zv; q.depthTest=z ? DepthTest::Less : DepthTest::None; q.updateZ=update; q.clip=c; return blitEtrle(b,p,o,x,y,i,q); }
BOOLEAN Blt8BPPDataTo16BPPBufferIntensity(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ return intensity(b,p,nullptr,0,o,x,y,i,nullptr,false); }
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityClip(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ return intensity(b,p,nullptr,0,o,x,y,i,effectiveClip(c),false); }
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZ(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ return intensity(b,p,z,zv,o,x,y,i,nullptr,true); }
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZNB(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ return intensity(b,p,z,zv,o,x,y,i,nullptr,false); }
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ return intensity(b,p,z,zv,o,x,y,i,effectiveClip(c),true); }
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZNBClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ return intensity(b,p,z,zv,o,x,y,i,effectiveClip(c),false); }

BOOLEAN obscuredShadow(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, HVOBJECT a,
	INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16* pal, BOOLEAN ignore)
{
	BlitPolicy q = shadowPolicy(z, zv, false, c, pal, ignore);
	q.depthTest = DepthTest::LessEqual;
	q.pixelation = Pixelation::WhenObscured;
	q.alphaObject = a;
	return blitEtrle(b, p, o, x, y, i, q);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscured(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, UINT16* pal, BOOLEAN ignore)
{ return obscuredShadow(b,p,z,zv,o,nullptr,x,y,i,nullptr,pal,ignore); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredTest(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, UINT16* pal, BOOLEAN ignore)
{ return obscuredShadow(b,p,z,zv,o,nullptr,x,y,i,nullptr,pal,ignore); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredAlpha(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, HVOBJECT a, INT32 x, INT32 y, UINT16 i, UINT16* pal, BOOLEAN ignore)
{ return obscuredShadow(b,p,z,zv,o,a,x,y,i,nullptr,pal,ignore); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16* pal, BOOLEAN ignore)
{ return obscuredShadow(b,p,z,zv,o,nullptr,x,y,i,effectiveClip(c),pal,ignore); }
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredClipAlpha(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, HVOBJECT a, INT32 x, INT32 y, UINT16 i, SGPRect* c, UINT16* pal, BOOLEAN ignore)
{ return obscuredShadow(b,p,z,zv,o,a,x,y,i,effectiveClip(c),pal,ignore); }

BOOLEAN outline(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y,
	UINT16 i, INT16 color, BOOLEAN enabled, SGPRect* c, bool update, Pixelation pixelation)
{
	BlitPolicy q = palettePolicy(z, zv, update, c);
	q.paint = Paint::Outline;
	if (pixelation == Pixelation::WhenObscured) q.depthTest = DepthTest::Less;
	q.outlineColor = color;
	q.outlineEnabled = enabled;
	q.pixelation = pixelation;
	return blitEtrle(b,p,o,x,y,i,q);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutline(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, INT16 color, BOOLEAN enabled)
{ return outline(b,p,nullptr,0,o,x,y,i,color,enabled,nullptr,false,Pixelation::None); }
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineClip(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, INT16 color, BOOLEAN enabled, SGPRect* c)
{ return outline(b,p,nullptr,0,o,x,y,i,color,enabled,effectiveClip(c),false,Pixelation::None); }
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZ(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, INT16 color, BOOLEAN enabled)
{ return outline(b,p,z,zv,o,x,y,i,color,enabled,nullptr,true,Pixelation::None); }
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, INT16 color, BOOLEAN enabled, SGPRect* c)
{ return outline(b,p,z,zv,o,x,y,i,color,enabled,effectiveClip(c),true,Pixelation::None); }
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZNB(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, INT16 color, BOOLEAN enabled)
{ return outline(b,p,z,zv,o,x,y,i,color,enabled,nullptr,false,Pixelation::None); }
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZPixelateObscured(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, INT16 color, BOOLEAN enabled)
{ return outline(b,p,z,zv,o,x,y,i,color,enabled,nullptr,true,Pixelation::WhenObscured); }
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZPixelateObscuredClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, INT16 color, BOOLEAN enabled, SGPRect* c)
{ return outline(b,p,z,zv,o,x,y,i,color,enabled,effectiveClip(c),true,Pixelation::WhenObscured); }
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineShadow(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i)
{ BlitPolicy q; q.paint=Paint::OutlineShadow; return blitEtrle(b,p,o,x,y,i,q); }
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineShadowClip(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ BlitPolicy q; q.paint=Paint::OutlineShadow; q.clip=effectiveClip(c); return blitEtrle(b,p,o,x,y,i,q); }

BOOLEAN Blt8BPPTo8BPP(UINT8* destination, UINT32 destinationPitch, UINT8* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height)
{
	if (!destination || !source) return FALSE;
	for (UINT32 row=0; row<height; ++row)
		std::memmove(destination + static_cast<std::size_t>(destinationY+row)*destinationPitch + destinationX,
			source + static_cast<std::size_t>(sourceY+row)*sourcePitch + sourceX, width);
	return TRUE;
}

BOOLEAN Blt16BPPTo16BPP(UINT16* destination, UINT32 destinationPitch, UINT16* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height)
{
	if (!destination || !source) return FALSE;
	for (UINT32 row=0; row<height; ++row)
		std::memmove(reinterpret_cast<UINT8*>(destination) + static_cast<std::size_t>(destinationY+row)*destinationPitch + destinationX*2,
			reinterpret_cast<UINT8*>(source) + static_cast<std::size_t>(sourceY+row)*sourcePitch + sourceX*2, width*2);
	return TRUE;
}

BOOLEAN Blt16BPPTo16BPPTrans(UINT16* destination, UINT32 destinationPitch, UINT16* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height, UINT16 transparent)
{
	if (!destination || !source) return FALSE;
	for (UINT32 row=0; row<height; ++row)
	{
		UINT16* d = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination) + static_cast<std::size_t>(destinationY+row)*destinationPitch) + destinationX;
		UINT16* s = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(source) + static_cast<std::size_t>(sourceY+row)*sourcePitch) + sourceX;
		for (UINT32 column=0; column<width; ++column) if (s[column] != transparent) d[column] = s[column];
	}
	return TRUE;
}

BOOLEAN Blt16BPPTo16BPPTransShadow(UINT16* destination, UINT32 destinationPitch, UINT16* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height, UINT16 transparent)
{
	if (!destination || !source) return FALSE;
	for (UINT32 row=0; row<height; ++row)
	{
		UINT16* d = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination) + static_cast<std::size_t>(destinationY+row)*destinationPitch) + destinationX;
		UINT16* s = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(source) + static_cast<std::size_t>(sourceY+row)*sourcePitch) + sourceX;
		for (UINT32 column=0; column<width; ++column) if (s[column] != transparent) d[column] = ShadeTable[d[column]];
	}
	return TRUE;
}

BOOLEAN Blt16BPPTo16BPPMirror(UINT16* destination, UINT32 destinationPitch, UINT16* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height)
{
	if (!destination || !source) return FALSE;
	for (UINT32 row=0; row<height; ++row)
	{
		UINT16* d = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination) + static_cast<std::size_t>(destinationY+row)*destinationPitch) + destinationX;
		UINT16* s = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(source) + static_cast<std::size_t>(sourceY+row)*sourcePitch) + sourceX;
		for (UINT32 column=0; column<width; ++column) d[column] = s[width-1-column];
	}
	return TRUE;
}

BOOLEAN blitFlat8(UINT16* destination, UINT32 destinationPitch, HVSURFACE surface, const UINT8* source,
	UINT32 sourcePitch, INT32 x, INT32 y, INT32 left, INT32 top, UINT32 width, UINT32 height, bool half)
{
	if (!destination || !surface || !source || !surface->p16BPPPalette || x < 0 || y < 0) return FALSE;
	const UINT32 step = half ? 2 : 1;
	const UINT32 outputWidth = width / step;
	const UINT32 outputHeight = height / step;
	for (UINT32 row=0; row<outputHeight; ++row)
	{
		UINT16* d = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination) + static_cast<std::size_t>(y+row)*destinationPitch) + x;
		const UINT8* s = source + static_cast<std::size_t>(top+row*step)*sourcePitch + left;
		for (UINT32 column=0; column<outputWidth; ++column) d[column] = surface->p16BPPPalette[s[column*step]];
	}
	return TRUE;
}

BOOLEAN Blt8BPPDataTo16BPPBuffer(UINT16* b, UINT32 p, HVSURFACE s, UINT8* source, INT32 x, INT32 y)
{ return blitFlat8(b,p,s,source,s ? s->usWidth : 0,x,y,0,0,s ? s->usWidth : 0,s ? s->usHeight : 0,false); }
BOOLEAN Blt8BPPDataSubTo16BPPBuffer(UINT16* b, UINT32 p, HVSURFACE s, UINT8* source, UINT32 sourcePitch, INT32 x, INT32 y, SGPRect* r)
{ return r && blitFlat8(b,p,s,source,sourcePitch,x,y,r->iLeft,r->iTop,r->iRight-r->iLeft,r->iBottom-r->iTop,false); }
BOOLEAN Blt8BPPDataTo16BPPBufferHalf(UINT16* b, UINT32 p, HVSURFACE s, UINT8* source, UINT32 sourcePitch, INT32 x, INT32 y)
{ return blitFlat8(b,p,s,source,sourcePitch,x,y,0,0,s ? s->usWidth : 0,s ? s->usHeight : 0,true); }
BOOLEAN Blt8BPPDataTo16BPPBufferHalfRect(UINT16* b, UINT32 p, HVSURFACE s, UINT8* source, UINT32 sourcePitch, INT32 x, INT32 y, SGPRect* r)
{ return r && blitFlat8(b,p,s,source,sourcePitch,x,y,r->iLeft,r->iTop,r->iRight-r->iLeft,r->iBottom-r->iTop,true); }

BOOLEAN blit16Object(UINT16* destination, UINT32 pitchBytes, UINT16* z, UINT16 zValue, HVOBJECT object,
	INT32 x, INT32 y, UINT16 index, SGPRect* clip, bool depthTest)
{
	if (!destination || !object || !object->p16BPPObject || index >= object->usNumberOf16BPPObjects) return FALSE;
	const SixteenBPPObjectInfo& frame = object->p16BPPObject[index];
	const UINT32 pitch = pitchBytes/2;
	for (UINT16 sy=0; sy<frame.usHeight; ++sy)
	for (UINT16 sx=0; sx<frame.usWidth; ++sx)
	{
		const INT32 dx=x+frame.sOffsetX+sx, dy=y+frame.sOffsetY+sy;
		if (dx<0 || dy<0 || (clip && (dx<clip->iLeft || dx>=clip->iRight || dy<clip->iTop || dy>=clip->iBottom))) continue;
		const UINT16 pixel=frame.p16BPPData[static_cast<std::size_t>(sy)*frame.usWidth+sx];
		if (!pixel) continue;
		UINT16* d=destination+static_cast<std::size_t>(dy)*pitch+dx;
		if (!depthTest || z[static_cast<std::size_t>(dy)*pitch+dx] <= zValue)
		{
			*d=pixel;
			if (depthTest) z[static_cast<std::size_t>(dy)*pitch+dx]=zValue;
		}
	}
	return TRUE;
}

BOOLEAN Blt16BPPDataTo16BPPBufferTransparentClip(UINT16* b, UINT32 p, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ return blit16Object(b,p,nullptr,0,o,x,y,i,effectiveClip(c),false); }
BOOLEAN Blt16BPPDataTo16BPPBufferTransZClip(UINT16* b, UINT32 p, UINT16* z, UINT16 zv, HVOBJECT o, INT32 x, INT32 y, UINT16 i, SGPRect* c)
{ return blit16Object(b,p,z,zv,o,x,y,i,effectiveClip(c),true); }

BOOLEAN Blt16BPPBufferPixelateRectWithColor(UINT16* b, UINT32 p, SGPRect* area, UINT8 pattern[8][8], UINT16 color)
{
	if (!b || !area || !pattern) return FALSE;
	const INT32 left=std::max(area->iLeft,ClippingRect.iLeft), top=std::max(area->iTop,ClippingRect.iTop);
	const INT32 right=std::min(area->iRight,ClippingRect.iRight-1), bottom=std::min(area->iBottom,ClippingRect.iBottom-1);
	if (left>right || top>bottom) return FALSE;
	const UINT32 pitch=p/2;
	for (INT32 y=top;y<=bottom;++y) for(INT32 x=left;x<=right;++x)
		if(pattern[(y-top)&7][(x-left)&7]) b[static_cast<std::size_t>(y)*pitch+x]=color;
	return TRUE;
}
BOOLEAN Blt16BPPBufferPixelateRect(UINT16* b, UINT32 p, SGPRect* a, UINT8 pattern[8][8])
{ return Blt16BPPBufferPixelateRectWithColor(b,p,a,pattern,0); }

namespace
{
UINT8 Hatch[8][8]={{1,0,1,0,1,0,1,0},{0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0},{0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0},{0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0},{0,1,0,1,0,1,0,1}};
UINT8 LooseHatch[8][8]={{1,0,0,0,1,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,0,0,0,1,0},{0,0,0,0,0,0,0,0},{1,0,0,0,1,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,0,0,0,1,0},{0,0,0,0,0,0,0,0}};
}
BOOLEAN Blt16BPPBufferHatchRectWithColor(UINT16* b, UINT32 p, SGPRect* a, UINT16 c) { return Blt16BPPBufferPixelateRectWithColor(b,p,a,Hatch,c); }
BOOLEAN Blt16BPPBufferHatchRect(UINT16* b, UINT32 p, SGPRect* a) { return Blt16BPPBufferHatchRectWithColor(b,p,a,0); }
BOOLEAN Blt16BPPBufferLooseHatchRectWithColor(UINT16* b, UINT32 p, SGPRect* a, UINT16 c) { return Blt16BPPBufferPixelateRectWithColor(b,p,a,LooseHatch,c); }
BOOLEAN Blt16BPPBufferLooseHatchRect(UINT16* b, UINT32 p, SGPRect* a) { return Blt16BPPBufferLooseHatchRectWithColor(b,p,a,0); }

BOOLEAN shadeRect(UINT16* b, UINT32 p, SGPRect* a, UINT16* table)
{
	if(!b || !a) return FALSE;
	const INT32 left=std::max(a->iLeft,ClippingRect.iLeft), top=std::max(a->iTop,ClippingRect.iTop);
	const INT32 right=std::min(a->iRight,ClippingRect.iRight-1), bottom=std::min(a->iBottom,ClippingRect.iBottom-1);
	if(left>right || top>bottom) return FALSE;
	const UINT32 pitch=p/2;
	for(INT32 y=top;y<=bottom;++y) for(INT32 x=left;x<=right;++x) { UINT16& pixel=b[static_cast<std::size_t>(y)*pitch+x]; pixel=table[pixel]; }
	return TRUE;
}
BOOLEAN Blt16BPPBufferShadowRect(UINT16* b, UINT32 p, SGPRect* a) { return shadeRect(b,p,a,ShadeTable); }
BOOLEAN Blt16BPPBufferShadowRectAlternateTable(UINT16* b, UINT32 p, SGPRect* a) { return shadeRect(b,p,a,IntensityTable); }

BOOLEAN FillRect16BPP(UINT16* b, UINT32 p, INT32 x1, INT32 y1, INT32 x2, INT32 y2, UINT16 color)
{
	if(!b || x1>=x2 || y1>=y2) return FALSE;
	const UINT32 pitch=p/2;
	for(INT32 y=y1;y<y2;++y) std::fill(b+static_cast<std::size_t>(y)*pitch+x1,b+static_cast<std::size_t>(y)*pitch+x2,color);
	return TRUE;
}

BOOLEAN blit32(UINT16* destination, UINT32 destinationPitch, UINT32* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height, bool shadow)
{
	if(!destination || !source) return FALSE;
	for(UINT32 y=0;y<height;++y)
	{
		UINT16* d=reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination)+static_cast<std::size_t>(destinationY+y)*destinationPitch)+destinationX;
		UINT32* s=reinterpret_cast<UINT32*>(reinterpret_cast<UINT8*>(source)+static_cast<std::size_t>(sourceY+y)*sourcePitch)+sourceX;
		for(UINT32 x=0;x<width;++x)
		{
			const UINT8 alpha=static_cast<UINT8>(s[x]>>24); if(!alpha) continue;
			UINT16 value;
			if(shadow) value=ShadeTable[d[x]];
			else { const UINT8 r=s[x]&0xff,g=(s[x]>>8)&0xff,b=(s[x]>>16)&0xff; value=static_cast<UINT16>(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
			d[x]=alphaBlend565(value,d[x],alpha);
		}
	}
	return TRUE;
}
BOOLEAN Blt32BPPTo16BPPTrans(UINT16* d, UINT32 dp, UINT32* s, UINT32 sp, INT32 dx, INT32 dy, INT32 sx, INT32 sy, UINT32 w, UINT32 h)
{ return blit32(d,dp,s,sp,dx,dy,sx,sy,w,h,false); }
BOOLEAN Blt32BPPTo16BPPTransShadow(UINT16* d, UINT32 dp, UINT32* s, UINT32 sp, INT32 dx, INT32 dy, INT32 sx, INT32 sy, UINT32 w, UINT32 h)
{ return blit32(d,dp,s,sp,dx,dy,sx,sy,w,h,true); }
