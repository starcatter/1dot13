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
// Palette entry 254 is interpreted as a shadow or outline by the relevant policies.
constexpr UINT8 ShadowOrOutlineIndex = 254;

bool clipKnownBuffer(BYTE* data, INT32& x, INT32& y, UINT32& width,
	UINT32& height, INT32& pairedX, INT32& pairedY)
{
	const SurfaceData::tID id = SurfaceData::GetSurfaceID(data);
	if (id == 0) return true;
	const auto bounds = g_SurfaceRectangle.find(static_cast<UINT32>(id));
	if (bounds == g_SurfaceRectangle.end()) return true;

	const INT32 oldX = x;
	const INT32 oldY = y;
	const ClipRectangle::ClipType result =
		bounds->second.Clip(x, y, width, height);
	if (result == ClipRectangle::FullClip) return false;
	if (result == ClipRectangle::PartialClip)
	{
		pairedX += x - oldX;
		pairedY += y - oldY;
	}
	return width != 0 && height != 0;
}

bool clipCopyRect(UINT16* destination, UINT16* source,
	INT32& destinationX, INT32& destinationY, INT32& sourceX,
	INT32& sourceY, UINT32& width, UINT32& height)
{
	if (!clipKnownBuffer(reinterpret_cast<BYTE*>(destination), destinationX,
		destinationY, width, height, sourceX, sourceY))
	{
		return false;
	}
	return clipKnownBuffer(reinterpret_cast<BYTE*>(source), sourceX, sourceY,
		width, height, destinationX, destinationY);
}

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
			if (change < 0)
			{
				level = static_cast<UINT16>(level - delta);
			}
			else if (change > 0)
			{
				level = static_cast<UINT16>(level + delta);
			}
			remaining = 20; // Only the first strip has a variable width.
		}
		else
		{
			remaining = 0xffff;
		}
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
	const UINT32 red = (((foreground >> 11) & 0x1f) * alpha +
		((background >> 11) & 0x1f) * inverse + 127) / 255;
	const UINT32 green = (((foreground >> 5) & 0x3f) * alpha +
		((background >> 5) & 0x3f) * inverse + 127) / 255;
	const UINT32 blue = ((foreground & 0x1f) * alpha + (background & 0x1f) * inverse + 127) / 255;
	return static_cast<UINT16>((red << 11) | (green << 5) | blue);
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
			else
			{
				result.color = policy.foreground;
			}
			result.applyAlpha = false;
			break;
		case Paint::Outline:
			if (source == ShadowOrOutlineIndex)
			{
				result.draw = policy.outlineEnabled;
				result.color = static_cast<UINT16>(policy.outlineColor);
			}
			else
			{
				result.color = policy.palette[source];
			}
			break;
		case Paint::OutlineShadow:
			result.draw = source != ShadowOrOutlineIndex;
			if (result.draw) result.color = ShadeTable[destination];
			result.applyAlpha = false;
			break;
		case Paint::Solid:
			result.color = policy.solidColor;
			result.applyAlpha = false;
			break;
		case Paint::Probe:
			result.draw = false;
			break;
		case Paint::Palette:
			if (source == ShadowOrOutlineIndex &&
				policy.shadowTreatment != ShadowTreatment::AsPaletteColor)
			{
				result.draw = policy.shadowTreatment == ShadowTreatment::ShadeDestination;
				result.color = ShadeTable[destination];
				result.applyAlpha = false;
			}
			else
			{
				result.color = policy.palette[source];
			}
			break;
	}
	return result;
}

