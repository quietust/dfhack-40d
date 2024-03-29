/*
https://github.com/peterix/dfhack
Copyright (c) 2009-2012 Petr Mrázek (peterix@gmail.com)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/


#include "Internal.h"

#include <string>
#include <vector>
#include <map>
using namespace std;

#include "modules/Gui.h"
#include "MemAccess.h"
#include "VersionInfo.h"
#include "Types.h"
#include "Error.h"
#include "ModuleFactory.h"
#include "Core.h"
#include "PluginManager.h"
#include "MiscUtils.h"
using namespace DFHack;

#include "modules/Job.h"
#include "modules/Screen.h"
#include "modules/Maps.h"

#include "DataDefs.h"
#include "df/world.h"
#include "df/global_objects.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_dungeonmodest.h"
#include "df/viewscreen_dungeon_monsterstatusst.h"
#include "df/viewscreen_unitjobsst.h"
#include "df/viewscreen_buildinglistst.h"
#include "df/viewscreen_itemst.h"
#include "df/viewscreen_layer.h"
#include "df/viewscreen_layer_workshop_profilest.h"
#include "df/viewscreen_layer_noblelistst.h"
#include "df/viewscreen_layer_assigntradest.h"
#include "df/viewscreen_layer_stockpilest.h"
#include "df/viewscreen_petst.h"
#include "df/viewscreen_tradegoodsst.h"
#include "df/viewscreen_storesst.h"
#include "df/ui_unit_view_mode.h"
#include "df/ui_sidebar_menus.h"
#include "df/ui_look_list.h"
#include "df/ui_advmode.h"
#include "df/job.h"
#include "df/ui_build_selector.h"
#include "df/building_workshopst.h"
#include "df/building_furnacest.h"
#include "df/building_trapst.h"
#include "df/building_civzonest.h"
#include "df/general_ref.h"
#include "df/unit_inventory_item.h"
#include "df/report.h"
#include "df/popup_message.h"
#include "df/interfacest.h"
#include "df/enabler.h"
#include "df/layer_object_listst.h"
#include "df/assign_trade_status.h"
#include "df/game_mode.h"
#include "df/unit.h"

using namespace df::enums;
using df::global::gview;
using df::global::init;
using df::global::enabler;
using df::global::ui;
using df::global::world;
using df::global::selection_rect;
using df::global::ui_menu_width;
using df::global::ui_area_map_width;
using df::global::gamemode;

static df::layer_object_listst *getLayerList(df::viewscreen_layer *layer, int idx)
{
    return virtual_cast<df::layer_object_listst>(vector_get(layer->layer_objects,idx));
}

static std::string getNameChunk(virtual_identity *id, int start, int end)
{
    if (!id)
        return "UNKNOWN";
    const char *name = id->getName();
    int len = strlen(name);
    if (len > start + end)
        return std::string(name+start, len-start-end);
    else
        return name;
}

/*
 * Classifying focus context by means of a string path.
 */

typedef void (*getFocusStringHandler)(std::string &str, df::viewscreen *screen);
static std::map<virtual_identity*, getFocusStringHandler> getFocusStringHandlers;

#define VIEWSCREEN(name) df::viewscreen_##name##st
#define DEFINE_GET_FOCUS_STRING_HANDLER(screen_type) \
    static void getFocusString_##screen_type(std::string &focus, VIEWSCREEN(screen_type) *screen);\
    DFHACK_STATIC_ADD_TO_MAP(\
        &getFocusStringHandlers, &VIEWSCREEN(screen_type)::_identity, \
        (getFocusStringHandler)getFocusString_##screen_type \
    ); \
    static void getFocusString_##screen_type(std::string &focus, VIEWSCREEN(screen_type) *screen)

