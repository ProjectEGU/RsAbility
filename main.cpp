#include <iostream>
#include <pthread.h>
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <time.h>
#include <assert.h>

using namespace std;
/**
 * Remember to consider order of fields in C, because they matter.
 * 
 * 12-25-2019 TODOs.
 * [ ] calculate the barge buff:   (abil)tick tick tick | tick tick tick | tick tick <BARGE> get buff.
 *          - Assumption made for channeled abilities: every 2 ticks, it will invoke a hit. therefore, there is a lastHitOffset of floor(castTime / 2.0) .
 *          - Alternate option is to forbid barge after channel.
 *          - Most versatile option is to create a "bargeHitTicksOffset" for specifying when the last hit of the ability occurs. The default value shall be 0.
 *                  -
 * [ ] estimation of DPS :  how to deal with <ALL STYLE> abilities when applying the ults ?
 *          - On the surface, it looks like we need to pick the correct style of ult to apply it to, but then that defeats the purpose of static evaluation.
 *          - However, we note that the abilities will be considered in sorted order. 
 *              This means, all ults will have been applied to stronger abilities - before the all-style ability was considered.
 *          - 
 * 
 * [ ] Precalculate ability table given input parameters:
 *      - Scrimshaw
 *      - Aura
 *      - Overloads
 * 
 * [-] Accomodate for ultis, and only apply for hits up to the affected amount of time.
 *      [-] sun/ds
 *      [-] zerk, zgs
 *      [ ] dbaxe
 * 
 * 
 * [ ] more efficient method for advancing abilities cooldown
 * 
 * [ ] (low priority) highly refined damage sequences which account for delayed hits of wild magic, etc.
 * */

const int MAX_ABIL_COUNT = 50;
const int MAX_DMG_SEQ = 20;
const int MAX_ABIL_SHARE = 6;

enum AbilityID
{
    abil_None,
    // all ults
    abil_DeathSwiftWithAuto,
    abil_SunshineWithAuto,
    abil_BerserkWithAuto,
    // abil_Metamorphosis, // not implemented
    abil_ZgsWithAuto,

    // magic thresh
    abil_WildMagic,
    abil_DeepImpact_F,
    abil_Asphyxiate,

    // magic basics
    abil_SonicWave,
    abil_ConcBlast,
    abil_DragonBreath,
    abil_Impact_F,
    abil_CorruptionBlast,
    abil_Combust,
    abil_Wrack,

    // universal
    abil_TuskaWrath,
    abil_Auto,

    AbilCount, // last ID
};

enum AbilStyle
{
    style_melee, // affected by zerk or zgs only.
    style_magic, // affected by sun only
    style_range, // affected by ds only
    style_all,   // affected by all damage boosting ultis
};

int buff_length_nat = 200;
int buff_length_sun = 64;
int buff_length_zerk = 34;
int buff_length_zgs = 34;
int buff_length_ds = 64;
int buff_length_barge = 10;
enum Buffs
{
    buff_nat,
    buff_sun,
    buff_zerk,
    buff_zgs,
    buff_ds,
    buff_barge,
    buffs_count
};

struct TimedBuffs
{
    int counters[buffs_count];
};

TimedBuffs default_buffs = {.counters = {0}};

struct Ability
{
    string name;
    int castTime;
    int adrReq;
    int adrGain;
    int cooldownMax;
    int totalDmg;

    int dmgSeq[MAX_DMG_SEQ];
    AbilStyle style;
    bool ult_boostable;
    bool hits_target;                        // whether or not the ability hits the target. this is used for barge calculations
    AbilityID sharedAbilIDs[MAX_ABIL_SHARE]; // is abilities shared.
    int sharedAbilCDs[MAX_ABIL_SHARE];       // the cooldown that should be set to the respective ability. if 0, then set the respective ability's original cooldown.

