// Speeds up item sorting within the Stocks screen

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <Modules/Items.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/item.h"
#include "df/viewscreen_storesst.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

bool isItemAvailInStocks (df::item *item)
{
    bool rv;
    __asm
    {
        mov eax, item
        mov ecx, 0x6fc0e0
        call ecx
        mov rv, al
    }
    return rv;
}

#define compare_sort(v1, v2) { auto a1 = (v1); auto a2 = (v2); if (a1 != a2) return a1 - a2; }

int qsort_item (const void *a, const void *b)
{
    df::item *item1 = *(df::item **)a;
    df::item *item2 = *(df::item **)b;

    compare_sort(isItemAvailInStocks(item2), isItemAvailInStocks(item1))
    compare_sort(item1->getType(), item2->getType())
    compare_sort(item1->getSubtype(), item2->getSubtype())
    compare_sort(item1->getMaterial(), item2->getMaterial())
    compare_sort(item1->getMatgloss(), item2->getMatgloss())
    compare_sort(Items::getValue(item2), Items::getValue(item1))
    compare_sort(item1->id, item2->id)
    return 0;
}


bool init_stores = true;
struct stocks_hook : df::viewscreen_storesst
{
    typedef df::viewscreen_storesst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        int old_cursor = category_cursor;
        INTERPOSE_NEXT(view)();
        if (init_stores || (old_cursor != category_cursor))
        {
            init_stores = false;
            qsort((void *)items.data(), items.size(), sizeof(void *), qsort_item);
        }
        if (breakdown_level != interface_breakdown_types::NONE)
        {
            init_stores = true;
            return;
        }
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(stocks_hook, view);

DFHACK_PLUGIN("stocks_fast");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(stocks_hook, view).apply(enable))
            return CR_FAILURE;

        is_enabled = enable;
        MemoryPatcher patcher;
        uint8_t *ptr = (uint8_t *)0x645f5c;
        patcher.verifyAccess(ptr, 2, true);
        if (is_enabled)
        {
            if (ptr[0] == 0x33 && ptr[1] == 0xFF)
            {
                ptr[0] = 0xEB;
                ptr[1] = 0x74;
            }
        }
        else
        {
            if (ptr[0] == 0xEB && ptr[1] == 0x74)
            {
                ptr[0] = 0x33;
                ptr[1] = 0xFF;
            }
        }
        patcher.verifyAccess(ptr, 2, false);
    }

    return CR_OK;
}

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    INTERPOSE_HOOK(stocks_hook, view).remove();
    return CR_OK;
}
