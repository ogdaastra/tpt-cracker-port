#ifndef ROTATION_HELPER_H
#define ROTATION_HELPER_H

#include <cmath>
#include <vector>
#include <unordered_map>
#include <map>

#include "gui/interface/Colour.h"
#include "gui/interface/Point.h"
#include "simulation/ElementCommon.h"

typedef std::vector<std::pair<ui::Point, ui::Colour> > pixel_vector;
typedef std::vector<std::pair<ui::Point, int> > element_vector;

void rotate(int &x, int &y, float rotation);
void rotate(float &x, float &y, float rotation);
void rotate_around(int &x, int &y, int cx, int cy, float rotation);
void rotate_around(float &x, float &y, float cx, float cy, float rotation);

// 2 ints -> 1 unique int
inline int map_key(int x, int y) {
    return (x + y) * (x + y + 1) / 2 + y;
}

/**
 * Rotate a pixel vector by float radians, and write the result to
 * the specified pixel array (which will be cleared). This is a simple
 * approximation and should not be used for anything where accuracy is
 * needed
 */
template <typename T> void fastrot_vector(const T &base, float angle, T &out, float cx=0.0f, float cy=0.0f) {
    out.clear();
    for (auto &p : base) {
        int x = p.first.X, y = p.first.Y;
        rotate_around(x, y, cx, cy, angle);
        out.push_back(std::make_pair(ui::Point(x, y), p.second));
    }
}

/**
 * Rotate a pixel vector by scaleX and scaleY, and write the result to
 * the specified pixel array (which will be cleared). This is a simple
 * approximation and should not be used for anything where accuracy is
 * needed. ScaleX and ScaleY can be negative (flips it)
 * 
 * Width and height are original (not scaled) selection box width and height
 * ocx and ocy are original selection center, not translated center (which is cx, cy)
 * 
 * T  = array type (ie pixel_array)
 * T2 = 2nd array pair type, (ie ui::Colour) 
 */
template <typename T, typename T2> void fastscale_vector(const T &base, float scaleX, float scaleY, T &out,
        int width, int height, float cx=0.0f, float cy=0.0f, float ocx=0.0f, float ocy=0.0f) {
    out.clear();
    
    // Create hashmap of coords
    std::map<std::pair<int, int>, T2> coord_map;
    for (auto &p : base)
        coord_map[std::make_pair(p.first.X, p.first.Y)] = p.second;

    // Iterate all coords in bounding box
    int x1 = std::max(0.0f, cx - width / 2 * scaleX);
    int x2 = std::min((float)XRES, cx + width / 2 * scaleX);
    if (x2 < x1) std::swap(x1, x2);

    int y1 = std::max(0.0f, cy - height / 2 * scaleY);
    int y2 = std::min((float)YRES, cy + height / 2 * scaleY);
    if (y2 < y1) std::swap(y1, y2);

    int tx, ty;
    std::pair<int, int> c;
    for (int x = x1; x < x2; x++)
    for (int y = y1; y < y2; y++) {
        tx = std::round((x - cx) / scaleX + ocx);
        ty = std::round((y - cy) / scaleY + ocy);
        c = std::make_pair(tx, ty);

        if (coord_map.find(c) != coord_map.end())
            out.push_back(std::make_pair(ui::Point(x, y), coord_map[c]));
    }
}


#endif