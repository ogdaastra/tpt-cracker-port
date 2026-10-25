#include "gui/game/Tool.h"

#include "simulation/Simulation.h"

#include "gui/Style.h"
#include "gui/interface/Window.h"
#include "gui/interface/Button.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/ScrollPanel.h"
#include "gui/game/GameModel.h"

#include "graphics/Graphics.h"
#include "gui/interface/Engine.h"

#include "simulation/quantum/quantum.h"
#include "gui/game/config_tool/util.h"

#include "Misc.h"
#include <cmath>

class ConfigQuantumWindow: public ui::Window {
public:
	ConfigTool * tool;
	Simulation * sim;
    ui::Point pos;

	ConfigQuantumWindow(ConfigTool *tool_, Simulation *sim_, ui::Point partpos);
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
	virtual ~ConfigQuantumWindow() {}
    void OnTryExit(ui::Window::ExitMethod method) override {
        CloseActiveWindow();
        SelfDestruct();
    }
};

ConfigQuantumWindow::ConfigQuantumWindow(ConfigTool * tool_, Simulation * sim_, ui::Point partpos):
	ui::Window(ui::Point(-1, -1), ui::Point(350, 200)),
	tool(tool_),
	sim(sim_),
    pos(partpos)
{
	ui::Label * messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X-8, 15), "Quantum State Information");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    ui::ScrollPanel * quantum_state = new ui::ScrollPanel(ui::Point(4, 20), ui::Point(Size.X - 8, Size.Y - 36));
    AddComponent(quantum_state);
    
    // Tmp2 saves the quantum state
    int state_id = -1;
    if (sim->photons[partpos.Y][partpos.X])
        state_id = sim->parts[ID(sim->photons[partpos.Y][partpos.X])].tmp2;

    if (!QUANTUM::quantum_states.count(state_id)) { // No quantum state exists
        ui::Label * error_label = new ui::Label(ui::Point(-1, -1), ui::Point(quantum_state->Size.X, quantum_state->Size.Y),
            "No quantum state exists for this particle");
        error_label->SetTextColour(RED_TEXT);
        quantum_state->AddChild(error_label);
    }
    else {
        StringBuilder state_title;
        state_title << QUANTUM::quantum_states[state_id].id_order.size() << "-particle state (State ID: " << state_id << ")";

        ui::Label * quantum_state_label = make_left_label(ui::Point(0, 0), ui::Point(quantum_state->Size.X, 15), state_title.Build());
        quantum_state_label->SetTextColour(LIGHT_GRAY_TEXT);
        quantum_state->AddChild(quantum_state_label);

        StringBuilder ids;
        ids << "IDs in state: ";
        for (unsigned int i = 0; i < QUANTUM::quantum_states[state_id].id_order.size(); ++i)
            ids << QUANTUM::quantum_states[state_id].id_order[i]
                << (i != QUANTUM::quantum_states[state_id].id_order.size() - 1 ? String(", ") : String(""));

        quantum_state_label = make_left_label(ui::Point(0, 13), ui::Point(quantum_state->Size.X, 15), ids.Build());
        quantum_state_label->SetTextColour(LIGHT_GRAY_TEXT);
        quantum_state->AddChild(quantum_state_label);
        
        int y = 0;
        for (auto &val : QUANTUM::quantum_states[state_id].state) {
            StringBuilder state1, state2, state3;
            state1 << "State: ";
            for (unsigned int pow = 0; pow < QUANTUM::quantum_states[state_id].id_order.size(); ++pow)
                state1 << (((1 << pow) & y) ? String("1") : String("0"));
            state2 << Format::Precision(3) << val.real() << "+" << val.imag() << "i";
            state3 << "Prob: " << Format::Precision(2) << (100 * abs(val) * abs(val)) << "%";
            
            ui::Label * header1 = new ui::Label(ui::Point(5, 34 + y * 15), ui::Point(quantum_state->Size.X, 15), state1.Build());
            ui::Label * header2 = new ui::Label(ui::Point(130, 34 + y * 15), ui::Point(quantum_state->Size.X, 15), state2.Build());
            ui::Label * header3 = new ui::Label(ui::Point(230, 34 + y * 15), ui::Point(quantum_state->Size.X, 15), state3.Build());
            
            if (abs(val) == 0) {
                header1->SetTextColour(ui::Colour(120, 120, 120));
                header2->SetTextColour(ui::Colour(120, 120, 120));
                header3->SetTextColour(ui::Colour(120, 120, 120));
            }
            else if (abs(val) == 1) {
                header1->SetTextColour(ui::Colour(0, 255, 100));
                header2->SetTextColour(ui::Colour(0, 255, 100));
                header3->SetTextColour(ui::Colour(0, 255, 100));
            }

            header1->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
            header2->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
            header3->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
            quantum_state->AddChild(header1);
            quantum_state->AddChild(header2);
            quantum_state->AddChild(header3);
            ++y;
        }

        // Allow for scrolling
        quantum_state->InnerSize = ui::Point(quantum_state->Size.X, 34 + (y + 1) * 15);
    }

	// Rest of window
	ui::Button * okayButton = make_center_button(ui::Point(0, Size.Y-16), ui::Point(Size.X, 16), "OK");
	okayButton->SetActionCallback({ [this] {
		CloseActiveWindow();
		SelfDestruct();
	}});
	AddComponent(okayButton);
	SetOkayButton(okayButton);
	MakeActiveWindow();
}

void ConfigQuantumWindow::DoDraw() {
	ui::Window::DoDraw();
}

void ConfigQuantumWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}