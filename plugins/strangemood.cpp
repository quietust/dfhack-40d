// Triggers a strange mood using (mostly) the same logic used in-game

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "modules/Gui.h"
#include "modules/Units.h"
#include "modules/Items.h"
#include "modules/Job.h"
#include "modules/Translation.h"
#include "modules/Random.h"

#include "DataDefs.h"
#include "df/init.h"
#include "df/world.h"
#include "df/ui.h"
#include "df/unit.h"
#include "df/skill_rating.h"
#include "df/unit_skill.h"
#include "df/unit_preference.h"
#include "df/map_block.h"
#include "df/job.h"
#include "df/buildings_other_id.h"
#include "df/items_other_id.h"
#include "df/material_type.h"
#include "df/matgloss_metal.h"
#include "df/historical_entity.h"
#include "df/entity_raw.h"
#include "df/general_ref_unit_workerst.h"

using std::string;
using std::vector;
using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::init;
using df::global::created_item_count;
using df::global::created_item_type;
using df::global::created_item_subtype;
using df::global::created_item_material;
using df::global::created_item_matgloss;

Random::MersenneRNG rng;

bool isUnitMoodable (df::unit *unit)
{
    if (!Units::isCitizen(unit))
        return false;
    if (!unit->status2.limbs_grasp_count)
        return false;
    if (unit->mood != mood_type::None)
        return false;
    if (!ENUM_ATTR(profession,moodable,unit->profession))
        return false;
    return true;
}

df::job_skill getMoodSkill (df::unit *unit)
{
    df::historical_entity *civ = df::historical_entity::find(unit->civ_id);
    vector<df::job_skill> skills;
    df::skill_rating level = skill_rating::Dabbling;
    for (size_t i = 0; i < unit->status.skills.size(); i++)
    {
        df::unit_skill *skill = unit->status.skills[i];
        switch (skill->id)
        {
        case job_skill::MINING:
        case job_skill::CARPENTRY:
        case job_skill::DETAILSTONE:
        case job_skill::MASONRY:
        case job_skill::TANNER:
        case job_skill::WEAVING:
        case job_skill::CLOTHESMAKING:
        case job_skill::FORGE_WEAPON:
        case job_skill::FORGE_ARMOR:
        case job_skill::FORGE_FURNITURE:
        case job_skill::CUTGEM:
        case job_skill::ENCRUSTGEM:
        case job_skill::WOODCRAFT:
        case job_skill::STONECRAFT:
        case job_skill::METALCRAFT:
        case job_skill::GLASSMAKER:
        case job_skill::LEATHERWORK:
        case job_skill::BONECARVE:
        case job_skill::BOWYER:
        case job_skill::MECHANICS:
            if (skill->rating > level)
            {
                skills.clear();
                level = skill->rating;
            }
            if (skill->rating == level)
                skills.push_back(skill->id);
            break;
        }
    }
    if (!skills.size() && civ)
    {
        if (civ->entity_raw->jobs.permitted_skill[job_skill::WOODCRAFT])
            skills.push_back(job_skill::WOODCRAFT);
        if (civ->entity_raw->jobs.permitted_skill[job_skill::STONECRAFT])
            skills.push_back(job_skill::STONECRAFT);
        if (civ->entity_raw->jobs.permitted_skill[job_skill::BONECARVE])
            skills.push_back(job_skill::BONECARVE);
    }
    if (!skills.size())
        skills.push_back(job_skill::STONECRAFT);
    return skills[rng.df_trandom(skills.size())];
}

int getCreatedMetalBars (int32_t metal)
{
    for (size_t i = 0; i < created_item_type->size(); i++)
    {
        if (created_item_type->at(i) == item_type::BAR &&
            created_item_subtype->at(i) == -1 &&
            created_item_material->at(i) == material_type::METAL &&
            created_item_matgloss->at(i) == metal)
            return created_item_count->at(i);
    }
    return 0;
}

void selectWord (const df::language_word_table &table, int32_t &word, df::enum_field<df::part_of_speech,int16_t> &part, int mode)
{
    if (table.parts[mode].size())
    {
        int offset = rng.df_trandom(table.parts[mode].size());
        word = table.words[mode][offset];
        part = table.parts[mode][offset];
    }
    else
    {
        word = rng.df_trandom(world->raws.language.words.size());
        part = (df::part_of_speech)(rng.df_trandom(9));
        Core::getInstance().getConsole().printerr("Impoverished Word Selector");
    }
}

