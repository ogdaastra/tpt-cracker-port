#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// A grid whose size depends on the world's (Config.h: XRES and YRES are chosen at startup), replacing the
// fixed-size 2D arrays. It reads as grid[y][x] like they did; rows are stored one after another, width items
// each, so code that walks one as width * height items in a row (&grid[0][0] + i) still works.
//
// GridView is the non-owning handle (copied freely, passed to element functions as pmap was); WorldGrid owns
// the items. Never take sizeof() of either for the items' size: use Bytes().
template<class Item>
struct GridView
{
	Item *items = nullptr;
	int width = 0;

	Item *operator [](int y) const
	{
		return items + std::ptrdiff_t(y) * width;
	}
};

template<class Item>
class WorldGrid
{
	std::vector<Item> storage;
	int width = 0;
	int height = 0;

public:
	WorldGrid() = default;
	WorldGrid(int newWidth, int newHeight)
	{
		Resize(newWidth, newHeight);
	}
	WorldGrid(const WorldGrid &) = delete;
	WorldGrid &operator =(const WorldGrid &) = delete;

	void Resize(int newWidth, int newHeight)
	{
		width = newWidth;
		height = newHeight;
		storage.assign(std::size_t(width) * std::size_t(height), Item());
	}

	Item *operator [](int y)
	{
		return storage.data() + std::ptrdiff_t(y) * width;
	}
	const Item *operator [](int y) const
	{
		return storage.data() + std::ptrdiff_t(y) * width;
	}

	GridView<Item> View()
	{
		return { storage.data(), width };
	}
	operator GridView<Item>()
	{
		return View();
	}

	Item *Data() { return storage.data(); }
	const Item *Data() const { return storage.data(); }
	std::size_t Count() const { return storage.size(); }
	std::size_t Bytes() const { return storage.size() * sizeof(Item); }
	int Width() const { return width; }
	int Height() const { return height; }
	void Fill(const Item &value) { std::fill(storage.begin(), storage.end(), value); }
	void Clear() { std::fill(storage.begin(), storage.end(), Item()); }
	void CopyFrom(const WorldGrid &other) { std::copy(other.storage.begin(), other.storage.end(), storage.begin()); }
};