BOOLEAN blitEtrle(UINT16* buffer, UINT32 pitchBytes, HVOBJECT object, INT32 x, INT32 y,
	UINT16 index, BlitPolicy policy)
{
	if ((!buffer && policy.paint != Paint::Probe) || !object || !object->pETRLEObject ||
		!object->pPixData || index >= object->usNumberOfObjects)
	{
		return FALSE;
	}

	const ETRLEObject& frame = object->pETRLEObject[index];
	const UINT8* source = static_cast<const UINT8*>(object->pPixData) + frame.uiDataOffset;
	const UINT8* const sourceEnd = source + frame.uiDataLength;
	const UINT8* alpha = nullptr;
	const UINT8* alphaEnd = nullptr;
	if (policy.alphaObject)
	{
		if (!policy.alphaObject->pETRLEObject || !policy.alphaObject->pPixData ||
			index >= policy.alphaObject->usNumberOfObjects)
		{
			return FALSE;
		}
		const ETRLEObject& alphaFrame = policy.alphaObject->pETRLEObject[index];
		if (alphaFrame.usWidth != frame.usWidth || alphaFrame.usHeight != frame.usHeight)
		{
			return FALSE;
		}
		alpha = static_cast<const UINT8*>(policy.alphaObject->pPixData) + alphaFrame.uiDataOffset;
		alphaEnd = alpha + alphaFrame.uiDataLength;
	}

	const INT32 originX = x + frame.sOffsetX;
	const INT32 originY = y + frame.sOffsetY;
	if (!policy.clip && (originX < 0 || originY < 0)) return FALSE;
	if (!policy.palette) policy.palette = object->pShadeCurrent;
	if ((policy.paint == Paint::Palette || policy.paint == Paint::Outline) && !policy.palette)
	{
		return FALSE;
	}

	const SGPRect* clip = policy.clip;
	const UINT32 pitchPixels = pitchBytes / sizeof(UINT16);
	for (UINT16 sourceY = 0; sourceY < frame.usHeight; ++sourceY)
	{
		ZStripCursor zStrip(policy);
		const INT32 destinationY = originY + sourceY;
		UINT32 sourceX = 0;
		while (true)
		{
			if (source >= sourceEnd || (alpha && alpha >= alphaEnd)) return FALSE;
			const UINT8 code = *source++;
			if (alpha) ++alpha; // Alpha ETRLE uses the same control-byte layout as the source.
			if (code == 0) break;

			// High-bit runs are transparent; only monochrome blits paint their background.
			if (code & 0x80)
			{
				const UINT32 count = code & 0x7f;
				if (sourceX + count > frame.usWidth) return FALSE;
				if (policy.paint == Paint::Mono && policy.background != 0 && destinationY >= 0)
				{
					for (UINT32 pixel = 0; pixel < count; ++pixel)
					{
						const UINT32 logicalX = sourceX + pixel;
						const INT32 destinationX = originX +
							(policy.mirror ? frame.usWidth - 1 - logicalX : logicalX);
						if (destinationX < 0 || (clip &&
							(destinationX < clip->iLeft || destinationX >= clip->iRight ||
							 destinationY < clip->iTop || destinationY >= clip->iBottom)))
						{
							continue;
						}
						buffer[static_cast<std::size_t>(destinationY) * pitchPixels + destinationX] =
							policy.background;
					}
				}
				for (UINT32 pixel = 0; pixel < count; ++pixel)
				{
					zStrip.advance();
				}
				sourceX += count;
				continue;
			}

			// Other runs contain literal palette indices and, if present, matching alpha values.
			const UINT32 count = code;
			if (sourceX + count > frame.usWidth || static_cast<std::size_t>(sourceEnd - source) < count ||
				(alpha && static_cast<std::size_t>(alphaEnd - alpha) < count))
			{
				return FALSE;
			}
			for (UINT32 pixel = 0; pixel < count; ++pixel)
			{
				const UINT8 sourcePixel = source[pixel];
				const UINT32 logicalX = sourceX + pixel;
				const UINT16 incomingZ = policy.zStrip ? zStrip.level : policy.zValue;
				// Strip position follows the source, even when a pixel is clipped or hidden.
				zStrip.advance();
				const INT32 destinationX = originX +
					(policy.mirror ? frame.usWidth - 1 - logicalX : logicalX);
				if (destinationX < 0 || destinationY < 0) continue;
				if (clip && (destinationX < clip->iLeft || destinationX >= clip->iRight ||
					destinationY < clip->iTop || destinationY >= clip->iBottom))
				{
					continue;
				}

				const std::size_t destinationOffset =
					static_cast<std::size_t>(destinationY) * pitchPixels + destinationX;
				UINT16* destination = buffer ? buffer + destinationOffset : nullptr;
				UINT16* depth = policy.zBuffer ? policy.zBuffer + destinationOffset : nullptr;
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
				if (policy.pixelation == Pixelation::Always)
				{
					draw = visible && checkerPixel(destinationX, destinationY);
				}
				else if (policy.pixelation == Pixelation::WhenObscured && !visible)
				{
					draw = checkerPixel(destinationX, destinationY);
				}
				if (!draw) continue;

				const bool isShadowOrOutline = sourcePixel == ShadowOrOutlineIndex;
				// Shadow markers must not show through foreground geometry unless explicitly allowed.
				if (!visible && isShadowOrOutline &&
					policy.shadowTreatment != ShadowTreatment::AsPaletteColor &&
					!policy.drawShadowWhenObscured)
				{
					continue;
				}
				// Equal-Z shadow markers are suppressed even for policies whose ordinary pixels use <=.
				if (depth && isShadowOrOutline &&
					*depth == incomingZ && policy.shadowTreatment == ShadowTreatment::ShadeDestination &&
					policy.suppressEqualShadow)
				{
					continue;
				}

				// Non-pixelated outline markers leave depth untouched. Other policies may write depth even
				// when paintPixel subsequently suppresses the color (e.g. ignored shadows).
				const bool outlineKeepsDepth = policy.paint == Paint::Outline &&
					isShadowOrOutline && policy.pixelation == Pixelation::None;
				if (depth && policy.updateZ && (visible || policy.updateZWhenObscured) && !outlineKeepsDepth)
				{
					*depth = incomingZ;
				}

				PaintedPixel painted = !visible && policy.solidWhenObscured
					? PaintedPixel{policy.solidColor, true, false}
					: paintPixel(sourcePixel, *destination, policy);
				if (!painted.draw) continue;
				if (policy.translucent)
				{
					painted.color = static_cast<UINT16>(((painted.color >> 1) & guiTranslucentMask) +
						((*destination >> 1) & guiTranslucentMask));
				}
				if (alpha && painted.applyAlpha)
				{
					painted.color = alphaBlend565(painted.color, *destination, alpha[pixel]);
				}
				*destination = painted.color;
			}
			source += count;
			if (alpha) alpha += count;
			sourceX += count;
		}
	}
	return TRUE;
}

BlitPolicy palettePolicy(UINT16* zBuffer, UINT16 zValue, bool updateZ, SGPRect* clip = nullptr)
{
	BlitPolicy policy;
	policy.zBuffer = zBuffer;
	policy.zValue = zValue;
	policy.updateZ = updateZ;
	policy.clip = clip;
	policy.depthTest = zBuffer ? DepthTest::LessEqual : DepthTest::None;
	return policy;
}

BlitPolicy shadowPolicy(UINT16* zBuffer, UINT16 zValue, bool updateZ, SGPRect* clip,
	const UINT16* palette, BOOLEAN ignoreShadows)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, updateZ, clip);
	// Z-writing variants require strict depth ordering. ZNB variants draw ordinary pixels at
	// equal depth, but still suppress equal-depth shadow markers (handled in blitEtrle).
	policy.depthTest = zBuffer ? (updateZ ? DepthTest::Less : DepthTest::LessEqual) : DepthTest::None;
	policy.palette = palette;
	policy.shadowTreatment = ignoreShadows ? ShadowTreatment::Skip : ShadowTreatment::ShadeDestination;
	return policy;
}

// These three painters cover the ordinary palette APIs. All other effects keep
// using blitEtrle, which also remains the independent reference for these paths.
template<bool testZ, bool writeZ>
struct PaletteSpanPainter
{
	const UINT16* palette;
	UINT16* zBuffer;
	UINT16 zValue;

	void draw(UINT16* destination, const UINT8* source, UINT32 count, std::size_t offset) const
	{
		if constexpr (testZ)
		{
			UINT16* depth = zBuffer + offset;
			for (UINT32 pixel = 0; pixel < count; ++pixel)
			{
				const UINT8 index = source[pixel];
				if (depth[pixel] > zValue) continue;
				if constexpr (writeZ) depth[pixel] = zValue;
				destination[pixel] = palette[index];
			}
		}
		else
		{
			const UINT16* const colors = palette;
			// Keep each lookup/store ordered: source and palette may alias the destination.
			for (; count >= 4; count -= 4)
			{
				destination[0] = colors[source[0]];
				destination[1] = colors[source[1]];
				destination[2] = colors[source[2]];
				destination[3] = colors[source[3]];
				destination += 4;
				source += 4;
			}
			for (; count != 0; --count)
				*destination++ = colors[*source++];
		}
	}
};

