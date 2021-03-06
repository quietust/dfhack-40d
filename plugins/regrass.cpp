// All above-ground soil not covered by buildings will be covered with grass.
// Necessary for worlds generated prior to version 0.31.19 - otherwise, outdoor shrubs and trees no longer grow.

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "DataDefs.h"
#include "df/world.h"
#include "df/world_raws.h"

#include "df/map_block.h"
#include "TileTypes.h"

using std::string;
using std::vector;
using namespace std;
using namespace DFHack;

using df::global::world;

DFHACK_PLUGIN("regrass");

command_result df_regrass (color_ostream &out, vector <string> & parameters);

DFhackCExport command_result plugin_init (color_ostream &out, std::vector<PluginCommand> &commands)
{
    commands.push_back(PluginCommand("regrass", "Regrows surface grass.", df_regrass, false,
        "Specify parameter 'max' to set all grass types to full density, otherwise only one type of grass will be restored per tile.\n"));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}

command_result df_regrass (color_ostream &out, vector <string> & parameters)
{
    bool max = false;
    if (!parameters.empty())
    {
        if(parameters[0] == "max")
            max = true;
        else
            return CR_WRONG_USAGE;
    }

    CoreSuspender suspend;

    int count = 0;
    for (size_t i = 0; i < world->map.map_blocks.size(); i++)
    {
        df::map_block *cur = world->map.map_blocks[i];

        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 16; x++)
            {
                if (   tileShape(cur->tiletype[x][y]) != tiletype_shape::FLOOR
                    || cur->designation[x][y].bits.subterranean
                    || cur->occupancy[x][y].bits.building)
                    continue;

                // don't touch furrowed tiles (dirt roads made on soil)
                if(tileSpecial(cur->tiletype[x][y]) == tiletype_special::FURROWED)
                    continue;

                int mat = tileMaterial(cur->tiletype[x][y]);
                if (   mat != tiletype_material::SOIL)
                    continue;

                cur->tiletype[x][y] = findRandomVariant((rand() & 1) ? tiletype::GrassLightFloor1 : tiletype::GrassDarkFloor1);
                count++;
            }
        }
    }

    if (count)
        out.print("Regrew %d tiles of grass.\n", count);
    return CR_OK;
}
