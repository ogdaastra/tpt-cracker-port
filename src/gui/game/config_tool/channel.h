#include "gui/game/Tool.h"

#include "simulation/Simulation.h"

#include "gui/Style.h"
#include "gui/interface/Window.h"
#include "gui/interface/Button.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"
#include "gui/game/GameModel.h"

#include "graphics/Graphics.h"
#include "gui/interface/Engine.h"
#include "gui/game/config_tool/util.h"

#include "Misc.h"
#include <cmath>

class ConfigChannelWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
    ui::Point pos;
    ui::Label * channel_label;
	ui::Textbox * channel_input;
	
	int channel = 0;

	ConfigChannelWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
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
	virtual ~ConfigChannelWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside)
           NOT_NEAR_WINDOW_BOTTOM_CHECK()
		CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigChannelWindow::ConfigChannelWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(190, 58)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	// Align WINDOW near the element
	align_window_near(Position, partpos, Size);

    // Calculate particle channel
    if (sim->pmap[partpos.Y][partpos.X])
        channel = (int)((sim->parts[ID(sim->pmap[partpos.Y][partpos.X])].temp - 73.15f) / 100 + 1);
    if (channel >= CHANNELS)
        channel = CHANNELS - 1;

	ui::Label * messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X-8, 15), "Configure Channel"); // Channel, not WIFI since this is also used for portals
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

	// Channel (temp)
	channel_label = make_left_label(ui::Point(4, 20), ui::Point(100, 15), String::Build("Change from ", channel, " to"));
	AddComponent(channel_label);

	channel_input = make_left_textbox(ui::Point(120, 20), ui::Point(60, 15), "", String::Build(channel));
    channel_input->SetInputType(ui::Textbox::Number);
	channel_input->SetActionCallback({ [this] {
		int val = channel_input->GetText().ToNumber<int>(true);
		if (val > CHANNELS || val < 0)
			channel_input->SetTextColour(RED_TEXT);
		else
			channel_input->SetTextColour(ui::Colour(255, 255, 255));
	}});
    channel_input->SetLimit(3);
	AddComponent(channel_input);

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
        // Configure new channel
        int new_chan = channel_input->GetText().ToNumber<int>(true);
        if (new_chan != channel && sim->pmap[pos.Y][pos.X]) { // If same channel don't change original temp
            float new_temp = (new_chan - 1) * 100 + 73.15f;
            sim->parts[ID(sim->pmap[pos.Y][pos.X])].temp = new_temp;

			// Floodfill if portal
			if (TYP(sim->pmap[pos.Y][pos.X]) == PT_PRTI || TYP(sim->pmap[pos.Y][pos.X]) == PT_PRTO) {
				PropertyValue value;
				value.Float = new_temp;
				sim->flood_prop(pos.X, pos.Y, offsetof(Particle, temp), value, StructProperty::Float);
			}
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

void ConfigChannelWindow::DoDraw() {
	ui::Window::DoDraw();
}

void ConfigChannelWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}
