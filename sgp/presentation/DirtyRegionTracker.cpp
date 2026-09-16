#include "presentation/DirtyRegionTracker.h"

#include <algorithm>
#include <stdexcept>

namespace ja2::presentation
{

DirtyRegionTracker::DirtyRegionTracker(
	INT32 screenWidth, INT32 screenHeight, std::size_t capacity)
	: screenWidth_(screenWidth), screenHeight_(screenHeight), capacity_(capacity)
{
	if (screenWidth <= 0 || screenHeight <= 0 || capacity == 0)
	{
		throw std::invalid_argument("Invalid dirty-region tracker dimensions");
	}
	regions_.reserve(capacity);
	extendedRegions_.reserve(capacity);
}

bool DirtyRegionTracker::clipAndValidate(SGPRect& region) const noexcept
{
	region.iLeft = std::max<INT32>(region.iLeft, 0);
	region.iTop = std::max<INT32>(region.iTop, 0);
	region.iRight = std::min(region.iRight, screenWidth_);
	region.iBottom = std::min(region.iBottom, screenHeight_);
	return region.iRight > region.iLeft && region.iBottom > region.iTop;
}

void DirtyRegionTracker::invalidate(
	INT32 left, INT32 top, INT32 right, INT32 bottom)
{
	if (fullRefresh_)
	{
		return;
	}
	if (regions_.size() >= capacity_)
	{
		overflowAllLists();
		return;
	}

	SGPRect region{left, top, right, bottom};
	if (clipAndValidate(region))
	{
		regions_.push_back(region);
	}
}

void DirtyRegionTracker::invalidate(const SGPRect& region)
{
	invalidate(region.iLeft, region.iTop, region.iRight, region.iBottom);
}

void DirtyRegionTracker::invalidateMany(
	const SGPRect* regions, std::size_t count)
{
	if (fullRefresh_)
	{
		return;
	}
	if (count > 0 && regions == nullptr)
	{
		throw std::invalid_argument("Null dirty-region batch");
	}
	if (count == 0)
	{
		return;
	}

	// The strict comparison is intentional and matches video.cpp.
	if (count < capacity_ - std::min(capacity_, regions_.size()))
	{
		regions_.insert(regions_.end(), regions, regions + count);
		return;
	}

	regions_.clear();
	fullRefresh_ = true;
}

void DirtyRegionTracker::invalidateExtended(
	INT32 left, INT32 top, INT32 right, INT32 bottom, UINT32 flags,
	INT32 viewportBoundaryY)
{
	// Legacy InvalidateRegionEx itself did not test the full-refresh flag;
	// AddRegionEx continued to collect entries until the next refresh.
	if (top <= viewportBoundaryY && bottom > viewportBoundaryY)
	{
		addExtended(left, top, right, viewportBoundaryY, flags);
		addExtended(left, viewportBoundaryY, right, bottom, flags);
	}
	else
	{
		addExtended(left, top, right, bottom, flags);
	}
}

void DirtyRegionTracker::addExtended(
	INT32 left, INT32 top, INT32 right, INT32 bottom, UINT32 flags)
{
	if (extendedRegions_.size() >= capacity_)
	{
		overflowAllLists();
		return;
	}

	SGPRect region{left, top, right, bottom};
	if (clipAndValidate(region))
	{
		extendedRegions_.push_back({region, flags});
	}
}

void DirtyRegionTracker::overflowAllLists()
{
	regions_.clear();
	extendedRegions_.clear();
	fullRefresh_ = true;
}

void DirtyRegionTracker::invalidateScreen()
{
	overflowAllLists();
}

void DirtyRegionTracker::clearAfterPresent()
{
	regions_.clear();
	extendedRegions_.clear();
	fullRefresh_ = false;
}

}