DEFINE_GET_FOCUS_STRING_HANDLER(dwarfmode)
{
    using namespace df::enums::ui_sidebar_mode;

    using df::global::ui_workshop_in_add;
    using df::global::ui_build_selector;
    using df::global::ui_selected_unit;
    using df::global::ui_look_list;
    using df::global::ui_look_cursor;
    using df::global::ui_building_item_cursor;
    using df::global::ui_building_assign_type;
    using df::global::ui_building_assign_is_marked;
    using df::global::ui_building_assign_units;
    using df::global::ui_building_assign_items;
    using df::global::ui_building_in_assign;

    focus += "/" + enum_item_key(ui->main.mode);

    switch (ui->main.mode)
    {
    case QueryBuilding:
        if (df::building *selected = world->selected_building)
        {
            if (!selected->jobs.empty() &&
                selected->jobs[0]->job_type == job_type::DestroyBuilding)
            {
                focus += "/Destroying";
                break;
            }

            focus += "/Some";

            virtual_identity *id = virtual_identity::get(selected);

            bool jobs = false;

            if (id == &df::building_workshopst::_identity ||
                id == &df::building_furnacest::_identity)
            {
                focus += "/Workshop";
                jobs = true;
            }
            else if (id == &df::building_trapst::_identity)
            {
                auto trap = (df::building_trapst*)selected;
                focus += "/" + enum_item_key(trap->trap_type);
                if (trap->trap_type == trap_type::Lever)
                    jobs = true;
            }
            else if (ui_building_in_assign && *ui_building_in_assign &&
                     ui_building_assign_type && ui_building_assign_units &&
                     ui_building_assign_type->size() == ui_building_assign_units->size())
            {
                focus += "/Assign";
                if (ui_building_item_cursor)
                {
                    auto unit = vector_get(*ui_building_assign_units, *ui_building_item_cursor);
                    focus += unit ? "/Unit" : "/None";
                }
            }
            else
                focus += "/" + enum_item_key(selected->getType());

            if (jobs)
            {
                if (ui_workshop_in_add && *ui_workshop_in_add)
                    focus += "/AddJob";
                else if (!selected->jobs.empty())
                    focus += "/Job";
                else
                    focus += "/Empty";
            }
        }
        else
            focus += "/None";
        break;

    case Build:
        if (ui_build_selector)
        {
            // Not selecting, or no choices?
            if (ui_build_selector->building_type < 0)
                focus += "/Type";
            else if (ui_build_selector->stage != 2)
            {
                if (ui_build_selector->stage != 1)
                    focus += "/NoMaterials";
                else
                    focus += "/Position";

                focus += "/" + enum_item_key(ui_build_selector->building_type);
            }
            else
            {
                focus += "/Material";
                if (ui_build_selector->is_grouped)
                    focus += "/Groups";
                else
                    focus += "/Items";
            }
        }
        break;

    case ViewUnits:
        if (ui_selected_unit)
        {
            if (auto unit = vector_get(world->units.active, *ui_selected_unit))
            {
                focus += "/Some";

                using df::global::ui_unit_view_mode;

                if (ui_unit_view_mode)
                    focus += "/" + enum_item_key(ui_unit_view_mode->value);
            }
            else
                focus += "/None";
        }
        break;

    case LookAround:
        if (ui_look_list && ui_look_cursor)
        {
            auto item = vector_get(ui_look_list->items, *ui_look_cursor);
            if (item)
                focus += "/" + enum_item_key(item->type);
            else
                focus += "/None";
        }
        break;

    case BuildingItems:
        if (VIRTUAL_CAST_VAR(selected, df::building_actual, world->selected_building))
        {
            if (selected->contained_items.empty())
                focus += "/Some/Empty";
            else
                focus += "/Some/Item";
        }
        else
            focus += "/None";
        break;

    default:
        break;
    }
}

DEFINE_GET_FOCUS_STRING_HANDLER(dungeonmode)
{
    using df::global::ui_advmode;

    if (!ui_advmode)
        return;

    focus += "/" + enum_item_key(ui_advmode->menu);
}

DEFINE_GET_FOCUS_STRING_HANDLER(unitjobs)
{
    focus += "/" + enum_item_key(screen->mode);
}