void generateName(df::language_name &output, int language, const df::language_word_table &table1, const df::language_word_table &table2)
{
    for (int i = 0; i < 100; i++)
    {
        output = df::language_name();
        if (language == -1)
            language = rng.df_trandom(world->raws.language.translations.size());
        output.type = df::language_name_type::Artifact; // no need to support the other name types
        output.language = language;
        output.has_name = 1;
        if (output.language == -1)
            output.language = rng.df_trandom(world->raws.language.translations.size());
        int r, r2, r3;
        r = rng.df_trandom(3);
        if (r == 0 || r == 1)
        {
            if (rng.df_trandom(2))
            {
                selectWord(table2, output.words[0], output.parts_of_speech[0], 0);
                selectWord(table1, output.words[1], output.parts_of_speech[1], 1);
            }
            else
            {
                selectWord(table1, output.words[0], output.parts_of_speech[0], 0);
                selectWord(table2, output.words[1], output.parts_of_speech[1], 1);
            }
        }
        if (r == 1 || r == 2)
        {
            r2 = rng.df_trandom(2);
            if (r2)
                selectWord(table1, output.words[5], output.parts_of_speech[5], 2);
            else
                selectWord(table2, output.words[5], output.parts_of_speech[5], 2);
            r3 = rng.df_trandom(3);
            if (rng.df_trandom(50))
                r3 = rng.df_trandom(2);
            switch (r3)
            {
            case 0:
            case 2:
                if (r3 == 2)
                    r2 = rng.df_trandom(2);
                if (r2)
                    selectWord(table2, output.words[6], output.parts_of_speech[6], 5);
                else
                    selectWord(table1, output.words[6], output.parts_of_speech[6], 5);
                if (r3 == 0)
                    break;
                r2 = -r2;
            case 1:
                if (r2)
                    selectWord(table1, output.words[2], output.parts_of_speech[2], 3);
                else
                    selectWord(table2, output.words[2], output.parts_of_speech[2], 3);
                if (!(rng.df_trandom(100)))
                    selectWord(table1, output.words[3], output.parts_of_speech[3], 3);
                break;
            }
        }
        if (rng.df_trandom(100))
        {
            if (rng.df_trandom(2))
                selectWord(table1, output.words[4], output.parts_of_speech[4], 4);
            else
                selectWord(table2, output.words[4], output.parts_of_speech[4], 4);
        }
        if (output.words[2] != -1 && output.words[3] != -1 &&
            world->raws.language.words[output.words[3]]->adj_dist < world->raws.language.words[output.words[2]]->adj_dist)
        {
            std::swap(output.words[2], output.words[3]);
            std::swap(output.parts_of_speech[2], output.parts_of_speech[3]);
        }
        bool next = false;
        if ((output.parts_of_speech[5] == df::part_of_speech::NounPlural) && (output.parts_of_speech[6] == df::part_of_speech::NounPlural))
            next = true;
        if (output.words[0] != -1)
        {
            if (output.words[6] == -1) next = true;
            if (output.words[4] == -1) next = true;
            if (output.words[2] == -1) next = true;
            if (output.words[3] == -1) next = true;
            if (output.words[5] == -1) next = true;
        }
        if (output.words[1] != -1)
        {
            if (output.words[6] == -1) next = true;
            if (output.words[4] == -1) next = true;
            if (output.words[2] == -1) next = true;
            if (output.words[3] == -1) next = true;
            if (output.words[5] == -1) next = true;
        }
        if (output.words[4] != -1)
        {
            if (output.words[6] == -1) next = true;
            if (output.words[2] == -1) next = true;
            if (output.words[3] == -1) next = true;
            if (output.words[5] == -1) next = true;
        }
        if (output.words[2] != -1)
        {
            if (output.words[6] == -1) next = true;
            if (output.words[3] == -1) next = true;
            if (output.words[5] == -1) next = true;
        }
        if (output.words[3] != -1)
        {
            if (output.words[6] == -1) next = true;
            if (output.words[5] == -1) next = true;
        }
        if (output.words[5] != -1)
        {
            if (output.words[6] == -1) next = true;
        }
        if (!next)
            return;
    }
}

