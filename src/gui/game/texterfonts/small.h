#ifndef SMALL_TEXTER_FONT_H
#define SMALL_TEXTER_FONT_H

// All credit for the font goes to Schicko
// https://starcatcher.us/scripts?view=53

#include "gui/game/texterfonts/font.h"
#include <map> // There's no hash for String and I don't feel like writing one

#define FONT_HEIGHT 4

namespace SMALLFONT {

class Small : public TexterFont {
public:
    Small() : TexterFont(String("4x4 Block"), FONT_HEIGHT){};
    virtual int get_width(const String &text) override;
    virtual void draw_graphic(const std::vector<String> &lines, int align, float spacing, int x, int y, Graphics *g, int color) override;
    virtual void draw_sim(const std::vector<String> &lines, int align, float spacing, int element, int x, int y, Simulation *sim) override;
};

std::map<String, FontCharacter *> font_map;
std::map<String, FontCharacter *> *get_font_map() {
    if (font_map.size())
        return &font_map;
    font_map[String(" ")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), 4, 4, 0);
    font_map[String("nil")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1}), 4, 4, 0);
    font_map[String("1")] = new FontCharacter(std::vector<int>({0, 28, 0, 28, 28, 0, 0, 28, 0, 28, 28, 28}), 3, 4, 0);
    font_map[String("0")] = new FontCharacter(std::vector<int>({0, 28, 28, 0, 28, 28, 0, 28, 28, 0, 28, 28, 0, 28, 28, 0}), 4, 4, 0);
    font_map[String("3")] = new FontCharacter(std::vector<int>({28, 28, 28, 0,28, 0, 0, 0, 28, 28, 28, 0}), 3, 4, 0);
    font_map[String("2")] = new FontCharacter(std::vector<int>({28, 28, 0, 0, 0, 28, 0, 28, 0, 28, 28, 28}), 3, 4, 0);
    font_map[String("5")] = new FontCharacter(std::vector<int>({28, 28, 28, 28, 28, 0, 0, 0, 28, 28, 28, 0}), 3, 4, 0);
    font_map[String("4")] = new FontCharacter(std::vector<int>({28, 0, 28, 0, 28, 0, 28, 0, 28, 28, 28, 28, 0, 0, 28, 0}), 4, 4, 0);
    font_map[String("7")] = new FontCharacter(std::vector<int>({28, 28, 28, 0,0, 28, 0, 28, 0, 0, 28, 0}), 3, 4, 0);
    font_map[String("6")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 28, 28, 28, 0, 28, 28, 28, 28}), 3, 4, 0);
    font_map[String("9")] = new FontCharacter(std::vector<int>({28, 28, 28, 28, 0, 28, 28, 28, 28, 0, 0, 28}), 3, 4, 0);
    font_map[String("8")] = new FontCharacter(std::vector<int>({0, 28, 28, 0, 0, 28, 28, 0, 28, 0, 0, 28, 0, 28, 28, 0}), 4, 4, 0);
    font_map[String("A")] = new FontCharacter(std::vector<int>({0, 28, 0, 28, 0, 28, 28, 28, 28, 28, 0, 28}), 3, 4, 0);
    font_map[String("C")] = new FontCharacter(std::vector<int>({0, 28, 28, 28,0, 0, 28, 0, 0, 0, 28, 28}), 3, 4, 0);
    font_map[String("B")] = new FontCharacter(std::vector<int>({28, 28, 0, 28,28, 0, 28, 0, 28, 28, 28, 0}), 3, 4, 0);
    font_map[String("E")] = new FontCharacter(std::vector<int>({28, 28, 28, 28, 28, 0, 28, 0, 0, 28, 28, 28}), 3, 4, 0);
    font_map[String("D")] = new FontCharacter(std::vector<int>({28, 28, 0, 28,0, 28, 28, 0, 28, 28, 28, 0}), 3, 4, 0);
    font_map[String("G")] = new FontCharacter(std::vector<int>({0, 28, 28, 28,0, 0, 28, 0, 28, 0, 28, 28}), 3, 4, 0);
    font_map[String("F")] = new FontCharacter(std::vector<int>({28, 28, 28, 28, 0, 0, 28, 28, 0, 28, 0, 0}), 3, 4, 0);
    font_map[String("I")] = new FontCharacter(std::vector<int>({28, 28, 28, 0,28, 0, 0, 28, 0, 28, 28, 28}), 3, 4, 0);
    font_map[String("H")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 28, 28, 28, 28, 28, 0, 0, 28, 28, 0, 0, 28}), 4, 4, 0);
    font_map[String("K")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 28, 28, 28, 0, 28, 0, 0, 28, 28, 0, 0, 28}), 4, 4, 0);
    font_map[String("J")] = new FontCharacter(std::vector<int>({28, 28, 28, 0,0, 28, 0, 0, 28, 28, 28, 0}), 3, 4, 0);
    font_map[String("M")] = new FontCharacter(std::vector<int>({28, 28, 0, 28,28, 0, 28, 28, 28, 0, 0, 28, 28, 0, 0, 28}), 4, 4, 0);
    font_map[String("L")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 0, 0, 28, 0, 0, 28, 28, 28}), 3, 4, 0);
    font_map[String("O")] = new FontCharacter(std::vector<int>({0, 28, 28, 0, 28, 0, 0, 28, 28, 0, 0, 28, 0, 28, 28, 0}), 4, 4, 0);
    font_map[String("N")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 28, 28, 0, 28, 28, 0, 28, 28, 28, 0, 0, 28}), 4, 4, 0);
    font_map[String("Q")] = new FontCharacter(std::vector<int>({0, 28, 28, 0, 28, 0, 0, 28, 28, 0, 28, 28, 0, 28, 28, 28}), 4, 4, 0);
    font_map[String("P")] = new FontCharacter(std::vector<int>({28, 28, 0, 28,0, 28, 28, 28, 0, 28, 0, 0}), 3, 4, 0);
    font_map[String("S")] = new FontCharacter(std::vector<int>({0, 28, 28, 28,0, 0, 0, 0, 28, 28, 28, 0}), 3, 4, 0);
    font_map[String("R")] = new FontCharacter(std::vector<int>({28, 28, 0, 28,0, 28, 28, 28, 0, 28, 0, 28}), 3, 4, 0);
    font_map[String("U")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 28, 0, 0, 28, 28, 0, 0, 28, 0, 28, 28, 28}), 4, 4, 0);
    font_map[String("T")] = new FontCharacter(std::vector<int>({28, 28, 28, 0,28, 0, 0, 28, 0, 0, 28, 0}), 3, 4, 0);
    font_map[String("W")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 28, 0, 0, 28, 28, 28, 28, 28, 28, 0, 28, 28}), 4, 4, 0);
    font_map[String("V")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 28, 0, 0, 28, 28, 0, 0, 28, 0, 28, 28, 0}), 4, 4, 0);
    font_map[String("Y")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 0, 28, 28, 28, 0, 0, 0, 28, 0, 28, 28, 0}), 4, 4, 0);
    font_map[String("X")] = new FontCharacter(std::vector<int>({28, 0, 0, 28, 0, 28, 28, 0, 0, 28, 28, 0, 28, 0, 0, 28}), 4, 4, 0);
    font_map[String("Z")] = new FontCharacter(std::vector<int>({28, 28, 28, 28, 0, 0, 28, 0, 0, 28, 0, 0, 28, 28, 28, 28}), 4, 4, 0);
    font_map[String("a")] = font_map[String("A")];
    font_map[String("b")] = font_map[String("B")];
    font_map[String("c")] = font_map[String("C")];
    font_map[String("d")] = font_map[String("D")];
    font_map[String("e")] = font_map[String("E")];
    font_map[String("f")] = font_map[String("F")];
    font_map[String("g")] = font_map[String("G")];
    font_map[String("h")] = font_map[String("H")];
    font_map[String("i")] = font_map[String("I")];
    font_map[String("j")] = font_map[String("J")];
    font_map[String("k")] = font_map[String("K")];
    font_map[String("l")] = font_map[String("L")];
    font_map[String("m")] = font_map[String("M")];
    font_map[String("n")] = font_map[String("N")];
    font_map[String("o")] = font_map[String("O")];
    font_map[String("p")] = font_map[String("P")];
    font_map[String("q")] = font_map[String("Q")];
    font_map[String("r")] = font_map[String("R")];
    font_map[String("s")] = font_map[String("S")];
    font_map[String("t")] = font_map[String("T")];
    font_map[String("u")] = font_map[String("U")];
    font_map[String("v")] = font_map[String("V")];
    font_map[String("w")] = font_map[String("W")];
    font_map[String("x")] = font_map[String("X")];
    font_map[String("y")] = font_map[String("Y")];
    font_map[String("z")] = font_map[String("Z")];
    return &font_map;
}

int Small::get_width(const String &s) {
    int size = 0;
    for (unsigned int i = 0; i < s.size(); ++i) {
        size += get_font_map()->count(s[i]) ?
            (*get_font_map())[s[i]]->sizeX :
            (*get_font_map())[String("nil")]->sizeX;
    }
    return size + 1 * s.size(); // 1 * s.size() is 1px letter spacing
}

void Small::draw_graphic(const std::vector<String> &lines, int align, float spacing, int x, int y, Graphics *g, int color) {
    render_text(lines, align, spacing, x, y, g, nullptr, color, FONT_HEIGHT, this, get_font_map());
}

void Small::draw_sim(const std::vector<String> &lines, int align, float spacing, int element, int x, int y, Simulation *sim) {
    render_text(lines, align, spacing, x, y, nullptr, sim, element, FONT_HEIGHT, this, get_font_map());
}

};
#undef FONT_HEIGHT
#endif
