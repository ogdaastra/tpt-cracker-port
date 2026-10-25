#include "gui/game/Tool.h"

#include "simulation/Simulation.h"
#include "simulation/ElementCommon.h"

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

class ConfigCLUDWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
	ui::Point pos;
	ui::Textbox * temp;
    ui::DropDown * ctype;
	ui::Label * messageLabel;

	int pid;
	float temp_val = 0.0f;

	ConfigCLUDWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
    void DoDraw() override {
        ui::Window::DoDraw();
    }
    void DoMouseMove(int x, int y, int dx, int dy) override {
        ui::Window::DoMouseMove(x, y, dx, dy);
    }
    void DoMouseDown(int x, int y, unsigned button) override {
        ui::Window::DoMouseDown(x, y, button);
    }
    void OnDraw() override {
        Graphics * g = GetGraphics();
        g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
        g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
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
	virtual ~ConfigCLUDWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside)
           NOT_NEAR_WINDOW_BOTTOM_CHECK()
        CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigCLUDWindow::ConfigCLUDWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(190, 82)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	// Move window near the cloud
	align_window_near(Position, partpos, Size);

	int pr = sim->pmap[pos.Y][pos.X];
	pid = ID(pr);

	messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X - 8, 15), "Edit CLUD");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

	// Ctype
	ui::Label * tempLabel = make_left_label(ui::Point(4, 24), ui::Point(100, 15), "Weather type");
	AddComponent(tempLabel);

	ctype = new ui::DropDown(ui::Point(100, 24), ui::Point(80, 15));
    AddComponent(ctype);
    ctype->AddOption(std::make_pair(String("Rain"), PT_WATR));
    ctype->AddOption(std::make_pair(String("Thunderstorm"), PT_LIGH));
    ctype->AddOption(std::make_pair(String("Snow"), PT_SNOW));
    ctype->AddOption(std::make_pair(String("Broken glass"), PT_GLAS));
    ctype->AddOption(std::make_pair(String("Lava"), PT_LAVA));
    ctype->AddOption(std::make_pair(String("Liquid nitrogen"), PT_LNTG));
    ctype->AddOption(std::make_pair(String("No weather"), PT_NONE));

    switch(sim->parts[pid].ctype) {
        case PT_WATR:
        case PT_DSTW:
            ctype->SetOption(PT_WATR);
            break;
        case PT_LIGH:
        case PT_THDR:
            ctype->SetOption(PT_THDR);
            break;
        case PT_SNOW:
        case PT_ICEI:
            ctype->SetOption(PT_SNOW);
            break;
        case PT_GLAS:
        case PT_BGLA:
            ctype->SetOption(PT_GLAS);
            break;
        case PT_LAVA:
            ctype->SetOption(PT_LAVA);
            break;
        case PT_LNTG:
            ctype->SetOption(PT_LNTG);
            break;
        default:
            ctype->SetOption(PT_NONE);
            break;
    }

	// Temp
	tempLabel = make_left_label(ui::Point(4, 44), ui::Point(100, 15), "Spawn temperature");
	AddComponent(tempLabel);

	temp_val = sim->parts[pid].temp;
	temp = make_left_textbox(ui::Point(100, 44), ui::Point(80, 15),
							 String::Build(round(sim->parts[pid].temp - 273.15f), " C"), "22 C");
	TEMPERATURE_INPUT(temp, temp_val)
	AddComponent(temp);

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
		// Set property
        PropertyValue value;
        value.Integer = ctype->GetOption().second;
        sim->flood_prop(pos.X, pos.Y, offsetof(Particle, ctype), value, StructProperty::Integer);
        value.Float = temp_val;
        sim->flood_prop(pos.X, pos.Y, offsetof(Particle, temp), value, StructProperty::Float);

		CloseActiveWindow();
		SelfDestruct();
	}});
	AddComponent(okayButton);
	SetOkayButton(okayButton);

	ui::Button * cancelButton = make_center_button(ui::Point(0, Size.Y - 16), ui::Point(Size.X / 2, 16), "Cancel");
    CANCEL_BUTTON(cancelButton);
    AddComponent(cancelButton);

	MakeActiveWindow();
}
