#include "Tool.h"

#include "client/Client.h"

#include "gui/Style.h"
#include "gui/dialogues/ErrorMessage.h"
#include "gui/game/Brush.h"
#include "gui/interface/Button.h"
#include "gui/interface/Keys.h"
#include "gui/interface/Label.h"
#include "gui/interface/ScrollPanel.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/Window.h"
#include "gui/game/config_tool/util.h"
#include "gui/interface/TextWrapper.h"

#include "simulation/Simulation.h"
#include "graphics/Graphics.h"

#include <iostream>

// Helper
String strip(const String &str) {
    String s(str);
    while (s.size() && s.EndsWith(" "))   s = s.Substr(0, s.size() - 1);
    while (s.size() && s.BeginsWith(" ")) s = s.Substr(1, s.size());
    return s;
}

enum PropMode { set, add, subtract, multiply, divide, XOR, modeAND, OR };

template <class T> T apply_method(T base, T modifier, int method) {
    switch (method) {
        case set: return modifier;
        case add: return base + modifier;
        case subtract: return base - modifier;
        case multiply: return base * modifier;
        case divide:
            if (modifier == 0.0f) return base; // Avoid division by 0
            return base / modifier;
        case modeAND: return (unsigned int)base & (unsigned int)modifier;
        case OR:  return (unsigned int)base | (unsigned int)modifier;
        case XOR: return (unsigned int)base ^ (unsigned int)modifier;
    }
    return modifier;
}

// Window
class PropertyWindow2: public ui::Window {
public:
	ui::Textbox * textField;
	PropertyTool2 * tool;
	Simulation *sim;
    ui::TextWrapper * textWrapper;

	std::vector<StructProperty> properties;
    std::vector<String> lines, error_lines;
    ui::Label * autocompleteLabel;
    std::vector<String> autocomplete_property;
    int autocomplete_index = 0;
    bool first_tab = true;

    const ui::Colour COLOR_WHITE = ui::Colour(255, 255, 255);
    const ui::Colour COLOR_ERROR = ui::Colour(255, 50, 0);

	PropertyWindow2(PropertyTool2 *tool_, Simulation *sim);
    void OkErrorCheck();
	void SetProperty();
	void OnDraw() override;
    void DoMouseDown(int x, int y, unsigned button) override;
	void OnKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt) override;

	void OnTryExit(ExitMethod method) override;
    void OnTryOkay(ui::Window::OkayMethod method) override;
	virtual ~PropertyWindow2() {}
};

