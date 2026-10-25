#include "Tool.h"

#include "simulation/Simulation.h"

#include "gui/Style.h"
#include "gui/game/GameModel.h"
#include "gui/interface/Button.h"
#include "gui/interface/DropDown.h"
#include "gui/interface/Keys.h"
#include "gui/interface/Label.h"
#include "gui/interface/ScrollPanel.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/Window.h"

#include "graphics/Graphics.h"
#include "gui/interface/Engine.h"
#include "client/Client.h"
#include "gui/game/config_tool/util.h"
#include "gui/interface/TextWrapper.h"

#include "gui/game/texterfonts/font.h"
#include "gui/game/texterfonts/all-fonts.h"



// Construct new fonts
std::vector<TexterFont*> texter_fonts({
    new SMALLFONT::Small(),
    new JOHHNYYOUFONT::JohnnyouMono(),
    new TPTSTANDARDFONT::TPTStandard(),
    new BIGNORMALFONT::BigNormal(),
    new BIGBOLDFONT::BigBold()
});
std::vector<std::pair<String, float> > text_spacing_options({
    std::make_pair(String("1x"), 1.0f),
    std::make_pair(String("1.15x"), 1.15f), 
    std::make_pair(String("1.5x"), 1.5f),
    std::make_pair(String("2x"), 2.0f),
    std::make_pair(String("2.5x"), 2.5f),
    std::make_pair(String("3x"), 3.0f)
});

class TextWindow: public ui::Window {
public:
    TextTool * tool;
    ui::Textbox * textField;
    ui::Textbox * elementField;
	ui::DropDown * justification;
    ui::DropDown * font;
    ui::DropDown * spacing;
    Simulation * sim;
	ui::Point textPosition;
    std::vector<String> lines;
    ui::TextWrapper * textWrapper;

    int elecolor;
    String element;
    bool drag = false;

	TextWindow(TextTool * tool_, Simulation * sim_,  ui::Point position_);
	void OnDraw() override;
	void DoDraw() override;
	void DoMouseMove(int x, int y, int dx, int dy) override;
	void DoMouseDown(int x, int y, unsigned button) override;
	void DoMouseUp(int x, int y, unsigned button) override {
        ui::Window::DoMouseUp(x, y, button);
        drag = false;
	}
	void DoMouseWheel(int x, int y, int d) override {
        ui::Window::DoMouseWheel(x, y, d);
	}
	void DoKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt) override {
        // Ctrl-enter for exit override
        if (ctrl && (key == SDLK_KP_ENTER || key == SDLK_RETURN)) {
            OnTryOkay(ui::Window::OkayButton);
            return;
        }
        ui::Window::DoKeyPress(key, scan, repeat, shift, ctrl, alt);
	}
	void DoKeyRelease(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt) override {
        ui::Window::DoKeyRelease(key, scan, repeat, shift, ctrl, alt);
	}
	virtual ~TextWindow() {}
	void OnTryExit(ui::Window::ExitMethod method) override;
    void OnTryOkay(ui::Window::OkayMethod method) override;

    void changeTextLocation(int x, int y) {
        if (x > XRES) x = XRES;
        if (y > YRES) y = YRES;
        textPosition = ui::Point(x, y);
        Position.X = textPosition.X > XRES / 2 ? 50 : XRES / 2;
        Position.Y = textPosition.Y > YRES / 2 ? 50 : YRES / 3;
    }

    void onClose() {
        // Render text to sim
        texter_fonts[font->GetOption().second]->draw_sim(lines,
            justification->GetOption().second,
            text_spacing_options[spacing->GetOption().second].second,
            sim->GetParticleType(element.ToUtf8()),
            textPosition.X, textPosition.Y, sim);
        sim->model->SetPaused(true);

        Client::Ref().SetPref("Text.Align", justification->GetOption().second);
        Client::Ref().SetPref("Text.Spacing", spacing->GetOption().second);
        Client::Ref().SetPref("Text.Font", font->GetOption().second);
        delete textWrapper;
    }
};

