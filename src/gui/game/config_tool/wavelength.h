#include "gui/game/Tool.h"

#include "simulation/Simulation.h"

#include "gui/Style.h"
#include "gui/interface/Window.h"
#include "gui/interface/Button.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/DropDown.h"
#include "gui/game/GameModel.h"

#include "graphics/Graphics.h"
#include "gui/interface/Engine.h"
#include "gui/game/config_tool/util.h"

#include "Misc.h"
#include <cmath>
#include <vector>

#define BITS 30

class ConfigWavelengthWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;

    ui::Textbox * ctype;
    ui::DropDown * mode;
    ui::Point pos;

    std::vector<bool> wavelength_bits;
    std::vector<ui::Colour> wavelength_colors;
    int wavelength, filt_mode, ptype;

	ConfigWavelengthWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
	void OnDraw() override;
	void DoDraw() override;
	void DoMouseMove(int x, int y, int dx, int dy) override {
        ui::Window::DoMouseMove(x, y, dx, dy);
    }
	void DoMouseUp(int x, int y, unsigned button) override {
        ui::Window::DoMouseUp(x, y, button);
	}
	void DoMouseWheel(int x, int y, int d) override {
        ui::Window::DoMouseWheel(x, y, d);
	}
	void DoKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt) override {
		ui::Window::DoKeyPress(key, scan, repeat, shift, ctrl, alt);
	}
	void DoKeyRelease(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt) override {
		ui::Window::DoKeyRelease(key, scan, repeat, shift, ctrl, alt);
	}
    void DoMouseDown(int x, int y, unsigned button) override {
        ui::Window::DoMouseDown(x, y, button);
    }
	virtual ~ConfigWavelengthWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside)
           NOT_NEAR_WINDOW_BOTTOM_CHECK()
        CloseActiveWindow();
        SelfDestruct();
    }

    void computeWavelength() {
        if (!wavelength_bits.size()) {
            wavelength_bits.reserve(BITS);
            while (wavelength_bits.size() < BITS)
                wavelength_bits.push_back(false);
        }
        for (unsigned int bit = 0; bit < BITS; ++bit)
            wavelength_bits[BITS - bit - 1] = (1 << bit) & wavelength;
    }
};

ConfigWavelengthWindow::ConfigWavelengthWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(ui::Point(-1, -1), ui::Point(320, 90)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	ui::Label * messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X-8, 15), "Config Wavelength");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    int pr = sim->pmap[pos.Y][pos.X];
    PR_PHOTONS_CHECK()
    int pid = ID(pr);
    ptype = TYP(pr);

    // Load particle data
    wavelength = sim->parts[pid].ctype;
    filt_mode = sim->parts[pid].tmp;

    // Generate the bits
    int r = 255, g = 0, b = 0;
    for (unsigned int i = 0; i < BITS; ++i) {
        if (i == 9) r = 255, g = 255, b = 0;
        else if (i == 12) r = 0, g = 255, b = 0;
        else if (i == 18) r = 0, g = 255, b = 255;
        else if (i == 21) r = 0, g = 0, b = 255;
        wavelength_colors.push_back(ui::Colour(r, g, b));
    }
    computeWavelength();

    // Wavelength input
    ctype = make_left_textbox(ui::Point(8, 50), ui::Point(Size.X - 16, 16), "", "0x3FFFFFFF");
    HEXDECIMAL_INPUT(ctype, wavelength, computeWavelength);
    ctype->SetLimit(10);

    StringBuilder wavelength_builder; wavelength_builder << "0x" << Format::Uppercase(Format::Hex(wavelength));
    ctype->SetText(wavelength_builder.Build());
    AddComponent(ctype);

    // FILT: gets extra option
    if (ptype == PT_FILT || ptype == PT_PFLT) {
        // Selector for FILT mode
        ui::Label * tempLabel = make_left_label(ui::Point(8, 75), ui::Point(40, 15), "FILT Operation");
        AddComponent(tempLabel);
        
        mode = new ui::DropDown(ui::Point(8, 91), ui::Point(Size.X - 16, 16));
        mode->AddOption(std::make_pair(String("Set (Default)"), 0));
        mode->AddOption(std::make_pair(String("AND"), 1));
        mode->AddOption(std::make_pair(String("OR"), 2));
        mode->AddOption(std::make_pair(String("SUB"), 3));
        mode->AddOption(std::make_pair(String("Red Shift"), 4));
        mode->AddOption(std::make_pair(String("Blue Shift"), 5));
        mode->AddOption(std::make_pair(String("Nothing"), 6));
        mode->AddOption(std::make_pair(String("XOR"), 7));
        mode->AddOption(std::make_pair(String("NOT"), 8));
        mode->AddOption(std::make_pair(String("Scatter"), 9));
        mode->AddOption(std::make_pair(String("Variable red shift"), 10));
        mode->AddOption(std::make_pair(String("Variable blue shift"), 11));
        mode->SetOption(filt_mode);
        mode->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
        AddComponent(mode);

        Size.Y += 41;
    }

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(0, Size.Y-16), ui::Point(Size.X, 16), "OK");
	okayButton->SetActionCallback({ [this] {
        // Set property
        PropertyValue value1;
        value1.Integer = wavelength;
        sim->flood_prop(pos.X, pos.Y, offsetof(Particle, ctype), value1, StructProperty::Integer);

        if (ptype == PT_FILT || ptype == PT_PFLT) {
            PropertyValue value2;
            value2.Integer = mode->GetOption().second;
            sim->flood_prop(pos.X, pos.Y, offsetof(Particle, tmp), value2, StructProperty::Integer);
        }

        CloseActiveWindow();
		SelfDestruct();
	}});
	AddComponent(okayButton);
	SetOkayButton(okayButton);
	MakeActiveWindow();
}

void ConfigWavelengthWindow::DoDraw() {
	ui::Window::DoDraw();

    // Draw the bits
    Graphics * gr = GetGraphics();
    for (unsigned int i = 0; i < wavelength_bits.size(); ++i) {
        float m = wavelength_bits[i] ? 1.0f : 0.4f;
        gr->fillrect(Position.X + 8 + i * 10, Position.Y + 21, 10, 15,
                     m * wavelength_colors[i].Red, m * wavelength_colors[i].Green, m * wavelength_colors[i].Blue,
                     255);
        gr->drawtext(Position.X + 11 + i * 10, Position.Y + 25, wavelength_bits[i] ? String("1") : String("0"), 0, 0, 0, 255);
    }

    // Get the actual wavelength color and draw below
    int x = 0, r = 0, g = 0, b = 0;
	for (x=0; x<12; x++) {
		r += (wavelength >> (x+18)) & 1;
		b += (wavelength >>  x)     & 1;
        g += (wavelength >> (x+9))  & 1;
	}
	x = 624 / (r + g + b + 1);
	r *= x, g *= x, b *= x;
    if (b > 255) b = 255;
    if (g > 255) g = 255;
    if (r > 255) r = 255;

    gr->fillrect(Position.X + 8, Position.Y + 36, 10 * BITS, 5, r, g, b, 255);
}

void ConfigWavelengthWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}

#undef BITS