PropertyWindow2::PropertyWindow2(PropertyTool2 * tool_, Simulation *sim_):
        ui::Window(ui::Point(-1, -1), ui::Point(200, 245)),
        tool(tool_),
        sim(sim_) {
    textWrapper = new ui::TextWrapper();
	properties = Particle::GetProperties();

	ui::Label * messageLabel = make_left_label(ui::Point(4, 5), ui::Point(Size.X-8, 14), "Edit properties");
	messageLabel->SetTextColour(style::Colour::InformationTitle);
	AddComponent(messageLabel);

    // 2 col for examples
     ui::Label * helpLabel = make_left_label(ui::Point(4, 20), ui::Point(Size.X-8, 14),
        "One property per line in the format\nprop:value, i.e.");
    helpLabel->Appearance.VerticalAlign = ui::Appearance::AlignTop;
    helpLabel->SetTextColour(ui::Colour(170, 170, 170));
	AddComponent(helpLabel);

    helpLabel = make_left_label(ui::Point(4, 36), ui::Point(Size.X-8, 14),
        "\n type:GOLD\n -life:100 \n +vx:10\n &ctype:0xFF");
    helpLabel->Appearance.VerticalAlign = ui::Appearance::AlignTop;
    helpLabel->SetTextColour(ui::Colour(255, 220, 170));
	AddComponent(helpLabel);

    helpLabel = make_left_label(ui::Point(70, 36), ui::Point(Size.X-8, 14),
        "\nSet type to gold\nSubtract 100 from life\nAdds 10 to vx\nBitwise AND the ctype");
    helpLabel->Appearance.VerticalAlign = ui::Appearance::AlignTop;
    helpLabel->SetTextColour(ui::Colour(170, 170, 170));
	AddComponent(helpLabel);

    helpLabel = make_left_label(ui::Point(4, 100), ui::Point(Size.X - 8, 14), "Operations: + - * / & ^ |");
    helpLabel->Appearance.VerticalAlign = ui::Appearance::AlignTop;
    helpLabel->SetTextColour(ui::Colour(170, 170, 170));
    AddComponent(helpLabel);

    ui::Button * okayButton = make_center_button(ui::Point(0, Size.Y-17), ui::Point(Size.X, 17), "OK");
	okayButton->Appearance.BorderInactive = ui::Colour(200, 200, 200);
	okayButton->SetActionCallback({ [this] {
        OnTryOkay(ui::Window::OkayButton);
	} });
	AddComponent(okayButton);
	SetOkayButton(okayButton);

    // Main property input
    ui::ScrollPanel *scrollPanel = new ui::ScrollPanel(ui::Point(8, 118), ui::Point(Size.X - 16, 17 * 6));
    AddComponent(scrollPanel);

    autocompleteLabel = make_left_label(ui::Point(0, 0), ui::Point(scrollPanel->Size.X, scrollPanel->Size.Y), "");
    autocompleteLabel->Appearance.VerticalAlign = ui::Appearance::AlignTop;
    autocompleteLabel->SetTextColour(ui::Colour(100, 100, 100));

    textField = new ui::Textbox(ui::Point(0, 0), ui::Point(scrollPanel->Size.X, scrollPanel->Size.Y), "", "[Properties]");
    textField->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
    textField->Appearance.VerticalAlign = ui::Appearance::AlignTop;
    textField->SetLimit(6000);
    textField->SetActionCallback({[this, scrollPanel] {
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

        // Update values
        PropertyWindow2::SetProperty();
    }});
    textField->SetInputType(ui::Textbox::Multiline);
    textField->SetMultiline(true);
    textField->SetText(Client::Ref().GetPrefString("Prop2.Value", ""));

    scrollPanel->AddChild(autocompleteLabel);
    scrollPanel->AddChild(textField);

    AddComponent(autocompleteLabel);
    AddComponent(textField);
	FocusComponent(textField);

	MakeActiveWindow();
    SetProperty(); // Initial error highlighting
}

void PropertyWindow2::DoMouseDown(int x, int y, unsigned button) {
    ui::Window::DoMouseDown(x, y, button);
}