TextWindow::TextWindow(TextTool * tool_, Simulation * sim_, ui::Point position_):
	ui::Window(ui::Point(-1, -1), ui::Point(250, 200)),
	tool(tool_),
	sim(sim_),
	textPosition(position_)
{
    textWrapper = new ui::TextWrapper();

    // Components
	ui::Label * messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X-8, 15), "Write text");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    ui::Button * cancelButton = make_center_button(ui::Point(0, Size.Y-16), ui::Point(Size.X / 2, 16), "Cancel");
    CANCEL_BUTTON(cancelButton);
	AddComponent(cancelButton);

	ui::Button * okayButton = make_center_button(ui::Point(Size.X / 2, Size.Y-16), ui::Point(Size.X / 2, 16), "OK");
	okayButton->SetActionCallback({ [this] {
        onClose();
        CloseActiveWindow();
        SelfDestruct();
    }});
	AddComponent(okayButton);
	SetOkayButton(okayButton);

    // Justification dropdown
	ui::Label * tempLabel = make_left_label(ui::Point(6, Size.Y - 42), ui::Point(40, 15), "Text Align:");
	AddComponent(tempLabel);

    justification = new ui::DropDown(ui::Point(70, Size.Y - 42), ui::Point(55, 16));
    AddComponent(justification);
    justification->AddOption(std::pair<String, int>(0xE020 + String(" Left"), TexterFont::Left));
    justification->AddOption(std::pair<String, int>(0xE01E + String(" Middle"), TexterFont::Middle));
	justification->AddOption(std::pair<String, int>(0xE01F + String(" Right"), TexterFont::Right));

    int just = Client::Ref().GetPrefInteger("Text.Align", 1);
    if (just < 0 || just > 3) just = 1;
	justification->SetOption(just);
	justification->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;

    // Font dropdown
    tempLabel = make_left_label(ui::Point(130, Size.Y - 42), ui::Point(40, 15), "Font:");
    AddComponent(tempLabel);

    font = new ui::DropDown(ui::Point(160, Size.Y - 42), ui::Point(80, 16));
    AddComponent(font);
   
    for (unsigned int i = 0; i < texter_fonts.size(); ++i)
        font->AddOption(std::pair<String, int>(String(texter_fonts[i]->name), i));
    int font_option = Client::Ref().GetPrefInteger("Text.Font", 0);
    if (font_option < 0 || font_option >= (int)texter_fonts.size())
        font_option = 0;
	font->SetOption(font_option);
    font->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;

    // Spacing dropdown
    tempLabel = make_left_label(ui::Point(130, Size.Y - 64), ui::Point(40, 15), "Spacing:");
    AddComponent(tempLabel);

    spacing = new ui::DropDown(ui::Point(200, Size.Y - 64), ui::Point(40, 16));
    AddComponent(spacing);
    for (unsigned int i = 0; i < text_spacing_options.size(); ++i)
        spacing->AddOption(std::make_pair(text_spacing_options[i].first, i));
    int space = Client::Ref().GetPrefInteger("Text.Spacing", 1);
    if (space < 0 || space >= (int)text_spacing_options.size())
        space = 1;
    spacing->SetOption(space);
    spacing->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
    
    tempLabel = make_left_label(ui::Point(6, Size.Y - 64), ui::Point(40, 15), "Element:");
    AddComponent(tempLabel);

    // Element name input
    elementField = make_left_textbox(ui::Point(70, Size.Y - 64), ui::Point(55, 17), element, "DMND");
	elementField->SetLimit(5);
    elementField->SetActionCallback({ [this] {
        // Change current element if type is valid
        if (sim->GetParticleType(elementField->GetText().ToUtf8()) != -1) {
            elementField->SetTextColour(ui::Colour(255, 255, 255));
            element = elementField->GetText();
            elecolor = sim->elements[sim->GetParticleType(elementField->GetText().ToUtf8())].Colour;
            Client::Ref().SetPrefUnicode("Text.Element", element);
        } else {
            int id = elementField->GetText().ToNumber<int>(true);
            if (id > 0 && id < PT_NUM) {
                elementField->SetTextColour(ui::Colour(255, 255, 255));
                element = sim->elements[id].Name;
                elecolor = sim->elements[id].Colour;
                Client::Ref().SetPrefUnicode("Text.Element", element);
            }
            else {
                elementField->SetTextColour(ui::Colour(255, 100, 0));
            }
        }
    }});
    AddComponent(elementField);

    // Textbox is in a scroll panel for scrolling
    ui::ScrollPanel *scrollPanel = new ui::ScrollPanel(ui::Point(8, 25), ui::Point(Size.X - 16, 17 * 6));
    AddComponent(scrollPanel);

	textField = new ui::Textbox(ui::Point(0, 0), ui::Point(scrollPanel->Size.X, scrollPanel->Size.Y), "", "[Text]");
    textField->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
    textField->Appearance.VerticalAlign = ui::Appearance::AlignTop;
    textField->SetLimit(6000);
	textField->SetActionCallback({ [this, scrollPanel] {
            lines = textField->GetText().PartitionBy('\n', true);
            int oldSize = textField->Size.Y;
            int oldScrollSize = scrollPanel->InnerSize.Y;

            textField->Size.Y = (FONT_H + 1) * textWrapper->Update(textField->GetText(), true, textField->Size.X);
            if (textField->Size.Y < scrollPanel->Size.Y)
                textField->Size.Y = scrollPanel->Size.Y;

            scrollPanel->InnerSize = ui::Point(scrollPanel->Size.X, textField->Position.Y + textField->Size.Y);
            
            // Auto scroll as ScrollPanel size increases
            if (oldSize < textField->Size.Y && oldScrollSize + scrollPanel->ViewportPosition.Y == scrollPanel->Size.Y)
                scrollPanel->SetScrollPosition(scrollPanel->InnerSize.Y - scrollPanel->Size.Y);
            else if (textField->Size.Y <= scrollPanel->Size.Y)
                scrollPanel->SetScrollPosition(0);
    }});
    textField->SetInputType(ui::Textbox::Multiline);
    textField->SetMultiline(true);
	scrollPanel->AddChild(textField);
    
	FocusComponent(textField);
	MakeActiveWindow();

    // Load preferences
    changeTextLocation(textPosition.X, textPosition.Y);

    element = Client::Ref().GetPrefString("Text.Element", "DMND");
    if (sim->GetParticleType(element.ToUtf8()) < 0)
        element = String("DMND"); // Default is DMND
    elecolor = sim->elements[sim->GetParticleType(element.ToUtf8())].Colour;
    elementField->SetText(element);
}