template<bool clipped, class Painter>
BOOLEAN walkEtrleSpans(UINT16* buffer, UINT32 pitchPixels, const ETRLEObject& frame,
	const UINT8* source, INT32 originX, INT32 originY, const SGPRect* clip, const Painter& painter)
{
	const UINT8* const sourceEnd = source + frame.uiDataLength;
	UINT32 firstVisibleX = 0;
	UINT32 lastVisibleX = frame.usWidth;
	if constexpr (clipped)
	{
		// Compute the visible interval in source coordinates once. Wide subtraction
		// handles negative origins and clip rectangles without overflowing INT32.
		firstVisibleX = static_cast<UINT32>(std::clamp<std::int64_t>(
			static_cast<std::int64_t>(std::max(0, clip->iLeft)) - originX, 0, frame.usWidth));
		lastVisibleX = static_cast<UINT32>(std::clamp<std::int64_t>(
			static_cast<std::int64_t>(clip->iRight) - originX, 0, frame.usWidth));
		if (firstVisibleX == 0 && lastVisibleX == frame.usWidth &&
			originY >= std::max(0, clip->iTop) &&
			static_cast<std::int64_t>(originY) + frame.usHeight <= clip->iBottom)
		{
			return walkEtrleSpans<false>(buffer, pitchPixels, frame, source, originX, originY,
				nullptr, painter);
		}
	}

	for (UINT16 sourceY = 0; sourceY < frame.usHeight; ++sourceY)
	{
		const INT32 destinationY = originY + sourceY;
		const std::size_t rowOffset = static_cast<std::size_t>(destinationY) * pitchPixels;
		bool visibleRow = true;
		if constexpr (clipped)
		{
			visibleRow = destinationY >= 0 && destinationY >= clip->iTop &&
				destinationY < clip->iBottom && firstVisibleX < lastVisibleX;
		}
		UINT32 sourceX = 0;
		while (true)
		{
			// Even fully clipped rows are decoded and checked: malformed input must
			// fail after the same partial writes as the generic reference.
			if (source >= sourceEnd) return FALSE;
			const UINT8 code = *source++;
			if (code == 0) break;
			const UINT32 count = code & 0x7f;
			if (sourceX + count > frame.usWidth) return FALSE;
			if (code & 0x80)
			{
				sourceX += count;
				continue;
			}
			if (static_cast<std::size_t>(sourceEnd - source) < count) return FALSE;

			if constexpr (clipped)
			{
				const UINT32 begin = std::max(sourceX, firstVisibleX);
				const UINT32 end = std::min(sourceX + count, lastVisibleX);
				if (visibleRow && begin < end)
				{
					const INT32 destinationX = originX + begin;
					const std::size_t offset = rowOffset + destinationX;
					painter.draw(buffer + offset, source + (begin - sourceX), end - begin, offset);
				}
			}
			else
			{
				const std::size_t offset = rowOffset + originX + sourceX;
				painter.draw(buffer + offset, source, count, offset);
			}
			source += count;
			sourceX += count;
		}
	}
	return TRUE;
}

template<bool clipped, bool writeZ, bool testZ = true>
BOOLEAN blitPalette(UINT16* buffer, UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip = nullptr)
{
#ifdef JA2_BLITTER_GENERIC_REFERENCE
	// The benchmark builds this same API with only the generic decoder enabled.
	return blitEtrle(buffer, pitchBytes, object, x, y, index,
		palettePolicy(zBuffer, zValue, writeZ, clip));
#else
	if (!buffer || !object || !object->pETRLEObject || !object->pPixData ||
		index >= object->usNumberOfObjects) return FALSE;
	const ETRLEObject& frame = object->pETRLEObject[index];
	const INT32 originX = x + frame.sOffsetX;
	const INT32 originY = y + frame.sOffsetY;
	if constexpr (!clipped)
	{
		if (originX < 0 || originY < 0) return FALSE;
	}
	const UINT16* palette = object->pShadeCurrent;
	if (!palette) return FALSE;
	const UINT8* source = static_cast<const UINT8*>(object->pPixData) + frame.uiDataOffset;
	const UINT32 pitchPixels = pitchBytes / sizeof(UINT16);
	if constexpr (testZ)
	{
		if (zBuffer)
		{
			return walkEtrleSpans<clipped>(buffer, pitchPixels, frame, source, originX, originY, clip,
				PaletteSpanPainter<true, writeZ>{palette, zBuffer, zValue});
		}
	}
	return walkEtrleSpans<clipped>(buffer, pitchPixels, frame, source, originX, originY, clip,
		PaletteSpanPainter<false, false>{palette, nullptr, 0});
#endif
}
} // namespace ja2::blitter

using namespace ja2::blitter;

unsigned short blendWithAlpha(unsigned int rgb565New, unsigned int rgb565Old, unsigned int alpha)
{
	return alphaBlend565(static_cast<UINT16>(rgb565New), static_cast<UINT16>(rgb565Old),
		static_cast<UINT8>(std::min(alpha, 255u)));
}

// Clipping and Z-buffer utilities.

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
	return left < clip->iLeft || top < clip->iTop ||
		left + frame.usWidth > clip->iRight || top + frame.usHeight > clip->iBottom;
}

CHAR8 BltIsClippedOrOffScreen(HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	if (!object || index >= object->usNumberOfObjects) return -1;
	if (!clip) clip = &ClippingRect;
	const ETRLEObject& frame = object->pETRLEObject[index];
	const INT32 left = x + frame.sOffsetX;
	const INT32 top = y + frame.sOffsetY;
	if (left >= clip->iRight || top >= clip->iBottom ||
		left + frame.usWidth <= clip->iLeft || top + frame.usHeight <= clip->iTop)
	{
		return -1;
	}
	return BltIsClipped(object, x, y, index, clip) ? 1 : 0;
}

UINT16* InitZBuffer(UINT32 pitchBytes, UINT32 height)
{
	ClippingRect.iLeft = 0;
	ClippingRect.iTop = 0;
	ClippingRect.iRight = SCREEN_WIDTH;
	ClippingRect.iBottom = SCREEN_HEIGHT;
	UINT16* buffer = static_cast<UINT16*>(MemAlloc(static_cast<std::size_t>(pitchBytes) * height));
	if (buffer)
	{
		std::memset(buffer, 0, static_cast<std::size_t>(pitchBytes) * height);
		BYTE* data = reinterpret_cast<BYTE*>(buffer);
		SurfaceData::SetApplicationData(data);
		g_SurfaceRectangle[static_cast<UINT32>(SurfaceData::GetSurfaceID(data))]
			.SetRect(pitchBytes / sizeof(UINT16), height);
	}
	return buffer;
}

