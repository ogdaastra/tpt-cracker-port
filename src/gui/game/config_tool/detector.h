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

// THIS DOES NOT INCLUDE TRBN AND LDTC

class ConfigDetectorWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
	ui::Point pos;

    ui::Textbox * ctype_or_temp;
    ui::DropDown * mode;
	ui::Label * messageLabel, * distanceLabel;

	int pid, distance_val, ctype_val;
    float temp_val = 0.0f;

    bool dragging = false;
    bool setting_dir = false;
    int dragdx, dragdy;

	ConfigDetectorWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
    void DoMouseDown(int x, int y, unsigned button) override;
    void DoMouseMove(int x, int y, int dx, int dy) override;
    void DoDraw() override;
    void OnDraw() override {
        Graphics * g = GetGraphics();
        g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
        g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
    }
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
	virtual ~ConfigDetectorWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside) // Prevent exiting on editing direction
			return;
        CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigDetectorWindow::ConfigDetectorWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(partpos, ui::Point(190, 78)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	// Move window away from the detector
	align_window_far(Position, partpos, Size);

	int pr = sim->pmap[pos.Y][pos.X];
	pid = ID(pr);

	messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X - 8, 15), "Config Detector");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    // DETC has ctype, others have temp
    if (TYP(pr) == PT_DTEC) {
        ctype_val = sim->parts[pid].ctype;
        ctype_or_temp = make_left_textbox(ui::Point(4, 24), ui::Point(Size.X - 8, 16), "", "NONE");
        String elename_temp = sim->ElementResolve(sim->parts[pid].ctype, -1);
        ctype_or_temp->SetText((!elename_temp.size() || elename_temp == "Empty") ? "" : elename_temp);
        ELEMENT_INPUT(ctype_or_temp, ctype_val)
    }
    else {
        temp_val = sim->parts[pid].temp;
        ctype_or_temp = make_left_textbox(ui::Point(4, 24), ui::Point(Size.X - 8, 16),
            String::Build(round(temp_val - 273.15f), " C"), "22 C");
        TEMPERATURE_INPUT(ctype_or_temp, temp_val)
    }
    AddComponent(ctype_or_temp);

	// Range label
    distance_val = sim->parts[pid].tmp2;
	distanceLabel = make_left_label(ui::Point(4, 42), ui::Point(100, 15), String::Build("Range: ", distance_val, " px"));
    distanceLabel->SetTextColour(LIGHT_GRAY_TEXT);
    AddComponent(distanceLabel);

    // Mode
	if (TYP(pr) != PT_DTEC) {
        Size.Y += 20;
        ui::Label * tempLabel = make_left_label(ui::Point(4, 56), ui::Point(100, 15), "Mode:");
        AddComponent(tempLabel);

        mode = new ui::DropDown(ui::Point(Size.X / 2 - 24, 56), ui::Point(Size.X / 2 + 20, 16));
        mode->AddOption(std::make_pair(String("Default"), 0));
        mode->AddOption(std::make_pair(String("Serialize to FILT"), 1));

        if (TYP(pr) == PT_TSNS)
            mode->AddOption(std::make_pair(String("Invert (Detect lower)"), 2));
        else if (TYP(pr) == PT_LSNS) {
            mode->AddOption(std::make_pair(String("Invert (Detect lower)"), 2));
            mode->AddOption(std::make_pair(String("Derialize (Set life)"), 3));
        }

        mode->SetOption(sim->parts[pid].tmp);
        AddComponent(mode);
    }

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
		if (sim->parts[pid].type == PT_DTEC)
            sim->parts[pid].ctype = ctype_val;
        else {
            sim->parts[pid].tmp = mode->GetOption().second;
            sim->parts[pid].temp = temp_val;
        }
        sim->parts[pid].tmp2 = distance_val;
        
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

void ConfigDetectorWindow::DoMouseMove(int x, int y, int dx, int dy) {
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
        distance_val = abs(x - pos.X) > abs(y - pos.Y) ? abs(x - pos.X) : abs(y - pos.Y);
	    if (distance_val <= 0) distance_val = 1;
        if (distance_val > 25) distance_val = 25;
        distanceLabel->SetText(String::Build("Range: ", distance_val, " px"));
	}
}

void ConfigDetectorWindow::DoMouseDown(int x, int y, unsigned button) {
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

void ConfigDetectorWindow::DoDraw()  {
    ui::Window::DoDraw();
    Graphics * g = GetGraphics();
    g->fillrect(pos.X - distance_val, pos.Y - distance_val, distance_val * 2 + 1, distance_val * 2 + 1,
        0, 255, 100, 150);

    g->clearrect(XRES, 0, WINDOWW - XRES, WINDOWH);
    g->clearrect(0, YRES - 1, WINDOWW, WINDOWH - YRES + 1);
}