DEFINE_GET_FOCUS_STRING_HANDLER(layer_workshop_profile)
{
    auto list1 = getLayerList(screen, 0);
    if (!list1) return;

    if (vector_get(screen->workers, list1->cursor))
        focus += "/Unit";
    else
        focus += "/None";
}

DEFINE_GET_FOCUS_STRING_HANDLER(layer_noblelist)
{
    auto list1 = getLayerList(screen, 0);
    auto list2 = getLayerList(screen, 1);
    if (!list1 || !list2) return;

    focus += "/" + enum_item_key(screen->mode);
}

DEFINE_GET_FOCUS_STRING_HANDLER(pet)
{
    focus += "/List";

    focus += vector_get(screen->is_vermin, screen->cursor) ? "/Vermin" : "/Unit";
}

DEFINE_GET_FOCUS_STRING_HANDLER(tradegoods)
{
    if (!screen->has_traders || screen->is_unloading)
        focus += "/NoTraders";
    else
        focus += (screen->in_right_pane ? "/Items/Broker" : "/Items/Trader");
}

DEFINE_GET_FOCUS_STRING_HANDLER(layer_assigntrade)
{
    auto list1 = getLayerList(screen, 0);
    auto list2 = getLayerList(screen, 1);
    if (!list1 || !list2) return;

    int list_idx = vector_get(screen->visible_lists, list1->cursor, (int16_t)-1);
    unsigned num_lists = sizeof(screen->lists)/sizeof(screen->lists[0]);
    if (unsigned(list_idx) >= num_lists)
        return;

    if (list1->active)
        focus += "/Groups";
    else
        focus += "/Items";
}

DEFINE_GET_FOCUS_STRING_HANDLER(stores)
{
    if (!screen->in_right_list)
        focus += "/Categories";
    else if (screen->in_group_mode)
        focus += "/Groups";
    else
        focus += "/Items";
}

DEFINE_GET_FOCUS_STRING_HANDLER(layer_stockpile)
{
    auto list1 = getLayerList(screen, 0);
    auto list2 = getLayerList(screen, 1);
    auto list3 = getLayerList(screen, 2);
    if (!list1 || !list2 || !list3 || !screen->settings) return;

    auto group = screen->cur_group;
    if (group != vector_get(screen->group_ids, list1->cursor))
        return;

    focus += "/" + enum_item_key(group);

    auto bits = vector_get(screen->group_bits, list1->cursor);
    if (bits.whole && !(bits.whole & screen->settings->item_categories.whole))
    {
        focus += "/Off";
        return;
    }

    focus += "/On";

    if (list2->active || list3->active || screen->list_ids.empty()) {
        focus += "/" + enum_item_key(screen->cur_list);

        if (list3->active)
            focus += (screen->item_names.empty() ? "/None" : "/Item");
    }
}

std::string Gui::getFocusString(df::viewscreen *top)
{
    if (!top)
        return "";

    if (virtual_identity *id = virtual_identity::get(top))
    {
        std::string name = getNameChunk(id, 11, 2);

        auto handler = map_find(getFocusStringHandlers, id);
        if (handler)
            handler(name, top);

        return name;
    }
    else if (dfhack_viewscreen::is_instance(top))
    {
        auto name = static_cast<dfhack_viewscreen*>(top)->getFocusString();
        return name.empty() ? "dfhack" : "dfhack/"+name;
    }
    else
    {
        Core &core = Core::getInstance();
        std::string name = core.p->readClassName(*(void**)top);
        return name.substr(11, name.size()-11-2);
    }
}

// Predefined common guard functions

bool Gui::default_hotkey(df::viewscreen *top)
{
    // Default hotkey guard function
    for (;top ;top = top->parent)
    {
        if (strict_virtual_cast<df::viewscreen_dwarfmodest>(top))
            return true;
        if (strict_virtual_cast<df::viewscreen_dungeonmodest>(top))
            return true;
    }
    return false;
}

bool Gui::dwarfmode_hotkey(df::viewscreen *top)
{
    // Require the main dwarf mode screen
    return !!strict_virtual_cast<df::viewscreen_dwarfmodest>(top);
}