    int dmgSeqCumulative[MAX_DMG_SEQ + 1];      // stores the cumulative damage sequence where index i is the total damage done after i ticks.
    float sunDmgSeqCumulative[MAX_DMG_SEQ + 1]; // the cumulative damage sequence is always 0 at index 0.
    float dsDmgSeqCumulative[MAX_DMG_SEQ + 1];
    float zerkDmgSeqCumulative[MAX_DMG_SEQ + 1];
    float zgsDmgSeqCumulative[MAX_DMG_SEQ + 1];

    float dptCumulative[MAX_DMG_SEQ + 1]; // cumulative DPT given N ticks of the ability has surpassed.
    // damage accumulated over each tick. this array is used if the remaining time in the rotation is less than the cast time of this ability.
    // it is consequential that dmgSeq[MAX_DMG_SEQ] == totalDmg.
    // it is also consequential that dmgSeq is monotonically nondecreasing.

    // TODO: abilities may have shared cooldowns.
};
// define custom ability sort operation
struct abilities_desc_sort
{
    inline bool operator()(const Ability &a1, const Ability &a2)
    {
        return (a1.totalDmg > a2.totalDmg);
    }
};

struct AbilityTable
{
    struct Ability tbl[AbilCount];

    /* float max_burst_dpt = 0; // = max_burst_dmg / max_burst_ticks
    int max_burst_dmg = 0;
    int max_burst_ticks = 0;*/

