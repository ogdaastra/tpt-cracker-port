#include "Tool.h"

#include "simulation/Simulation.h"

#include "gui/Style.h"
#include "gui/interface/Window.h"
#include "gui/interface/Button.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/DropDown.h"
#include "gui/interface/Checkbox.h"
#include "gui/game/GameModel.h"

#include "graphics/Graphics.h"
#include "gui/interface/Engine.h"
#include "gui/game/config_tool/util.h"
#include "simulation/transform/transform.h"

#include <cmath>
#include <unordered_map>

#define PI 3.141592f

class TransformWindow: public ui::Window {
public:
	TransformTool * tool;
	Simulation * sim;
    ui::Textbox * scalex_input, * scaley_input, * rotation_input;
    ui::Checkbox * layer, * quality_rotate;
    ui::Label * messageLabel;

    int dragging = 0; // 1 = Scale, 2 = window
    int drag_type = 0; // 0 = Move, 1 = scaleX, 2 = scaleY, 3 = scaleBoth, 4 = rotate
    ui::Point center = ui::Point(0, 0);
    ui::Point pcenter = ui::Point(0, 0);

    float scaleX = 1.0f, scaleY = 1.0f;
    float pscaleX = 1.0f, pscaleY = 1.0f;
    int dragdx, dragdy;
    
    const int button_knob_radius = 3;
    const int move_knob_radius = 8;
    const int rotate_knob_radius = 7;

    int point_count = 0, radius = 0;
    float rotation = 0.0f, prev_rotation = 0.0f; // In degrees for input

    pixel_vector pixels, transform_pixels, buffer;
    element_vector elements, buffer2;

    ui::Point p1 = ui::Point(0, 0);
    ui::Point p2 = ui::Point(0, 0);

	TransformWindow(TransformTool * tool_, Simulation * sim_, ui::Point pos);
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
    void DoMouseDown(int x, int y, unsigned button) override;
	virtual ~TransformWindow() {}
	void OnTryExit(ui::Window::ExitMethod method) override;
};

