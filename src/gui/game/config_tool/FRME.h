#include "gui/game/Tool.h"

#include "simulation/Simulation.h"

#include "gui/Style.h"
#include "gui/interface/Window.h"
#include "gui/interface/Button.h"
#include "gui/interface/Label.h"
#include "gui/interface/DropDown.h"
#include "gui/game/GameModel.h"

#include "graphics/Graphics.h"
#include "gui/interface/Engine.h"
#include "gui/game/config_tool/util.h"

#include "Misc.h"
#include <cmath>

class ConfigFRMEWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
    ui::Point pos;
	ui::DropDown * input;
    int pid;

	ConfigFRMEWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
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
	virtual ~ConfigFRMEWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside)
           NOT_NEAR_WINDOW_BOTTOM_CHECK()
		CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigFRMEWindow::ConfigFRMEWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(190, 58)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	// Align WINDOW near the element
	align_window_near(Position, partpos, Size);

    int pr = sim->pmap[pos.Y][pos.X];
	pid = ID(pr);

	ui::Label * messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X-8, 15), "Configure FRME");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    input = new ui::DropDown(ui::Point(4, 20), ui::Point(Size.X - 8, 15));
    input->AddOption(std::make_pair(String("Sticky"), 0));
    input->AddOption(std::make_pair(String("Non-Sticky"), 1));

    input->SetOption((sim->parts[pid].tmp >= 0 && sim->parts[pid].tmp <= 1) ? sim->parts[pid].tmp : 0);
	AddComponent(input);

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
        // Configure new Frame
        PropertyValue value;
        value.Integer = input->GetOption().second;
        sim->flood_prop(pos.X, pos.Y, offsetof(Particle, tmp), value, StructProperty::Integer);

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

void ConfigFRMEWindow::DoDraw() {
	ui::Window::DoDraw();
}

void ConfigFRMEWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}
