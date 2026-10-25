#ifndef FONT_TEXTER_H
#define FONT_TEXTER_H

#include "common/String.h"
#include "simulation/Simulation.h"
#include "graphics/Graphics.h"
#include <vector>

class TexterFont {
public:
    enum FONTALIGN { Left, Middle, Right };

    TexterFont(String name_, int h): name(name_), height(h) {};

    virtual int get_width(const String &text) = 0;
    virtual void draw_graphic(const std::vector<String> &lines, int align, float spacing, int x, int y, Graphics * g, int color) = 0;
    virtual void draw_sim(const std::vector<String> &lines, int align, float spacing, int element, int x, int y, Simulation * sim) = 0;

    const String name;
    const int height;
};

// A single character in a font
class FontCharacter {
public:
    FontCharacter(const std::vector<int> chars_, int sizeX_, int sizeY_, int descender_=0, int kerning_=0):
        chars(chars_), sizeX(sizeX_), sizeY(sizeY_), descender(descender_), kerning(kerning_) {};
    int getEffectiveWidth() { return sizeX + kerning; }

    const std::vector<int> chars;
    const int sizeX, sizeY;
    const int descender, kerning;
};

// Fancy helper func with a shit ton of stuff, because it's called both when
// rendering preview text and the actual elements. If rendering graphics, set sim = nullptr
// otherwise set g = nullptr. FONT_HEIGHT is max font height of each char (the bounding box height)
void render_text(const std::vector<String> &lines, int align, float spacing, int x, int y,
        Graphics * g, Simulation * sim, int color_or_element, int FONT_HEIGHT,
        TexterFont * font, std::map<String, FontCharacter*> * map) {
    int cx = x, cy = y;
    for (unsigned int i = 0; i < lines.size(); ++i) {
        for (unsigned int j = 0; j < lines[i].size(); ++j) {
            String key = lines[i][j];
            if (!map->count(key))
                key = String("nil");

            // Draw each individual char. Refuse to draw char if it goes out of bounds
            cx = x + font->get_width(lines[i].substr(0, j));
            cy = y + FONT_HEIGHT * i * spacing;

            // Alignment:
            if (align == TexterFont::Middle)
                cx -= font->get_width(lines[i]) / 2;
            else if (align == TexterFont::Right)
                cx -= font->get_width(lines[i]);

            if (cx < 0 || cy < 0 || cx + (*map)[key]->sizeX >= XRES || cy + FONT_HEIGHT >= YRES)
                break;

            // Descender
            cy += (*map)[key]->descender;

            for (unsigned short k = 0; k < (*map)[key]->sizeX; ++k)
            for (unsigned short l = 0; l < (*map)[key]->sizeY; ++l) {
                if ((*map)[key]->chars[(*map)[key]->sizeX * l + k]) {
                    if (g)
                        g->drawrect(cx + k, cy + l, 1, 1, PIXR(color_or_element), PIXG(color_or_element), PIXB(color_or_element), 255);
                    else
                        sim->create_part(-1, cx + k, cy + l, color_or_element);
                }
            }
        }

        // Font will be cut off
        if (cy + FONT_HEIGHT > YRES)
            break;
    }
}

#endif