    void init_abil_table()
    {
        // define abilities below. all dmgSeqCumulative will be generated from the dmgSeq afterwards.
        tbl[abil_None] = (Ability){
            .name = "None",
            .castTime = 0,
            .adrReq = 0,
            .adrGain = 0,
            .cooldownMax = 0,
            .totalDmg = 0,
            .dmgSeq = {0},
            .style = style_all,
            .ult_boostable = false,
            .hits_target = false,
        };
        tbl[abil_SunshineWithAuto] = (Ability){
            .name = "Sunshine (Auto)",
            .castTime = 3,
            .adrReq = 100,
            .adrGain = -90,
            .cooldownMax = 100,
            .totalDmg = 77,
            .dmgSeq = {77},
            .style = style_magic,
            .ult_boostable = true,
            .hits_target = true,
        };
        tbl[abil_WildMagic] = (Ability){
            .name = "Wild Magic",
            .castTime = 3,
            .adrReq = 50,
            .adrGain = -15,
            .cooldownMax = 34,
            .totalDmg = 265,
            .dmgSeq = {265},
            .style = style_magic,
            .ult_boostable = true,
            .hits_target = true,
        };
        tbl[abil_DeepImpact_F] = (Ability){
            .name = "Deep Impact (F)",
            .castTime = 3,
            .adrReq = 50,
            .adrGain = -15,
            .cooldownMax = 25,
            .totalDmg = 174,
            .dmgSeq = {174},
            .style = style_magic,
            .ult_boostable = true,
            .hits_target = true,
        };
        tbl[abil_Asphyxiate] = (Ability){
            .name = "Asphyxiate",
            .castTime = 7,
            .adrReq = 50,
            .adrGain = -15,
            .cooldownMax = 3,
            .totalDmg = 452,
            .dmgSeq = {113, 0, 113, 0, 113, 0, 113, 0},
            .style = style_magic,
            .ult_boostable = true,
            .hits_target = true,
            .sharedAbilIDs = {abil_Auto},
            .sharedAbilCDs = {8}, // cannot auto directly after asphyx. but asphyx cast time and auto's cooldown are the same, which would allow the auto to be cast right after asphyx, despite the mutual exclusion.
            // so, specify an explicit cooldown of 8.
        };
        tbl[abil_ConcBlast] = (Ability){
            .name = "Concentrated Blast",
            .castTime = 3,
            .adrReq = 0,
            .adrGain = 8,
            .cooldownMax = 9,
            .totalDmg = 94,
            .dmgSeq = {94},
            .style = style_magic,
            .ult_boostable = true,
            .hits_target = true,
            .sharedAbilIDs = {abil_Auto}, // cannot auto after conc blast.
        };
        tbl[abil_SonicWave] = (Ability){
            .name = "Sonic Wave",
            .castTime = 3,
            .adrReq = 0,
            .adrGain = 8,
            .cooldownMax = 9,
            .totalDmg = 94,
            .dmgSeq = {94},
            .style = style_magic,
            .ult_boostable = true,
            .hits_target = true,
        };
        tbl[abil_DragonBreath] = (Ability){
            .name = "Dragon Breath",
            .castTime = 3,
            .adrReq = 0,
            .adrGain = 8,
            .cooldownMax = 17,
            .totalDmg = 113,
            .dmgSeq = {113},
            .style = style_magic,
            .ult_boostable = true,
            .hits_target = true,
        };
        tbl[abil_Impact_F] = (Ability){
            .name = "Impact (F)",
            .castTime = 3,
            .adrReq = 0,
            .adrGain = 8,
            .cooldownMax = 25,
            .totalDmg = 132,
            .dmgSeq = {132},
            .style = style_magic,
            .ult_boostable = true,
            .hits_target = true,
        };
        tbl[abil_CorruptionBlast] = (Ability){
            .name = "Corruption Blast",
            .castTime = 3,
            .adrReq = 0,
            .adrGain = 8,
            .cooldownMax = 25,
            .totalDmg = 200,
            .dmgSeq = {200},
            .style = style_magic,
            .ult_boostable = false,
            .hits_target = true,
        };
        tbl[abil_Combust] = (Ability){
            .name = "Combust",
            .castTime = 3,
            .adrReq = 0,
            .adrGain = 8,
            .cooldownMax = 25,
            .totalDmg = 121,
            .dmgSeq = {121},
            .style = style_magic,
            .ult_boostable = false,
            .hits_target = true,
        };
        tbl[abil_TuskaWrath] = (Ability){
            .name = "Tuska's Wrath",
            .castTime = 3,
            .adrReq = 0,
            .adrGain = 8,
            .cooldownMax = 25,
            .totalDmg = 69,
            .dmgSeq = {69},
            .style = style_all,
            .ult_boostable = true,
            .hits_target = true,
        };
        tbl[abil_Wrack] = (Ability){
            .name = "Wrack",
            .castTime = 3,
            .adrReq = 0,
            .adrGain = 8,
            .cooldownMax = 5,
            .totalDmg = 56,
            .dmgSeq = {56},
            .style = style_magic,
            .ult_boostable = true,
            .hits_target = true,
        };
        tbl[abil_Auto] = (Ability){
            .name = "Auto",
            .castTime = 3,
            .adrReq = 0,
            .adrGain = 2,
            .cooldownMax = 7,
            .totalDmg = 77,
            .dmgSeq = {77},
            .style = style_all,
            .ult_boostable = true,
            .hits_target = true,
        };

        // for all abilities, convert the dmgSeq into dmgSeqCumulative
        // for all abilities, replace sharedAbilCDs of 0 with the CD of the original ability.

        // cout << "------ Ability tbl ------ " << endl;
        for (size_t i = 1; i < AbilCount; i++)
        {
            Ability *thisAbil = &tbl[i];

            thisAbil->dmgSeqCumulative[0] = 0;
            thisAbil->dptCumulative[0] = 0;
            thisAbil->dsDmgSeqCumulative[0] = 0;
            thisAbil->sunDmgSeqCumulative[0] = 0;
            thisAbil->zerkDmgSeqCumulative[0] = 0;
            thisAbil->zgsDmgSeqCumulative[0] = 0;
            for (size_t s = 1; s < MAX_DMG_SEQ + 1; s++)
            {
                thisAbil->dmgSeqCumulative[s] = thisAbil->dmgSeqCumulative[s - 1] + thisAbil->dmgSeq[s - 1];
                AbilStyle &style = thisAbil->style;
                thisAbil->dsDmgSeqCumulative[s] = ((thisAbil->ult_boostable && (style == style_all || style_range)) ? 1.50 : 1.0) * (thisAbil->dmgSeqCumulative[s]);
                thisAbil->zerkDmgSeqCumulative[s] = ((thisAbil->ult_boostable && (style == style_all || style_melee)) ? 2.00 : 1.0) * (thisAbil->dmgSeqCumulative[s]);
                thisAbil->zgsDmgSeqCumulative[s] = ((thisAbil->ult_boostable && (style == style_all || style_melee)) ? 1.25 : 1.0) * (thisAbil->dmgSeqCumulative[s]);
                thisAbil->sunDmgSeqCumulative[s] = ((thisAbil->ult_boostable && (style == style_all || style_magic)) ? 1.50 : 1.0) * (thisAbil->dmgSeqCumulative[s]);
                thisAbil->dptCumulative[s] = ((float)(thisAbil->dmgSeqCumulative[s])) / thisAbil->castTime;
            }
            assert(tbl[i].totalDmg == tbl[i].dmgSeqCumulative[MAX_DMG_SEQ]);

            for (size_t sa = 0; sa < MAX_ABIL_SHARE; sa++)
            {
                // if the ability did not specify a shared cooldown, then use the respective ability's shared CD.
                if (thisAbil->sharedAbilIDs[sa] == 0) // continue to next ability, if the shared ability is null
                    continue;
                if (thisAbil->sharedAbilCDs[sa] == 0) // if no explicit CD specified for shared ability, then use the original CD of the shared ability.
                    thisAbil->sharedAbilCDs[sa] = tbl[thisAbil->sharedAbilIDs[sa]].cooldownMax;
            }

            // cout << tbl[i].name << endl;

            // cout << "   dmgSeq=" << tbl[i].dmgSeq[0] << "," << tbl[i].dmgSeq[1] << "," << tbl[i].dmgSeq[2] << "," << tbl[i].dmgSeq[3] << "," << tbl[i].dmgSeq[4] << "," << tbl[i].dmgSeq[5] << endl;
            // cout << "   dmgSeqCumulative=" << tbl[i].dmgSeqCumulative[0] << "," << tbl[i].dmgSeqCumulative[1] << "," << tbl[i].dmgSeqCumulative[2] << "," << tbl[i].dmgSeqCumulative[3] << "," << tbl[i].dmgSeqCumulative[4] << "," << tbl[i].dmgSeqCumulative[5] << endl;
        }

        // cout << "------------------------- " << endl;

        // calculate the max burst dpt
    }
    inline Ability lookup(AbilityID id)
    {
        return tbl[id];
    }
};