TransformWindow::TransformWindow(TransformTool * tool_, Simulation * sim_, ui::Point pos):
	ui::Window(ui::Point(XRES - 200, YRES - 120), ui::Point(200, 130)),
	tool(tool_),
	sim(sim_),
    p1(pos), p2(pos)
{
    align_window_far(Position, pos, Size);

	messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X-8, 15), "Transform Selection");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    // Scale textbox
    ui::Label * tempLabel = make_left_label(ui::Point(4, 22), ui::Point(Size.X - 8, 15), "ScaleX");
    AddComponent(tempLabel);
    scalex_input = make_left_textbox(ui::Point(Size.X / 2 - 4, 22), ui::Point(Size.X / 2 - 4, 15), "1.0", "1.0");
    FLOAT_INPUT(scalex_input, scaleX);
    AddComponent(scalex_input);

    tempLabel = make_left_label(ui::Point(4, 40), ui::Point(Size.X - 8, 15), "ScaleY");
    AddComponent(tempLabel);
    scaley_input = make_left_textbox(ui::Point(Size.X / 2 - 4, 40), ui::Point(Size.X / 2 - 4, 15), "1.0", "1.0");
    FLOAT_INPUT(scaley_input, scaleY);
    AddComponent(scaley_input);

    tempLabel = make_left_label(ui::Point(4, 62), ui::Point(Size.X - 8, 15), "Rotation (deg)");
    AddComponent(tempLabel);
    rotation_input = make_left_textbox(ui::Point(Size.X / 2 - 4, 62), ui::Point(Size.X / 2 - 4, 15), "0.0", "0.0");
    FLOAT_INPUT(rotation_input, rotation);
    AddComponent(rotation_input);

    // Checkboxes
    layer = new ui::Checkbox(ui::Point(6, 78), ui::Point(Size.X, 15), "Delete stacked particles (If any)", "idk");
    layer->SetChecked(true);
    AddComponent(layer);

    quality_rotate = new ui::Checkbox(ui::Point(6, 94), ui::Point(Size.X, 15), "Auto-fill gaps", "idk");
    AddComponent(quality_rotate);

	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
        // Apply scale
        fastscale_vector<element_vector, int>(elements, scaleX, scaleY, buffer2, abs(p1.X - p2.X),
            abs(p1.Y - p2.Y), center.X, center.Y, (p1.X + p2.X) / 2, (p1.Y + p2.Y) / 2);

        // Apply rotation
        element_vector final;
        fastrot_vector<element_vector>(buffer2, rotation / 180 * PI, final, center.X, center.Y);
        std::unordered_map<int, int> stacking_check;
    
        for (auto &point : final) {
            // Skip if already have new element at this location
            // Key is a fancy somewhat unique function for a pair of coordinates
            // (Cantor pairing function)
            int key = map_key(point.first.X, point.first.Y);
            if (layer->GetChecked() && stacking_check.count(key)) {
                sim->kill_part(point.second);
                continue;
            }

            // Setting .x and .y doesn't update pmap, so we're creating an exact copy
            int ni = sim->create_part(-3, point.first.X, point.first.Y, sim->parts[point.second].type);
            if (ni < 0) { // Failed to create??
                sim->kill_part(point.second);
                continue;
            }
            sim->parts[ni].temp = sim->parts[point.second].temp;
            sim->parts[ni].vx = sim->parts[point.second].vx;
            sim->parts[ni].vy = sim->parts[point.second].vy;
            sim->parts[ni].tmp = sim->parts[point.second].tmp;
            sim->parts[ni].tmp2 = sim->parts[point.second].tmp2;
            sim->parts[ni].tmp3 = sim->parts[point.second].tmp3;
            sim->parts[ni].tmp4 = sim->parts[point.second].tmp4;
            sim->parts[ni].flags = sim->parts[point.second].flags;
            sim->parts[ni].ctype = sim->parts[point.second].ctype;
            sim->parts[ni].life = sim->parts[point.second].life;
            sim->parts[ni].dcolour = sim->parts[point.second].dcolour;

            stacking_check[key] = sim->parts[ni].type;
        }

        // Delete original
        for (auto &point : elements)
            sim->kill_part(point.second);

        // Auto detect gaps
        if (quality_rotate->GetChecked()) {
            for (auto &point : final) {
                for (int rx1 = -1; rx1 <= 1; ++rx1)
                for (int ry1 = -1; ry1 <= 1; ++ry1) {
                    if (!stacking_check.count(map_key(point.first.X + rx1, point.first.Y + ry1))) { // Potential gap?
                        int element = 0, key;
                        int copyx, copyy;
                        for (int rx2 = -1; rx2 <= 1; ++rx2)
                        for (int ry2 = -1; ry2 <= 1; ++ry2) {
                            if (!((rx2 == 0 || ry2 == 0) && (rx2 || ry2)))
                                continue;
                            key = map_key(point.first.X + rx1 + rx2, point.first.Y + ry1 + ry2);
                            if (!stacking_check.count(key))
                                goto failed; // A gap must have at least 4 surrounding elements not diagonal
                            else {
                                element = stacking_check[key];
                                copyx = point.first.X + rx1 + rx2;
                                copyy = point.first.Y + ry1 + ry2;
                            }
                        }
                        int ni = sim->create_part(-1, point.first.X + rx1, point.first.Y + ry1, element);
                        int r = sim->pmap[copyy][copyx];
                        if (ni < 0) goto failed;

                        sim->parts[ni].ctype = sim->parts[ID(r)].ctype;
                        sim->parts[ni].life = sim->parts[ID(r)].life;
                        sim->parts[ni].tmp = sim->parts[ID(r)].tmp;
                        sim->parts[ni].tmp2 = sim->parts[ID(r)].tmp2;
                        sim->parts[ni].tmp3 = sim->parts[ID(r)].tmp3;
                        sim->parts[ni].tmp4 = sim->parts[ID(r)].tmp4;
                    }
                    failed:
                    continue;
                }
            }
        }

		CloseActiveWindow();
		SelfDestruct();
	}});
	AddComponent(okayButton);
	SetOkayButton(okayButton);

    ui::Button * cancelButton = make_center_button(ui::Point(0, Size.Y-16), ui::Point(Size.X / 2, 16), "Cancel");
    CANCEL_BUTTON(cancelButton);
    AddComponent(cancelButton);
	MakeActiveWindow();
}