command_result df_strangemood (color_ostream &out, vector <string> & parameters)
{
    if (!Translation::IsValid())
    {
        out.printerr("Translation data unavailable!\n");
        return CR_FAILURE;
    }
    bool force = false;
    df::unit *unit = NULL;
    df::mood_type type = mood_type::None;
    df::job_skill skill = job_skill::NONE;

    for (size_t i = 0; i < parameters.size(); i++)
    {
        if(parameters[i] == "help" || parameters[i] == "?")
            return CR_WRONG_USAGE;
        else if(parameters[i] == "-force")
            force = true;
        else if(parameters[i] == "-unit")
        {
            unit = DFHack::Gui::getSelectedUnit(out);
            if (!unit)
                return CR_FAILURE;
        }
        else if (parameters[i] == "-type")
        {
            i++;
            if (i == parameters.size())
            {
                out.printerr("No mood type specified!\n");
                return CR_WRONG_USAGE;
            }
            if (parameters[i] == "fey")
                type = mood_type::Fey;
            else if (parameters[i] == "secretive")
                type = mood_type::Secretive;
            else if (parameters[i] == "possessed")
                type = mood_type::Possessed;
            else if (parameters[i] == "fell")
                type = mood_type::Fell;
            else if (parameters[i] == "macabre")
                type = mood_type::Macabre;
            else
            {
                out.printerr("Mood type '%s' not recognized!\n", parameters[i].c_str());
                return CR_WRONG_USAGE;
            }
        }
        else if (parameters[i] == "-skill")
        {
            i++;
            if (i == parameters.size())
            {
                out.printerr("No mood skill specified!\n");
                return CR_WRONG_USAGE;
            }
            else if (parameters[i] == "miner")
                skill = job_skill::MINING;
            else if (parameters[i] == "carpenter")
                skill = job_skill::CARPENTRY;
            else if (parameters[i] == "engraver")
                skill = job_skill::DETAILSTONE;
            else if (parameters[i] == "mason")
                skill = job_skill::MASONRY;
            else if (parameters[i] == "tanner")
                skill = job_skill::TANNER;
            else if (parameters[i] == "weaver")
                skill = job_skill::WEAVING;
            else if (parameters[i] == "clothier")
                skill = job_skill::CLOTHESMAKING;
            else if (parameters[i] == "weaponsmith")
                skill = job_skill::FORGE_WEAPON;
            else if (parameters[i] == "armorsmith")
                skill = job_skill::FORGE_ARMOR;
            else if (parameters[i] == "metalsmith")
                skill = job_skill::FORGE_FURNITURE;
            else if (parameters[i] == "gemcutter")
                skill = job_skill::CUTGEM;
            else if (parameters[i] == "gemsetter")
                skill = job_skill::ENCRUSTGEM;
            else if (parameters[i] == "woodcrafter")
                skill = job_skill::WOODCRAFT;
            else if (parameters[i] == "stonecrafter")
                skill = job_skill::STONECRAFT;
            else if (parameters[i] == "metalcrafter")
                skill = job_skill::METALCRAFT;
            else if (parameters[i] == "glassmaker")
                skill = job_skill::GLASSMAKER;
            else if (parameters[i] == "leatherworker")
                skill = job_skill::LEATHERWORK;
            else if (parameters[i] == "bonecarver")
                skill = job_skill::BONECARVE;
            else if (parameters[i] == "bowyer")
                skill = job_skill::BOWYER;
            else if (parameters[i] == "mechanic")
                skill = job_skill::MECHANICS;
            else
            {
                out.printerr("Mood skill '%s' not recognized!\n", parameters[i].c_str());
                return CR_WRONG_USAGE;
            }
        }
        else
        {
            out.printerr("Unrecognized parameter: %s\n", parameters[i].c_str());
            return CR_WRONG_USAGE;
        }
    }

    CoreSuspender suspend;

    // First, check if moods are enabled at all
    if (!init->game.flags.is_set(init_game_flags::ARTIFACTS))
    {
        out.printerr("ARTIFACTS are not enabled!\n");
        return CR_FAILURE;
    }
    if (*df::global::debug_nomoods)
    {
        out.printerr("Strange moods disabled via debug flag!\n");
        return CR_FAILURE;
    }
    if (ui->mood_cooldown && !force)
    {
        out.printerr("Last strange mood happened too recently!\n");
        return CR_FAILURE;
    }

    // Also, make sure there isn't a mood already running
    for (size_t i = 0; i < world->units.active.size(); i++)
    {
        df::unit *cur = world->units.active[i];
        if (Units::isCitizen(cur) && cur->flags1.bits.has_mood)
        {
            ui->mood_cooldown = 1000;
            out.printerr("A strange mood is already in progress!\n");
            return CR_FAILURE;
        }
    }

    // See which units are eligible to enter moods
    vector<df::unit *> moodable_units;
    bool mood_available = false;
    for (size_t i = 0; i < world->units.active.size(); i++)
    {
        df::unit *cur = world->units.active[i];
        if (!isUnitMoodable(cur))
            continue;
        if (!cur->flags1.bits.had_mood)
            mood_available = true;
        moodable_units.push_back(cur);
    }
    if (!mood_available)
    {
        out.printerr("No dwarves are available to enter a mood!\n");
        return CR_FAILURE;
    }

    // If unit was manually selected, redo checks explicitly
    if (unit)
    {
        if (!isUnitMoodable(unit))
        {
            out.printerr("Selected unit is not eligible to enter a strange mood!\n");
            return CR_FAILURE;
        }
        if (unit->flags1.bits.had_mood)
        {
            out.printerr("Selected unit has already had a strange mood!\n");
            return CR_FAILURE;
        }
    }

    // Obey in-game mood limits
    if (!force)
    {
        if (moodable_units.size() < 20)
        {
            out.printerr("Fortress is not eligible for a strange mood at this time - not enough moodable units.\n");
            return CR_FAILURE;
        }
        int num_items = 0;
        for (size_t i = 0; i < created_item_count->size(); i++)
            num_items += created_item_count->at(i);

        int num_revealed_tiles = 0;
        for (size_t i = 0; i < world->map.map_blocks.size(); i++)
        {
            df::map_block *blk = world->map.map_blocks[i];
            for (int x = 0; x < 16; x++)
                for (int y = 0; y < 16; y++)
                    if (blk->designation[x][y].bits.subterranean && !blk->designation[x][y].bits.hidden)
                        num_revealed_tiles++;
        }
        if (num_revealed_tiles / 2304 < ui->tasks.num_artifacts)
        {
            out.printerr("Fortress is not eligible for a strange mood at this time - not enough subterranean tiles revealed.\n");
            return CR_FAILURE;
        }
        if (num_items / 200 < ui->tasks.num_artifacts)
        {
            out.printerr("Fortress is not eligible for a strange mood at this time - not enough items created\n");
            return CR_FAILURE;
        }
    }

    // Randomly select a unit to enter a mood
    if (!unit)
    {
        vector<int32_t> tickets;
        for (size_t i = 0; i < moodable_units.size(); i++)
        {
            df::unit *cur = moodable_units[i];
            if (cur->flags1.bits.had_mood)
                continue;
            if (cur->relations.dragger_id != -1)
                continue;
            if (cur->relations.draggee_id != -1)
                continue;
            tickets.push_back(i);
            for (int j = 0; j < 5; j++)
                tickets.push_back(i);
            switch (cur->profession)
            {
            case profession::WOODWORKER:
            case profession::CARPENTER:
            case profession::BOWYER:
            case profession::STONEWORKER:
            case profession::MASON:
                for (int j = 0; j < 5; j++)
                    tickets.push_back(i);
                break;
            case profession::METALSMITH:
            case profession::WEAPONSMITH:
            case profession::ARMORER:
            case profession::BLACKSMITH:
            case profession::METALCRAFTER:
            case profession::JEWELER:
            case profession::GEM_CUTTER:
            case profession::GEM_SETTER:
            case profession::CRAFTSMAN:
            case profession::WOODCRAFTER:
            case profession::STONECRAFTER:
            case profession::LEATHERWORKER:
            case profession::BONE_CARVER:
            case profession::WEAVER:
            case profession::CLOTHIER:
            case profession::GLASSMAKER:
                for (int j = 0; j < 15; j++)
                    tickets.push_back(i);
                break;
            }
        }
        if (!tickets.size())
        {
            out.printerr("No units are eligible to enter a mood!\n");
            return CR_FAILURE;
        }
        unit = moodable_units[tickets[rng.df_trandom(tickets.size())]];
    }

    // Cancel selected unit's current job
    if (unit->job.current_job)
    {
        // TODO: cancel job
        out.printerr("Chosen unit '%s' has active job, cannot start mood!\n", Translation::TranslateName(&unit->name, false).c_str());
        return CR_FAILURE;
    }
    // TODO: remove moody dwarf from squad

    ui->mood_cooldown = 1000;
    // If no mood type was specified, pick one randomly
    if (type == mood_type::None)
    {
        if (rng.df_trandom(100) > unit->status.happiness)
        {
            switch (rng.df_trandom(2))
            {
            case 0: type = mood_type::Fell;    break;
            case 1: type = mood_type::Macabre; break;
            }
            // TODO: if creature has ITEMCORPSE, force to Macabre
        }
        else
        {
            switch (rng.df_trandom(3))
            {
            case 0: type = mood_type::Fey;         break;
            case 1: type = mood_type::Secretive;   break;
            case 2: type = mood_type::Possessed;   break;
            }
        }
    }

    // Display announcement and start setting up the mood job
    int color = 0;
    bool bright = false;
    string msg = Translation::TranslateName(&unit->name, false) + ", " + Units::getProfessionName(unit);

    switch (type)
    {
    case mood_type::Fey:
        color = 7;
        bright = true;
        msg += " is taken by a fey mood!";
        break;
    case mood_type::Secretive:
        color = 7;
        bright = false;
        msg += " withdraws from society...";
        break;
    case mood_type::Possessed:
        color = 5;
        bright = true;
        msg += " has been possessed!";
        break;
    case mood_type::Macabre:
        color = 0;
        bright = true;
        msg += " begins to stalk and brood...";
        break;
    case mood_type::Fell:
        color = 5;
        bright = false;
        msg += " looses a roaring laughter, fell and terrible!";
        break;
    default:
        out.printerr("Invalid mood type selected?\n");
        return CR_FAILURE;
    }

    unit->mood = type;
    Gui::showZoomAnnouncement(unit->pos, msg, color, bright);
    
    unit->status.happiness = 100;
    // TODO: make sure unit drops any wrestle items
    unit->job.mood_timeout = 50000;
    unit->flags1.bits.has_mood = true;
    unit->flags1.bits.had_mood = true;
    if (skill == job_skill::NONE)
        skill = getMoodSkill(unit);

    unit->job.mood_skill = skill;
    df::job *job = new df::job();
    Job::linkIntoWorld(job);

    // Choose the job type
    if (unit->mood == mood_type::Fell)
        job->job_type = job_type::StrangeMoodFell;
    else if (unit->mood == mood_type::Macabre)
        job->job_type = job_type::StrangeMoodBrooding;
    else
    {
        switch (skill)
        {
        case job_skill::MINING:
        case job_skill::MASONRY:
            job->job_type = job_type::StrangeMoodMason;
            break;
        case job_skill::CARPENTRY:
            job->job_type = job_type::StrangeMoodCarpenter;
            break;
        case job_skill::DETAILSTONE:
        case job_skill::WOODCRAFT:
        case job_skill::STONECRAFT:
        case job_skill::BONECARVE:
            job->job_type = job_type::StrangeMoodCrafter;
            break;
        case job_skill::TANNER:
        case job_skill::LEATHERWORK:
            job->job_type = job_type::StrangeMoodTanner;
            break;
        case job_skill::WEAVING:
        case job_skill::CLOTHESMAKING:
            job->job_type = job_type::StrangeMoodWeaver;
            break;
        case job_skill::FORGE_WEAPON:
        case job_skill::FORGE_ARMOR:
        case job_skill::FORGE_FURNITURE:
        case job_skill::METALCRAFT:
            // if there are any magma forges, then use one
            if (world->buildings.other[buildings_other_id::WORKSHOP_FORGE_MAGMA].size())
                job->job_type = job_type::StrangeMoodMagmaForge;
            else
                job->job_type = job_type::StrangeMoodForge;
            break;
        case job_skill::CUTGEM:
        case job_skill::ENCRUSTGEM:
            job->job_type = job_type::StrangeMoodJeweller;
            break;
        case job_skill::GLASSMAKER:
            job->job_type = job_type::StrangeMoodGlassmaker;
            break;
        case job_skill::BOWYER:
            job->job_type = job_type::StrangeMoodBowyer;
            break;
        case job_skill::MECHANICS:
            job->job_type = job_type::StrangeMoodMechanics;
            break;
        }
    }
    // Check which types of glass are available - we'll need this information later
    bool have_glass[3] = {false, false, false};
    for (size_t i = 0; i < created_item_type->size(); i++)
    {
        if (created_item_type->at(i) == item_type::ROUGH)
        {
            switch (created_item_material->at(i))
            {
            case material_type::GLASS_GREEN:
                have_glass[0] = true;
                break;
            case material_type::GLASS_CLEAR:
                have_glass[1] = true;
                break;
            case material_type::GLASS_CRYSTAL:
                have_glass[2] = true;
                break;
            }
        }
    }

    // The dwarf will want 1-3 of the base material
    int base_item_count = 1 + rng.df_trandom(3);
    // Gem Cutters and Gem Setters have a 50% chance of using only one base item
    if (((skill == job_skill::CUTGEM) || (skill == job_skill::ENCRUSTGEM)) && (rng.df_trandom(2)))
        base_item_count = 1;

    // Choose the base material
    if (job->job_type == job_type::StrangeMoodBrooding)
    {
        switch (rng.df_trandom(3))
        {
        case 0:
            unit->job.mood_item_type.push_back(item_type::REMAINS);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::NONE);
            unit->job.mood_matgloss.push_back(-1);
            break;
        case 1:
            unit->job.mood_item_type.push_back(item_type::BONES);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::NONE);
            unit->job.mood_matgloss.push_back(-1);
            break;
        case 2:
            unit->job.mood_item_type.push_back(item_type::SKULL);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::NONE);
            unit->job.mood_matgloss.push_back(-1);
            break;
        }
    }
    else if (job->job_type != job_type::StrangeMoodFell)
    {
        df::item *filter;
        bool found_pref;
        switch (skill)
        {
        case job_skill::MINING:
        case job_skill::DETAILSTONE:
        case job_skill::MASONRY:
        case job_skill::STONECRAFT:
        case job_skill::MECHANICS:
            unit->job.mood_item_type.push_back(item_type::STONE);
            unit->job.mood_item_subtype.push_back(-1);

            found_pref = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial &&
                    pref->material == material_type::STONE)
                {
                    unit->job.mood_material.push_back(pref->material);
                    found_pref = true;
                    break;
                }
            }
            if (!found_pref)
                unit->job.mood_material.push_back(material_type::STONE);
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::CARPENTRY:
        case job_skill::WOODCRAFT:
        case job_skill::BOWYER:
            unit->job.mood_item_type.push_back(item_type::WOOD);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::WOOD);
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::TANNER:
        case job_skill::LEATHERWORK:
            unit->job.mood_item_type.push_back(item_type::SKIN_TANNED);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::LEATHER);
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::WEAVING:
        case job_skill::CLOTHESMAKING:
            filter = NULL;
            // TODO: do proper search through world->items.other[items_other_id::ANY_GENERIC32] for item_type CLOTH, material 2, and flags2.deep_material
            for (size_t i = 0; i < world->items.other[items_other_id::ANY_GENERIC32].size(); i++)
            {
                filter = world->items.other[items_other_id::ANY_GENERIC32][i];
                if (filter->getType() != item_type::CLOTH)
                {
                    filter = NULL;
                    continue;
                }
                if (filter->getActualMaterial() != material_type::METAL)
                {
                    filter = NULL;
                    continue;
                }
                MaterialInfo mat(filter->getActualMaterial(), filter->getActualMatgloss());
                if (!mat.metal->flags.is_set(matgloss_metal_flags::DEEP))
                {
                    filter = NULL;
                    continue;
                }
                break;
            }
            if (filter)
            {
                unit->job.mood_item_type.push_back(item_type::CLOTH);
                unit->job.mood_item_subtype.push_back(-1);
                unit->job.mood_material.push_back(filter->getActualMaterial());
                unit->job.mood_matgloss.push_back(filter->getActualMatgloss());
            }
            else
            {
                unit->job.mood_item_type.push_back(item_type::CLOTH);
                unit->job.mood_item_subtype.push_back(-1);

                bool found_pref = false;
                for (size_t i = 0; i < unit->status.preferences.size(); i++)
                {
                    df::unit_preference *pref = unit->status.preferences[i];
                    if (pref->active == 1 &&
                        pref->type == df::unit_preference::T_type::LikeMaterial &&
                        ((pref->material == material_type::PLANT) || (pref->material == material_type::SILK)))
                    {
                        unit->job.mood_material.push_back(pref->material);
                        unit->job.mood_matgloss.push_back(-1);
                        found_pref = true;
                        break;
                    }
                }
                if (!found_pref)
                {
                    unit->job.mood_material.push_back(rng.df_trandom(2) ? material_type::SILK : material_type::PLANT);
                    unit->job.mood_matgloss.push_back(-1);
                }
            }
            break;

        case job_skill::FORGE_WEAPON:
        case job_skill::FORGE_ARMOR:
        case job_skill::FORGE_FURNITURE:
        case job_skill::METALCRAFT:
            filter = NULL;
            // TODO: do proper search through world->items.other[items_other_id::ANY_GENERIC32] for item_type BAR, material 2, and flags2.deep_material
            for (size_t i = 0; i < world->items.other[items_other_id::ANY_GENERIC32].size(); i++)
            {
                filter = world->items.other[items_other_id::ANY_GENERIC32][i];
                if (filter->getType() != item_type::BAR)
                {
                    filter = NULL;
                    continue;
                }
                if (filter->getActualMaterial() != material_type::METAL)
                {
                    filter = NULL;
                    continue;
                }
                MaterialInfo mat(filter->getActualMaterial(), filter->getActualMatgloss());
                if (!mat.metal->flags.is_set(matgloss_metal_flags::DEEP))
                {
                    filter = NULL;
                    continue;
                }
                break;
            }
            if (filter)
            {
                unit->job.mood_item_type.push_back(item_type::BAR);
                unit->job.mood_item_subtype.push_back(-1);
                unit->job.mood_material.push_back(filter->getActualMaterial());
                unit->job.mood_matgloss.push_back(filter->getActualMatgloss());
            }
            else
            {
                unit->job.mood_item_type.push_back(item_type::BAR);
                unit->job.mood_item_subtype.push_back(-1);
                unit->job.mood_material.push_back(material_type::METAL);

                vector<int32_t> metals;
                for (size_t i = 0; i < unit->status.preferences.size(); i++)
                {
                    df::unit_preference *pref = unit->status.preferences[i];
                    if (pref->active == 1 &&
                        pref->type == df::unit_preference::T_type::LikeMaterial &&
                        pref->material == material_type::METAL && getCreatedMetalBars(pref->matgloss) > 0)
                        metals.push_back(pref->matgloss);
                }
                if (metals.size())
                    unit->job.mood_matgloss.push_back(metals[rng.df_trandom(metals.size())]);
                else
                    unit->job.mood_matgloss.push_back(-1);
            }
            break;

        case job_skill::CUTGEM:
        case job_skill::ENCRUSTGEM:
            unit->job.mood_item_type.push_back(item_type::ROUGH);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::STONE);
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::GLASSMAKER:
            unit->job.mood_item_type.push_back(item_type::ROUGH);
            unit->job.mood_item_subtype.push_back(-1);

            found_pref = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial &&
                    ((pref->material == material_type::GLASS_GREEN) ||
                     (pref->material == material_type::GLASS_CLEAR && have_glass[1]) ||
                     (pref->material == material_type::GLASS_CRYSTAL && have_glass[2])))
                {
                    unit->job.mood_material.push_back(pref->material);
                    unit->job.mood_matgloss.push_back(pref->matgloss);
                    found_pref = true;
                }
            }
            if (!found_pref)
            {
                vector<df::material_type> mats;
                mats.push_back(material_type::GLASS_GREEN);
                if (have_glass[1])
                    mats.push_back(material_type::GLASS_CLEAR);
                if (have_glass[2])
                    mats.push_back(material_type::GLASS_CRYSTAL);
                unit->job.mood_material.push_back(mats[rng.df_trandom(mats.size())]);
                unit->job.mood_matgloss.push_back(-1);
            }
            break;

        case job_skill::BONECARVE:
            found_pref = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial &&
                    (pref->material == material_type::BONE || pref->material == material_type::SHELL))
                {
                    if (pref->material == material_type::BONE)
                        unit->job.mood_item_type.push_back(item_type::BONES);
                    else
                        unit->job.mood_item_type.push_back(item_type::SHELL);
                    unit->job.mood_item_subtype.push_back(-1);
                    unit->job.mood_material.push_back(material_type::NONE);
                    unit->job.mood_matgloss.push_back(-1);
                    found_pref = true;
                    break;
                }
            }
            if (!found_pref)
            {
                unit->job.mood_item_type.push_back(rng.df_trandom(2) ? item_type::SHELL : item_type::BONES);
                unit->job.mood_item_subtype.push_back(-1);
                unit->job.mood_material.push_back(material_type::NONE);
                unit->job.mood_matgloss.push_back(-1);
            }
            break;
        }
    }
    if (job->job_type != job_type::StrangeMoodFell && base_item_count > 1)
    {
        for (int i = 1; i < base_item_count; i++)
        {
            unit->job.mood_item_type.push_back(unit->job.mood_item_type[0]);
            unit->job.mood_item_subtype.push_back(unit->job.mood_item_subtype[0]);
            unit->job.mood_material.push_back(unit->job.mood_material[0]);
            unit->job.mood_matgloss.push_back(unit->job.mood_matgloss[0]);
        }
    }
    

    // Choose additional mood materials
    // Gem cutters/setters using a single gem require nothing else, and fell moods need only their corpse
    if (!(
         (((skill == job_skill::CUTGEM) || (skill == job_skill::ENCRUSTGEM)) && base_item_count == 1) ||
         (job->job_type == job_type::StrangeMoodFell)
       ))
    {
        int extra_items = std::min(rng.df_trandom((ui->tasks.num_artifacts * 20 + moodable_units.size()) / 20 + 1), 7);
        df::item_type avoid_type = item_type::NONE;
        int avoid_glass = 0;
        switch (skill)
        {
        case job_skill::MINING:
        case job_skill::DETAILSTONE:
        case job_skill::MASONRY:
        case job_skill::STONECRAFT:
            avoid_type = item_type::BLOCKS;
            break;
        case job_skill::CARPENTRY:
        case job_skill::WOODCRAFT:
        case job_skill::BOWYER:
            avoid_type = item_type::WOOD;
            break;
        case job_skill::TANNER:
        case job_skill::LEATHERWORK:
            avoid_type = item_type::SKIN_TANNED;
            break;
        case job_skill::WEAVING:
        case job_skill::CLOTHESMAKING:
            avoid_type = item_type::CLOTH;
            break;
        case job_skill::FORGE_WEAPON:
        case job_skill::FORGE_ARMOR:
        case job_skill::FORGE_FURNITURE:
        case job_skill::METALCRAFT:
            avoid_type = item_type::BAR;
            break;
        case job_skill::CUTGEM:
        case job_skill::ENCRUSTGEM:
            avoid_type = item_type::SMALLGEM;
        case job_skill::GLASSMAKER:
            avoid_glass = 1;
            break;
        }
        for (size_t i = 0; i < extra_items; i++)
        {
            if ((job->job_type == job_type::StrangeMoodBrooding) && (rng.df_trandom(2)))
            {
                switch (rng.df_trandom(3))
                {
                case 0:
                    unit->job.mood_item_type.push_back(item_type::REMAINS);
                    break;
                case 1:
                    unit->job.mood_item_type.push_back(item_type::BONES);
                    break;
                case 2:
                    unit->job.mood_item_type.push_back(item_type::SKULL);
                    break;
                }
                unit->job.mood_item_subtype.push_back(-1);
                unit->job.mood_material.push_back(material_type::NONE);
                unit->job.mood_matgloss.push_back(-1);
            }
            else
            {
                df::item_type item_type;
                df::material_type material;
                do
                {
                    item_type = item_type::NONE;
                    material = material_type::NONE;
                    switch (rng.df_trandom(11))
                    {
                    case 0:
                        item_type = item_type::WOOD;
                        material = material_type::NONE;
                        break;
                    case 1:
                        item_type = item_type::BAR;
                        material = material_type::METAL;
                        break;
                    case 2:
                        item_type = item_type::SMALLGEM;
                        material = material_type::NONE;
                        break;
                    case 3:
                        item_type = item_type::BLOCKS;
                        material = material_type::STONE;
                        break;
                    case 4:
                        item_type = item_type::ROUGH;
                        material = material_type::STONE;
                        break;
                    case 5:
                        item_type = item_type::STONE;
                        material = material_type::STONE;
                        break;
                    case 6:
                        item_type = item_type::BONES;
                        material = material_type::NONE;
                        break;
                    case 7:
                        item_type = item_type::SHELL;
                        material = material_type::NONE;
                        break;
                    case 8:
                        item_type = item_type::SKIN_TANNED;
                        material = material_type::NONE;
                        break;
                    case 9:
                        item_type = item_type::CLOTH;
                        switch (rng.df_trandom(2))
                        {
                        case 0:
                            material = material_type::PLANT;
                            break;
                        case 1:
                            material = material_type::SILK;
                            break;
                        }
                        break;
                    case 10:
                        item_type = item_type::ROUGH;
                        switch (rng.df_trandom(3))
                        {
                        case 0:
                            material = material_type::GLASS_GREEN;
                            break;
                        case 1:
                            material = material_type::GLASS_CLEAR;
                            break;
                        case 2:
                            material = material_type::GLASS_CRYSTAL;
                            break;
                        }
                        break;
                    }
                    if (unit->job.mood_item_type[0] == item_type && unit->job.mood_material[0] == material)
                        continue;
                    if (item_type == avoid_type)
                        continue;
                    if (avoid_glass && ((material == material_type::GLASS_GREEN) || (material == material_type::GLASS_CLEAR) || (material == material_type::GLASS_CRYSTAL)))
                        continue;
                    if ((material == material_type::GLASS_GREEN) && !have_glass[0])
                        continue;
                    if ((material == material_type::GLASS_CLEAR) && !have_glass[1])
                        continue;
                    if ((material == material_type::GLASS_CRYSTAL) && !have_glass[2])
                        continue;
                    break;
                } while (1);


                unit->job.mood_item_type.push_back(item_type);
                unit->job.mood_item_subtype.push_back(-1);
                unit->job.mood_material.push_back(material);
                unit->job.mood_matgloss.push_back(-1);
            }
        }
    }

    // Attach the Strange Mood job to the dwarf
    unit->path.dest.x = -30000;
    unit->path.dest.y = -30000;
    unit->path.dest.z = -30000;
    unit->path.goal = unit_path_goal::None;
    unit->path.path.x.clear();
    unit->path.path.y.clear();
    unit->path.path.z.clear();
    job->flags.bits.special = true;
    df::general_ref *ref = df::allocate<df::general_ref_unit_workerst>();
    ref->setID(unit->id);
    job->general_refs.push_back(ref);
    unit->job.current_job = job;
    job->wait_timer = 0;

    // Generate the artifact's name
    if (type == mood_type::Fell || type == mood_type::Macabre)
        generateName(unit->status.artifact_name, unit->name.language, world->raws.language.word_table[0][2], world->raws.language.word_table[1][2]);
    else
    {
        generateName(unit->status.artifact_name, unit->name.language, world->raws.language.word_table[0][1], world->raws.language.word_table[1][1]);
        if (!rng.df_trandom(100))
            unit->status.artifact_name = unit->name;
    }
    unit->mood_claimedWorkshop = 0;
    return CR_OK;
}

DFHACK_PLUGIN("strangemood");

DFhackCExport command_result plugin_init (color_ostream &out, std::vector<PluginCommand> &commands)
{
    commands.push_back(PluginCommand("strangemood", "Force a strange mood to happen.\n", df_strangemood, false,
        "Options:\n"
         "  -force         - Ignore standard mood preconditions.\n"
         "  -unit          - Use the selected unit instead of picking one randomly.\n"
         "  -type <type>   - Force the mood to be of a specific type.\n"
         "                   Valid types: fey, secretive, possessed, fell, macabre\n"
         "  -skill <skill> - Force the mood to use a specific skill.\n"
         "                   Skill name must be lowercase and without spaces.\n"
         "                   Example: miner, gemcutter, metalcrafter, bonecarver, mason\n"
    ));
    rng.init();

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}
