#include <iostream>
#include <pthread.h>

#include <algorithm>
#include <vector>

#include <stdio.h> /* printf */
#include <time.h>  /* time_t, struct tm, difftime, time, mktime */

using namespace std;

enum AbilityID
{
    None,

    WildMagic,
    DeepImpact_F,
    Asphyxiate,

    SonicWave,
    ConcBlast,
    DragonBreath,
    Impact_F,
    CorruptionBlast,
    Combust,

    TuskaWrath,
    Wrack,

    Auto,

    AbilCount // last ID
};

struct Ability
{
    AbilityID id;
    string name;
    int castTime;
    int adrReq;
    int adrGain;
    int cooldownMax;
    int dmgSeq[20];
};

Ability *init_abil_table()
{
    Ability *tbl = new Ability[AbilCount](); // array initializer, the () makes cpp initialize all values to 0.
    // tbl[None] = {.id = None, .name = "None", .castTime = 3, .adrReq = 50, .adrGain = 10, .cooldownMax = 49, .dmgSeq = {}};
    cout << "size: " << sizeof(*tbl) << '\n';
    return tbl;
}

struct TimedBuffs
{
    int nat;
    int sun;
    int zerk;
    int ds;
};

struct AbState
{
    int totalDmg;
    int curTicks;
    int maxTicks;

    TimedBuffs buffs;

    // for now: keep heaps of thresholds and basics
};
struct abc
{
    int k[5];
    int v;
};
// define custom ability sort operation
struct abc_desc_sort
{
    int k_idx;

    inline bool operator()(const abc &a1, const abc &a2)
    {
        return (a1.k[k_idx] > a2.k[k_idx]);
    }
};
int main(int argc, char *argv[])
{
    vector<abc> sort_test;

    // sort_test.push_back(abc({1, 2, 3, 4, 5, 3}));             // flat inline initializer: initialize struct abc with k = {1, 2, 3, 4, 5}, v = 3
    // sort_test.push_back(abc({.k = {1, 2, 3, 4, 5}, .v = 3})); // named inline initializer: initialize struct abc with k = {1, 2, 3, 4, 5}, v = 3
    sort_test.push_back(abc({.k = {5, 1, 5, 4, 1}, .v = 4})); // named inline initializer: initialize struct abc with k = {1, 2, 3, 4, 5}, v = 3
    sort_test.push_back(abc({.k = {4, 2, 4, 2, 2}, .v = 2})); // named inline initializer: initialize struct abc with k = {1, 2, 3, 4, 5}, v = 3
    sort_test.push_back(abc({.k = {3, 3, 3, 5, 3}, .v = 5})); // named inline initializer: initialize struct abc with k = {1, 2, 3, 4, 5}, v = 3
    sort_test.push_back(abc({.k = {2, 4, 2, 1, 4}, .v = 1})); // named inline initializer: initialize struct abc with k = {1, 2, 3, 4, 5}, v = 3
    sort_test.push_back(abc({.k = {1, 5, 1, 3, 5}, .v = 3})); // named inline initializer: initialize struct abc with k = {1, 2, 3, 4, 5}, v = 3

    abc_desc_sort sorter = {3}; // initialize a sorter to pick index 3 from the "k array" of the "abc" struct, for sorting.
    sort(sort_test.begin(), sort_test.end(), sorter);
    cout << " sort result " << endl;
    for (size_t i = 0; i < 5; i++)
    {
        cout << sort_test[i].v << endl;
    }
    
    // cout << sort_test[1].k[1] << " " << sort_test[1].v << endl;
    //Ability *tbl_ptr = init_abil_table();
    //
    //string rmx = to_string(tbl_ptr[1].dmgSeq[0]);
    //cout << rmx << '\n';
    //
    //string ddd = "";
    //cout << "sizeof string = " << sizeof(ddd) << " : " << ddd << '\n';
}

struct canUassign
{
    int a;

    void ax()
    {
        a = 5;
    }
};
void testMyAss()
{
    canUassign myass = {0};
    cout << "first: " << myass.a << endl; // 0
    myass.ax();
    cout << "second: " << myass.a << endl; // 5
}
// g++ main.cpp -o main.exe; ./main