void TransformWindow::OnTryExit(ui::Window::ExitMethod method) {
    if (method == ui::Window::MouseOutside) // Clicking outside shouldn't exit since we're clicking
        return;
	CloseActiveWindow();
	SelfDestruct();
}

void TransformWindow::DoMouseDown(int x, int y, unsigned button) {
    ui::Window::DoMouseDown(x, y, button);

    // Clicked on header
	if (x >= Position.X && x <= Position.X + Size.X && y >= Position.Y && y <= Position.Y + 16) {
		dragging = 2;
		dragdx = x - Position.X;
		dragdy = y - Position.Y;
        return;
	}

    if (point_count <= 2) {
        ++point_count;
        if (point_count == 3) { // Create a graphic to Scale
            ++point_count;
            
            // Determine top left corner
            int topx = std::min(p1.X, p2.X);
            int topy = std::min(p1.Y, p2.Y);
            int botx = std::max(p1.X, p2.X);
            int boty = std::max(p1.Y, p2.Y);

            // Iterate
            for (int sx = topx; sx <= botx; ++sx)
                for (int sy = topy; sy <= boty; ++sy) {
                    int r = sim->pmap[sy][sx];
                    if (!r) r = sim->photons[sy][sx];
                    if (!r) continue;

                    int color = sim->elements[TYP(r)].Colour;
                    elements.push_back(std::make_pair(ui::Point(sx, sy), ID(r)));
                    pixels.push_back(std::make_pair(ui::Point(sx, sy),
                        ui::Colour(PIXR(color), PIXG(color), PIXB(color), 150)));
                }
        }
    }

    // We're still placing down points
    if (point_count == 2) {
        p2 = ui::Point(x, y);
        center = ui::Point((p1.X + p2.X) / 2, (p1.Y + p2.Y) / 2);
    }

    // Drag the rotation thing
    if (point_count > 2) {
        int dx = x - center.X, dy = y - center.Y;
        int dx2 = x - (center.X + radius * cos(rotation / 180 * PI));
        int dy2 = y - (center.Y + radius * sin(rotation / 180 * PI));

        if (sqrtf(dx * dx + dy * dy) <= move_knob_radius) {
            dragging = 1;
            drag_type = 0;
        }
        else if (sqrtf(dx2 * dx2 + dy2 * dy2) <= rotate_knob_radius) {
            dragging = 1;
            drag_type = 4;
        }
        else {
            int on_x = abs(abs(dx) - abs(p1.X - p2.X) * fabs(scaleX) / 2);
            int on_y = abs(abs(dy) - abs(p1.Y - p2.Y) * fabs(scaleY) / 2);

            // Check if on the border
            if (on_x <= 1.5 * button_knob_radius || on_y <= 1.5 * button_knob_radius) {
                dragging = 1;

                // Corner: scale both X and Y
                if (on_x <= 4 * button_knob_radius && on_y <= 4 * button_knob_radius)
                    drag_type = 3;
                // X > Y: Scale X
                else if (abs(dx) > abs(dy))
                    drag_type = 1;
                // Y > X: Scale Y
                else
                    drag_type = 2;
            }
        }
    }
}