BOOLEAN ShutdownZBuffer(UINT16* buffer)
{
	SurfaceData::ReleaseApplicationData(reinterpret_cast<BYTE*>(buffer));
	MemFree(buffer);
	return TRUE;
}

BOOLEAN BlitZRect(UINT16* buffer, UINT32 pitchBytes,
	INT16 left, INT16 top, INT16 right, INT16 bottom, UINT16 value)
{
	if (!buffer) return FALSE;
	left = static_cast<INT16>(std::max<INT32>(left, ClippingRect.iLeft));
	top = static_cast<INT16>(std::max<INT32>(top, ClippingRect.iTop));
	right = static_cast<INT16>(std::min<INT32>(right, ClippingRect.iRight));
	bottom = static_cast<INT16>(std::min<INT32>(bottom, ClippingRect.iBottom));
	if (left >= right || top >= bottom) return FALSE;
	const UINT32 pitchPixels = pitchBytes / sizeof(UINT16);
	for (INT32 y = top; y < bottom; ++y)
	{
		std::fill_n(buffer + static_cast<std::size_t>(y) * pitchPixels + left, right - left, value);
	}
	return TRUE;
}

// Transparent blitters.
// In the API names, Z selects depth testing and NB disables depth writes.
// Clip variants use ClippingRect when the caller supplies no rectangle.

