#include "Tool.h"

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

#include "gui/game/config_tool/CRAY.h"
#include "gui/game/config_tool/DRAY.h"
#include "gui/game/config_tool/FFGN.h"
#include "gui/game/config_tool/CLUD.h"
#include "gui/game/config_tool/TRBN.h"
#include "gui/game/config_tool/PSTN.h"
#include "gui/game/config_tool/FRME.h"

#include "gui/game/config_tool/detector.h"
#include "gui/game/config_tool/channel.h"
#include "gui/game/config_tool/quantum.h"
#include "gui/game/config_tool/wavelength.h"
#include "gui/game/config_tool/powered.h"
#include "gui/game/config_tool/temperature.h"
#include "gui/game/config_tool/life.h"
#include "gui/game/config_tool/force.h"
#include "gui/game/config_tool/ctype.h"

// Determine which config element to start
void ConfigTool::Click(Simulation * sim, Brush * brush, ui::Point position) {
    int type = TYP(sim->pmap[position.Y][position.X]);
    if (!type) type = TYP(sim->photons[position.Y][position.X]);

    switch(type) {
        case PT_WIFI:
        case PT_PRTI:
        case PT_PRTO:
            new ConfigChannelWindow(this, sim, position);
            return;
        case PT_ION:
            new ConfigQuantumWindow(this, sim, position);
            return;
        case PT_FILT:
        case PT_PHOT:
        case PT_PFLT:
            new ConfigWavelengthWindow(this, sim, position);
            return;
        case PT_FFGN:
            new ConfigFFGNWindow(this, sim, position);
            return;
        case PT_CRAY:
            new ConfigCRAYWindow(this, sim, position);
            return;
        case PT_CLUD:
            new ConfigCLUDWindow(this, sim, position);
            return;
        case PT_TRBN:
            new ConfigTRBNWindow(this, sim, position);
            return;
        case PT_DTEC:
        case PT_TSNS:
        case PT_PSNS:
        case PT_LSNS:
            new ConfigDetectorWindow(this, sim, position);
            return;
        case PT_HSWC:
        case PT_PVOD:
        case PT_PUMP:
            new ConfigPoweredWindow(this, sim, position);
            return;
        case PT_DRAY:
            new ConfigDRAYWindow(this, sim, position);
            return;
        case PT_PSTN:
            new ConfigPSTNWindow(this, sim, position);
            return;
        case PT_FRME:
            new ConfigFRMEWindow(this, sim, position);
            return;
        case PT_TIME:
        case PT_GPMP:
        case PT_TPRS:
        case PT_DLAY:
            new ConfigTemperatureWindow(this, sim, position);
            return;
        case PT_FRAY:
        case PT_RPEL:
            new ConfigForceWindow(this, sim, position);
            return;
        case PT_DEUT:
        case PT_MERC:
        case PT_ACEL:
        case PT_DCEL:
        case PT_SPRK:
            new ConfigLifeWindow(this, sim, position);
            return;
        default:
            break;
    }

    // Draw on ctype
    if (type == PT_VOID || sim->elements[type].CtypeDraw) {
        new ConfigCtypeWindow(this, sim, position);
        return;
    }
}