void PropertyWindow2::SetProperty() {
    // Any keypress forces recalculation of autocomplete
    autocomplete_property.clear();
    autocomplete_index = 0;
    first_tab = true;
    tool->changed_type = false;

	if (textField->GetText().length() > 0) {
        tool->props.clear();
        error_lines.clear();
        textField->SetTextColour(COLOR_WHITE);
        lines = textField->GetText().PartitionBy('\n', true);

        String new_autocomplete_text = "";
        size_t index = 0;

        for (String &s : lines) {
            // Pre-define for goto
            bool line_contains_error = false;
            std::vector<String> temp;
            String prop, value;
            PropertyValue final_val;
            PropMode method;
            int propi = -1;

            if (!s.Contains(":") || s.EndsWith(":")) { // Invalid line
                line_contains_error = true;
                goto error_check;
            }

            // Define now since partition and such can cause errors if no : is present
            temp   = s.PartitionBy(':', true);
            prop   = strip(temp[0]);
            value  = temp[1];
            method = set;

            // Special modifiders, ie +10 adds 10 to current
            if (prop.BeginsWith('+')) {
                prop = prop.Substr(1, prop.size());
                method = add;
            }
            else if (prop.BeginsWith('-')) {
                prop = prop.Substr(1, prop.size());
                method = subtract;
            }
            else if (prop.BeginsWith('*')) {
                prop = prop.Substr(1, prop.size());
                method = multiply;
            }
            else if (prop.BeginsWith('/')) {
                prop = prop.Substr(1, prop.size());
                method = divide;
            }
             else if (prop.BeginsWith('&')) {
                prop = prop.Substr(1, prop.size());
                method = modeAND;
            }
             else if (prop.BeginsWith('^')) {
                prop = prop.Substr(1, prop.size());
                method = XOR;
            }
             else if (prop.BeginsWith('|')) {
                prop = prop.Substr(1, prop.size());
                method = OR;
            }

            value = value.Substitute(" ", "");
            prop = prop.Substitute(" ", "");

            for (size_t i = 0; i < properties.size(); i++)
                if (properties[i].Name.FromAscii().ToLower() == prop.ToLower()) {
                    propi = i;
                    break;
                }
            if (propi < 0) { // Invalid property name
                line_contains_error = true;
                goto error_check;
            }

            try {
                switch (properties[propi].Type) {
                    case StructProperty::Integer:
                    case StructProperty::ParticleType: {
                        int v;
                        if(value.length() > 2 && value.BeginsWith("0x")) // 0xC0FFEE
                            v = value.Substr(2).ToNumber<unsigned int>(Format::Hex());
                        else if(value.length() > 1 && value.BeginsWith("#")) // #C0FFEE
                            v = value.Substr(1).ToNumber<unsigned int>(Format::Hex());
                        else {
                            int type;
                            if ((type = sim->GetParticleType(value.ToUtf8())) != -1)
                                v = type;
                            else
                                v = value.ToNumber<int>();
                        }

                        if (properties[propi].Name == "type" && (v < 0 || v >= PT_NUM || !sim->elements[v].Enabled)) {
                            line_contains_error = true;
                            goto error_check;
                        }

                        final_val.Integer = v;
                        break;
                    }
                    case StructProperty::UInteger: {
                        unsigned int v;
                        if(value.length() > 2 && value.BeginsWith("0x")) // 0xC0FFEE
                            v = value.Substr(2).ToNumber<unsigned int>(Format::Hex());
                        else if(value.length() > 1 && value.BeginsWith("#")) // #C0FFEE	
                            v = value.Substr(1).ToNumber<unsigned int>(Format::Hex());
                        else
                            v = value.ToNumber<unsigned int>();
                        final_val.UInteger = v;
                        break;
                    }
                    case StructProperty::Float: {
                        if (value.EndsWith("C")) {
                            float v = value.SubstrFromEnd(1).ToNumber<float>();
                            final_val.Float = v + 273.15;
                        }
                        else if(value.EndsWith("F")) {
                            float v = value.SubstrFromEnd(1).ToNumber<float>();
                            final_val.Float = (v-32.0f)*5/9+273.15f;
                        }
                        else
                            final_val.Float = value.ToNumber<float>();
                        break;
                    }
                    default:
                        line_contains_error = true;
                        goto error_check;
                }
            } catch (const std::exception& ex) {
                line_contains_error = true;
                goto error_check;
            }

            if (properties[propi].Name.FromAscii() == "type") {
                tool->change_type = std::make_tuple(
                    properties[propi].Type, final_val, method,
                    properties[propi].Offset);
                tool->changed_type = true;
            } else {
                tool->props.push_back(std::make_tuple(
                    properties[propi].Type, final_val, method,
                    properties[propi].Offset));
            }
            error_check:;

            if (!strip(s).size())
                line_contains_error = false; // Empty lines are ignored
            if (line_contains_error)
                error_lines.push_back(s);

            // If error is on line the cursor is currently on don't highlight error
            if ((size_t)textField->GetCursorY() / 12 == index) { // 12 is text height, check if cursor is on current line
                line_contains_error = false;
                if (!s.Contains(':') && s.size()) { // Autocomplete
                    bool contains_modifier = s.BeginsWith('+') || s.BeginsWith('-') || s.BeginsWith('*') || s.BeginsWith('/')
                        || s.BeginsWith('&') || s.BeginsWith('|') || s.BeginsWith('^');
                    size_t s_size = contains_modifier ? s.size() - 1 : s.size();

                    for (size_t i = 0; i < properties.size(); i++) {
                        String lower_prop_str = properties[i].Name.FromAscii().ToLower();

                        // Check if property begins with s or s minus first letter (which can be a modifider ie +)
                        if ((!contains_modifier && lower_prop_str.BeginsWith(s.ToLower())) ||
                            (contains_modifier  && lower_prop_str.BeginsWith(s.Substr(1).ToLower()) && s.size() > 1)) {

                            // Suggest only the 1st result
                            if (!autocomplete_property.size()) {
                                for (size_t j = 0; j < s.size(); j++) // s.size() is correct, not s_size
                                    new_autocomplete_text += " ";
                                new_autocomplete_text = String::Build(new_autocomplete_text,
                                    lower_prop_str.Substr(s_size), ':', '\n');
                            }
                            autocomplete_property.push_back(String::Build(
                                lower_prop_str.Substr(s_size), ':'));
                        }
                    }
                }
            }
            if (!autocomplete_property.size())
                new_autocomplete_text = String::Build(new_autocomplete_text, '\n');
            if (line_contains_error)
                textField->SetTextColour(COLOR_ERROR);
            index++;
        }

        autocompleteLabel->SetText(new_autocomplete_text);
    }
    else {
        textField->SetTextColour(COLOR_WHITE);
        autocompleteLabel->SetText("");
        error_lines.clear();
    }
}