BOOLEAN Blt8BPPDataTo16BPPBufferTransZ(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	return blitPalette<false, true>(buffer, pitchBytes, zBuffer, zValue, object, x, y, index);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNB(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	return blitPalette<false, false>(buffer, pitchBytes, zBuffer, zValue, object, x, y, index);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	return blitPalette<true, true>(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, effectiveClip(clip));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	return blitPalette<true, false>(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, effectiveClip(clip));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransparent(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	return blitPalette<false, false, false>(buffer, pitchBytes, nullptr, 0, object, x, y, index);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransparentClip(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	return blitPalette<true, false, false>(buffer, pitchBytes, nullptr, 0, object, x, y, index, effectiveClip(clip));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransMirror(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	BlitPolicy policy;
	policy.mirror = true;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBColor(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, UINT16 color)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, false);
	policy.solidWhenObscured = true;
	policy.solidColor = color;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipColor(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	SGPRect* clip, UINT16 color)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, false, effectiveClip(clip));
	policy.solidWhenObscured = true;
	policy.solidColor = color;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

// Pixelated and translucent blitters.

BOOLEAN Blt8BPPDataTo16BPPBufferTransZPixelate(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, true, nullptr);
	policy.pixelation = Pixelation::Always;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBPixelate(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, false, nullptr);
	policy.pixelation = Pixelation::Always;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZPixelateObscured(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, true);
	policy.depthTest = DepthTest::Less;
	policy.pixelation = Pixelation::WhenObscured;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipPixelate(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, true, effectiveClip(clip));
	policy.pixelation = Pixelation::Always;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipPixelate(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, false, effectiveClip(clip));
	policy.pixelation = Pixelation::Always;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipPixelateObscured(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, true, effectiveClip(clip));
	policy.depthTest = DepthTest::Less;
	policy.pixelation = Pixelation::WhenObscured;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZTranslucent(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, true);
	policy.translucent = true;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBTranslucent(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, false);
	policy.translucent = true;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipTranslucent(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, true, effectiveClip(clip));
	policy.translucent = true;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipTranslucent(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, false, effectiveClip(clip));
	policy.translucent = true;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

// Monochrome and transparent-shadow blitters.

BOOLEAN Blt8BPPDataTo16BPPBufferMonoShadowClip(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip,
	UINT16 foreground, UINT16 background, UINT16 shadow)
{
	BlitPolicy policy;
	policy.paint = Paint::Mono;
	policy.clip = effectiveClip(clip);
	policy.foreground = foreground;
	policy.background = background;
	policy.shadow = shadow;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadow(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, UINT16* palette, BOOLEAN ignoreShadows)
{
	return blitEtrle(buffer, pitchBytes, object, x, y, index,
		shadowPolicy(nullptr, 0, false, nullptr, palette, ignoreShadows));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowAlpha(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, HVOBJECT alphaObject, INT32 x, INT32 y, UINT16 index,
	UINT16* palette, BOOLEAN ignoreShadows)
{
	BlitPolicy policy = shadowPolicy(nullptr, 0, false, nullptr, palette, ignoreShadows);
	policy.alphaObject = alphaObject;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowClip(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip,
	UINT16* palette, BOOLEAN ignoreShadows)
{
	return blitEtrle(buffer, pitchBytes, object, x, y, index,
		shadowPolicy(nullptr, 0, false, effectiveClip(clip), palette, ignoreShadows));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowClipAlpha(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, HVOBJECT alphaObject, INT32 x, INT32 y, UINT16 index,
	SGPRect* clip, UINT16* palette, BOOLEAN ignoreShadows)
{
	BlitPolicy policy = shadowPolicy(nullptr, 0, false, effectiveClip(clip), palette, ignoreShadows);
	policy.alphaObject = alphaObject;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZ(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	UINT16* palette, BOOLEAN ignoreShadows)
{
	return blitEtrle(buffer, pitchBytes, object, x, y, index,
		shadowPolicy(zBuffer, zValue, true, nullptr, palette, ignoreShadows));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNB(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	UINT16* palette, BOOLEAN ignoreShadows)
{
	return blitEtrle(buffer, pitchBytes, object, x, y, index,
		shadowPolicy(zBuffer, zValue, false, nullptr, palette, ignoreShadows));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZAlpha(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, HVOBJECT alphaObject,
	INT32 x, INT32 y, UINT16 index, UINT16* palette, BOOLEAN ignoreShadows)
{
	BlitPolicy policy = shadowPolicy(zBuffer, zValue, true, nullptr, palette, ignoreShadows);
	policy.alphaObject = alphaObject;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBAlpha(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, HVOBJECT alphaObject,
	INT32 x, INT32 y, UINT16 index, UINT16* palette, BOOLEAN ignoreShadows)
{
	BlitPolicy policy = shadowPolicy(zBuffer, zValue, false, nullptr, palette, ignoreShadows);
	policy.alphaObject = alphaObject;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	SGPRect* clip, UINT16* palette, BOOLEAN ignoreShadows)
{
	return blitEtrle(buffer, pitchBytes, object, x, y, index,
		shadowPolicy(zBuffer, zValue, true, effectiveClip(clip), palette, ignoreShadows));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	SGPRect* clip, UINT16* palette, BOOLEAN ignoreShadows)
{
	return blitEtrle(buffer, pitchBytes, object, x, y, index,
		shadowPolicy(zBuffer, zValue, false, effectiveClip(clip), palette, ignoreShadows));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZClipAlpha(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, HVOBJECT alphaObject,
	INT32 x, INT32 y, UINT16 index, SGPRect* clip, UINT16* palette, BOOLEAN ignoreShadows)
{
	BlitPolicy policy = shadowPolicy(zBuffer, zValue, true, effectiveClip(clip), palette, ignoreShadows);
	policy.alphaObject = alphaObject;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBClipAlpha(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, HVOBJECT alphaObject,
	INT32 x, INT32 y, UINT16 index, SGPRect* clip, UINT16* palette, BOOLEAN ignoreShadows)
{
	BlitPolicy policy = shadowPolicy(zBuffer, zValue, false, effectiveClip(clip), palette, ignoreShadows);
	policy.alphaObject = alphaObject;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

// Shadow-only blitters.

BOOLEAN shadowZ(UINT16* buffer, UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip, bool updateZ)
{
	BlitPolicy policy;
	policy.paint = Paint::Shadow;
	policy.zBuffer = zBuffer;
	policy.zValue = zValue;
	policy.depthTest = DepthTest::Less;
	policy.updateZ = updateZ;
	policy.clip = clip;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferShadowZ(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	return shadowZ(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, nullptr, true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferShadowZNB(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	return shadowZ(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, nullptr, false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferShadowZClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	return shadowZ(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, effectiveClip(clip), true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferShadowZNBClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	return shadowZ(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, effectiveClip(clip), false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferShadow(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	BlitPolicy policy;
	policy.paint = Paint::Shadow;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferShadowClip(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	BlitPolicy policy;
	policy.paint = Paint::Shadow;
	policy.clip = effectiveClip(clip);
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

// Intensity blitters.

BOOLEAN intensity(UINT16* buffer, UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip, bool updateZ)
{
	BlitPolicy policy;
	policy.paint = Paint::Intensity;
	policy.zBuffer = zBuffer;
	policy.zValue = zValue;
	policy.depthTest = zBuffer ? DepthTest::Less : DepthTest::None;
	policy.updateZ = updateZ;
	policy.clip = clip;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferIntensity(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	return intensity(buffer, pitchBytes, nullptr, 0, object, x, y, index, nullptr, false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferIntensityClip(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	return intensity(buffer, pitchBytes, nullptr, 0, object, x, y, index, effectiveClip(clip), false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZ(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	return intensity(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, nullptr, true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZNB(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	return intensity(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, nullptr, false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	return intensity(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, effectiveClip(clip), true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZNBClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	return intensity(buffer, pitchBytes, zBuffer, zValue, object, x, y, index, effectiveClip(clip), false);
}

// Obscured transparent-shadow blitters.

BOOLEAN obscuredShadow(UINT16* buffer, UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue,
	HVOBJECT object, HVOBJECT alphaObject, INT32 x, INT32 y, UINT16 index,
	SGPRect* clip, UINT16* palette, BOOLEAN ignoreShadows)
{
	BlitPolicy policy = shadowPolicy(zBuffer, zValue, false, clip, palette, ignoreShadows);
	policy.depthTest = DepthTest::LessEqual;
	policy.pixelation = Pixelation::WhenObscured;
	policy.alphaObject = alphaObject;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscured(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	UINT16* palette, BOOLEAN ignoreShadows)
{
	return obscuredShadow(buffer, pitchBytes, zBuffer, zValue, object, nullptr,
		x, y, index, nullptr, palette, ignoreShadows);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredTest(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	UINT16* palette, BOOLEAN ignoreShadows)
{
	return obscuredShadow(buffer, pitchBytes, zBuffer, zValue, object, nullptr,
		x, y, index, nullptr, palette, ignoreShadows);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredAlpha(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, HVOBJECT alphaObject,
	INT32 x, INT32 y, UINT16 index, UINT16* palette, BOOLEAN ignoreShadows)
{
	return obscuredShadow(buffer, pitchBytes, zBuffer, zValue, object, alphaObject,
		x, y, index, nullptr, palette, ignoreShadows);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	SGPRect* clip, UINT16* palette, BOOLEAN ignoreShadows)
{
	return obscuredShadow(buffer, pitchBytes, zBuffer, zValue, object, nullptr,
		x, y, index, effectiveClip(clip), palette, ignoreShadows);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredClipAlpha(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, HVOBJECT alphaObject,
	INT32 x, INT32 y, UINT16 index, SGPRect* clip, UINT16* palette, BOOLEAN ignoreShadows)
{
	return obscuredShadow(buffer, pitchBytes, zBuffer, zValue, object, alphaObject,
		x, y, index, effectiveClip(clip), palette, ignoreShadows);
}

// Outline blitters.

BOOLEAN outline(UINT16* buffer, UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, INT16 color, BOOLEAN enabled,
	SGPRect* clip, bool updateZ, Pixelation pixelation)
{
	BlitPolicy policy = palettePolicy(zBuffer, zValue, updateZ, clip);
	policy.paint = Paint::Outline;
	if (pixelation == Pixelation::WhenObscured)
	{
		policy.depthTest = DepthTest::Less;
	}
	policy.outlineColor = color;
	policy.outlineEnabled = enabled;
	policy.pixelation = pixelation;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutline(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, INT16 color, BOOLEAN enabled)
{
	return outline(buffer, pitchBytes, nullptr, 0, object, x, y, index,
		color, enabled, nullptr, false, Pixelation::None);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutlineClip(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, INT16 color, BOOLEAN enabled, SGPRect* clip)
{
	return outline(buffer, pitchBytes, nullptr, 0, object, x, y, index,
		color, enabled, effectiveClip(clip), false, Pixelation::None);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZ(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	INT16 color, BOOLEAN enabled)
{
	return outline(buffer, pitchBytes, zBuffer, zValue, object, x, y, index,
		color, enabled, nullptr, true, Pixelation::None);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	INT16 color, BOOLEAN enabled, SGPRect* clip)
{
	return outline(buffer, pitchBytes, zBuffer, zValue, object, x, y, index,
		color, enabled, effectiveClip(clip), true, Pixelation::None);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZNB(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	INT16 color, BOOLEAN enabled)
{
	return outline(buffer, pitchBytes, zBuffer, zValue, object, x, y, index,
		color, enabled, nullptr, false, Pixelation::None);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZPixelateObscured(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	INT16 color, BOOLEAN enabled)
{
	return outline(buffer, pitchBytes, zBuffer, zValue, object, x, y, index,
		color, enabled, nullptr, true, Pixelation::WhenObscured);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZPixelateObscuredClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y, UINT16 index,
	INT16 color, BOOLEAN enabled, SGPRect* clip)
{
	return outline(buffer, pitchBytes, zBuffer, zValue, object, x, y, index,
		color, enabled, effectiveClip(clip), true, Pixelation::WhenObscured);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutlineShadow(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	BlitPolicy policy;
	policy.paint = Paint::OutlineShadow;
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferOutlineShadowClip(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index, SGPRect* clip)
{
	BlitPolicy policy;
	policy.paint = Paint::OutlineShadow;
	policy.clip = effectiveClip(clip);
	return blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

// Raw buffer copies.

BOOLEAN Blt8BPPTo8BPP(UINT8* destination, UINT32 destinationPitch, UINT8* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height)
{
	if (!destination || !source)
	{
		return FALSE;
	}
	for (UINT32 row = 0; row < height; ++row)
	{
		std::memmove(
			destination + static_cast<std::size_t>(destinationY + row) * destinationPitch + destinationX,
			source + static_cast<std::size_t>(sourceY + row) * sourcePitch + sourceX, width);
	}
	return TRUE;
}

BOOLEAN Blt16BPPTo16BPP(UINT16* destination, UINT32 destinationPitch, UINT16* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height)
{
	if (!destination || !source)
	{
		return FALSE;
	}
	if (!clipCopyRect(destination, source, destinationX, destinationY, sourceX,
		sourceY, width, height))
	{
		return TRUE;
	}
	const bool copyBottomUp = destination == source && destinationY > sourceY;
	for (UINT32 step = 0; step < height; ++step)
	{
		const UINT32 row = copyBottomUp ? height - 1 - step : step;
		std::memmove(reinterpret_cast<UINT8*>(destination) +
			static_cast<std::size_t>(destinationY + row) * destinationPitch +
			static_cast<std::size_t>(destinationX) * sizeof(UINT16),
			reinterpret_cast<UINT8*>(source) +
			static_cast<std::size_t>(sourceY + row) * sourcePitch +
			static_cast<std::size_t>(sourceX) * sizeof(UINT16),
			static_cast<std::size_t>(width) * sizeof(UINT16));
	}
	return TRUE;
}

BOOLEAN Blt16BPPTo16BPPTrans(UINT16* destination, UINT32 destinationPitch, UINT16* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY,
	UINT32 width, UINT32 height, UINT16 transparent)
{
	if (!destination || !source)
	{
		return FALSE;
	}
	if (!clipCopyRect(destination, source, destinationX, destinationY, sourceX,
		sourceY, width, height))
	{
		return TRUE;
	}
	for (UINT32 row = 0; row < height; ++row)
	{
		UINT16* destinationRow = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination) +
			static_cast<std::size_t>(destinationY + row) * destinationPitch) + destinationX;
		UINT16* sourceRow = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(source) +
			static_cast<std::size_t>(sourceY + row) * sourcePitch) + sourceX;
		for (UINT32 column = 0; column < width; ++column)
		{
			if (sourceRow[column] != transparent)
			{
				destinationRow[column] = sourceRow[column];
			}
		}
	}
	return TRUE;
}

BOOLEAN Blt16BPPTo16BPPTransShadow(UINT16* destination, UINT32 destinationPitch,
	UINT16* source, UINT32 sourcePitch, INT32 destinationX, INT32 destinationY,
	INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height, UINT16 transparent)
{
	if (!destination || !source)
	{
		return FALSE;
	}
	if (!clipCopyRect(destination, source, destinationX, destinationY, sourceX,
		sourceY, width, height))
	{
		return TRUE;
	}
	for (UINT32 row = 0; row < height; ++row)
	{
		UINT16* destinationRow = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination) +
			static_cast<std::size_t>(destinationY + row) * destinationPitch) + destinationX;
		UINT16* sourceRow = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(source) +
			static_cast<std::size_t>(sourceY + row) * sourcePitch) + sourceX;
		for (UINT32 column = 0; column < width; ++column)
		{
			if (sourceRow[column] != transparent)
			{
				destinationRow[column] = ShadeTable[destinationRow[column]];
			}
		}
	}
	return TRUE;
}

BOOLEAN Blt16BPPTo16BPPMirror(UINT16* destination, UINT32 destinationPitch, UINT16* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height)
{
	if (!destination || !source)
	{
		return FALSE;
	}
	for (UINT32 row = 0; row < height; ++row)
	{
		UINT16* destinationRow = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination) +
			static_cast<std::size_t>(destinationY + row) * destinationPitch) + destinationX;
		UINT16* sourceRow = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(source) +
			static_cast<std::size_t>(sourceY + row) * sourcePitch) + sourceX;
		for (UINT32 column = 0; column < width; ++column)
		{
			destinationRow[column] = sourceRow[width - 1 - column];
		}
	}
	return TRUE;
}

// Uncompressed 8-bit surfaces and 16-bit sprites.

BOOLEAN blitFlat8(UINT16* destination, UINT32 destinationPitch, HVSURFACE surface, const UINT8* source,
	UINT32 sourcePitch, INT32 destinationX, INT32 destinationY, INT32 sourceLeft, INT32 sourceTop,
	UINT32 width, UINT32 height, bool halfSize)
{
	if (!destination || !surface || !source || !surface->p16BPPPalette ||
		destinationX < 0 || destinationY < 0)
	{
		return FALSE;
	}
	const UINT32 step = halfSize ? 2 : 1;
	const UINT32 outputWidth = width / step;
	const UINT32 outputHeight = height / step;
	for (UINT32 row = 0; row < outputHeight; ++row)
	{
		UINT16* destinationRow = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination) +
			static_cast<std::size_t>(destinationY + row) * destinationPitch) + destinationX;
		const UINT8* sourceRow = source +
			static_cast<std::size_t>(sourceTop + row * step) * sourcePitch + sourceLeft;
		for (UINT32 column = 0; column < outputWidth; ++column)
		{
			destinationRow[column] = surface->p16BPPPalette[sourceRow[column * step]];
		}
	}
	return TRUE;
}

BOOLEAN Blt8BPPDataTo16BPPBuffer(UINT16* destination, UINT32 destinationPitch, HVSURFACE surface,
	UINT8* source, INT32 destinationX, INT32 destinationY)
{
	return blitFlat8(destination, destinationPitch, surface, source, surface ? surface->usWidth : 0,
		destinationX, destinationY, 0, 0, surface ? surface->usWidth : 0,
		surface ? surface->usHeight : 0, false);
}

BOOLEAN Blt8BPPDataSubTo16BPPBuffer(UINT16* destination, UINT32 destinationPitch, HVSURFACE surface,
	UINT8* source, UINT32 sourcePitch, INT32 destinationX, INT32 destinationY, SGPRect* sourceRect)
{
	return sourceRect && blitFlat8(destination, destinationPitch, surface, source, sourcePitch,
		destinationX, destinationY, sourceRect->iLeft, sourceRect->iTop,
		sourceRect->iRight - sourceRect->iLeft, sourceRect->iBottom - sourceRect->iTop, false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferHalf(UINT16* destination, UINT32 destinationPitch, HVSURFACE surface,
	UINT8* source, UINT32 sourcePitch, INT32 destinationX, INT32 destinationY)
{
	return blitFlat8(destination, destinationPitch, surface, source, sourcePitch,
		destinationX, destinationY, 0, 0, surface ? surface->usWidth : 0,
		surface ? surface->usHeight : 0, true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferHalfRect(UINT16* destination, UINT32 destinationPitch, HVSURFACE surface,
	UINT8* source, UINT32 sourcePitch, INT32 destinationX, INT32 destinationY, SGPRect* sourceRect)
{
	return sourceRect && blitFlat8(destination, destinationPitch, surface, source, sourcePitch,
		destinationX, destinationY, sourceRect->iLeft, sourceRect->iTop,
		sourceRect->iRight - sourceRect->iLeft, sourceRect->iBottom - sourceRect->iTop, true);
}

BOOLEAN blit16Object(UINT16* destination, UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue,
	HVOBJECT object, INT32 destinationX, INT32 destinationY, UINT16 frameIndex, SGPRect* clip,
	bool depthTest)
{
	if (!destination || !object || !object->p16BPPObject || frameIndex >= object->usNumberOf16BPPObjects)
	{
		return FALSE;
	}
	const SixteenBPPObjectInfo& frame = object->p16BPPObject[frameIndex];
	const UINT32 pitchPixels = pitchBytes / sizeof(UINT16);
	for (UINT16 sourceY = 0; sourceY < frame.usHeight; ++sourceY)
	{
		for (UINT16 sourceX = 0; sourceX < frame.usWidth; ++sourceX)
		{
			const INT32 pixelX = destinationX + frame.sOffsetX + sourceX;
			const INT32 pixelY = destinationY + frame.sOffsetY + sourceY;
			if (pixelX < 0 || pixelY < 0 ||
				(clip && (pixelX < clip->iLeft || pixelX >= clip->iRight ||
					pixelY < clip->iTop || pixelY >= clip->iBottom)))
			{
				continue;
			}
			const UINT16 pixel = frame.p16BPPData[static_cast<std::size_t>(sourceY) * frame.usWidth + sourceX];
			if (!pixel)
			{
				continue;
			}
			UINT16* destinationPixel = destination + static_cast<std::size_t>(pixelY) * pitchPixels + pixelX;
			if (!depthTest || zBuffer[static_cast<std::size_t>(pixelY) * pitchPixels + pixelX] <= zValue)
			{
				*destinationPixel = pixel;
				if (depthTest)
				{
					zBuffer[static_cast<std::size_t>(pixelY) * pitchPixels + pixelX] = zValue;
				}
			}
		}
	}
	return TRUE;
}

BOOLEAN Blt16BPPDataTo16BPPBufferTransparentClip(UINT16* destination, UINT32 destinationPitch,
	HVOBJECT object, INT32 destinationX, INT32 destinationY, UINT16 frameIndex, SGPRect* clip)
{
	return blit16Object(destination, destinationPitch, nullptr, 0, object,
		destinationX, destinationY, frameIndex, effectiveClip(clip), false);
}

BOOLEAN Blt16BPPDataTo16BPPBufferTransZClip(UINT16* destination, UINT32 destinationPitch,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 destinationX, INT32 destinationY,
	UINT16 frameIndex, SGPRect* clip)
{
	return blit16Object(destination, destinationPitch, zBuffer, zValue, object,
		destinationX, destinationY, frameIndex, effectiveClip(clip), true);
}

// Rectangle fills, patterns and shading.

BOOLEAN Blt16BPPBufferPixelateRectWithColor(UINT16* destination, UINT32 pitchBytes, SGPRect* area,
	UINT8 pattern[8][8], UINT16 color)
{
	if (!destination || !area || !pattern)
	{
		return FALSE;
	}
	// The area is inclusive; ClippingRect's right and bottom edges are exclusive.
	const INT32 left = std::max(area->iLeft, ClippingRect.iLeft);
	const INT32 top = std::max(area->iTop, ClippingRect.iTop);
	const INT32 right = std::min(area->iRight, ClippingRect.iRight - 1);
	const INT32 bottom = std::min(area->iBottom, ClippingRect.iBottom - 1);
	if (left > right || top > bottom)
	{
		return FALSE;
	}
	const UINT32 pitchPixels = pitchBytes / sizeof(UINT16);
	for (INT32 pixelY = top; pixelY <= bottom; ++pixelY)
	{
		for (INT32 pixelX = left; pixelX <= right; ++pixelX)
		{
			if (pattern[(pixelY - top) & 7][(pixelX - left) & 7])
			{
				destination[static_cast<std::size_t>(pixelY) * pitchPixels + pixelX] = color;
			}
		}
	}
	return TRUE;
}

BOOLEAN Blt16BPPBufferPixelateRect(UINT16* destination, UINT32 pitchBytes, SGPRect* area,
	UINT8 pattern[8][8])
{
	return Blt16BPPBufferPixelateRectWithColor(destination, pitchBytes, area, pattern, 0);
}

namespace
{
UINT8 Hatch[8][8] =
{
	{1, 0, 1, 0, 1, 0, 1, 0},
	{0, 1, 0, 1, 0, 1, 0, 1},
	{1, 0, 1, 0, 1, 0, 1, 0},
	{0, 1, 0, 1, 0, 1, 0, 1},
	{1, 0, 1, 0, 1, 0, 1, 0},
	{0, 1, 0, 1, 0, 1, 0, 1},
	{1, 0, 1, 0, 1, 0, 1, 0},
	{0, 1, 0, 1, 0, 1, 0, 1}
};

UINT8 LooseHatch[8][8] =
{
	{1, 0, 0, 0, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 0, 0, 0, 1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{1, 0, 0, 0, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 0, 0, 0, 1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
};
}

BOOLEAN Blt16BPPBufferHatchRectWithColor(UINT16* destination, UINT32 pitchBytes, SGPRect* area,
	UINT16 color)
{
	return Blt16BPPBufferPixelateRectWithColor(destination, pitchBytes, area, Hatch, color);
}

BOOLEAN Blt16BPPBufferHatchRect(UINT16* destination, UINT32 pitchBytes, SGPRect* area)
{
	return Blt16BPPBufferHatchRectWithColor(destination, pitchBytes, area, 0);
}

BOOLEAN Blt16BPPBufferLooseHatchRectWithColor(UINT16* destination, UINT32 pitchBytes, SGPRect* area,
	UINT16 color)
{
	return Blt16BPPBufferPixelateRectWithColor(destination, pitchBytes, area, LooseHatch, color);
}

BOOLEAN Blt16BPPBufferLooseHatchRect(UINT16* destination, UINT32 pitchBytes, SGPRect* area)
{
	return Blt16BPPBufferLooseHatchRectWithColor(destination, pitchBytes, area, 0);
}

BOOLEAN shadeRect(UINT16* destination, UINT32 pitchBytes, SGPRect* area, UINT16* shadeTable)
{
	if (!destination || !area)
	{
		return FALSE;
	}
	// The area is inclusive; ClippingRect's right and bottom edges are exclusive.
	const INT32 left = std::max(area->iLeft, ClippingRect.iLeft);
	const INT32 top = std::max(area->iTop, ClippingRect.iTop);
	const INT32 right = std::min(area->iRight, ClippingRect.iRight - 1);
	const INT32 bottom = std::min(area->iBottom, ClippingRect.iBottom - 1);
	if (left > right || top > bottom)
	{
		return FALSE;
	}
	const UINT32 pitchPixels = pitchBytes / sizeof(UINT16);
	for (INT32 pixelY = top; pixelY <= bottom; ++pixelY)
	{
		for (INT32 pixelX = left; pixelX <= right; ++pixelX)
		{
			UINT16& pixel = destination[static_cast<std::size_t>(pixelY) * pitchPixels + pixelX];
			pixel = shadeTable[pixel];
		}
	}
	return TRUE;
}

BOOLEAN Blt16BPPBufferShadowRect(UINT16* destination, UINT32 pitchBytes, SGPRect* area)
{
	return shadeRect(destination, pitchBytes, area, ShadeTable);
}

BOOLEAN Blt16BPPBufferShadowRectAlternateTable(UINT16* destination, UINT32 pitchBytes, SGPRect* area)
{
	return shadeRect(destination, pitchBytes, area, IntensityTable);
}

BOOLEAN FillRect16BPP(UINT16* destination, UINT32 pitchBytes, INT32 left, INT32 top,
	INT32 right, INT32 bottom, UINT16 color)
{
	if (!destination || left >= right || top >= bottom)
	{
		return FALSE;
	}
	const UINT32 pitchPixels = pitchBytes / sizeof(UINT16);
	for (INT32 pixelY = top; pixelY < bottom; ++pixelY)
	{
		std::fill(destination + static_cast<std::size_t>(pixelY) * pitchPixels + left,
			destination + static_cast<std::size_t>(pixelY) * pitchPixels + right, color);
	}
	return TRUE;
}

// 32-bit RGBA sources blended into RGB565 destinations.

BOOLEAN blit32(UINT16* destination, UINT32 destinationPitch, UINT32* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY,
	UINT32 width, UINT32 height, bool shadow)
{
	if (!destination || !source)
	{
		return FALSE;
	}
	for (UINT32 row = 0; row < height; ++row)
	{
		UINT16* destinationRow = reinterpret_cast<UINT16*>(reinterpret_cast<UINT8*>(destination) +
			static_cast<std::size_t>(destinationY + row) * destinationPitch) + destinationX;
		UINT32* sourceRow = reinterpret_cast<UINT32*>(reinterpret_cast<UINT8*>(source) +
			static_cast<std::size_t>(sourceY + row) * sourcePitch) + sourceX;
		for (UINT32 column = 0; column < width; ++column)
		{
			const UINT8 alpha = static_cast<UINT8>(sourceRow[column] >> 24);
			if (!alpha)
			{
				continue;
			}
			UINT16 value;
			if (shadow)
			{
				value = ShadeTable[destinationRow[column]];
			}
			else
			{
				const UINT8 red = sourceRow[column] & 0xff;
				const UINT8 green = (sourceRow[column] >> 8) & 0xff;
				const UINT8 blue = (sourceRow[column] >> 16) & 0xff;
				value = static_cast<UINT16>(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
			}
			destinationRow[column] = alphaBlend565(value, destinationRow[column], alpha);
		}
	}
	return TRUE;
}

BOOLEAN Blt32BPPTo16BPPTrans(UINT16* destination, UINT32 destinationPitch, UINT32* source, UINT32 sourcePitch,
	INT32 destinationX, INT32 destinationY, INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height)
{
	return blit32(destination, destinationPitch, source, sourcePitch,
		destinationX, destinationY, sourceX, sourceY, width, height, false);
}

BOOLEAN Blt32BPPTo16BPPTransShadow(UINT16* destination, UINT32 destinationPitch,
	UINT32* source, UINT32 sourcePitch, INT32 destinationX, INT32 destinationY,
	INT32 sourceX, INT32 sourceY, UINT32 width, UINT32 height)
{
	return blit32(destination, destinationPitch, source, sourcePitch,
		destinationX, destinationY, sourceX, sourceY, width, height, true);
}