AbilityTable abil_tbl;

struct AbState
{
    int ticksRemain[MAX_ABIL_COUNT];
    int totalDmg[MAX_ABIL_COUNT];
    int adrenaline[MAX_ABIL_COUNT];
    int coolDowns[AbilCount][MAX_ABIL_COUNT];

    int nAbil;                         // counter to the abilities used in the current state. assert: nAbil < MAX_ABIL_COUNT
    AbilityID abilSeq[MAX_ABIL_COUNT]; // list of abilities used in the current state: one-indexed. the first one is always "None".

    TimedBuffs buffs[MAX_ABIL_COUNT];

    void UseAbil(AbilityID abil)
    {
        // struct arrays will be indexed starting from 1.
        int oldIdx = this->nAbil;
        int newIdx = this->nAbil + 1;
        this->nAbil = newIdx;

        Ability abilInfo = abil_tbl.lookup(abil);

        int ticksAdvanced = min(abilInfo.castTime, ticksRemain[oldIdx]);

        // copy the abil cooldowns
        for (size_t i = 0; i < AbilCount; i++)
        {
            coolDowns[i][newIdx] = coolDowns[i][oldIdx] - ticksAdvanced;
        }

        // copy (but don't advance) buff timers
        for (size_t i = 0; i < buffs_count; i++)
        {
            buffs[newIdx].counters[i] = buffs[oldIdx].counters[i];
        }

        // check for refresh of buff timers due to their respective invoking-ability used. subtract ticksAdvanced from the result.
        switch (abil)
        {
        case abil_BerserkWithAuto:
            buffs[newIdx].counters[buff_zerk] = buff_length_zerk;
            break;
        case abil_DeathSwiftWithAuto:
            buffs[newIdx].counters[buff_ds] = buff_length_ds;
            break;
        case abil_SunshineWithAuto:
            buffs[newIdx].counters[buff_sun] = buff_length_sun;
            break;
        case abil_ZgsWithAuto:
            buffs[newIdx].counters[buff_zgs] = buff_length_zgs;
            break;
        default:
            break;
        }

        // calculate the part of the ability that is boosted by ulti.
        // the buff may only apply to parts of the ability.

        // dmgSeqCumulative stores the total damage done by the ability after x ticks, at index x.
        int abilDmg = 0;
        int ticksTotal = min(MAX_DMG_SEQ, ticksRemain[newIdx]); // ticks that the ability effect got applied. this is limited by the max_dmg_seq

        if (!abilInfo.ult_boostable)
        {
            // the ability is unaffected by ults, so just use its regular dmg seq.
            abilDmg = (abilInfo.dmgSeqCumulative[ticksTotal]);
        }
        // the ability is affected by ults:
        // if the style is style_all, then compute all individual boosts for each possible buff, and only take the maximum out of each.
        // otherwise, compute the boost only for its specific style.
        else if (abilInfo.style == style_magic || abilInfo.style == style_all)
        {
            int ticksBoostedSun = min(ticksTotal, buffs[newIdx].counters[buff_sun]);
            int sunDmgPortion = abilInfo.sunDmgSeqCumulative[ticksBoostedSun];
            int regDmgPortion = abilInfo.dmgSeqCumulative[ticksTotal] - abilInfo.dmgSeqCumulative[ticksBoostedSun];
            abilDmg = max(abilDmg, sunDmgPortion + regDmgPortion);
        }
        else if (abilInfo.style == style_range || abilInfo.style == style_all)
        {
            int ticksBoostedDs = min(ticksTotal, buffs[newIdx].counters[buff_ds]);
            int dsDmgPortion = abilInfo.dsDmgSeqCumulative[ticksBoostedDs];
            int regDmgPortion = abilInfo.dmgSeqCumulative[ticksTotal] - abilInfo.dmgSeqCumulative[ticksBoostedDs];
            abilDmg = max(abilDmg, dsDmgPortion + regDmgPortion);
        }
        else if (abilInfo.style == style_melee || abilInfo.style == style_all)
        {
            int ticksBoostedZerk = min(ticksTotal, buffs[newIdx].counters[buff_zerk]);
            // this is the end of the range of ticks boosted by zgs, after the zerk boost is gone.
            // it is also the total amount of ticks under zgs effect, including the zerk effect that is stacked with it.
            int ticksBoostedZgs = ticksBoostedZerk + max(0, min(ticksTotal, buffs[newIdx].counters[buff_zgs]) - ticksBoostedZerk);
            int zerkDmgPortion = abilInfo.zerkDmgSeqCumulative[ticksBoostedZerk];
            int zgsDmgPortion = abilInfo.zgsDmgSeqCumulative[ticksBoostedZgs] - abilInfo.zgsDmgSeqCumulative[ticksBoostedZerk];
            int regDmgPortion = abilInfo.dmgSeqCumulative[ticksTotal] - abilInfo.dmgSeqCumulative[ticksBoostedZgs];
            abilDmg = max(abilDmg, zerkDmgPortion + zgsDmgPortion + regDmgPortion);
        }
        else
        {
            cout << "Invalid Style. Ability Damage set to 0. " << endl;
        }

        totalDmg[newIdx] = totalDmg[oldIdx] + abilDmg;
        ticksRemain[newIdx] = ticksRemain[oldIdx] - ticksAdvanced;
        adrenaline[newIdx] = min(100, max(0, adrenaline[oldIdx] + abilInfo.adrGain));

        // advance buff timers
        for (size_t i = 0; i < buffs_count; i++)
        {
            buffs[newIdx].counters[i] -= ticksAdvanced;
        }

        // set currently used ability on cooldown
        coolDowns[abil][newIdx] = abilInfo.cooldownMax - ticksAdvanced;
        // set all consequent abilities on cooldown too
        for (size_t i = 0; i < MAX_ABIL_SHARE; i++)
        {
            AbilityID sharedAbil = abilInfo.sharedAbilIDs[i];
            int sharedCD = abilInfo.sharedAbilIDs[i];
            if (sharedAbil == 0)
                break;
            else
                coolDowns[sharedAbil][newIdx] = max(0, sharedCD - ticksAdvanced);
        }

        abilSeq[newIdx] = abil;
    }
    void RevertStep()
    {
        nAbil -= 1;
    }
    vector<AbilityID> GetAvailableAbil(vector<AbilityID> allowed)
    {
        // TODO: sort by dmg
        vector<AbilityID> avail;
        for (size_t i = 0; i < allowed.size(); i++)
        {
            AbilityID abilID = allowed[i];
            Ability abilInfo = abil_tbl.lookup(abilID);
            // check if ability is available: it is off cooldown and there is enough adrenaline.
            if (coolDowns[abilID][nAbil] <= 0 && adrenaline[nAbil] >= abilInfo.adrReq)
                avail.push_back(abilID);
        }

        return avail;
    }