void PropertyWindow2::OnTryExit(ExitMethod method) {
    Client::Ref().SetPrefUnicode("Prop2.Value", textField->GetText());
	CloseActiveWindow();
	SelfDestruct();
}

void PropertyWindow2::OnTryOkay(ui::Window::OkayMethod method) {
    if (method == ui::Window::Enter) // Enter is newline in textbox, prevent exiting
        return;

    if (error_lines.size()) {
        String error_str = "";
        for (auto &s : error_lines)
            error_str = String::Build(error_str, s, '\n');
        new ErrorMessage("Could not set some properties:", error_str);
        return;
    }

    Client::Ref().SetPrefUnicode("Prop2.Value", textField->GetText());
    if (textField->GetText().length())
        SetProperty();

    delete textWrapper;
	CloseActiveWindow();
	SelfDestruct();
}

void PropertyWindow2::OnDraw() {
	Graphics * g = GetGraphics();

	g->clearrect(Position.X-2, Position.Y-2, Size.X+3, Size.Y+3);
	g->drawrect(Position.X, Position.Y, Size.X, Size.Y, 200, 200, 200, 255);
}

void PropertyWindow2::OnKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt) {
    FocusComponent(textField);

    // Ctrl-enter for exit override
    if (ctrl && (key == SDLK_KP_ENTER || key == SDLK_RETURN)) {
        OnTryOkay(ui::Window::OkayButton);
        return;
    }
    // Tab for autocomplete
    else if (key == SDLK_TAB && autocomplete_property.size()) {
        // Not first time autocompleting, remove previous autocomplete first
        if (!first_tab) {
            textField->SetText(textField->GetText().erase(
                textField->GetCursor() - autocomplete_property[(autocomplete_index - 1) % autocomplete_property.size()].size(),
                textField->GetCursor()
            ));
        }

        first_tab = false;
        textField->SetText(String::Build(textField->GetText(), autocomplete_property[autocomplete_index]));

        autocomplete_index = (autocomplete_index + 1) % autocomplete_property.size();
        autocompleteLabel->SetText("");
    }
}

void PropertyTool2::OpenWindow(Simulation *sim) {
	new PropertyWindow2(this, sim);
}

void PropertyTool2::SetProperty(Simulation *sim, ui::Point position) {
	if (position.X < 0 || position.X >XRES || position.Y < 0 || position.Y > YRES)
		return;
	int i = sim->pmap[position.Y][position.X];
	if (!i) i = sim->photons[position.Y][position.X];
	if (!i) return;

    // Try to change type
    // Props tuple format: type, val, method, offset
    if (changed_type) {
        int new_type = apply_method<int>(sim->parts[ID(i)].type, std::get<1>(change_type).Integer, (PropMode)std::get<2>(change_type));
        sim->part_change_type(ID(i), sim->parts[ID(i)].x+0.5f, sim->parts[ID(i)].y+0.5f, new_type);
    }

    // Change other props (same tuple format)
    for (auto &prop : props) {
        float nf; int ni; unsigned int nui;

        switch (std::get<0>(prop)) {
        	case StructProperty::Float:
                nf = apply_method<float>(*((float*)(((char*)&sim->parts[ID(i)]) + std::get<3>(prop))),
                    std::get<1>(prop).Float, (PropMode)std::get<2>(prop));
        		*((float*)(((char*)&sim->parts[ID(i)]) + std::get<3>(prop))) = nf;
        		break;
        	case StructProperty::ParticleType:
        	case StructProperty::Integer:
                ni = apply_method<int>(*((int*)(((char*)&sim->parts[ID(i)]) + std::get<3>(prop))),
                    std::get<1>(prop).Integer, (PropMode)std::get<2>(prop));
        		*((int*)(((char*)&sim->parts[ID(i)]) + std::get<3>(prop))) = ni;
        		break;
        	case StructProperty::UInteger:
                nui = apply_method<unsigned int>(*((unsigned int*)(((char*)&sim->parts[ID(i)]) + std::get<3>(prop))),
                    std::get<1>(prop).UInteger, (PropMode)std::get<2>(prop));
                *((unsigned int *)(((char *)&sim->parts[ID(i)]) + std::get<3>(prop))) = nui;
                break;
        	default:
        		break;
        }
    }
}