bool Gui::unitjobs_hotkey(df::viewscreen *top)
{
    // Require the unit or jobs list
    return !!strict_virtual_cast<df::viewscreen_unitjobsst>(top);
}

bool Gui::item_details_hotkey(df::viewscreen *top)
{
    // Require the main dwarf mode screen
    return !!strict_virtual_cast<df::viewscreen_itemst>(top);
}

bool Gui::cursor_hotkey(df::viewscreen *top)
{
    if (!dwarfmode_hotkey(top))
        return false;

    // Also require the cursor.
    if (!df::global::cursor || df::global::cursor->x == -30000)
        return false;

    return true;
}

bool Gui::workshop_job_hotkey(df::viewscreen *top)
{
    using namespace ui_sidebar_mode;
    using df::global::ui;
    using df::global::world;
    using df::global::ui_workshop_in_add;
    using df::global::ui_workshop_job_cursor;

    if (!dwarfmode_hotkey(top))
        return false;

    switch (ui->main.mode) {
    case QueryBuilding:
        {
            if (!ui_workshop_job_cursor) // allow missing
                return false;

            df::building *selected = world->selected_building;
            if (!virtual_cast<df::building_workshopst>(selected) &&
                !virtual_cast<df::building_furnacest>(selected))
                return false;

            // No jobs?
            if (selected->jobs.empty() ||
                selected->jobs[0]->job_type == job_type::DestroyBuilding)
                return false;

            // Add job gui activated?
            if (ui_workshop_in_add && *ui_workshop_in_add)
                return false;

            return true;
        };
    default:
        return false;
    }
}

bool Gui::build_selector_hotkey(df::viewscreen *top)
{
    using namespace ui_sidebar_mode;
    using df::global::ui;
    using df::global::ui_build_selector;

    if (!dwarfmode_hotkey(top))
        return false;

    switch (ui->main.mode) {
    case Build:
        {
            if (!ui_build_selector) // allow missing
                return false;

            // Not selecting, or no choices?
            if (ui_build_selector->building_type < 0 ||
                ui_build_selector->stage != 2 ||
                ui_build_selector->choices.empty())
                return false;

            return true;
        };
    default:
        return false;
    }
}

bool Gui::view_unit_hotkey(df::viewscreen *top)
{
    using df::global::ui;
    using df::global::world;
    using df::global::ui_selected_unit;

    if (!dwarfmode_hotkey(top))
        return false;
    if (ui->main.mode != ui_sidebar_mode::ViewUnits)
        return false;
    if (!ui_selected_unit) // allow missing
        return false;

    return vector_get(world->units.active, *ui_selected_unit) != NULL;
}

bool Gui::unit_inventory_hotkey(df::viewscreen *top)
{
    using df::global::ui_unit_view_mode;

    if (!view_unit_hotkey(top))
        return false;
    if (!ui_unit_view_mode)
        return false;

    return ui_unit_view_mode->value == df::ui_unit_view_mode::Inventory;
}

df::job *Gui::getSelectedWorkshopJob(color_ostream &out, bool quiet)
{
    using df::global::world;
    using df::global::ui_workshop_job_cursor;

    if (!workshop_job_hotkey(Core::getTopViewscreen())) {
        if (!quiet)
            out.printerr("Not in a workshop, or no job is highlighted.\n");
        return NULL;
    }

    df::building *selected = world->selected_building;
    int idx = *ui_workshop_job_cursor;

    if (size_t(idx) >= selected->jobs.size())
    {
        out.printerr("Invalid job cursor index: %d\n", idx);
        return NULL;
    }

    return selected->jobs[idx];
}

bool Gui::any_job_hotkey(df::viewscreen *top)
{
    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_unitjobsst, top))
        return vector_get(screen->jobs, screen->cursor_pos) != NULL;

    return workshop_job_hotkey(top);
}

