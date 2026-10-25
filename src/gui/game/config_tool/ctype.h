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

class ConfigCtypeWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
    ui::Point pos;
	ui::Textbox * input;
	
	int ctype = 0;
    int pid, rt;

	ConfigCtypeWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
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
	virtual ~ConfigCtypeWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside)
           NOT_NEAR_WINDOW_BOTTOM_CHECK()
		CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigCtypeWindow::ConfigCtypeWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(190, 58)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
    int pr = sim->pmap[pos.Y][pos.X];
    rt = TYP(pr);
    pid = ID(pr);

	align_window_near(Position, partpos, Size);

    String title = "Configure Ctype";

    ui::Label *messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X - 8, 15), title);
    messageLabel->SetTextColour(style::Colour::InformationTitle);
    AddComponent(messageLabel);

    ui::Label *channel_label = make_left_label(ui::Point(4, 20), ui::Point(100, 15), "Change ctype");
    AddComponent(channel_label);

    input = make_left_textbox(ui::Point(120, 20), ui::Point(60, 15), "", "NONE");
    ctype = sim->parts[pid].ctype;
    String elename_temp = sim->ElementResolve(sim->parts[pid].ctype, -1);
    input->SetText((!elename_temp.size() || elename_temp == "Empty") ? "" : elename_temp);
    ELEMENT_INPUT(input, ctype);
    AddComponent(input);

    // Rest of window
    ui::Button *okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y - 16), ui::Point(Size.X / 2, 16), "OK");
    okayButton->SetActionCallback({[this] {
        PropertyValue value;
        value.Integer = ctype;
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

void ConfigCtypeWindow::DoDraw() {
	ui::Window::DoDraw();
}

void ConfigCtypeWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}
