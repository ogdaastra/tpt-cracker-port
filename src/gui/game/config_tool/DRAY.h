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

#define DEG45 3.1415f / 4

class ConfigDRAYWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
	ui::Point pos;
	ui::Textbox * ctype, * gap;
	ui::Label * messageLabel;

	bool dragging = false;
	bool setting_dir = false;
	int dragdx, dragdy, ctype_val, pid;
	int distance = 256, gap_val = 0;
	float angle = 0.0f;

	ConfigDRAYWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
	void OnDraw() override;
	void DoDraw() override;
	void DoMouseMove(int x, int y, int dx, int dy) override;
	void DoMouseUp(int x, int y, unsigned button) override {
        ui::Window::DoMouseUp(x, y, button);
		dragging = false;
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
    void DoMouseDown(int x, int y, unsigned button) override;
	virtual ~ConfigDRAYWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
		if (method == ui::Window::MouseOutside) // Prevent exiting on editing direction
			return;
        CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigDRAYWindow::ConfigDRAYWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(190, 80)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	// Move window into corner furthest from the DRAY
	align_window_far(Position, partpos, Size);

	int pr = sim->pmap[pos.Y][pos.X];
    PR_PHOTONS_CHECK()
	pid = ID(pr);

	messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X - 8, 15), "Edit DRAY");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

	// Ctype
	ui::Label * tempLabel = make_left_label(ui::Point(4, 24), ui::Point(100, 15), "Element to stop on");
	AddComponent(tempLabel);

	ctype = make_left_textbox(ui::Point(100, 24), ui::Point(80, 15), "", "NONE");
	ELEMENT_INPUT(ctype, ctype_val)
	ctype_val = sim->parts[pid].ctype;
	String elename_temp = sim->ElementResolve(sim->parts[pid].ctype, -1);
    ctype->SetText((!elename_temp.size() || elename_temp == "Empty") ? "" : elename_temp);
	AddComponent(ctype);

	// Distance
	distance = sim->parts[pid].tmp <= 0 ? 256 : sim->parts[pid].tmp;

	// Gap
	tempLabel = make_left_label(ui::Point(4, 42), ui::Point(100, 15), "Gap before spawn");
	AddComponent(tempLabel);
	gap_val = sim->parts[pid].tmp2;
	gap = make_left_textbox(ui::Point(100, 42), ui::Point(80, 15), String::Build(sim->parts[pid].tmp2), "0");
	INTEGER_INPUT(gap, gap_val);
	AddComponent(gap);

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
		// Set property
		sim->parts[pid].ctype = ctype_val;
		sim->parts[pid].tmp = distance;
		sim->parts[pid].tmp2 = gap_val;

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

void ConfigDRAYWindow::DoDraw() {
	// Draw the line indicating direction
	Graphics * g = GetGraphics();

	// Bound checking for end line
	int endx = pos.X + distance * isign_angle(cos(angle));
	int endy = pos.Y + distance * isign_angle(sin(angle));
	int gapstartx = pos.X + (distance + gap_val) * isign_angle(cos(angle));
	int gapstarty = pos.Y + (distance + gap_val) * isign_angle(sin(angle));
    int gapendx = pos.X + (2 * distance + gap_val) * isign_angle(cos(angle));
	int gapendy = pos.Y + (2 * distance + gap_val) * isign_angle(sin(angle));

	g->draw_line(pos.X, pos.Y, endx, endy, 255, 0, 0, 255);
	g->draw_line(gapstartx, gapstarty, gapendx, gapendy, 135, 0, 0, 255);

	String minilabel = String::Build(distance, "px / ", "Gap: ", gap_val, " px");
	g->drawtext((pos.X + endx) / 2 - g->textwidth(minilabel) / 2, (pos.Y + endy) / 2 - 8,
		minilabel, 255, 255, 255, 255);
	g->drawcircle(pos.X, pos.Y, 4, 4, 255, 255, 255, 255);

	g->clearrect(XRES, 0, WINDOWW - XRES, WINDOWH);
	g->clearrect(0, YRES - 1, WINDOWW, WINDOWH - YRES + 1);
	ui::Window::DoDraw();
}

void ConfigDRAYWindow::DoMouseMove(int x, int y, int dx, int dy) {
	ui::Window::DoMouseMove(x, y, dx, dy);
	if (dragging) {
		Position.X = x - dragdx;
		Position.Y = y - dragdy;

		if (Position.X < 0) Position.X = 0;
		if (Position.Y < 0) Position.Y = 0;
		if (Position.X + Size.X > XRES) Position.X = XRES - Size.X;
		if (Position.Y + Size.Y > YRES) Position.Y = YRES - Size.Y;
		messageLabel->ClearSelection(); // Avoid selecting while dragging
	} else if (setting_dir) {
		angle = atan2(y - pos.Y, x - pos.X);
		angle = round(angle / (DEG45)) * DEG45;

		distance = sqrt((x - pos.X) * (x - pos.X) + (y - pos.Y) * (y - pos.Y));
		if (distance <= 0) distance = 256;
	}
}

void ConfigDRAYWindow::DoMouseDown(int x, int y, unsigned button) {
	ui::Window::DoMouseDown(x, y, button);
	// Clicked on header
	if (x >= Position.X && x <= Position.X + Size.X && y >= Position.Y && y <= Position.Y + 16) {
		dragging = true;
		dragdx = x - Position.X;
		dragdy = y - Position.Y;
	}
	// Outside, configurate
	else if (x < Position.X || x > Position.X + Size.X || y < Position.Y || y > Position.Y + Size.Y)
		setting_dir = !setting_dir;
}

void ConfigDRAYWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}

#undef DEG45