void TransformWindow::DoMouseMove(int x, int y, int dx, int dy) {
	ui::Window::DoMouseMove(x, y, dx, dy);

    if (point_count == 1) { // Seting 2nd point
        p2 = ui::Point(x, y);
        center = ui::Point((p1.X + p2.X) / 2, (p1.Y + p2.Y) / 2);
    }
    else if (point_count > 2 && dragging == 1) { // Set rotation
        pscaleX = scaleX;
        pscaleY = scaleY;
        pcenter = center;

        // Scale both X and Y (keep aspect ratio)
        if (drag_type == 3) {
            if (fabs(scaleX) > 0.1f) {
                float orgr = scaleY / scaleX;
                scaleX = 2.0f * (x - center.X) / abs(p1.X - p2.X);
                if (orgr < 1000)
                    scaleY = scaleX * orgr;
            } else {
                float orgr = scaleX / scaleY;
                scaleY = 2.0f * (center.Y - y) / abs(p1.Y - p2.Y);
                if (orgr < 1000)
                    scaleX = scaleY * orgr;
            }
        }
        // Scale X only
        else if (drag_type == 1)
            scaleX = 2.0f * (x - center.X) / abs(p1.X - p2.X);
        // Scale Y only
        else if (drag_type == 2)
            scaleY = 2.0f * (center.Y - y) / abs(p1.Y - p2.Y);
        // Move center
        else if (drag_type == 0)
            center = ui::Point(x, y);
        // Rotate
        else {
            prev_rotation = rotation;
            rotation = atan2(y - center.Y, x - center.X) / PI * 180;
            if (rotation < 0)
                rotation = 360 + rotation;

            // Round rotation to nearest 1
            rotation = round(rotation);
            rotation_input->SetText(String::Build(rotation));
        }

        scalex_input->SetText(String::Build((int)(100 * scaleX) / 100.0f));
        scaley_input->SetText(String::Build((int)(100 * scaleY) / 100.0f));
    }

    if (dragging == 2) {
        Position.X = x - dragdx;
		Position.Y = y - dragdy;

		if (Position.X < 0) Position.X = 0;
		if (Position.Y < 0) Position.Y = 0;
		if (Position.X + Size.X > XRES) Position.X = XRES - Size.X;
		if (Position.Y + Size.Y > YRES) Position.Y = YRES - Size.Y;
		messageLabel->ClearSelection(); // Avoid selecting while dragging
    }
}

