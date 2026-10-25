#include "Tool.h"

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

#include <cmath>

class RulerWindow: public ui::Window {
public:
	RulerTool * tool;
	Simulation * sim;
    int point_count = 0;
    int dragging = 0; // 1 = point 1, 2 = point 2
    ui::Point p1 = ui::Point(0, 0);
    ui::Point p2 = ui::Point(0, 0);

	RulerWindow(RulerTool * tool_, Simulation * sim_);
	void OnDraw() override;
	void DoDraw() override;
	void DoMouseMove(int x, int y, int dx, int dy) override;
	void DoMouseUp(int x, int y, unsigned button) override {
        ui::Window::DoMouseUp(x, y, button);
        dragging = 0;
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
    void DoMouseDown(int x, int y, unsigned button) {
        ui::Window::DoMouseDown(x, y, button);
    }
	virtual ~RulerWindow() {}
	void OnTryExit(ui::Window::ExitMethod method) override;
};

RulerWindow::RulerWindow(RulerTool * tool_, Simulation * sim_):
	ui::Window(ui::Point(WINDOWW - 105, WINDOWH - 45), ui::Point(100, 40)),
	tool(tool_),
	sim(sim_)
{
	ui::Label * messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X-8, 15), "Close Ruler");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

	ui::Button * okayButton = make_center_button(ui::Point(0, Size.Y-16), ui::Point(Size.X, 16), "OK");
	okayButton->SetActionCallback({ [this] {
		CloseActiveWindow();
		SelfDestruct();
	}});
	AddComponent(okayButton);
	SetOkayButton(okayButton);
	MakeActiveWindow();
}

void RulerWindow::OnTryExit(ui::Window::ExitMethod method) {
    if (method == ui::Window::MouseOutside) { // Clicking outside shouldn't exit since we're placing rulers
        if (point_count <= 2) ++point_count;
        int x = ui::Engine::Ref().GetMouseX();
        int y = ui::Engine::Ref().GetMouseY();

        // We're still placing down points
        if (point_count == 2)
            p2 = ui::Point(x, y);

        // Drag point enable
        float disp1 = sqrtf((x - p1.X) * (x - p1.X) + (y - p1.Y) * (y - p1.Y));
        if (disp1 <= 10) dragging = 1;
        else {
            float disp2 = sqrtf((x - p2.X) * (x - p2.X) + (y - p2.Y) * (y - p2.Y));
            if (disp2 <= 10) dragging = 2;
        }
        return;
    }
	CloseActiveWindow();
	SelfDestruct();
}

void RulerWindow::DoDraw() {
    // Draw new ruler here
    Graphics * g = GetGraphics();

    // First point
    if (point_count >= 1) {
        StringBuilder c1; c1 << p1.X << ", " << p1.Y;
        g->drawcircle(p1.X, p1.Y, 6, 6, 255, 255, 255, 200);
        g->drawtext(p1.X - g->textwidth(c1.Build()) / 2, p1.Y + (p1.Y < YRES / 2 ? 15 : -20), c1.Build(), 255, 255, 255, 255);
    }
    // Second point and connecting line
    if (point_count >= 2) {
        StringBuilder c2; c2 << p2.X << ", " << p2.Y;
        g->drawcircle(p2.X, p2.Y, 6, 6, 255, 255, 255, 200);
        g->drawtext(p2.X - g->textwidth(c2.Build()) / 2, p2.Y + (p2.Y < YRES / 2 ? 15 : -20), c2.Build(), 255, 255, 255, 255);

        g->draw_line(p1.X, p1.Y, p2.X, p2.Y, 255, 255, 255, 200);
        StringBuilder len; len << sqrtf((p1.X - p2.X) * (p1.X - p2.X) + (p1.Y - p2.Y) * (p1.Y - p2.Y)) << " px";
        String lenstr = len.Build();
        int lenstrlen = g->textwidth(lenstr);

        g->clearrect((p1.X + p2.X) / 2 - lenstrlen / 2, (p1.Y + p2.Y) / 2, lenstrlen, 10);
        g->drawtext((p1.X + p2.X) / 2 - lenstrlen / 2, (p1.Y + p2.Y) / 2, lenstr, 255, 255, 255, 255);
    }

    g->clearrect(XRES, 0, WINDOWW - XRES, WINDOWH);
	g->clearrect(0, YRES - 1, WINDOWW, WINDOWH - YRES + 1);

    ui::Window::DoDraw();
}

void RulerWindow::DoMouseMove(int x, int y, int dx, int dy) {
	ui::Window::DoMouseMove(x, y, dx, dy);

    // Restrict x and y
    if (x > XRES) x = XRES;
    if (y > YRES) y = YRES;
    if (dragging == 1) p1 = ui::Point(x, y);
    else if (dragging == 2) p2 = ui::Point(x, y);
}

void RulerWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}

VideoBuffer * RulerTool::GetIcon(int toolID, int width, int height) {
	VideoBuffer * newTexture = new VideoBuffer(width, height);
	for (int y=0; y<height; y++) {
		for (int x=0; x<width; x++) {
			pixel pc =  x==0||x==width-1||y==0||y==height-1 ? PIXPACK(0xA0A0A0) : PIXPACK(0x000000);
			newTexture->SetPixel(x, y, PIXR(pc), PIXG(pc), PIXB(pc), 255);
		}
	}
	newTexture->BlendCharacter((width/2)-7, (height/2)-6, 0xE078, 255, 255, 255, 255);
	return newTexture;
}

void RulerTool::Click(Simulation * sim, Brush * brush, ui::Point position) {
    RulerWindow * t = new RulerWindow(this, sim);
    ++t->point_count;
    t->p1 = position;
}