void PropertyTool2::Draw(Simulation *sim, Brush *cBrush, ui::Point position) {
	if (cBrush) {
        counter++;

		int radiusX = cBrush->GetRadius().X, radiusY = cBrush->GetRadius().Y, sizeX = cBrush->GetSize().X, sizeY = cBrush->GetSize().Y;
		unsigned char *bitmap = cBrush->GetBitmap();
		for(int y = 0; y < sizeY; y++)
			for(int x = 0; x < sizeX; x++)
				if(bitmap[(y*sizeX)+x] && (position.X+(x-radiusX) >= 0 && position.Y+(y-radiusY) >= 0 && position.X+(x-radiusX) < XRES && position.Y+(y-radiusY) < YRES))
					SetProperty(sim, ui::Point(position.X+(x-radiusX), position.Y+(y-radiusY)));
	}
}

void PropertyTool2::DrawLine(Simulation *sim, Brush *cBrush, ui::Point position, ui::Point position2, bool dragging) {
	int x1 = position.X, y1 = position.Y, x2 = position2.X, y2 = position2.Y;
	bool reverseXY = abs(y2-y1) > abs(x2-x1);
	int x, y, dx, dy, sy, rx = cBrush->GetRadius().X, ry = cBrush->GetRadius().Y;
	float e = 0.0f, de;
	if (reverseXY) {
		y = x1;
		x1 = y1;
		y1 = y;
		y = x2;
		x2 = y2;
		y2 = y;
	}
	if (x1 > x2) {
		y = x1;
		x1 = x2;
		x2 = y;
		y = y1;
		y1 = y2;
		y2 = y;
	}
	dx = x2 - x1;
	dy = abs(y2 - y1);
	if (dx)
		de = dy/(float)dx;
	else
		de = 0.0f;
	y = y1;
	sy = (y1<y2) ? 1 : -1;
	for (x=x1; x<=x2; x++) {
		if (reverseXY)
			Draw(sim, cBrush, ui::Point(y, x));
		else
			Draw(sim, cBrush, ui::Point(x, y));
		e += de;
		if (e >= 0.5f) {
			y += sy;
			if (!(rx+ry) && ((y1<y2) ? (y<=y2) : (y>=y2))) {
				if (reverseXY)
					Draw(sim, cBrush, ui::Point(y, x));
				else
					Draw(sim, cBrush, ui::Point(x, y));
			}
			e -= 1.0f;
		}
	}
}

void PropertyTool2::DrawRect(Simulation *sim, Brush *cBrush, ui::Point position, ui::Point position2) {
	int x1 = position.X, y1 = position.Y, x2 = position2.X, y2 = position2.Y;
	int i, j;
	if (x1>x2) {
		i = x2;
		x2 = x1;
		x1 = i;
	}
	if (y1>y2) {
		j = y2;
		y2 = y1;
		y1 = j;
	}
	for (j=y1; j<=y2; j++)
		for (i=x1; i<=x2; i++)
			SetProperty(sim, ui::Point(i, j));
}

void PropertyTool2::DrawFill(Simulation *sim, Brush *cBrush, ui::Point position) {
    CoordStack cs;
    int x, y, rx, ry;
    bool* allocated = new bool[YRES * XRES];
    
    std::fill(allocated, allocated + sizeof(allocated), false);
    cs.push(position.X, position.Y);

    try {
        do {
            cs.pop(x, y);
            allocated[y * XRES + x] = true;
            SetProperty(sim, ui::Point(x, y));

            for (rx = -1; rx <= 1; rx++)
            for (ry = -1; ry <= 1; ry++)
                if ((rx || ry) && x + rx < XRES && x + rx >= 0 && y + ry < YRES && y + ry >= 0 &&
                        !allocated[(y + ry) * XRES + (x + rx)]) {
                    cs.push(x + rx, y + ry);
                    allocated[(y + ry) * XRES + (x + rx)] = true;
                }
        } while (cs.getSize() > 0);
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        delete allocated;
        return;
    }

    delete allocated;
}
