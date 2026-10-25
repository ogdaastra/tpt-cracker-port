#ifndef JOHNNYOU_TEXTER_FONT_H
#define JOHNNYOU_TEXTER_FONT_H

// All credit for the font goes to Johnnyou
// https://starcatcher.us/scripts?view=141

#include "gui/game/texterfonts/font.h"
#include <map>  // There's no hash for String and I don't feel like writing one

namespace JOHHNYYOUFONT {

#define FONT_WIDTH 3
#define FONT_HEIGHT 5

class JohnnyouMono : public TexterFont {
public:
    JohnnyouMono(): TexterFont(String("3x5 Mono"), FONT_HEIGHT) {};
    virtual int get_width(const String &text) override;
    virtual void draw_graphic(const std::vector<String> &lines, int align, float spacing, int x, int y, Graphics * g, int color) override;
    virtual void draw_sim(const std::vector<String> &lines, int align, float spacing, int element, int x, int y, Simulation * sim) override;
};

std::map<String, FontCharacter*> font_map;
std::map<String, FontCharacter*> * get_font_map() {
    if (font_map.size())
        return &font_map;
    font_map[String("a")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 42, 42, 42, 0, 42, 0, 42, 42}), 3, 5);
    font_map[String("`")] = new FontCharacter(std::vector<int>({0, 42, 0, 0, 0, 42, 0, 0, 0, 0, 0, 0, 0, 0, 0}), 3, 5);
    font_map[String("c")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 42, 42, 42, 0, 0, 0, 42, 42}), 3, 5);
    font_map[String("b")] = new FontCharacter(std::vector<int>({42, 0, 0, 42, 0, 0, 42, 42, 0, 42, 0, 42, 42, 42, 0}), 3, 5);
    font_map[String("e")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 42, 42, 42, 0, 0, 42, 42, 42}), 3, 5);
    font_map[String("d")] = new FontCharacter(std::vector<int>({0, 0, 42, 0, 0, 42, 0, 42, 42, 42, 0, 42, 0, 42, 42}), 3, 5);
    font_map[String("g")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 0, 42, 0, 42, 42, 0, 0, 42, 42, 42, 0}), 3, 5, FONT_HEIGHT / 2);
    font_map[String("f")] = new FontCharacter(std::vector<int>({0, 0, 42, 0, 42, 0, 42, 42, 42, 0, 42, 0, 0, 42, 0}), 3, 5);
    font_map[String("i")] = new FontCharacter(std::vector<int>({0, 42, 0, 0, 0, 0, 0, 42, 0, 0, 42, 0, 0, 42, 0}), 3, 5);
    font_map[String("h")] = new FontCharacter(std::vector<int>({42, 0, 0, 42, 0, 0, 42, 42, 0, 42, 0, 42, 42, 0, 42}), 3, 5);
    font_map[String("k")] = new FontCharacter(std::vector<int>({42, 0, 0, 42, 0, 0, 42, 0, 42, 42, 42, 0, 42, 0, 42}), 3, 5);
    font_map[String("j")] = new FontCharacter(std::vector<int>({0, 42, 0, 0, 0, 0, 42, 42, 0, 0, 42, 0, 42, 0, 0}), 3, 5, FONT_HEIGHT / 2 - 1);
    font_map[String("m")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 42, 42, 42, 42, 42, 42, 42, 0, 42}), 3, 5);
    font_map[String("l")] = new FontCharacter(std::vector<int>({42, 42, 0, 0, 42, 0, 0, 42, 0, 0, 42, 0, 0, 42, 0}), 3, 5);
    font_map[String("o")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 42, 0, 42, 0, 42, 0, 42, 0}), 3, 5);
    font_map[String("n")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 42, 42, 0, 42, 0, 42, 42, 0, 42}), 3, 5);
    font_map[String("q")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 0, 42, 0, 42, 42, 0, 0, 42, 0, 0, 42}), 3, 5, FONT_HEIGHT / 2);
    font_map[String("p")] = new FontCharacter(std::vector<int>({0, 0, 0, 42, 42, 0, 42, 0, 42, 42, 42, 0, 42, 0, 0}), 3, 5, FONT_HEIGHT / 2 - 1);
    font_map[String("s")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 42, 42, 0, 42, 0, 42, 42, 0}), 3, 5);
    font_map[String("r")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 42, 42, 42, 0, 0, 42, 0, 0}), 3, 5);
    font_map[String("u")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 42, 0, 42, 42, 0, 42, 0, 42, 42}), 3, 5);
    font_map[String("t")] = new FontCharacter(std::vector<int>({0, 42, 0, 42, 42, 42, 0, 42, 0, 0, 42, 0, 0, 0, 42}), 3, 5);
    font_map[String("w")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 42, 0, 42, 42, 42, 42, 42, 42, 42}), 3, 5);
    font_map[String("v")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 42, 0, 42, 42, 0, 42, 0, 42, 0}), 3, 5);
    font_map[String("y")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 0, 42, 0, 0, 42, 0, 42, 0, 0}), 3, 5, FONT_HEIGHT / 2);
    font_map[String("x")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 42, 0, 42, 0, 42, 0, 42, 0, 42}), 3, 5);
    font_map[String("{")] = new FontCharacter(std::vector<int>({0, 42, 42, 0, 42, 0, 42, 0, 0, 0, 42, 0, 0, 42, 42}), 3, 5);
    font_map[String("z")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 42, 42, 0, 0, 42, 0, 0, 42, 42}), 3, 5);
    font_map[String("}")] = new FontCharacter(std::vector<int>({42, 42, 0, 0, 42, 0, 0, 0, 42, 0, 42, 0, 42, 42, 0}), 3, 5);
    font_map[String("|")] = new FontCharacter(std::vector<int>({0, 42, 0, 0, 42, 0, 0, 42, 0, 0, 42, 0, 0, 42, 0}), 3, 5);
    font_map[String("~")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 42, 42, 42, 42, 42, 0, 0, 0, 0, 0}), 3, 5);
    font_map[String("(")] = new FontCharacter(std::vector<int>({0, 0, 42, 0, 42, 0, 0, 42, 0, 0, 42, 0, 0, 0, 42}), 3, 5);
    font_map[String("!")] = new FontCharacter(std::vector<int>({0, 42, 0, 0, 42, 0, 0, 42, 0, 0, 0, 0, 0, 42, 0}), 3, 5);
    font_map[String(" ")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), 3, 5);
    font_map[String("#")] = new FontCharacter(std::vector<int>({42, 0, 42, 0, 42, 0, 42, 0, 42, 0, 42, 0, 42, 0, 42}), 3, 5);
    font_map[String("\"")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 0, 0, 0,0, 0, 0, 0, 0, 0}), 3, 5);
    font_map[String("%")] = new FontCharacter(std::vector<int>({42, 0, 42, 0, 0, 42, 0, 42, 0,42, 0, 0, 42, 0, 42}), 3, 5);
    font_map[String("$")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 0, 0, 0, 42, 0,0, 42, 42, 42, 42, 0}), 3, 5);
    font_map[String("'")] = new FontCharacter(std::vector<int>({0, 42, 0, 0, 42, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), 3, 5);
    font_map[String("&")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 42, 0, 0, 42, 0, 42, 42, 0, 0, 42, 42}), 3, 5);
    font_map[String(")")] = new FontCharacter(std::vector<int>({42, 0, 0, 0, 42, 0, 0, 42, 0, 0, 42, 0, 42, 0, 0}), 3, 5);
    font_map[String("nil")] = new FontCharacter(std::vector<int>({42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42}), 3, 5);
    font_map[String("+")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 42, 0, 42, 42, 42,0, 42, 0, 0, 0, 0}), 3, 5);
    font_map[String("*")] = new FontCharacter(std::vector<int>({42, 0, 42, 0, 42, 0, 42, 0, 42, 0, 0, 0, 0, 0, 0}), 3, 5);
    font_map[String("-")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 42, 42, 42, 0, 0, 0, 0, 0, 0}), 3, 5);
    font_map[String(",")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42, 0, 0, 42, 0}), 3, 5);
    font_map[String("/")] = new FontCharacter(std::vector<int>({0, 0, 42, 0, 0, 42, 0, 42, 0, 42, 0, 0, 42, 0, 0}), 3, 5);
    font_map[String(".")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42, 0, 0}), 3, 5);
    font_map[String("1")] = new FontCharacter(std::vector<int>({0, 42, 0, 42, 42, 0, 0, 42, 0,0, 42, 0, 0, 42, 0}), 3, 5);
    font_map[String("0")] = new FontCharacter(std::vector<int>({42, 42, 42, 42, 0, 42, 42, 0, 42, 42, 0, 42, 42, 42, 42}), 3, 5);
    font_map[String("3")] = new FontCharacter(std::vector<int>({42, 42, 0, 0, 0, 42, 0, 42, 0,0, 0, 42, 42, 42, 0}), 3, 5);
    font_map[String("2")] = new FontCharacter(std::vector<int>({42, 42, 0, 0, 0, 42, 0, 42, 0,42, 0, 0, 42, 42, 42}), 3, 5);
    font_map[String("5")] = new FontCharacter(std::vector<int>({42, 42, 42, 42, 0, 0, 0, 42, 0, 0, 0, 42, 42, 42, 0}), 3, 5);
    font_map[String("4")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 42, 42, 42, 0, 0, 42, 0, 0, 42}), 3, 5);
    font_map[String("7")] = new FontCharacter(std::vector<int>({42, 42, 42, 0, 0, 42, 0, 42, 0, 0, 42, 0, 0, 42, 0}), 3, 5);
    font_map[String("6")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 0, 0, 42, 42, 0, 42, 0, 42, 42, 42, 42}), 3, 5);
    font_map[String("9")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 0, 42, 0, 42, 42, 0, 0, 42, 42, 42, 0}), 3, 5);
    font_map[String("8")] = new FontCharacter(std::vector<int>({42, 42, 42, 42, 0, 42, 0, 42, 0, 42, 0, 42, 42, 42, 42}), 3, 5);
    font_map[String(";")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 42, 0, 0, 0, 0, 0,42, 0, 0, 42, 0}), 3, 5);
    font_map[String(":")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 42, 0, 0, 0, 0, 0,42, 0, 0, 0, 0}), 3, 5);
    font_map[String("=")] = new FontCharacter(std::vector<int>({0, 0, 0, 42, 42, 42, 0, 0, 0, 42, 42, 42, 0, 0, 0}), 3, 5);
    font_map[String("<")] = new FontCharacter(std::vector<int>({0, 0, 42, 0, 42, 0, 42, 0, 0, 0, 42, 0, 0, 0, 42}), 3, 5);
    font_map[String("?")] = new FontCharacter(std::vector<int>({42, 42, 0, 0, 0, 42, 0, 42, 0,0, 0, 0, 0, 42, 0}), 3, 5);
    font_map[String(">")] = new FontCharacter(std::vector<int>({42, 0, 0, 0, 42, 0, 0, 0, 42, 0, 42, 0, 42, 0, 0}), 3, 5);
    font_map[String("A")] = new FontCharacter(std::vector<int>({0, 42, 0, 42, 0, 42, 42, 42, 42, 42, 0, 42, 42, 0, 42}), 3, 5);
    font_map[String("@")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 0, 42, 42, 0, 42, 42, 0, 0, 0, 42, 42}), 3, 5);
    font_map[String("C")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 0, 0, 42, 0, 0,42, 0, 0, 0, 42, 42}), 3, 5);
    font_map[String("B")] = new FontCharacter(std::vector<int>({42, 42, 0, 42, 0, 42, 42, 42, 0, 42, 0, 42, 42, 42, 0}), 3, 5);
    font_map[String("E")] = new FontCharacter(std::vector<int>({42, 42, 42, 42, 0, 0, 42, 42, 0, 42, 0, 0, 42, 42, 42}), 3, 5);
    font_map[String("D")] = new FontCharacter(std::vector<int>({42, 42, 0, 42, 0, 42, 42, 0, 42, 42, 0, 42, 42, 42, 0}), 3, 5);
    font_map[String("G")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 0, 0, 42, 0, 42, 42, 0, 42, 0, 42, 42}), 3, 5);
    font_map[String("F")] = new FontCharacter(std::vector<int>({42, 42, 42, 42, 0, 0, 42, 42, 0, 42, 0, 0, 42, 0, 0}), 3, 5);
    font_map[String("I")] = new FontCharacter(std::vector<int>({42, 42, 42, 0, 42, 0, 0, 42, 0, 0, 42, 0, 42, 42, 42}), 3, 5);
    font_map[String("H")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 42, 42, 42, 42, 0, 42, 42, 0, 42}), 3, 5);
    font_map[String("K")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 42, 42, 0, 42, 0, 42, 42, 0, 42}), 3, 5);
    font_map[String("J")] = new FontCharacter(std::vector<int>({42, 42, 42, 0, 42, 0, 0, 42, 0, 0, 42, 0, 42, 0, 0}), 3, 5);
    font_map[String("M")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 42, 42, 42, 0, 42, 42, 0, 42, 42, 0, 42}), 3, 5);
    font_map[String("L")] = new FontCharacter(std::vector<int>({42, 0, 0, 42, 0, 0, 42, 0, 0, 42, 0, 0, 42, 42, 42}), 3, 5);
    font_map[String("O")] = new FontCharacter(std::vector<int>({0, 42, 0, 42, 0, 42, 42, 0, 42, 42, 0, 42, 0, 42, 0}), 3, 5);
    font_map[String("N")] = new FontCharacter(std::vector<int>({42, 42, 0, 42, 42, 42, 42, 0, 42, 42, 0, 42, 42, 0, 42}), 3, 5);
    font_map[String("Q")] = new FontCharacter(std::vector<int>({0, 42, 0, 42, 0, 42, 42, 0, 42, 0, 42, 0, 0, 0, 42}), 3, 5);
    font_map[String("P")] = new FontCharacter(std::vector<int>({42, 42, 0, 42, 0, 42, 42, 42, 0, 42, 0, 0, 42, 0, 0}), 3, 5);
    font_map[String("S")] = new FontCharacter(std::vector<int>({0, 42, 42, 42, 0, 0, 0, 42, 0,0, 0, 42, 42, 42, 0}), 3, 5);
    font_map[String("R")] = new FontCharacter(std::vector<int>({42, 42, 0, 42, 0, 42, 42, 42, 0, 42, 0, 42, 42, 0, 42}), 3, 5);
    font_map[String("U")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 42, 0, 42, 42, 0, 42, 0, 42, 42}), 3, 5);
    font_map[String("T")] = new FontCharacter(std::vector<int>({42, 42, 42, 0, 42, 0, 0, 42, 0, 0, 42, 0, 0, 42, 0}), 3, 5);
    font_map[String("W")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 42, 0, 42, 42, 42, 42, 42, 0, 42}), 3, 5);
    font_map[String("V")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 42, 0, 42, 42, 0, 42, 0, 42, 0}), 3, 5);
    font_map[String("Y")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 0, 42, 0, 0, 42, 0, 0, 42, 0}), 3, 5);
    font_map[String("X")] = new FontCharacter(std::vector<int>({42, 0, 42, 42, 0, 42, 0, 42, 0, 42, 0, 42, 42, 0, 42}), 3, 5);
    font_map[String("[")] = new FontCharacter(std::vector<int>({0, 42, 42, 0, 42, 0, 0, 42, 0,0, 42, 0, 0, 42, 42}), 3, 5);
    font_map[String("Z")] = new FontCharacter(std::vector<int>({42, 42, 42, 0, 0, 42, 0, 42, 0, 42, 0, 0, 42, 42, 42}), 3, 5);
    font_map[String("]")] = new FontCharacter(std::vector<int>({42, 42, 0, 0, 42, 0, 0, 42, 0,0, 42, 0, 42, 42, 0}), 3, 5);
    font_map[String("\\")] = new FontCharacter(std::vector<int>({42, 0, 0, 42, 0, 0, 0, 42, 0, 0, 0, 42, 0, 0, 42}), 3, 5);
    font_map[String("_")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 0, 0, 42, 42, 42, 0, 0, 0, 0, 0, 0}), 3, 5);
    font_map[String("^")] = new FontCharacter(std::vector<int>({0, 42, 0, 42, 0, 42, 0, 0, 0, 0, 0, 0, 0, 0, 0}), 3, 5);

    return &font_map;
}

int JohnnyouMono::get_width(const String &s) {
    return s.size() * FONT_WIDTH + s.size(); // s.size() is 1px letter spacing
}

void JohnnyouMono::draw_graphic(const std::vector<String> &lines, int align, float spacing, int x, int y, Graphics * g, int color) {
    render_text(lines, align, spacing, x, y, g, nullptr, color, FONT_HEIGHT, this, get_font_map());
}

void JohnnyouMono::draw_sim(const std::vector<String> &lines, int align, float spacing, int element, int x, int y, Simulation * sim) {
    render_text(lines, align, spacing, x, y, nullptr, sim, element, FONT_HEIGHT, this, get_font_map());
}

};
#undef FONT_WIDTH
#undef FONT_HEIGHT
#endif