// Portable implementations of the Z-strip blitters historically embedded as
// MSVC/x86 assembly in renderworld.cpp.  They use the same policy-driven ETRLE
// walker as the ordinary video-object blitters.

#if !defined(_MSC_VER) || !defined(_M_IX86)

#include "renderworld.h"

#include "PortableBlitterCore.h"
#include "vobject_blitters.h"

namespace
{
using ja2::blitter::BlitPolicy;
using ja2::blitter::DepthTest;
using ja2::blitter::Paint;
using ja2::blitter::Pixelation;
using ja2::blitter::ShadowTreatment;

SGPRect* effectiveClip(SGPRect* clip)
{
	return clip ? clip : &ClippingRect;
}

const ZStripInfo* getZStrip(HVOBJECT object, INT16 index)
{
	if (!object || !object->ppZStripInfo || index < 0 ||
		static_cast<UINT16>(index) >= object->usNumberOfObjects) return nullptr;
	return object->ppZStripInfo[index];
}

BlitPolicy stripPolicy(UINT16* zBuffer, UINT16 zValue, const ZStripInfo* strip,
	UINT16 stripDelta, DepthTest depthTest, SGPRect* clip)
{
	BlitPolicy policy;
	policy.zBuffer = zBuffer;
	policy.zValue = zValue;
	policy.zStrip = strip;
	policy.zStripInitialDelta = Z_SUBLAYERS * 10;
	policy.zStripDelta = stripDelta;
	policy.depthTest = depthTest;
	policy.updateZ = true;
	policy.clip = effectiveClip(clip);
	return policy;
}

BOOLEAN blitZStrip(UINT16* buffer, UINT32 pitchBytes, UINT16* zBuffer,
	UINT16 zValue, HVOBJECT object, HVOBJECT alphaObject, INT32 x, INT32 y,
	UINT16 index, SGPRect* clip, INT16 zStripIndex, UINT16 stripDelta,
	DepthTest depthTest, UINT16* palette, BOOLEAN ignoreShadows, bool shadow,
	bool obscure)
{
	const ZStripInfo* strip = getZStrip(object, zStripIndex);
	if (!strip) return FALSE;

	BlitPolicy policy = stripPolicy(zBuffer, zValue, strip, stripDelta, depthTest, clip);
	policy.alphaObject = alphaObject;
	if (shadow)
	{
		policy.palette = palette;
		policy.shadowTreatment = ignoreShadows
			? ShadowTreatment::Skip : ShadowTreatment::ShadeDestination;
	}
	if (obscure)
	{
		policy.pixelation = Pixelation::WhenObscured;
		policy.updateZWhenObscured = true;
		policy.drawShadowWhenObscured = true;
		policy.suppressEqualObscuredShadow = false;
	}
	return ja2::blitter::blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZIncClip(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y,
	UINT16 index, SGPRect* clip)
{
	return blitZStrip(buffer, pitchBytes, zBuffer, zValue, object, nullptr, x, y,
		index, clip, static_cast<INT16>(index), Z_SUBLAYERS * 10,
		DepthTest::Less, nullptr, FALSE, false, false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZIncClipZSameZBurnsThrough(UINT16* buffer,
	UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x,
	INT32 y, UINT16 index, SGPRect* clip, INT16 zStripIndex)
{
	return blitZStrip(buffer, pitchBytes, zBuffer, zValue, object, nullptr, x, y,
		index, clip, zStripIndex, Z_SUBLAYERS * 10, DepthTest::LessEqual,
		nullptr, FALSE, false, false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZIncObscureClip(UINT16* buffer,
	UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x,
	INT32 y, UINT16 index, SGPRect* clip)
{
	return blitZStrip(buffer, pitchBytes, zBuffer, zValue, object, nullptr, x, y,
		index, clip, static_cast<INT16>(index), Z_SUBLAYERS * 10,
		DepthTest::Less, nullptr, FALSE, false, true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZTransShadowIncObscureClip(UINT16* buffer,
	UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x,
	INT32 y, UINT16 index, SGPRect* clip, INT16 zStripIndex, UINT16* palette,
	BOOLEAN ignoreShadows)
{
	return blitZStrip(buffer, pitchBytes, zBuffer, zValue, object, nullptr, x, y,
		index, clip, zStripIndex, Z_SUBLAYERS, DepthTest::LessEqual, palette,
		ignoreShadows, true, true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZTransShadowIncObscureClipAlpha(
	UINT16* buffer, UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue,
	HVOBJECT object, HVOBJECT alphaObject, INT32 x, INT32 y, UINT16 index,
	SGPRect* clip, INT16 zStripIndex, UINT16* palette, BOOLEAN ignoreShadows)
{
	return blitZStrip(buffer, pitchBytes, zBuffer, zValue, object, alphaObject,
		x, y, index, clip, zStripIndex, Z_SUBLAYERS, DepthTest::Less, palette,
		ignoreShadows, true, true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZTransShadowIncClip(UINT16* buffer,
	UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x,
	INT32 y, UINT16 index, SGPRect* clip, INT16 zStripIndex, UINT16* palette,
	BOOLEAN ignoreShadows)
{
	return blitZStrip(buffer, pitchBytes, zBuffer, zValue, object, nullptr, x, y,
		index, clip, zStripIndex, Z_SUBLAYERS, DepthTest::LessEqual, palette,
		ignoreShadows, true, false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZTransShadowIncClipAlpha(UINT16* buffer,
	UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue, HVOBJECT object,
	HVOBJECT alphaObject, INT32 x, INT32 y, UINT16 index, SGPRect* clip,
	INT16 zStripIndex, UINT16* palette, BOOLEAN ignoreShadows)
{
	return blitZStrip(buffer, pitchBytes, zBuffer, zValue, object, alphaObject,
		x, y, index, clip, zStripIndex, Z_SUBLAYERS, DepthTest::LessEqual,
		palette, ignoreShadows, true, false);
}

BOOLEAN Zero8BPPDataTo16BPPBufferTransparent(UINT16* buffer, UINT32 pitchBytes,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	BlitPolicy policy;
	policy.paint = Paint::Solid;
	policy.solidColor = 0;
	return ja2::blitter::blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransInvZ(UINT16* buffer, UINT32 pitchBytes,
	UINT16* zBuffer, UINT16 zValue, HVOBJECT object, INT32 x, INT32 y,
	UINT16 index)
{
	BlitPolicy policy;
	policy.zBuffer = zBuffer;
	policy.zValue = zValue;
	policy.depthTest = DepthTest::Equal;
	return ja2::blitter::blitEtrle(buffer, pitchBytes, object, x, y, index, policy);
}

BOOLEAN IsTileRedundent(UINT32 pitchBytes, UINT16* zBuffer, UINT16 zValue,
	HVOBJECT object, INT32 x, INT32 y, UINT16 index)
{
	bool anyVisible = false;
	BlitPolicy policy;
	policy.paint = Paint::Probe;
	policy.zBuffer = zBuffer;
	policy.zValue = zValue;
	policy.depthTest = DepthTest::Less;
	policy.probeHit = &anyVisible;
	if (!ja2::blitter::blitEtrle(nullptr, pitchBytes, object, x, y, index, policy))
		return FALSE;
	return anyVisible ? FALSE : TRUE;
}

#endif
