#ifndef CONFIG_TOOL_UTIL_H
#define CONFIG_TOOL_UTIL_H

#include "gui/Style.h"
#include "gui/game/GameModel.h"
#include "gui/interface/Button.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/Window.h"
#include "simulation/ElementCommon.h"

const int FONT_H = 10; // TODO: what was this

// Collection of util for config tool
/* Pre aligned elements:
 * make_left_textbox(same args as ui::Textbox constructor)
 *      textbox, but left aligned
 * make_left_label(same args as ui::Label constructor)
 *      label, but left aligned
 * make_center_button(same args as ui::Button constructor)
 *      Button, but center aligned, used for the OK and cancel buttons
 * 
 * align_window_near(ui::Point &Position, const ui::Point &partpos, const ui::Point &Size)
 * align_window_far(ui::Point &Position, const ui::Point &partpos, const ui::Point &Size)
 * 
 * Align the window either near the element clicked or the corner furthest away. Will
 * try to avoid going off the screen.
 * 
 * LIGHT_GRAY_TEXT - ui::Colour constant for light gray text
 * RED_TEXT        - ui::Colour constant for reddish warning text
 */
/* Setting callbacks
 * CANCEL_BUTTON  : convert a new ui::Button into the window's cancel button
 * PR_PHOTONS_CHECK  : 
 *                  Checks an int called pr (parts[ID(r)] and TYP(pr))
 *                  If none, sets it to photons, if none still (should never happen)
 *                  does nothing
 * NOT_NEAR_WINDOW_BOTTOM_CHECK :
 *                  Returns if near the bottom of the window, use when exiting
 * 
 * INTEGER_INPUT  : Whenever input updates and the input is a valid int, writes value to val
 *                  Turns red when input text is invalid
 * FLOAT_INPUT    : Like integer, but for float
 * HEXDECIMAL_INPUT : 
 *                  Accept a hex number like 0xFF00, or a regular number, writes to variable
 *                  Also calls func() after the variable is updated. Turns red if invalid
 * ELEMENT_INPUT  : Allows element names (like TTAN) or IDs, turns red if invalid
 * TEMPERATURE_INPUT : Allows decimal temperatures, optionally with C, K or F suffix (at most 1 extra space,
 *                  so, for example, "5  C" won't work but "5 C" and "5C" will)
 */

#define CANCEL_BUTTON(btn) btn->SetActionCallback({[this] { \
        CloseActiveWindow(); \
        SelfDestruct(); }}); \
        SetCancelButton(btn);
#define PR_PHOTONS_CHECK() \
    if (!pr) { pr = sim->photons[pos.Y][pos.X]; }
#define NOT_NEAR_WINDOW_BOTTOM_CHECK() \
    if (ui::Engine::Ref().GetMouseY() < Position.Y + Size.Y + 20 && \
        ui::Engine::Ref().GetMouseX() > Position.X - 20 && ui::Engine::Ref().GetMouseX() < Position.X + 20 + Size.X) \
        return;

#define INTEGER_INPUT(input, val) input->SetInputType(ui::Textbox::Number); \
    input->SetActionCallback({[this] { \
    try { \
        input->SetTextColour(ui::Colour(255, 255, 255)); \
        val = input->GetText().ToNumber<int>(); \
    }  \
    catch(...) { input->SetTextColour(ui::Colour(255, 100, 0));  }}});
#define FLOAT_INPUT(input, val) input->SetActionCallback({[this] { \
    try { \
        input->SetTextColour(ui::Colour(255, 255, 255)); \
        val = input->GetText().ToNumber<float>(); \
    }  \
    catch(...) { input->SetTextColour(ui::Colour(255, 100, 0));  }}});
#define HEXDECIMAL_INPUT(input, val, func) input->SetActionCallback({ [this] { \
        try { \
            input->SetTextColour(ui::Colour(255, 255, 255)); \
            val = input->GetText().BeginsWith("0x") ? \
                input->GetText().ToNumber<unsigned int>(Format::Hex(), false) : \
                input->GetText().ToNumber<unsigned int>(false); \
            func(); \
        } catch (...) { \
            input->SetTextColour(ui::Colour(255, 100, 0)); } \
    }});
