 // A container for random minor tweaks that don't fit anywhere else,
// in order to avoid creating lots of plugins and polluting the namespace.

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "modules/Materials.h"

#include "MiscUtils.h"

#include "DataDefs.h"
#include <VTableInterpose.h>
#include "df/item_actual.h"
#include "df/item_crafted.h"
#include "df/item_armorst.h"
#include "df/item_helmst.h"
#include "df/item_glovesst.h"
#include "df/item_shoesst.h"
#include "df/item_pantsst.h"
#include "df/item_clothst.h"

#include <stdlib.h>

using std::vector;
using std::string;
using namespace DFHack;
using namespace df::enums;

static command_result tweak(color_ostream &out, vector <string> & parameters);

DFHACK_PLUGIN("tweak");

DFhackCExport command_result plugin_init (color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "tweak", "Various tweaks for minor bugs.", tweak, false,
        "  tweak stable-temp [disable]\n"
        "    Fixes performance bug 6012 by squashing jitter in temperature updates.\n"
        "  tweak adamantine-cloth-wear [disable]\n"
        "    Stops adamantine clothing from wearing out while being worn (bug 6481).\n"
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown (color_ostream &out)
{
    return CR_OK;
}


struct stable_temp_hook : df::item_actual {
    typedef df::item_actual interpose_base;

    DEFINE_VMETHOD_INTERPOSE(bool, adjustTemperature, (uint16_t temp))
    {
        if (temperature.whole != temp)
        {
            // Bug 6012 is caused by fixed-point precision mismatch jitter
            // when an item is being pushed by two sources at N and N+1.
            // This check suppresses it altogether.
            if (temp == temperature.whole+1 ||
                (temp == temperature.whole-1 && temperature.fraction == 0))
                temp = temperature.whole;
            // When SPEC_HEAT is NONE, the original function seems to not
            // change the temperature, yet return true, which is silly.
            else if (getSpecHeat() == 60001)
                temp = temperature.whole;
        }

        return INTERPOSE_NEXT(adjustTemperature)(temp);
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(stable_temp_hook, adjustTemperature);

static bool inc_wear_timer (df::item_constructed *item, int amount)
{
    if (item->flags.bits.artifact)
        return false;

    MaterialInfo mat(item->material, item->matgloss);
    if (mat.isMetal() && mat.metal->flags.is_set(matgloss_metal_flags::DEEP))
        return false;

    item->wear_timer += amount;
    return (item->wear_timer > 806400);
}

struct adamantine_cloth_wear_armor_hook : df::item_armorst {
    typedef df::item_armorst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(bool, incWearTimer, (int amount))
    {
        return inc_wear_timer(this, amount);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(adamantine_cloth_wear_armor_hook, incWearTimer);

struct adamantine_cloth_wear_helm_hook : df::item_helmst {
    typedef df::item_helmst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(bool, incWearTimer, (int amount))
    {
        return inc_wear_timer(this, amount);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(adamantine_cloth_wear_helm_hook, incWearTimer);

struct adamantine_cloth_wear_gloves_hook : df::item_glovesst {
    typedef df::item_glovesst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(bool, incWearTimer, (int amount))
    {
        return inc_wear_timer(this, amount);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(adamantine_cloth_wear_gloves_hook, incWearTimer);

struct adamantine_cloth_wear_shoes_hook : df::item_shoesst {
    typedef df::item_shoesst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(bool, incWearTimer, (int amount))
    {
        return inc_wear_timer(this, amount);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(adamantine_cloth_wear_shoes_hook, incWearTimer);

struct adamantine_cloth_wear_pants_hook : df::item_pantsst {
    typedef df::item_pantsst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(bool, incWearTimer, (int amount))
    {
        return inc_wear_timer(this, amount);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(adamantine_cloth_wear_pants_hook, incWearTimer);

struct adamantine_cloth_wear_cloth_hook : df::item_clothst {
    typedef df::item_clothst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(bool, incWearTimer, (int amount))
    {
        return inc_wear_timer(this, amount);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(adamantine_cloth_wear_cloth_hook, incWearTimer);

static void enable_hook(color_ostream &out, VMethodInterposeLinkBase &hook, vector <string> &parameters)
{
    if (vector_get(parameters, 1) == "disable")
    {
        hook.remove();
        out.print("Disabled tweak %s (%s)\n", parameters[0].c_str(), hook.name());
    }
    else
    {
        if (hook.apply())
            out.print("Enabled tweak %s (%s)\n", parameters[0].c_str(), hook.name());
        else
            out.printerr("Could not activate tweak %s (%s)\n", parameters[0].c_str(), hook.name());
    }
}

static command_result tweak(color_ostream &out, vector <string> &parameters)
{
    CoreSuspender suspend;

    if (parameters.empty())
        return CR_WRONG_USAGE;

    string cmd = parameters[0];

    if (cmd == "stable-temp")
    {
        enable_hook(out, INTERPOSE_HOOK(stable_temp_hook, adjustTemperature), parameters);
    }
    else if (cmd == "adamantine-cloth-wear")
    {
        enable_hook(out, INTERPOSE_HOOK(adamantine_cloth_wear_armor_hook, incWearTimer), parameters);
        enable_hook(out, INTERPOSE_HOOK(adamantine_cloth_wear_helm_hook, incWearTimer), parameters);
        enable_hook(out, INTERPOSE_HOOK(adamantine_cloth_wear_gloves_hook, incWearTimer), parameters);
        enable_hook(out, INTERPOSE_HOOK(adamantine_cloth_wear_shoes_hook, incWearTimer), parameters);
        enable_hook(out, INTERPOSE_HOOK(adamantine_cloth_wear_pants_hook, incWearTimer), parameters);
        enable_hook(out, INTERPOSE_HOOK(adamantine_cloth_wear_cloth_hook, incWearTimer), parameters);
    }
    else
        return CR_WRONG_USAGE;

    return CR_OK;
}