df::job *Gui::getSelectedJob(color_ostream &out, bool quiet)
{
    df::viewscreen *top = Core::getTopViewscreen();

    if (VIRTUAL_CAST_VAR(joblist, df::viewscreen_unitjobsst, top))
    {
        df::job *job = vector_get(joblist->jobs, joblist->cursor_pos);

        if (!job && !quiet)
            out.printerr("Selected unit has no job\n");

        return job;
    }
    else if (auto dfscreen = dfhack_viewscreen::try_cast(top))
        return dfscreen->getSelectedJob();
    else
        return getSelectedWorkshopJob(out, quiet);
}

static df::unit *getAnyUnit(df::viewscreen *top)
{
    using namespace ui_sidebar_mode;
    using df::global::ui;
    using df::global::world;
    using df::global::ui_look_cursor;
    using df::global::ui_look_list;
    using df::global::ui_selected_unit;

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_unitjobsst, top))
    {
        if (auto unit = vector_get(screen->units, screen->cursor_pos))
            return unit;
        if (auto job = vector_get(screen->jobs, screen->cursor_pos))
            return Job::getWorker(job);
        return NULL;
    }

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_dungeon_monsterstatusst, top))
        return screen->unit;

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_itemst, top))
    {
        df::general_ref *ref = vector_get(screen->entry_ref, screen->cursor_pos);
        return ref ? ref->getUnit() : NULL;
    }

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_layer_workshop_profilest, top))
    {
        if (auto list1 = getLayerList(screen, 0))
            return vector_get(screen->workers, list1->cursor);
        return NULL;
    }

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_layer_noblelistst, top))
    {
        switch (screen->mode)
        {
        case df::viewscreen_layer_noblelistst::List:
            if (auto list1 = getLayerList(screen, 0))
            {
                if (auto info = vector_get(screen->info, list1->cursor))
                    return info->unit;
            }
            return NULL;

        case df::viewscreen_layer_noblelistst::Appoint:
            if (auto list2 = getLayerList(screen, 1))
            {
                if (auto info = vector_get(screen->candidates, list2->cursor))
                    return info->unit;
            }
            return NULL;

        default:
            return NULL;
        }
    }

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_petst, top))
    {
        if (!vector_get(screen->is_vermin, screen->cursor))
            return vector_get(screen->animal, screen->cursor).unit;
        return NULL;
    }

    if (auto dfscreen = dfhack_viewscreen::try_cast(top))
        return dfscreen->getSelectedUnit();

    if (!Gui::dwarfmode_hotkey(top))
        return NULL;

    switch (ui->main.mode) {
    case ViewUnits:
    {
        if (!ui_selected_unit)
            return NULL;

        return vector_get(world->units.active, *ui_selected_unit);
    }
    case LookAround:
    {
        if (!ui_look_list || !ui_look_cursor)
            return NULL;

        auto item = vector_get(ui_look_list->items, *ui_look_cursor);
        if (item && item->type == df::ui_look_list::T_items::Unit)
            return item->unit;
        else
            return NULL;
    }
    default:
        return NULL;
    }
}

bool Gui::any_unit_hotkey(df::viewscreen *top)
{
    return getAnyUnit(top) != NULL;
}

df::unit *Gui::getSelectedUnit(color_ostream &out, bool quiet)
{
    df::unit *unit = getAnyUnit(Core::getTopViewscreen());

    if (!unit && !quiet)
        out.printerr("No unit is selected in the UI.\n");

    return unit;
}

