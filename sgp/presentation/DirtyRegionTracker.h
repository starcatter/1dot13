#ifndef JA2_DIRTY_REGION_TRACKER_H
#define JA2_DIRTY_REGION_TRACKER_H

#include "types.h"

#include <cstddef>
#include <vector>

namespace ja2::presentation
{

struct ExtendedDirtyRegion
{
	SGPRect bounds{};
	UINT32 flags = 0;
};

// Characterizes the legacy video.cpp damage-list rules. Rectangles use the
// engine's half-open screen convention: [left, right) x [top, bottom).
class DirtyRegionTracker
{
public:
	static constexpr std::size_t LegacyCapacity = 128;

	explicit DirtyRegionTracker(
		INT32 screenWidth, INT32 screenHeight,
		std::size_t capacity = LegacyCapacity);

	void invalidate(INT32 left, INT32 top, INT32 right, INT32 bottom);
	void invalidate(const SGPRect& region);

	// Matches legacy InvalidateRegions: the batch is accepted only when the
	// resulting count is strictly below the capacity and its entries are not
	// individually clipped or validated.
	void invalidateMany(const SGPRect* regions, std::size_t count);

	// Matches InvalidateRegionEx, including splitting a rectangle which spans
	// the supplied viewport boundary.
	void invalidateExtended(INT32 left, INT32 top, INT32 right, INT32 bottom,
		UINT32 flags, INT32 viewportBoundaryY);

	void invalidateScreen();
	void clearAfterPresent();

	bool fullRefresh() const noexcept { return fullRefresh_; }
	const std::vector<SGPRect>& regions() const noexcept { return regions_; }
	const std::vector<ExtendedDirtyRegion>& extendedRegions() const noexcept
	{
		return extendedRegions_;
	}

private:
	bool clipAndValidate(SGPRect& region) const noexcept;
	void addExtended(INT32 left, INT32 top, INT32 right, INT32 bottom,
		UINT32 flags);
	void overflowAllLists();

	INT32 screenWidth_;
	INT32 screenHeight_;
	std::size_t capacity_;
	bool fullRefresh_ = true;
	std::vector<SGPRect> regions_;
	std::vector<ExtendedDirtyRegion> extendedRegions_;
};

}

#endif
