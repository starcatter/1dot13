#include "vobject_blitters.h"

ClipRectangle::ClipRectangle()
{
	cr.iLeft = 0;
	cr.iTop = 0;
	cr.iRight = 0;
	cr.iBottom = 0;
}

void ClipRectangle::SetRect(SGPRect const& rect)
{
	Set(rect.iLeft, rect.iTop, rect.iRight, rect.iBottom);
}

void ClipRectangle::SetRect(unsigned int w, unsigned int h, int x, int y)
{
	Set(x, y, x + static_cast<int>(w) - 1,
		y + static_cast<int>(h) - 1);
}

void ClipRectangle::Set(int x1, int y1, int x2, int y2)
{
	cr.iLeft = x1;
	cr.iRight = x2;
	cr.iTop = y1;
	cr.iBottom = y2;
}

ClipRectangle::ClipType ClipRectangle::Clip(int& x, int& y,
	unsigned int& w, unsigned int& h)
{
	if (w == 0 || h == 0) return FullClip;
	int right = x + static_cast<int>(w) - 1;
	int bottom = y + static_cast<int>(h) - 1;
	const ClipType result = Clip(x, y, right, bottom);
	if (result == PartialClip)
	{
		w = static_cast<unsigned int>(right - x + 1);
		h = static_cast<unsigned int>(bottom - y + 1);
	}
	return result;
}

ClipRectangle::ClipType ClipRectangle::Clip(int& x1, int& y1,
	int& x2, int& y2)
{
	if (x1 >= cr.iLeft && x2 <= cr.iRight &&
		y1 >= cr.iTop && y2 <= cr.iBottom)
	{
		return NoClip;
	}
	if (x1 > cr.iRight || x2 < cr.iLeft ||
		y1 > cr.iBottom || y2 < cr.iTop)
	{
		return FullClip;
	}
	if (x1 < cr.iLeft) x1 = cr.iLeft;
	if (x2 > cr.iRight) x2 = cr.iRight;
	if (y1 < cr.iTop) y1 = cr.iTop;
	if (y2 > cr.iBottom) y2 = cr.iBottom;
	return PartialClip;
}
