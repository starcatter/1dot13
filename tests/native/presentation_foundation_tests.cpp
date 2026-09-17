#include "presentation/DirtyRegionTracker.h"
#include "presentation/PixelSurface.h"
#include "presentation/Presenter.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>

namespace
{

using ja2::presentation::BlitOptions;
using ja2::presentation::ConstPixelBuffer;
using ja2::presentation::DirtyRegionTracker;
using ja2::presentation::MutablePixelBuffer;
using ja2::presentation::PixelFormat;
using ja2::presentation::PixelSurface;
using ja2::presentation::PresentFrame;
using ja2::presentation::Presenter;

UINT16 pixelAt(const PixelSurface& surface, INT32 x, INT32 y)
{
	const ConstPixelBuffer pixels = surface.pixels();
	const BYTE* location = pixels.pixels + y * pixels.pitchBytes +
		x * (pixels.format == PixelFormat::indexed8 ? 1 : 2);
	if (pixels.format == PixelFormat::indexed8)
	{
		return *location;
	}
	UINT16 value;
	std::memcpy(&value, location, sizeof(value));
	return value;
}

template<std::size_t Size>
void assertVisibleEquals(
	const PixelSurface& surface, const std::array<UINT16, Size>& expected)
{
	assert(Size == static_cast<std::size_t>(surface.width()) * surface.height());
	for (INT32 y = 0; y < surface.height(); ++y)
	{
		for (INT32 x = 0; x < surface.width(); ++x)
		{
			const std::size_t index = static_cast<std::size_t>(y) * surface.width() + x;
			assert(pixelAt(surface, x, y) == expected[index]);
		}
	}
}

void testDirtyRegions()
{
	DirtyRegionTracker tracker(640, 480, 3);
	assert(tracker.fullRefresh());
	tracker.invalidate(1, 2, 3, 4);
	assert(tracker.regions().empty());

	tracker.clearAfterPresent();
	tracker.invalidate(-4, -3, 10, 12);
	tracker.invalidate(20, 20, 20, 25);
	assert(tracker.regions().size() == 1);
	assert(tracker.regions()[0].iLeft == 0);
	assert(tracker.regions()[0].iTop == 0);
	assert(tracker.regions()[0].iRight == 10);
	assert(tracker.regions()[0].iBottom == 12);

	tracker.invalidateExtended(5, 100, 30, 300, 0x42, 200);
	assert(tracker.extendedRegions().size() == 2);
	assert(tracker.extendedRegions()[0].bounds.iBottom == 200);
	assert(tracker.extendedRegions()[1].bounds.iTop == 200);
	assert(tracker.extendedRegions()[1].flags == 0x42);

	tracker.invalidate(30, 30, 40, 40);
	tracker.invalidate(50, 50, 60, 60);
	tracker.invalidate(70, 70, 80, 80);
	assert(tracker.fullRefresh());
	assert(tracker.regions().empty());
	assert(tracker.extendedRegions().empty());

	tracker.clearAfterPresent();
	const std::array<SGPRect, 2> batch{{
		{1, 1, 2, 2},
		{-20, -20, -10, -10},
	}};
	tracker.invalidateMany(batch.data(), batch.size());
	assert(tracker.regions().size() == 2);
	// Legacy batch invalidation does not clip or reject entries.
	assert(tracker.regions()[1].iLeft == -20);

	tracker.clearAfterPresent();
	const std::array<SGPRect, 3> fullBatch{{
		{1, 1, 2, 2}, {2, 2, 3, 3}, {3, 3, 4, 4}}};
	tracker.invalidateMany(fullBatch.data(), fullBatch.size());
	assert(tracker.fullRefresh());
	assert(tracker.regions().empty());
}

void testPixelStorageAndPalette()
{
	PixelSurface rgb(3, 2, PixelFormat::rgb565, 8);
	assert(rgb.pitchBytes() == 8);
	auto locked = rgb.lock();
	assert(locked.pixels != nullptr);
	assert(locked.pitchBytes == 8);
	rgb.unlock();
	rgb.fill(0x1234);
	assert(pixelAt(rgb, 2, 1) == 0x1234);
	rgb.fillRect({1, -2, 4, 1}, 0xabcd);
	assert(pixelAt(rgb, 0, 0) == 0x1234);
	assert(pixelAt(rgb, 1, 0) == 0xabcd);

	PixelSurface indexed(2, 2, PixelFormat::indexed8);
	indexed.fill(0x0123);
	assert(pixelAt(indexed, 1, 1) == 0x23);
	std::array<SGPPaletteEntry, 256> palette{};
	palette[35] = {12, 34, 56, 0};
	indexed.setPalette(palette.data(), palette.size());
	assert(indexed.palette() != nullptr);
	assert(indexed.palette()[35].peGreen == 34);
}

void testExternalPixelTransferWithDifferentPitch()
{
	constexpr UINT32 sourcePitch = 10;
	constexpr UINT32 destinationPitch = 12;
	std::array<BYTE, sourcePitch * 2> source{};
	const std::array<UINT16, 6> expected{{1, 2, 3, 4, 5, 6}};
	for (std::size_t index = 0; index < expected.size(); ++index)
	{
		const std::size_t y = index / 3;
		const std::size_t x = index % 3;
		std::memcpy(source.data() + y * sourcePitch + x * 2,
			&expected[index], sizeof(UINT16));
	}
	std::fill(source.begin() + 6, source.begin() + sourcePitch, 0xee);
	std::fill(source.begin() + sourcePitch + 6, source.end(), 0xee);

	PixelSurface surface(3, 2, PixelFormat::rgb565, 8);
	assert(surface.replacePixelsFrom(
		{source.data(), sourcePitch, 3, 2, PixelFormat::rgb565}));
	assertVisibleEquals(surface, expected);

	std::array<BYTE, destinationPitch * 2> destination{};
	destination.fill(0xdd);
	assert(surface.copyPixelsTo(
		{destination.data(), destinationPitch, 3, 2, PixelFormat::rgb565}));
	for (std::size_t index = 0; index < expected.size(); ++index)
	{
		UINT16 value = 0;
		const std::size_t y = index / 3;
		const std::size_t x = index % 3;
		std::memcpy(&value, destination.data() + y * destinationPitch + x * 2,
			sizeof(value));
		assert(value == expected[index]);
	}
	for (std::size_t y = 0; y < 2; ++y)
	{
		for (std::size_t byte = 6; byte < destinationPitch; ++byte)
		{
			assert(destination[y * destinationPitch + byte] == 0xdd);
		}
	}

	assert(!surface.replacePixelsFrom(
		{source.data(), 5, 3, 2, PixelFormat::rgb565}));
	assert(!surface.copyPixelsTo(
		{destination.data(), destinationPitch, 3, 2, PixelFormat::indexed8}));
}

void testCompatibilityMirrorRoundTrip()
{
	constexpr UINT32 mirrorPitch = 12;
	PixelSurface canonical(2, 2, PixelFormat::rgb565, 8);
	canonical.fill(0x1234);

	std::array<BYTE, mirrorPitch * 2> mirror{};
	mirror.fill(0xcc);
	assert(canonical.copyPixelsTo(
		{mirror.data(), mirrorPitch, 2, 2, PixelFormat::rgb565}));

	const UINT16 compatibilityWrite = 0xabcd;
	std::memcpy(mirror.data() + mirrorPitch + sizeof(UINT16),
		&compatibilityWrite, sizeof(compatibilityWrite));
	assert(canonical.replacePixelsFrom(
		{mirror.data(), mirrorPitch, 2, 2, PixelFormat::rgb565}));
	assertVisibleEquals(canonical,
		std::array<UINT16, 4>{{0x1234, 0x1234, 0x1234, 0xabcd}});

	MutablePixelBuffer pixels = canonical.lock();
	const UINT16 portableWrite = 0x5678;
	std::memcpy(pixels.pixels, &portableWrite, sizeof(portableWrite));
	canonical.unlock();
	assert(canonical.copyPixelsTo(
		{mirror.data(), mirrorPitch, 2, 2, PixelFormat::rgb565}));
	UINT16 mirroredPixel = 0;
	std::memcpy(&mirroredPixel, mirror.data(), sizeof(mirroredPixel));
	assert(mirroredPixel == portableWrite);
	for (std::size_t y = 0; y < 2; ++y)
	{
		for (std::size_t byte = 4; byte < mirrorPitch; ++byte)
		{
			assert(mirror[y * mirrorPitch + byte] == 0xcc);
		}
	}
}

void testBlits()
{
	PixelSurface source(3, 2, PixelFormat::rgb565);
	PixelSurface destination(4, 3, PixelFormat::rgb565);
	for (INT32 y = 0; y < 2; ++y)
	{
		for (INT32 x = 0; x < 3; ++x)
		{
			source.fillRect({x, y, x + 1, y + 1},
				static_cast<UINT16>(1 + x + y * 3));
		}
	}
	destination.fill(9);
	assert(destination.blitFrom(source, {0, 0, 3, 2}, -1, 1));
	assert(pixelAt(destination, 0, 1) == 2);
	assert(pixelAt(destination, 1, 1) == 3);
	assert(pixelAt(destination, 0, 2) == 5);

	source.setColorKey(2);
	destination.fill(9);
	assert(destination.blitFrom(source, {0, 0, 3, 1}, 0, 0,
		BlitOptions{true, false}));
	assert(pixelAt(destination, 0, 0) == 1);
	assert(pixelAt(destination, 1, 0) == 9);
	assert(pixelAt(destination, 2, 0) == 3);

	destination.setColorKey(9);
	destination.fill(8);
	destination.fillRect({1, 0, 2, 1}, 9);
	assert(destination.blitFrom(source, {0, 0, 3, 1}, 0, 0,
		BlitOptions{false, true}));
	assert(pixelAt(destination, 0, 0) == 8);
	assert(pixelAt(destination, 1, 0) == 2);

	PixelSurface overlap(4, 1, PixelFormat::indexed8);
	for (INT32 x = 0; x < 4; ++x)
	{
		overlap.fillRect({x, 0, x + 1, 1}, static_cast<UINT16>(x + 1));
	}
	assert(overlap.blitFrom(overlap, {0, 0, 3, 1}, 1, 0));
	assert(pixelAt(overlap, 0, 0) == 1);
	assert(pixelAt(overlap, 1, 0) == 1);
	assert(pixelAt(overlap, 2, 0) == 2);
	assert(pixelAt(overlap, 3, 0) == 3);

	PixelSurface vertical(2, 4, PixelFormat::rgb565, 8);
	vertical.fillRect({0, 0, 2, 1}, 1);
	vertical.fillRect({0, 1, 2, 2}, 2);
	vertical.fillRect({0, 2, 2, 3}, 3);
	vertical.fillRect({0, 3, 2, 4}, 4);
	assert(vertical.blitFrom(vertical, {0, 0, 2, 3}, 0, 1));
	assert(pixelAt(vertical, 0, 0) == 1);
	assert(pixelAt(vertical, 0, 1) == 1);
	assert(pixelAt(vertical, 0, 2) == 2);
	assert(pixelAt(vertical, 0, 3) == 3);
}

void testNearestStretch()
{
	PixelSurface source(2, 2, PixelFormat::indexed8);
	source.fillRect({0, 0, 1, 1}, 1);
	source.fillRect({1, 0, 2, 1}, 2);
	source.fillRect({0, 1, 1, 2}, 3);
	source.fillRect({1, 1, 2, 2}, 4);
	PixelSurface destination(4, 4, PixelFormat::indexed8);
	assert(destination.stretchFrom(source, {0, 0, 2, 2}, {0, 0, 4, 4}));
	assert(pixelAt(destination, 0, 0) == 1);
	assert(pixelAt(destination, 3, 0) == 2);
	assert(pixelAt(destination, 0, 3) == 3);
	assert(pixelAt(destination, 3, 3) == 4);
}

void testGoldenFramebufferCompositionAndBackupRestore()
{
	PixelSurface frame(6, 4, PixelFormat::rgb565, 16);
	assert(frame.pitchBytes() == 16); // two padding pixels per physical row
	frame.fill(0x1111);

	PixelSurface background(4, 3, PixelFormat::rgb565);
	for (INT32 y = 0; y < background.height(); ++y)
	{
		for (INT32 x = 0; x < background.width(); ++x)
		{
			background.fillRect({x, y, x + 1, y + 1},
				static_cast<UINT16>(1 + x + y * background.width()));
		}
	}
	assert(frame.blitFrom(background, {0, 0, 4, 3}, -1, 0));

	PixelSurface sprite(3, 2, PixelFormat::rgb565);
	sprite.setColorKey(0);
	sprite.fill(0);
	sprite.fillRect({1, 0, 2, 1}, 0x20);
	sprite.fillRect({2, 0, 3, 1}, 0x21);
	sprite.fillRect({0, 1, 1, 2}, 0x22);
	sprite.fillRect({2, 1, 3, 2}, 0x23);
	assert(frame.blitFrom(sprite, {0, 0, 3, 2}, 2, 1, {true, false}));

	frame.setColorKey(0x1111);
	PixelSurface reveal(2, 1, PixelFormat::rgb565);
	reveal.fillRect({0, 0, 1, 1}, 0x30);
	reveal.fillRect({1, 0, 2, 1}, 0x31);
	assert(frame.blitFrom(reveal, {0, 0, 2, 1}, 0, 3, {false, true}));

	const std::array<UINT16, 24> withoutCursor{{
		2, 3, 4, 0x1111, 0x1111, 0x1111,
		6, 7, 8, 0x20, 0x21, 0x1111,
		10, 11, 0x22, 0x1111, 0x23, 0x1111,
		0x30, 0x31, 0x1111, 0x1111, 0x1111, 0x1111,
	}};
	assertVisibleEquals(frame, withoutCursor);

	// Preserve the exact direction expected by UpdateBackupSurface and
	// RestoreVideoSurface: primary -> backup, then backup -> primary.
	PixelSurface cursorBackup(2, 2, PixelFormat::rgb565);
	assert(cursorBackup.blitFrom(frame, {4, 2, 6, 4}, 0, 0));
	PixelSurface cursor(2, 2, PixelFormat::rgb565);
	cursor.setColorKey(0);
	cursor.fill(0);
	cursor.fillRect({1, 0, 2, 1}, 0xaa);
	cursor.fillRect({0, 1, 1, 2}, 0xbb);
	cursor.fillRect({1, 1, 2, 2}, 0xcc);
	assert(frame.blitFrom(cursor, {0, 0, 2, 2}, 4, 2, {true, false}));

	const std::array<UINT16, 24> withCursor{{
		2, 3, 4, 0x1111, 0x1111, 0x1111,
		6, 7, 8, 0x20, 0x21, 0x1111,
		10, 11, 0x22, 0x1111, 0x23, 0xaa,
		0x30, 0x31, 0x1111, 0x1111, 0xbb, 0xcc,
	}};
	assertVisibleEquals(frame, withCursor);
	assert(frame.blitFrom(cursorBackup, {0, 0, 2, 2}, 4, 2));
	assertVisibleEquals(frame, withoutCursor);

	const ConstPixelBuffer pixels = frame.pixels();
	for (INT32 y = 0; y < frame.height(); ++y)
	{
		for (UINT32 byte = frame.width() * 2; byte < pixels.pitchBytes; ++byte)
		{
			assert(pixels.pixels[y * pixels.pitchBytes + byte] == 0);
		}
	}
}

class RecordingPresenter final : public Presenter
{
public:
	bool present(const PresentFrame& frame) override
	{
		lastFrame = frame;
		++presentCount;
		return true;
	}
	void suspend() override { suspended = true; }
	bool resume() override
	{
		suspended = false;
		return true;
	}

	PresentFrame lastFrame{};
	int presentCount = 0;
	bool suspended = false;
};

void testPresenterContract()
{
	PixelSurface surface(4, 3, PixelFormat::rgb565);
	const SGPRect dirty{1, 1, 3, 2};
	RecordingPresenter presenter;
	assert(presenter.present({surface.pixels(), &dirty, 1, false}));
	assert(presenter.presentCount == 1);
	assert(presenter.lastFrame.buffer.pitchBytes == 8);
	assert(presenter.lastFrame.dirtyRegions[0].iRight == 3);
	presenter.suspend();
	assert(presenter.suspended);
	assert(presenter.resume());
	assert(!presenter.suspended);
}

}

int main()
{
	testDirtyRegions();
	testPixelStorageAndPalette();
	testExternalPixelTransferWithDifferentPitch();
	testCompatibilityMirrorRoundTrip();
	testBlits();
	testNearestStretch();
	testGoldenFramebufferCompositionAndBackupRestore();
	testPresenterContract();
	return 0;
}
