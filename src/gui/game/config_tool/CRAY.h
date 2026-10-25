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

int isign_angle(float angle) {
	if (fabs(angle) < 0.01f) angle = 0;
	return isign(angle);
}

class ConfigCRAYWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
	ui::Point pos;
	ui::Textbox * ctype, * temp, * life, * gap;
	ui::Label * messageLabel;

	bool dragging = false;
	bool setting_dir = false;
	int dragdx, dragdy, ctype_val, pid;
	int distance = 256, gap_val = 0, life_val = 0;
	float temp_val = 0.0f, angle = 0.0f;

	ConfigCRAYWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
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
	virtual ~ConfigCRAYWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
		if (method == ui::Window::MouseOutside) // Prevent exiting on editing direction
			return;
        CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigCRAYWindow::ConfigCRAYWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(190, 114)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	// Move window into corner furthest from the CRAY
	align_window_far(Position, partpos, Size);

	int pr = sim->pmap[pos.Y][pos.X];
    PR_PHOTONS_CHECK()
	pid = ID(pr);

	messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X - 8, 15), "Edit CRAY");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

	// Ctype
	ui::Label * tempLabel = make_left_label(ui::Point(4, 24), ui::Point(100, 15), "Element to make");
	AddComponent(tempLabel);

	ctype = make_left_textbox(ui::Point(100, 24), ui::Point(80, 15), "", "NONE");
	ELEMENT_INPUT(ctype, ctype_val)
	ctype_val = sim->parts[pid].ctype;
	String elename_temp = sim->ElementResolve(sim->parts[pid].ctype, -1);
    ctype->SetText((!elename_temp.size() || elename_temp == "Empty") ? "" : elename_temp);
	AddComponent(ctype);

	// Distance
	distance = sim->parts[pid].tmp <= 0 ? 256 : sim->parts[pid].tmp;

	// Temp
	tempLabel = make_left_label(ui::Point(4, 40), ui::Point(100, 15), "Spawn temperature");
	AddComponent(tempLabel);

	temp_val = sim->parts[pid].temp;
	temp = make_left_textbox(ui::Point(100, 40), ui::Point(80, 15),
							 String::Build(round(sim->parts[pid].temp - 273.15f), " C"), "22 C");
	TEMPERATURE_INPUT(temp, temp_val)
	AddComponent(temp);

	// Life
	tempLabel = make_left_label(ui::Point(4, 56), ui::Point(100, 15), "Spawn life");
	AddComponent(tempLabel);
	life_val = sim->parts[pid].life;
	life = make_left_textbox(ui::Point(100, 56), ui::Point(80, 15), String::Build(sim->parts[pid].life), "0");
	INTEGER_INPUT(life, life_val);
	AddComponent(life);

	// Gap
	tempLabel = make_left_label(ui::Point(4, 72), ui::Point(100, 15), "Gap before spawn");
	AddComponent(tempLabel);
	gap_val = sim->parts[pid].tmp2;
	gap = make_left_textbox(ui::Point(100, 72), ui::Point(80, 15), String::Build(sim->parts[pid].tmp2), "0");
	INTEGER_INPUT(gap, gap_val);
	AddComponent(gap);

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
		// Set property
		sim->parts[pid].temp = temp_val;
		sim->parts[pid].ctype = ctype_val;
		sim->parts[pid].tmp = distance;
		sim->parts[pid].tmp2 = gap_val;
		sim->parts[pid].life = life_val;

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

void ConfigCRAYWindow::DoDraw() {
	// Draw the line indicating direction
	Graphics * g = GetGraphics();

	// Bound checking for end line
	int endx = pos.X + (gap_val + distance) * isign_angle(cos(angle));
	int endy = pos.Y + (gap_val + distance) * isign_angle(sin(angle));
	int gapendx = pos.X + gap_val * isign_angle(cos(angle));
	int gapendy = pos.Y + gap_val * isign_angle(sin(angle));

	g->draw_line(pos.X, pos.Y, endx, endy, 255, 0, 0, 255);
	g->draw_line(pos.X, pos.Y, gapendx, gapendy, 135, 0, 0, 255);

	String minilabel = String::Build(distance, "px / ", gap_val + distance, " px");
	g->drawtext((pos.X + endx) / 2 - g->textwidth(minilabel) / 2, (pos.Y + endy) / 2 - 8,
		minilabel, 255, 255, 255, 255);
	g->drawcircle(pos.X, pos.Y, 4, 4, 255, 255, 255, 255);

	g->clearrect(XRES, 0, WINDOWW - XRES, WINDOWH);
	g->clearrect(0, YRES - 1, WINDOWW, WINDOWH - YRES + 1);
	ui::Window::DoDraw();
}

void ConfigCRAYWindow::DoMouseMove(int x, int y, int dx, int dy) {
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

void ConfigCRAYWindow::DoMouseDown(int x, int y, unsigned button) {
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

void ConfigCRAYWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}

#undef DEG45