void TransformWindow::DoDraw() {
    Graphics * g = GetGraphics();

    // Restrict scale
    if (fabs(scaleX) > 1000) {
        scaleX = isign(scaleX) * 1000;
        scalex_input->SetText(String::Build((int)scaleX));
    }
    if (fabs(scaleY) > 1000) {
        scaleY = isign(scaleY) * 1000;
        scaley_input->SetText(String::Build((int)scaleY));
    }

    // Create corners and resized corners (for bounding box)
    int c1x = p1.X, c1y = p1.Y, c2x = p1.X, c2y = p2.Y,
        c3x = p2.X, c3y = p1.Y, c4x = p2.X, c4y = p2.Y;
    int width = abs(p1.X - p2.X), height = abs(p1.Y - p2.Y);
    int n1x = center.X - width * fabs(scaleX) / 2, n2x = n1x,
        n3x = center.X + width * fabs(scaleX) / 2, n4x = n3x,
        n1y = center.Y - height * fabs(scaleY) / 2, n3y = n1y,
        n2y = center.Y + height * fabs(scaleY) / 2, n4y = n2y;

    radius = std::min(abs(width * scaleX), abs(height * scaleY)) / 3;
    radius = std::max(radius, 30);

    // Draw original bounding box
    if (point_count >= 1) {
        // Original bounding box for selection
        g->draw_line(c1x, c1y, c2x, c2y, 200, 200, 200, 150);
        g->draw_line(c1x, c1y, c3x, c3y, 200, 200, 200, 150);
        g->draw_line(c4x, c4y, c2x, c2y, 200, 200, 200, 150);
        g->draw_line(c4x, c4y, c3x, c3y, 200, 200, 200, 150);

        // Rotated bounding box
        int n1x2 = n1x, n2x2 = n2x, n3x2 = n3x, n4x2 = n4x;
        int n1y2 = n1y, n2y2 = n2y, n3y2 = n3y, n4y2 = n4y;
        rotate_around(n1x2, n1y2, center.X, center.Y, rotation / 180 * PI);
        rotate_around(n2x2, n2y2, center.X, center.Y, rotation / 180 * PI);
        rotate_around(n3x2, n3y2, center.X, center.Y, rotation / 180 * PI);
        rotate_around(n4x2, n4y2, center.X, center.Y, rotation / 180 * PI);

        g->draw_line(n1x2, n1y2, n2x2, n2y2, 255, 255, 255, 100);
        g->draw_line(n1x2, n1y2, n3x2, n3y2, 255, 255, 255, 100);
        g->draw_line(n4x2, n4y2, n2x2, n2y2, 255, 255, 255, 100);
        g->draw_line(n4x2, n4y2, n3x2, n3y2, 255, 255, 255, 100);
    }
    // Draw Scale circle option and preview
    if (point_count >= 2) {
        // Rescsale or rotation has occured
        if (pscaleX != scaleX || pscaleY != scaleY || pcenter.X != center.X || pcenter.Y != center.Y || prev_rotation != rotation) {
            fastscale_vector<pixel_vector, ui::Colour>(pixels, scaleX, scaleY, buffer,
                width, height, center.X, center.Y, (p1.X + p2.X) / 2, (p1.Y + p2.Y) / 2);
            fastrot_vector<pixel_vector>(buffer, rotation / 180 * PI, transform_pixels, center.X, center.Y);
        }

        for (auto &t : transform_pixels)
            g->drawrect(t.first.X, t.first.Y, 1, 1, t.second.Red, t.second.Green, t.second.Blue, t.second.Alpha);

        // Draw rescaled box
        g->draw_line(n1x, n1y, n2x, n2y, 255, 255, 255, 150);
        g->draw_line(n1x, n1y, n3x, n3y, 255, 255, 255, 150);
        g->draw_line(n4x, n4y, n2x, n2y, 255, 255, 255, 150);
        g->draw_line(n4x, n4y, n3x, n3y, 255, 255, 255, 150);

        // Draw selector circles for bounding
        g->fillcircle(center.X, center.Y - height * scaleY / 2, button_knob_radius, button_knob_radius, 255, 255, 255, 255);
        g->fillcircle(center.X, center.Y + height * scaleY / 2, button_knob_radius, button_knob_radius, 255, 255, 255, 255);
        g->fillcircle(center.X - width * fabs(scaleX) / 2, center.Y, button_knob_radius, button_knob_radius, 255, 255, 255, 255);
        g->fillcircle(center.X + width * fabs(scaleX) / 2, center.Y, button_knob_radius, button_knob_radius, 255, 255, 255, 255);
        g->fillcircle(n1x, n1y, button_knob_radius, button_knob_radius, 255, 255, 255, 255);
        g->fillcircle(n2x, n2y, button_knob_radius, button_knob_radius, 255, 255, 255, 255);
        g->fillcircle(n3x, n3y, button_knob_radius, button_knob_radius, 255, 255, 255, 255);
        g->fillcircle(n4x, n4y, button_knob_radius, button_knob_radius, 255, 255, 255, 255);

        // Selector for moving
        g->drawcircle(center.X, center.Y, move_knob_radius, move_knob_radius, 255, 255, 255, 255);

        // Rotate controller
        g->drawcircle(center.X, center.Y, radius, radius, 255, 255, 255, 155);
        g->fillcircle(center.X + radius * cos(rotation / 180 * PI), center.Y + radius * sin(rotation / 180 * PI),
            rotate_knob_radius, rotate_knob_radius, 255, 255, 255, 255);
    }

    g->clearrect(XRES, 0, WINDOWW - XRES, WINDOWH);
    g->clearrect(0, YRES - 1, WINDOWW, WINDOWH - YRES + 1);

    ui::Window::DoDraw();
}

void TransformWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}

void TransformTool::Click(Simulation * sim, Brush * brush, ui::Point position) {
    TransformWindow * t = new TransformWindow(this, sim, position);
    t->p1 = position;
    ++t->point_count;
}