static df::item *getAnyItem(df::viewscreen *top)
{
    using namespace ui_sidebar_mode;
    using df::global::ui;
    using df::global::world;
    using df::global::ui_look_cursor;
    using df::global::ui_look_list;
    using df::global::ui_unit_view_mode;
    using df::global::ui_building_item_cursor;
    using df::global::ui_sidebar_menus;

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_itemst, top))
    {
        df::general_ref *ref = vector_get(screen->entry_ref, screen->cursor_pos);
        return ref ? ref->getItem() : NULL;
    }

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_layer_assigntradest, top))
    {
        auto list1 = getLayerList(screen, 0);
        auto list2 = getLayerList(screen, 1);
        if (!list1 || !list2 || !list2->active)
            return NULL;

        int list_idx = vector_get(screen->visible_lists, list1->cursor, (int16_t)-1);
        unsigned num_lists = sizeof(screen->lists)/sizeof(std::vector<int32_t>);
        if (unsigned(list_idx) >= num_lists)
            return NULL;

        int idx = vector_get(screen->lists[list_idx], list2->cursor, -1);
        if (auto info = vector_get(screen->info, idx))
            return info->item;

        return NULL;
    }

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_tradegoodsst, top))
    {
        if (screen->in_right_pane)
            return vector_get(screen->items[1], screen->cursor[1]);
        else
            return vector_get(screen->items[0], screen->cursor[0]);
    }

    if (VIRTUAL_CAST_VAR(screen, df::viewscreen_storesst, top))
    {
        if (screen->in_right_list && !screen->in_group_mode)
            return vector_get(screen->items, screen->item_cursor);

        return NULL;
    }

    if (auto dfscreen = dfhack_viewscreen::try_cast(top))
        return dfscreen->getSelectedItem();

    if (!Gui::dwarfmode_hotkey(top))
        return NULL;

    switch (ui->main.mode) {
    case ViewUnits:
    {
        if (!ui_unit_view_mode || !ui_look_cursor || !ui_sidebar_menus)
            return NULL;

        if (ui_unit_view_mode->value != df::ui_unit_view_mode::Inventory)
            return NULL;

        auto inv_item = vector_get(ui_sidebar_menus->unit.inv_items, *ui_look_cursor);
        return inv_item ? inv_item->item : NULL;
    }
    case LookAround:
    {
        if (!ui_look_list || !ui_look_cursor)
            return NULL;

        auto item = vector_get(ui_look_list->items, *ui_look_cursor);
        if (item && item->type == df::ui_look_list::T_items::Item)
            return item->item;
        else
            return NULL;
    }
    case BuildingItems:
    {
        if (!ui_building_item_cursor)
            return NULL;

        VIRTUAL_CAST_VAR(selected, df::building_actual, world->selected_building);
        if (!selected)
            return NULL;

        auto inv_item = vector_get(selected->contained_items, *ui_building_item_cursor);
        return inv_item ? inv_item->item : NULL;
    }
    default:
        return NULL;
    }
}

bool Gui::any_item_hotkey(df::viewscreen *top)
{
    return getAnyItem(top) != NULL;
}

df::item *Gui::getSelectedItem(color_ostream &out, bool quiet)
{
    df::item *item = getAnyItem(Core::getTopViewscreen());

    if (!item && !quiet)
        out.printerr("No item is selected in the UI.\n");

    return item;
}

static df::building *getAnyBuilding(df::viewscreen *top)
{
    using namespace ui_sidebar_mode;
    using df::global::ui;
    using df::global::ui_look_list;
    using df::global::ui_look_cursor;
    using df::global::world;
    using df::global::ui_sidebar_menus;

    if (auto screen = strict_virtual_cast<df::viewscreen_buildinglistst>(top))
        return vector_get(screen->buildings, screen->cursor);

    if (auto dfscreen = dfhack_viewscreen::try_cast(top))
        return dfscreen->getSelectedBuilding();

    if (!Gui::dwarfmode_hotkey(top))
        return NULL;

    switch (ui->main.mode) {
    case LookAround:
    {
        if (!ui_look_list || !ui_look_cursor)
            return NULL;

        auto item = vector_get(ui_look_list->items, *ui_look_cursor);
        if (item && item->type == df::ui_look_list::T_items::Building)
            return item->building;
        else
            return NULL;
    }
    case QueryBuilding:
    case BuildingItems:
    {
        return world->selected_building;
    }
/*
    case Zones:
    case ZonesPitInfo:
    {
        if (ui_sidebar_menus)
            return ui_sidebar_menus->zone.selected;
        return NULL;
    }
*/
    default:
        return NULL;
    }
}

bool Gui::any_building_hotkey(df::viewscreen *top)
{
    return getAnyBuilding(top) != NULL;
}