void TextWindow::OnTryExit(ui::Window::ExitMethod method) {
    if (method == ui::Window::MouseOutside) { // Clicking outside shouldn't exit - relocate the cursor instead
        changeTextLocation(ui::Engine::Ref().GetMouseX(), ui::Engine::Ref().GetMouseY());
        return;
    }
    onClose();
	CloseActiveWindow();
	SelfDestruct();
}

void TextWindow::OnTryOkay(ui::Window::OkayMethod method) {
    if (method == ui::Window::Enter) // Enter is newline in textbox, prevent exiting
        return;
    onClose();
	CloseActiveWindow();
	SelfDestruct();
}

void TextWindow::DoDraw() {
    Graphics * g = GetGraphics();

    // Ghost text for texter
    texter_fonts[font->GetOption().second]->draw_graphic(lines,
        justification->GetOption().second,
        text_spacing_options[spacing->GetOption().second].second,
        textPosition.X, textPosition.Y, g, elecolor);

    // Draw corner label
    StringBuilder coord_label;
    coord_label << textPosition.X << ", " << textPosition.Y << "\n";
    g->drawcircle(textPosition.X, textPosition.Y, 5, 5, 255, 255, 255, 105);
    g->drawtext(textPosition.X - g->textwidth(coord_label.Build()) / 2,
                textPosition.Y - 20, coord_label.Build(), 255, 255, 255, 255);

    ui::Window::DoDraw();
}

void TextWindow::DoMouseMove(int x, int y, int dx, int dy) {
    ui::Window::DoMouseMove(x, y, dx, dy);
    if (drag) changeTextLocation(x, y);
}

void TextWindow::DoMouseDown(int x, int y, unsigned button) {
    ui::Window::DoMouseDown(x, y, button);
    // Enable dragging if in the circle of radius 5
    if ((x - textPosition.X) * (x - textPosition.X) + (y - textPosition.Y) * (y - textPosition.Y) <= 25)
        drag = true;
}

void TextWindow::OnDraw() {
	Graphics * g = GetGraphics();
	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}

VideoBuffer * TextTool::GetIcon(int toolID, int width, int height) {
	VideoBuffer * newTexture = new VideoBuffer(width, height);
	for (int y=0; y<height; y++) {
		for (int x=0; x<width; x++) {
			pixel pc =  x==0||x==width-1||y==0||y==height-1 ? PIXPACK(0xA0A0A0) : PIXPACK(0x000000);
			newTexture->SetPixel(x, y, PIXR(pc), PIXG(pc), PIXB(pc), 255);
		}
	}
	newTexture->BlendCharacter((width/2)-6, (height/2)-6, 0xE079, 255, 255, 255, 255);
	return newTexture;
}

void TextTool::Click(Simulation * sim, Brush * brush, ui::Point position) {
    new TextWindow(this, sim, position);
}