#define ELEMENT_INPUT(input, val) input->SetActionCallback({ [this] { \
        int v = sim->GetParticleType(input->GetText().ToUtf8()); \
        if (v != -1) { val = v; } \
        else { v = input->GetText().ToNumber<int>(true); } \
        if (v > 0 && v < PT_NUM) { \
            input->SetTextColour(ui::Colour(255, 255, 255)); \
            val = v; \
        } else { \
            input->SetTextColour(ui::Colour(255, 100, 0)); \
        } \
    }});
#define TEMPERATURE_INPUT(input, val) input->SetActionCallback({ [this] { \
		try { \
			input->SetTextColour(ui::Colour(255, 255, 255)); \
            if (input->GetText() == String("0")) { /* Exception since 0 is considered an error due to being too short */\
                val = 0.0f; \
            } else if (input->GetText().size() >= 3 && input->GetText()[input->GetText().size() - 2] == ' ') { \
                if (input->GetText().EndsWith(" C")) { \
                    val = input->GetText().Substr(0, input->GetText().size() - 2).ToNumber<float>() + 273.15f; \
                } else if (input->GetText().EndsWith(" F")) { \
                    val = (input->GetText().Substr(0, input->GetText().size() - 2).ToNumber<float>() - 32.0f) * 5.0f / 9.0f + 273.15f; \
                } else if (input->GetText().EndsWith(" K")) { \
                    val = input->GetText().Substr(0, input->GetText().size() - 2).ToNumber<float>(); \
                } else { \
                    val = input->GetText().ToNumber<float>(); \
                } \
            } else if (input->GetText().EndsWith("C")) { \
				val = input->GetText().Substr(0, input->GetText().size() - 1).ToNumber<float>() + 273.15f; \
            } else if (input->GetText().EndsWith("F")) { \
				val = (input->GetText().Substr(0, input->GetText().size() - 1).ToNumber<float>() - 32.0f) * 5.0f / 9.0f + 273.15f; \
            } else if (input->GetText().EndsWith("K")) { \
				val = input->GetText().Substr(0, input->GetText().size() - 1).ToNumber<float>(); \
            } else { \
				val = input->GetText().ToNumber<float>(); \
            } \
		} catch(...) { \
			input->SetTextColour(ui::Colour(255, 100, 0)); \
		} \
	}});

// Text colors:
const ui::Colour LIGHT_GRAY_TEXT(200, 200, 200);
const ui::Colour RED_TEXT(255, 100, 0);

// Creation of pre-aligned components
inline ui::Label *make_left_label(ui::Point pos, ui::Point size, const String &text) {
    ui::Label * tempLabel = new ui::Label(pos, size, text);
    tempLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
    tempLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
    return tempLabel;
}

inline ui::Textbox * make_left_textbox(ui::Point pos, ui::Point size, const String &val="", const String &placeholder="") {
    ui::Textbox * t = new ui::Textbox(pos, size, val, placeholder);
    t->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
    t->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
    return t;
}

inline ui::Button * make_center_button(const ui::Point &pos, const ui::Point &size, const String &text="") {
    ui::Button * btn = new ui::Button(pos, size, text);
    btn->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
    btn->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
    btn->Appearance.BorderInactive = (ui::Colour(200, 200, 200));
    return btn;
}

// Align windows
inline void align_window_near(ui::Point &Position, const ui::Point &partpos, const ui::Point &Size) {
    Position.X = partpos.X - Size.X / 2; // Default: center align x
    if (partpos.X < XRES / 4)
        Position.X = partpos.X + 10;
    else if (partpos.X > 3 * XRES / 4)
        Position.X = partpos.X - Size.X - 10;
    Position.Y = partpos.Y < YRES / 2 ? partpos.Y + 10 : partpos.Y - 10 - Size.Y; // Align Y depending on height
}

inline void align_window_far(ui::Point &Position, const ui::Point &partpos, const ui::Point &Size) {
    if (partpos.X < XRES / 2) Position.X = XRES - Size.X - 10;
	else Position.X = 10;
	if (partpos.Y < YRES / 2) Position.Y = YRES - Size.Y - 10;
	else Position.Y = 10;
}

#endif