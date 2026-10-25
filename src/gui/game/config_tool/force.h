#include "gui/game/Tool.h"

#include "simulation/Simulation.h"
#include "simulation/ElementCommon.h"

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

class ConfigForceWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
    ui::Point pos;
	ui::Textbox * input, * input2;
	
	float temp = 0;
    int pid, rt, val;

	ConfigForceWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
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
	virtual ~ConfigForceWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside)
           NOT_NEAR_WINDOW_BOTTOM_CHECK()
		CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigForceWindow::ConfigForceWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(190, 74)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
    int pr = sim->pmap[pos.Y][pos.X];
    rt = TYP(pr);
    pid = ID(pr);

    align_window_far(Position, partpos, Size);

    String title = "Configure Force";
    if (rt == PT_FRAY)
        title = "FRAY: + = Push, - = Pull";
    else if (rt == PT_RPEL)
        title = "RPEL: + = Push, - = Pull";

    ui::Label *messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X - 8, 15), title);
    messageLabel->SetTextColour(style::Colour::InformationTitle);
    AddComponent(messageLabel);

    ui::Label *channel_label = make_left_label(ui::Point(4, 20), ui::Point(100, 15), "Change force");
    AddComponent(channel_label);

    temp = sim->parts[pid].temp;
    input = make_left_textbox(ui::Point(120, 20), ui::Point(60, 15), String::Build(round(temp - 273.15), " C"), "0 C");
    TEMPERATURE_INPUT(input, temp);
    AddComponent(input);

    if (rt == PT_FRAY) {
        ui::Label *channel_label = make_left_label(ui::Point(4, 36), ui::Point(100, 15), "Range");
        AddComponent(channel_label);

        val = sim->parts[pid].tmp == 0 ? 10 : sim->parts[pid].tmp;
        input2 = make_left_textbox(ui::Point(120, 36), ui::Point(60, 15), String::Build(val), "10");
        INTEGER_INPUT(input2, val);
    }
    else if (rt == PT_RPEL) {
        ui::Label *channel_label = make_left_label(ui::Point(4, 36), ui::Point(100, 15), "Only affect this");
        AddComponent(channel_label);

        input2 = make_left_textbox(ui::Point(120, 36), ui::Point(60, 15), "", "NONE");
        val = sim->parts[pid].ctype;
        String elename_temp = sim->ElementResolve(sim->parts[pid].ctype, -1);
        input2->SetText((!elename_temp.size() || elename_temp == "Empty") ? "" : elename_temp);
        ELEMENT_INPUT(input2, val);
    }
    AddComponent(input2);

    // Rest of window
    ui::Button *okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y - 16), ui::Point(Size.X / 2, 16), "OK");
    okayButton->SetActionCallback({[this] {
        PropertyValue value;
        value.Float = temp;
        sim->flood_prop(pos.X, pos.Y, offsetof(Particle, temp), value, StructProperty::Float);
        value.Integer = val;
        if (rt == PT_FRAY)
            sim->flood_prop(pos.X, pos.Y, offsetof(Particle, tmp), value, StructProperty::Integer);
        else if (rt == PT_RPEL)
            sim->flood_prop(pos.X, pos.Y, offsetof(Particle, ctype), value, StructProperty::Integer);

        CloseActiveWindow();
        SelfDestruct();
    }});
    AddComponent(okayButton);
    SetOkayButton(okayButton);

    ui::Button *cancelButton = make_center_button(ui::Point(0, Size.Y - 16), ui::Point(Size.X / 2, 16), "Cancel");
    CANCEL_BUTTON(cancelButton);
    AddComponent(cancelButton);
    MakeActiveWindow();
}

void ConfigForceWindow::DoDraw() {
    if (rt == PT_FRAY || rt == PT_RPEL) {
        Graphics * g = GetGraphics();
        g->drawcircle(pos.X, pos.Y, 1, 1, 255, 255, 255, 255);

        // FRAY: draw the lines around it
        if (rt == PT_FRAY) {
            g->draw_line(pos.X, pos.Y + 1, pos.X, pos.Y + val, 255, 0, 0, 160);
            g->draw_line(pos.X, pos.Y - 1, pos.X, pos.Y - val, 255, 0, 0, 160);
            g->draw_line(pos.X - 1, pos.Y, pos.X + val, pos.Y, 255, 0, 0, 160);
            g->draw_line(pos.X + 1, pos.Y, pos.X - val, pos.Y, 255, 0, 0, 160);
        }
        // RPEL: Draw the affected region
        else if (rt == PT_RPEL) {
            g->fillrect(pos.X - 10, pos.Y - 10, 21, 21, 0, 255, 0, 160);
        }

        g->clearrect(XRES, 0, WINDOWW - XRES, WINDOWH);
        g->clearrect(0, YRES - 1, WINDOWW, WINDOWH - YRES + 1);
    }

	ui::Window::DoDraw();
}

void ConfigForceWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}
