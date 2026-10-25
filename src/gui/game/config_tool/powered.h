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

class ConfigPoweredWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
	ui::Point pos;

    ui::Textbox * ctype_or_temp;
    ui::DropDown * mode;
	ui::Label * messageLabel;

	int pid, ctype_val;
    float temp_val = 0.0f;

	ConfigPoweredWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
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
    void DoMouseMove(int x, int y, int dx, int dy) override {
        ui::Window::DoMouseMove(x, y, dx, dy);
    }
    void DoMouseDown(int x, int y, unsigned button) override {
        ui::Window::DoMouseDown(x, y, button);
    }
    void DoDraw() override {
        ui::Window::DoDraw();
    }
	virtual ~ConfigPoweredWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside)
           NOT_NEAR_WINDOW_BOTTOM_CHECK()
        CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigPoweredWindow::ConfigPoweredWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(190, 64)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	// Move window near the powered element
	align_window_near(Position, partpos, Size);

	int pr = sim->pmap[pos.Y][pos.X];
	pid = ID(pr);

	messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X - 8, 15), "Config " + sim->elements[TYP(pr)].Name +
        (TYP(pr) == PT_DLAY ? " (Delay in C)" : String("")));
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    // PVOD has ctype, others have temp except for 
    temp_val = sim->parts[pid].temp;
    ctype_val = sim->parts[pid].ctype;

    if (TYP(pr) == PT_PVOD) {
        ctype_or_temp = make_left_textbox(ui::Point(4, 24), ui::Point(Size.X - 8, 16), "", "NONE");
        String elename_temp = sim->ElementResolve(sim->parts[pid].ctype, -1);
        ctype_or_temp->SetText((!elename_temp.size() || elename_temp == "Empty") ? "" : elename_temp);
        ELEMENT_INPUT(ctype_or_temp, ctype_val)
        AddComponent(ctype_or_temp);
    }
    else if (TYP(pr) != PT_HSWC) { // HSWC has no input
        ctype_or_temp = make_left_textbox(ui::Point(4, 24), ui::Point(Size.X - 8, 16),
            String::Build(round(temp_val - 273.15f), " C"), "22 C");
        TEMPERATURE_INPUT(ctype_or_temp, temp_val)
        AddComponent(ctype_or_temp);
    }

    // Mode
    if (TYP(pr) != PT_HSWC) // HSWC only has 1 button
        Size.Y += 20;
    int componenty = TYP(pr) == PT_HSWC ? 24 : 46;
    ui::Label * tempLabel = make_left_label(ui::Point(4, componenty), ui::Point(100, 15), "Mode:");
    AddComponent(tempLabel);

    mode = new ui::DropDown(ui::Point(Size.X / 2 - 24, componenty), ui::Point(Size.X / 2 + 20, 16));
    mode->AddOption(std::make_pair(String("Default"), 0));
    if (TYP(pr) == PT_PVOD)
        mode->AddOption(std::make_pair(String("Delete all but ctype"), 1));
    else
        mode->AddOption(std::make_pair(String("Serialize from FILT"), 1));

    mode->SetOption(sim->parts[pid].tmp);
    AddComponent(mode);

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
        PropertyValue value;
        if (sim->parts[pid].type != PT_HSWC && sim->parts[pid].type != PT_PVOD) {
            value.Float = temp_val;
            sim->flood_prop(pos.X, pos.Y, offsetof(Particle, temp), value, StructProperty::Float);
        }
        else if (sim->parts[pid].type == PT_PVOD) {
            value.Integer = ctype_val;
            sim->flood_prop(pos.X, pos.Y, offsetof(Particle, ctype), value, StructProperty::Integer);
        }
        if (mode) {
            value.Integer = mode->GetOption().second;
            sim->flood_prop(pos.X, pos.Y, offsetof(Particle, tmp), value, StructProperty::Integer);
        }
        
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