    bool NoTimeLeft()
    {
        assert(ticksRemain[nAbil] >= 0);
        return ticksRemain[nAbil] == 0;
    }

    int EstimateRemainingDPT()
    {
        return 50; // hardcoded temp value
        // DPT estimation algorithm.

        // Abilities are sorted by their maximum potential DPT.
        // Calculate buff occurrence ticks.
    }

    int GetTicksRemaining() { return ticksRemain[nAbil]; }
    int GetDmg() { return totalDmg[nAbil]; }
    vector<AbilityID> GetAbilSeq()
    {
        vector<AbilityID> result;
        for (size_t i = 0; i < nAbil; i++)
        {
            result.push_back(abilSeq[i]);
        }
        return result;
    }
};
int default_cooldowns[AbilCount] = {0};

AbState best_state;
int best_dmg;

struct CalculationStats
{
    int endOfRotReached;
    int estimateDpsPruned;
};
CalculationStats calc_stats;

void CalcBest_R(AbState state, const vector<AbilityID> abilList)
{
    vector<AbilityID> nextAbils = state.GetAvailableAbil(abilList);

    // Optimization: RemainingDeeps
    float requiredDpt = ((float)(best_dmg - state.GetDmg()) / state.GetTicksRemaining()); // dpt required to beat the best sequence
    if (state.GetTicksRemaining() > 3 && requiredDpt > state.EstimateRemainingDPT())      // compare required DPT with the maximum achievable DPT.
    {
        // if the required DPT is greater than the max achievable DPT, then it is not possible to beat the best DMG sequence by going down this path.
        calc_stats.estimateDpsPruned += 1;
        return;
    }

    // recursion base case
    if (state.NoTimeLeft() || nextAbils.size() == 0)
    {
        calc_stats.endOfRotReached += 1;
        // overwrite the current best_state if this newfound state is indeed superior
        if (state.GetDmg() > best_dmg)
        {
            best_state = state; // copy by value since AbState is a struct
            best_dmg = state.GetDmg();
        }
    }

    for (auto &&i : nextAbils)
    {
        state.UseAbil(i);
        CalcBest_R(state, abilList);
        state.RevertStep();
    }
}