df::building *Gui::getSelectedBuilding(color_ostream &out, bool quiet)
{
    df::building *building = getAnyBuilding(Core::getTopViewscreen());

    if (!building && !quiet)
        out.printerr("No building is selected in the UI.\n");

    return building;
}

//
void Gui::showZoomAnnouncement(df::coord pos, std::string message, int color, bool bright)
{
    showAnnouncement(message, color, bright);
    resetDwarfmodeView(true);
    revealInDwarfmodeMap(pos, true);
}

DFHACK_EXPORT void Gui::writeToGamelog(std::string message)
{
    if (message.empty())
        return;

    std::ofstream fseed("gamelog.txt", std::ios::out | std::ios::app);
    if(fseed.is_open())
        fseed << message << std::endl;
    fseed.close();
}

void Gui::showAnnouncement(std::string message, int color, bool bright)
{
    writeToGamelog(message);

    bool continued = false;

    while (!message.empty())
    {
        df::report *new_rep = new df::report();

        new_rep->color = color;
        new_rep->bright = bright;

        new_rep->flags.bits.continuation = continued;

        int size = std::min(message.size(), (size_t)73);
        strcpy(new_rep->text, message.substr(0, size).c_str());
        message = message.substr(size);

        continued = true;

        // Add the object to the lists

        gview->announcements.reports.push_back(new_rep);
        gview->announcements.report_timer = 2000;
    }
}

void Gui::showPopupAnnouncement(std::string message, int color, bool bright)
{
    df::popup_message *popup = new df::popup_message();
    popup->text = message.c_str();
    popup->color = color;
    popup->bright = bright;
    gview->announcements.popups.push_back(popup);
}

df::viewscreen *Gui::getCurViewscreen(bool skip_dismissed)
{
    df::viewscreen * ws = &gview->view;
    while (ws && ws->child)
        ws = ws->child;

    if (skip_dismissed)
    {
        while (ws && Screen::isDismissed(ws) && ws->parent)
            ws = ws->parent;
    }

    return ws;
}

df::coord Gui::getViewportPos()
{
    if (!df::global::window_x || !df::global::window_y || !df::global::window_z)
        return df::coord(0,0,0);

    return df::coord(*df::global::window_x, *df::global::window_y, *df::global::window_z);
}

df::coord Gui::getCursorPos()
{
    using df::global::cursor;
    if (!cursor)
        return df::coord();

    return df::coord(cursor->x, cursor->y, cursor->z);
}

Gui::DwarfmodeDims Gui::getDwarfmodeViewDims()
{
    DwarfmodeDims dims;

    auto ws = Screen::getWindowSize();
    dims.y1 = 1;
    dims.y2 = ws.y-2;
    dims.map_x1 = 1;
    dims.map_x2 = ws.x-2;
    dims.area_x1 = dims.area_x2 = dims.menu_x1 = dims.menu_x2 = -1;
    dims.menu_forced = false;

    int menu_pos = (ui_menu_width ? *ui_menu_width : 2);
    int area_pos = (ui_area_map_width ? *ui_area_map_width : 3);

    if (ui && ui->main.mode && menu_pos >= area_pos)
    {
        dims.menu_forced = true;
        menu_pos = area_pos-1;
    }

    dims.area_on = (area_pos < 3);
    dims.menu_on = (menu_pos < area_pos);

    if (dims.menu_on)
    {
        dims.menu_x2 = ws.x - 2;
        dims.menu_x1 = dims.menu_x2 - Gui::MENU_WIDTH + 1;
        if (menu_pos == 1)
            dims.menu_x1 -= Gui::AREA_MAP_WIDTH + 1;
        dims.map_x2 = dims.menu_x1 - 2;
    }
    if (dims.area_on)
    {
        dims.area_x2 = ws.x-2;
        dims.area_x1 = dims.area_x2 - Gui::AREA_MAP_WIDTH + 1;
        if (dims.menu_on)
            dims.menu_x2 = dims.area_x1 - 2;
        else
            dims.map_x2 = dims.area_x1 - 2;
    }

    return dims;
}

