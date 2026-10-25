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

class ConfigPSTNWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
	ui::Point pos;

    ui::Textbox * tmp, * tmp2, * ctype;
	ui::Label * messageLabel;

	int pid, ctype_val, tmp_val, tmp2_val;

	ConfigPSTNWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
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
    void DoDraw() override;
	virtual ~ConfigPSTNWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside)
           NOT_NEAR_WINDOW_BOTTOM_CHECK()
        CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigPSTNWindow::ConfigPSTNWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(260, 94)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	// Move window near the PSTN element
	align_window_far(Position, partpos, Size);

	int pr = sim->pmap[pos.Y][pos.X];
	pid = ID(pr);

	messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X - 8, 15), "Config " + sim->elements[TYP(pr)].Name);
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    tmp_val = sim->parts[pid].tmp ? sim->parts[pid].tmp : 31;
    tmp2_val = sim->parts[pid].tmp2 ? sim->parts[pid].tmp2 : 255;

    ui::Label * tempLabel = make_left_label(ui::Point(4, 24), ui::Point(Size.X, 15), "Max pixels to push");
    AddComponent(tempLabel);

    tmp = make_left_textbox(ui::Point(Size.X / 2 - 4, 24), ui::Point(Size.X / 2 - 8, 16), String::Build(tmp_val), "31");
    INTEGER_INPUT(tmp, tmp_val)
    AddComponent(tmp);

    tempLabel = make_left_label(ui::Point(4, 40), ui::Point(Size.X, 15), "Max extension length");
    AddComponent(tempLabel);

    tmp2 = make_left_textbox(ui::Point(Size.X / 2 - 4, 40), ui::Point(Size.X / 2 - 8, 16), String::Build(tmp2_val), "256");
    INTEGER_INPUT(tmp2, tmp2_val)
    AddComponent(tmp2);

    // Ctype
    tempLabel = make_left_label(ui::Point(4, 56), ui::Point(Size.X, 15), "Ctype to stop on");
    AddComponent(tempLabel);

    ctype = make_left_textbox(ui::Point(Size.X / 2 - 4, 56), ui::Point(Size.X / 2 - 8, 16), "", "NONE");
    ctype_val = sim->parts[pid].ctype;
    String elename_temp = sim->ElementResolve(sim->parts[pid].ctype, -1);
    ctype->SetText((!elename_temp.size() || elename_temp == "Empty") ? "" : elename_temp);

    ELEMENT_INPUT(ctype, ctype_val)
    AddComponent(ctype);

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
        sim->parts[pid].tmp = tmp_val;
        sim->parts[pid].tmp2 = tmp2_val;
        sim->parts[pid].ctype = ctype_val;
        
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

void ConfigPSTNWindow::DoDraw() {
    // Draw the line indicating direction
    Graphics *g = GetGraphics();

    g->drawcircle(pos.X, pos.Y, 1, 1, 255, 255, 255, 255);
    g->draw_line(pos.X, pos.Y + 1, pos.X, pos.Y + tmp2_val, 255, 0, 0, 160);
    g->draw_line(pos.X, pos.Y - 1, pos.X, pos.Y - tmp2_val, 255, 0, 0, 160);
    g->draw_line(pos.X - 1, pos.Y, pos.X + tmp2_val, pos.Y, 255, 0, 0, 160);
    g->draw_line(pos.X + 1, pos.Y, pos.X - tmp2_val, pos.Y, 255, 0, 0, 160);

    g->clearrect(XRES, 0, WINDOWW - XRES, WINDOWH);
    g->clearrect(0, YRES - 1, WINDOWW, WINDOWH - YRES + 1);
    ui::Window::DoDraw();
}