#include "gui/game/Tool.h"

#include "simulation/Simulation.h"

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
#include <cmath>

class ConfigFFGNWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;

    ui::Textbox * ctype;
    ui::DropDown * shape, * mode;
    ui::Point pos;
    ui::Label * messageLabel, * distance_label;

    bool dragging = false;
    bool setting_dir = false;
    int dragdx, dragdy;
    int distance, ctype_val;

    ConfigFFGNWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
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
	virtual ~ConfigFFGNWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        if (method == ui::Window::MouseOutside) // Prevent exiting on editing direction
			return;
        CloseActiveWindow();
        SelfDestruct();
    }

    String GetDistanceString() {
        StringBuilder distance_builder;
        if (mode->GetOption().second != 3)
            distance_builder << "Force field size: " << distance << " px";
        else // Connect to nearest makes multiple lines, so size is not applicable
            distance_builder << "Force field size: Not applicable";
        return distance_builder.Build();
    }
};

ConfigFFGNWindow::ConfigFFGNWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(ui::Point(-1, -1), ui::Point(200, 166)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
    // Move window away from the position
    align_window_far(Position, partpos, Size);

    // Header
	messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X-8, 15), "Config Force Field Generator");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    int pr = sim->pmap[pos.Y][pos.X];
    PR_PHOTONS_CHECK()
    int pid = ID(pr);
    distance = sim->parts[pid].temp;

    // Ctype
    ui::Label *tempLabel = make_left_label(ui::Point(6, 40), ui::Point(40, 15), "Ctype to affect");
    AddComponent(tempLabel);

    ctype_val = sim->parts[pid].ctype;
    ctype = make_left_textbox(ui::Point(8, 56), ui::Point(Size.X - 16, 16), "", "NONE");
    ELEMENT_INPUT(ctype, ctype_val)
    String elename_temp = sim->ElementResolve(sim->parts[pid].ctype, -1);
    ctype->SetText((!elename_temp.size() || elename_temp == "Empty") ? "" : elename_temp);
    AddComponent(ctype);

    // Selector for mode
    tempLabel = make_left_label(ui::Point(6, 75), ui::Point(40, 15), "FFLD Mode");
    AddComponent(tempLabel);
    
    mode = new ui::DropDown(ui::Point(8, 91), ui::Point(Size.X - 16, 16));
    mode->AddOption(std::make_pair(String("Deflect all (Default)"), 0));
    mode->AddOption(std::make_pair(String("Delete ctype"), 1));
    mode->AddOption(std::make_pair(String("Delete all but ctype"), 2));
    mode->AddOption(std::make_pair(String("Allow only ctype to pass"), 3));
    mode->AddOption(std::make_pair(String("Repel with gravity"), 4));
    mode->AddOption(std::make_pair(String("Superheat"), 5));
    mode->AddOption(std::make_pair(String("Supercool"), 6));
    mode->AddOption(std::make_pair(String("Stasis field"), 7));

    mode->SetOption((sim->parts[pid].tmp2 <= 7 && sim->parts[pid].tmp2 >= 0) ? sim->parts[pid].tmp2 : 0);
    mode->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
    AddComponent(mode);

    // Distance label, must be after mode is created
    distance_label = make_left_label(ui::Point(6, 22), ui::Point(40, 15), GetDistanceString());
    distance_label->SetTextColour(LIGHT_GRAY_TEXT);
    AddComponent(distance_label);

    // Shape
    tempLabel = make_left_label(ui::Point(6, 110), ui::Point(40, 15), "Shape");
    AddComponent(tempLabel);

    shape = new ui::DropDown(ui::Point(8, 126), ui::Point(Size.X - 16, 16));
    shape->AddOption(std::make_pair(String("Circle (Default)"), 0));
    shape->AddOption(std::make_pair(String("Square"), 1));
    shape->AddOption(std::make_pair(String("Connect nearest (Max range: 100px)"), 2));

    shape->SetOption((sim->parts[pid].tmp <= 2 && sim->parts[pid].tmp >= 0) ? sim->parts[pid].tmp : 0);
    shape->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
    AddComponent(shape);

    // Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
        // Set property
        sim->parts[ID(sim->pmap[pos.Y][pos.X])].ctype = ctype_val;
        sim->parts[ID(sim->pmap[pos.Y][pos.X])].temp = distance;
        sim->parts[ID(sim->pmap[pos.Y][pos.X])].tmp = shape->GetOption().second;
        sim->parts[ID(sim->pmap[pos.Y][pos.X])].tmp2 = mode->GetOption().second;

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

void ConfigFFGNWindow::DoDraw() {
    Graphics * g = GetGraphics();

    if (shape->GetOption().second == 0) // Circle
        g->drawcircle(pos.X, pos.Y, distance, distance, 25, 200, 255, 185);
    else if (shape->GetOption().second == 1) // Rectangle
        g->drawrect(pos.X - distance, pos.Y - distance, 2 * distance, 2 * distance, 25, 200, 255, 185);
    g->clearrect(XRES, 0, WINDOWW - XRES, WINDOWH);
    g->clearrect(0, YRES - 1, WINDOWW, WINDOWH - YRES + 1);
    
    ui::Window::DoDraw();
}

void ConfigFFGNWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}

void ConfigFFGNWindow::DoMouseMove(int x, int y, int dx, int dy) {
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
		distance = sqrt((x - pos.X) * (x - pos.X) + (y - pos.Y) * (y - pos.Y));
		if (distance <= 0) distance = 0;
        distance_label->SetText(GetDistanceString());
	}
}

void ConfigFFGNWindow::DoMouseDown(int x, int y, unsigned button) {
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
