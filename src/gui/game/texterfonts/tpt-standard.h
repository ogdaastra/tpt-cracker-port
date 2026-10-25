#ifndef TPTSTANDARD_TEXTER_FONT_H
#define TPTSTANDARD_TEXTER_FONT_H

// All credit for the font goes to byzod
// https://starcatcher.us/scripts?view=52

#include "gui/game/texterfonts/font.h"
#include <map> // There's no hash for String and I don't feel like writing one

#define FONT_HEIGHT 7

namespace TPTSTANDARDFONT {

class TPTStandard : public TexterFont {
public:
    TPTStandard() : TexterFont(String("4x7 TPT"), FONT_HEIGHT){};
    virtual int get_width(const String &text) override;
    virtual void draw_graphic(const std::vector<String> &lines, int align, float spacing, int x, int y, Graphics *g, int color) override;
    virtual void draw_sim(const std::vector<String> &lines, int align, float spacing, int element, int x, int y, Simulation *sim) override;
};

std::map<String, FontCharacter *> font_map;
std::map<String, FontCharacter *> *get_font_map() {
    if (font_map.size())
        return &font_map;
    font_map[String("a")] = new FontCharacter(std::vector<int>({0, 1, 1, 0, 0,0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1}), 4, 5, 2);
    font_map[String("`")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 1,0, 0, 0, 1}), 3, 3, 0);
    font_map[String("c")] = new FontCharacter(std::vector<int>({0, 1, 1, 0, 1,0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0}), 4, 5, 2);
    font_map[String("b")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0}), 4, 7, 0);
    font_map[String("e")] = new FontCharacter(std::vector<int>({0, 1, 1, 0, 1,0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0}), 4, 5, 2);
    font_map[String("d")] = new FontCharacter(std::vector<int>({0, 0, 0, 1, 0,0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1}), 4, 7, 0);
    font_map[String("g")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 1,0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0}), 4, 7, 2);
    font_map[String("f")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 0,0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0}), 3, 7, 0);
    font_map[String("i")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 1,1, 0, 1, 0, 1, 0, 1}), 2, 6, 1);
    font_map[String("h")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}), 4, 7, 0);
    font_map[String("k")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1}), 4, 7, 0);
    font_map[String("j")] = new FontCharacter(std::vector<int>({0, 0, 1, 0, 0,0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0}), 3, 8, 1);
    font_map[String("m")] = new FontCharacter(std::vector<int>({1, 1, 0, 1, 0,1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1}), 5, 5, 2);
    font_map[String("l")] = new FontCharacter(std::vector<int>({1, 0, 0, 1, 0,0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1}), 3, 7, 0);
    font_map[String("o")] = new FontCharacter(std::vector<int>({0, 1, 1, 0, 1,0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0}), 4, 5, 2);
    font_map[String("n")] = new FontCharacter(std::vector<int>({1, 1, 1, 0, 1,0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}), 4, 5, 2);
    font_map[String("q")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 1,0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1}), 4, 7, 2);
    font_map[String("p")] = new FontCharacter(std::vector<int>({1, 1, 1, 0, 1,0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0}), 4, 7, 2);
    font_map[String("s")] = new FontCharacter(std::vector<int>({0, 1, 1, 0, 1,0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0}), 4, 5, 2);
    font_map[String("r")] = new FontCharacter(std::vector<int>({1, 0, 1, 1, 1,0, 1, 0, 0, 1, 0, 0, 1, 0, 0}), 3, 5, 2);
    font_map[String("u")] = new FontCharacter(std::vector<int>({1, 0, 0, 1, 1,0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1}), 4, 5, 2);
    font_map[String("t")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 1,0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1}), 3, 7, 0);
    font_map[String("w")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}), 5, 5, 2);
    font_map[String("v")] = new FontCharacter(std::vector<int>({1, 0, 0, 1, 1,0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0}), 4, 5, 2);
    font_map[String("y")] = new FontCharacter(std::vector<int>({1, 0, 0, 1, 1,0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0}), 4, 7, 2);
    font_map[String("x")] = new FontCharacter(std::vector<int>({1, 0, 0, 1, 1,0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1}), 4, 5, 2);
    font_map[String("{")] = new FontCharacter(std::vector<int>({0, 0, 1, 0, 1,0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1}), 3, 7, 0);
    font_map[String("z")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 0,0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1}), 4, 5, 2);
    font_map[String("}")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0}), 3, 7, 0);
    font_map[String("|")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 1,0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}), 3, 7, 0);
    font_map[String("~")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 0,1, 0, 1, 0, 1, 0, 0, 0, 1, 0}), 5, 3, 2);
    font_map[String("(")] = new FontCharacter(std::vector<int>({0, 0, 1, 0, 1,0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1}), 3, 7, 0);
    font_map[String("!")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 1,0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0}), 3, 7, 0);
    font_map[String(" ")] = new FontCharacter(std::vector<int>({0, 0, 0, 0}), 4, 1, 0);
    font_map[String("#")] = new FontCharacter(std::vector<int>({0, 1, 0, 1, 0,0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0,0, 1, 0, 1, 0}), 5, 7, 0);
    font_map[String("\"")] = new FontCharacter(std::vector<int>({0, 1, 0, 1, 0,1, 0, 1, 1, 0, 1, 0}), 4, 3, 0);
    font_map[String("%")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 0,1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0,0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0}), 6, 7, 0);
    font_map[String("$")] = new FontCharacter(std::vector<int>({0, 0, 1, 0, 0,0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1,1, 1, 1, 1, 0, 0, 0, 1, 0, 0}), 5, 8, 0);
    font_map[String("'")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 1,0, 1, 0, 0}), 3, 3, 0);
    font_map[String("&")] = new FontCharacter(std::vector<int>({0, 1, 1, 0, 0,1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0,0, 1, 1, 0, 1}), 5, 7, 0);
    font_map[String(")")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0}), 3, 7, 0);
    font_map[String("nil")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}), 5, 5, 0);
    font_map[String("+")] = new FontCharacter(std::vector<int>({0, 0, 1, 0, 0,0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}), 5, 5, 1);
    font_map[String("*")] = new FontCharacter(std::vector<int>({0, 1, 0, 1, 0,0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0}), 5, 5, 0);
    font_map[String("-")] = new FontCharacter(std::vector<int>({1, 1, 1, 1}), 4, 1, 3);
    font_map[String(",")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 1,0, 1, 0, 0}), 3, 3, 6);
    font_map[String("/")] = new FontCharacter(std::vector<int>({0, 0, 0, 0, 1,0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,1, 0, 0, 0, 0}), 5, 7, 0);
    font_map[String(".")] = new FontCharacter(std::vector<int>({1, 0}), 2, 1, 6);
    font_map[String("1")] = new FontCharacter(std::vector<int>({0, 0, 1, 0, 0,1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0}), 4, 7, 0);
    font_map[String("0")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 0,1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1,0, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("3")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 0,1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1,0, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("2")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 0,1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0,1, 1, 1, 1, 1}), 5, 7, 0);
    font_map[String("5")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1,1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,1, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("4")] = new FontCharacter(std::vector<int>({0, 0, 0, 1, 1,0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,0, 0, 0, 0, 1}), 5, 7, 0);
    font_map[String("7")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1,0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,0, 1, 0, 0, 0}), 5, 7, 0);
    font_map[String("6")] = new FontCharacter(std::vector<int>({0, 0, 1, 1, 1,0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,0, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("9")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 0,1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1,0, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("8")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 0,1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,0, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String(";")] = new FontCharacter(std::vector<int>({0, 1, 0, 0, 0,0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0}), 3, 6, 3);
    font_map[String(":")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 0,0, 0, 0, 1, 0}), 2, 5, 2);
    font_map[String("=")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1,0, 0, 0, 0, 0, 1, 1, 1, 1, 1}), 5, 3, 3);
    font_map[String("<")] = new FontCharacter(std::vector<int>({0, 0, 0, 1, 1,0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1}), 5, 5, 2);
    font_map[String("?")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 0,1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,0, 0, 1, 0, 0}), 5, 7, 0);
    font_map[String(">")] = new FontCharacter(std::vector<int>({1, 1, 0, 0, 0,0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0}), 5, 5, 2);
    font_map[String("A")] = new FontCharacter(std::vector<int>({0, 0, 1, 0, 0,0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,1, 0, 0, 0, 1}), 5, 7, 0);
    font_map[String("@")] = new FontCharacter(std::vector<int>({0, 0, 1, 1, 1,1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0,1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0}), 7, 7, 0);
    font_map[String("C")] = new FontCharacter(std::vector<int>({0, 0, 1, 1, 0,0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1,0, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("B")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 0,1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,1, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("E")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1,1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,1, 1, 1, 1, 1}), 5, 7, 0);
    font_map[String("D")] = new FontCharacter(std::vector<int>({1, 1, 1, 0, 0,1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,1, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("G")] = new FontCharacter(std::vector<int>({0, 0, 1, 1, 0,0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,0, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("F")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1,1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,1, 0, 0, 0, 0}), 5, 7, 0);
    font_map[String("I")] = new FontCharacter(std::vector<int>({1, 1, 1, 0, 1,0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1}), 3, 7, 0);
    font_map[String("H")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1,1, 0, 0, 0, 1}), 5, 7, 0);
    font_map[String("K")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0,1, 0, 0, 0, 1}), 5, 7, 0);
    font_map[String("J")] = new FontCharacter(std::vector<int>({0, 0, 1, 1, 1,0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,1, 1, 1, 0, 0}), 5, 7, 0);
    font_map[String("M")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 0,0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0,0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1}), 7, 7, 0);
    font_map[String("L")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 0,1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,1, 1, 1, 1, 1}), 5, 7, 0);
    font_map[String("O")] = new FontCharacter(std::vector<int>({0, 0, 1, 1, 0,0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1,1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0}), 6, 7, 0);
    font_map[String("N")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 0,1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 1,1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1}), 6, 7, 0);
    font_map[String("Q")] = new FontCharacter(std::vector<int>({0, 0, 1, 1, 0,0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1,1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 1}), 6, 7, 0);
    font_map[String("P")] = new FontCharacter(std::vector<int>({1, 1, 1, 0, 0,1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0,1, 0, 0, 0, 0}), 5, 7, 0);
    font_map[String("S")] = new FontCharacter(std::vector<int>({0, 1, 1, 1, 0,1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1,0, 1, 1, 1, 0}), 5, 7, 0);
    font_map[String("R")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 0,1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1,1, 0, 0, 0, 1}), 5, 7, 0);
    font_map[String("U")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 0,1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1,1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0}), 6, 7, 0);
    font_map[String("T")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1,0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0,0, 0, 1, 0, 0}), 5, 7, 0);
    font_map[String("W")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 0,0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1,1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0}), 7, 7, 0);
    font_map[String("V")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0,0, 0, 1, 0, 0}), 5, 7, 0);
    font_map[String("Y")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 1,1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0,0, 0, 1, 0, 0}), 5, 7, 0);
    font_map[String("X")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 0,1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0,0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1}), 6, 7, 0);
    font_map[String("[")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 0,0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1}), 3, 7, 0);
    font_map[String("Z")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1,1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}), 6, 7, 0);
    font_map[String("]")] = new FontCharacter(std::vector<int>({1, 1, 1, 0, 0,1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1}), 3, 7, 0);
    font_map[String("\\")] = new FontCharacter(std::vector<int>({1, 0, 0, 0, 0,0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,0, 0, 0, 0, 1}), 5, 7, 0);
    font_map[String("_")] = new FontCharacter(std::vector<int>({1, 1, 1, 1, 1}), 5, 1, 6);
    font_map[String("^")] = new FontCharacter(std::vector<int>({0, 1, 0, 1, 0, 1}), 3, 2, 0);

    return &font_map;
}

int TPTStandard::get_width(const String &s) {
    int size = 0;
    for (unsigned int i = 0; i < s.size(); ++i) {
        size += get_font_map()->count(s[i]) ?
            (*get_font_map())[s[i]]->sizeX :
            (*get_font_map())[String("nil")]->sizeX;
    }
    return size + 1.2f * s.size(); // 1.2 * s.size() is 1.2px letter spacing
}

void TPTStandard::draw_graphic(const std::vector<String> &lines, int align, float spacing, int x, int y, Graphics *g, int color) {
    render_text(lines, align, spacing, x, y, g, nullptr, color, FONT_HEIGHT, this, get_font_map());
}

void TPTStandard::draw_sim(const std::vector<String> &lines, int align, float spacing, int element, int x, int y, Simulation *sim) {
    render_text(lines, align, spacing, x, y, nullptr, sim, element, FONT_HEIGHT, this, get_font_map());
}

};
#undef FONT_HEIGHT
#endif