void Gui::resetDwarfmodeView(bool pause)
{
    using df::global::cursor;

    if (ui)
    {
        ui->main.mode = ui_sidebar_mode::Default;
    }

    if (selection_rect)
    {
        selection_rect->start_x = -30000;
        selection_rect->end_x = -30000;
    }

    if (cursor)
        cursor->x = cursor->y = cursor->z = -30000;

    if (pause && df::global::pause_state)
        *df::global::pause_state = true;
}

bool Gui::revealInDwarfmodeMap(df::coord pos, bool center)
{
    using df::global::window_x;
    using df::global::window_y;
    using df::global::window_z;

    if (!window_x || !window_y || !window_z || !world)
        return false;
    if (!Maps::isValidTilePos(pos))
        return false;

    auto dims = getDwarfmodeViewDims();
    int w = dims.map_x2 - dims.map_x1 + 1;
    int h = dims.y2 - dims.y1 + 1;

    *window_z = pos.z;

    if (center)
    {
        *window_x = pos.x - w/2;
        *window_y = pos.y - h/2;
    }
    else
    {
        while (*window_x + w < pos.x+5) *window_x += 10;
        while (*window_y + h < pos.y+5) *window_y += 10;
        while (*window_x + 5 > pos.x) *window_x -= 10;
        while (*window_y + 5 > pos.y) *window_y -= 10;
    }

    *window_x = std::max(0, std::min(*window_x, world->map.x_count-w));
    *window_y = std::max(0, std::min(*window_y, world->map.y_count-h));
    return true;
}

bool Gui::getViewCoords (int32_t &x, int32_t &y, int32_t &z)
{
    x = *df::global::window_x;
    y = *df::global::window_y;
    z = *df::global::window_z;
    return true;
}

bool Gui::setViewCoords (const int32_t x, const int32_t y, const int32_t z)
{
    (*df::global::window_x) = x;
    (*df::global::window_y) = y;
    (*df::global::window_z) = z;
    return true;
}

bool Gui::getCursorCoords (int32_t &x, int32_t &y, int32_t &z)
{
    x = df::global::cursor->x;
    y = df::global::cursor->y;
    z = df::global::cursor->z;
    return (x == -30000) ? false : true;
}

//FIXME: confine writing of coords to map bounds?
bool Gui::setCursorCoords (const int32_t x, const int32_t y, const int32_t z)
{
    df::global::cursor->x = x;
    df::global::cursor->y = y;
    df::global::cursor->z = z;
    return true;
}

bool Gui::getDesignationCoords (int32_t &x, int32_t &y, int32_t &z)
{
    x = df::global::selection_rect->start_x;
    y = df::global::selection_rect->start_y;
    z = df::global::selection_rect->start_z;
    return (x == -30000) ? false : true;
}

bool Gui::setDesignationCoords (const int32_t x, const int32_t y, const int32_t z)
{
    df::global::selection_rect->start_x = x;
    df::global::selection_rect->start_y = y;
    df::global::selection_rect->start_z = z;
    return true;
}

bool Gui::getMousePos (int32_t & x, int32_t & y)
{
    if (enabler && init) {
        x = enabler->mouse_x / (enabler->window_width / init->display.grid_x);
        y = enabler->mouse_y / (enabler->window_height / init->display.grid_y);
    }
    else {
        x = -1;
        y = -1;
    }
    return (x == -1) ? false : true;
}

bool Gui::getWindowSize (int32_t &width, int32_t &height)
{
    if (init) {
        width = init->display.grid_x;
        height = init->display.grid_y;
        return true;
    }
    else {
        width = 80;
        height = 25;
        return false;
    }
}

bool Gui::getMenuWidth(uint8_t &menu_width, uint8_t &area_map_width)
{
    menu_width = *df::global::ui_menu_width;
    area_map_width = *df::global::ui_area_map_width;
    return true;
}

bool Gui::setMenuWidth(const uint8_t menu_width, const uint8_t area_map_width)
{
    *df::global::ui_menu_width = menu_width;
    *df::global::ui_area_map_width = area_map_width;
    return true;
}