/**
 * Returns the best ability sequence given the input parameters.
 * @param abilList The list of abilities to include in the sequence
 * @param initialBuffs The initial buff timers (by default, they are all 0)
 * @param initialAdren The initial adrenaline timer
 * @param maxTicks How many ticks the ability sequence can last.
 */
AbState CalcBest(const vector<AbilityID> abilList, const TimedBuffs initialBuffs, const int initialAdren, const int maxTicks)
{
    AbState initial_state = {
        .ticksRemain = {maxTicks},
        .totalDmg = {0},
        .adrenaline = {initialAdren},
        .coolDowns = {{0}},
        .nAbil = 0,
        .abilSeq = {abil_None},
        .buffs = {initialBuffs},
    };
    best_state = (AbState){0};
    best_dmg = 0;
    calc_stats = (CalculationStats){0};

    CalcBest_R(initial_state, abilList);

    return best_state;
}

void print_abstate(AbState ab)
{
    for (size_t i = 0; i < ab.nAbil; i++)
    {
        cout << abil_tbl.lookup(ab.abilSeq[i + 1]).name << '\n';
    }
}

int main(int argc, char *argv[])
{
    abil_tbl.init_abil_table();

    // Starting configuration
    vector<AbilityID> allowedAbils = {
        abil_WildMagic,
        abil_DeepImpact_F,
        abil_Asphyxiate,
        abil_SonicWave,
        abil_ConcBlast,
        abil_SunshineWithAuto,
        abil_Impact_F,
        abil_Auto,
        abil_Combust,
        abil_CorruptionBlast,
        abil_TuskaWrath,
        abil_Wrack,
    };
    int intial_adren = 100;
    int sequence_length = 10; // the length of the ability sequence in ticks.

    // assert that all abilities in the chosen calculation set has cooldown or cast-time greater than 0.
    for (auto abilID : allowedAbils)
    {
        Ability thisAbil = abil_tbl.lookup(abilID);
        if (thisAbil.cooldownMax == 0 && thisAbil.castTime == 0)
        {
            cerr << "Malformed ability with both cooldown and cast time of 0: id = " << abilID << ", name = " << thisAbil.name << endl;
            return 1;
        }
    }

    clock_t start;
    double duration;

    start = clock();
    AbState result = CalcBest(allowedAbils, default_buffs, intial_adren, sequence_length);
    duration = (clock() - start) / (double)CLOCKS_PER_SEC;

    cout << "-------- Calculation result --------" << endl;
    cout << "nAbil: " << result.nAbil << endl;
    cout << "endOfRotReached: " << calc_stats.endOfRotReached << endl;
    cout << "estimateDpsPruned: " << calc_stats.estimateDpsPruned << endl;
    cout << "------------------------------------" << endl;
    cout << "final dmg: " << result.GetDmg() << endl;
    cout << "final adr: " << result.adrenaline[result.nAbil] << endl;
    cout << "------------------------------------" << endl;
    print_abstate(result);
    cout << "------------------------------------" << endl;
    cout << "CalcBest Runtime: " << duration << "s" << endl;
}

// g++ main.cpp -o main.exe; ./main

/*
    // ->  if curTicks is equal to ticksRemain[MAX_ABIL_COUNT], then compare the ability with the max one, and add to solution set if needed. assert: curTicks <= ticksRemain[MAX_ABIL_COUNT].
    // ->  list potential abilities, in order of preference
    // ->  for each ability:
    //          -> advance the state with the ability.
    //                  -> set the cooldown of the affected abilities
    //                  -> advance all abilities cooldowns by the cast time
    //                  -> increment the curticks up to the max cast time of the ability
    //                  -> increment the calculated dmg of the ability
    //                      -> if the ticks remaining is less than the cast time of the ability, then make use of the dmgSeq,
    //                         by taking the dmgSeq only up to the amount of ticks that remain.
    //          -> call calcBest_R with that state
    //          -> undo the addition of ability to the state
    //                  -> revert the cooldowns of affected abilities
    //                  -> revert  all abilities cooldowns by the cast time
    //                  -> decrement curticks
    //                  -> decrement the calculated dmg of the ability

*/