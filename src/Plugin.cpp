// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "Scene.h"
#include "Holster.h"
#include "DrinkTiming.h"
#include "Torso.h"

namespace canteen {
unsigned installDrift();
void publishMotion(float amount,bool firearm);
}

namespace {
struct NVSE {
    unsigned version, runtime, editor, isEditor;
    void *registerCommand, *setOpcode;
    void *(__cdecl *query)(unsigned);
    unsigned (__cdecl *handle)();
};
struct Info { unsigned info; const char *name; unsigned version; };
struct Message { const char *sender; unsigned type, length; void *data; };
struct Messaging { unsigned version; bool (__cdecl *listen)(unsigned,const char *,void (__cdecl *)(Message *)); };
struct Result { union { double value{}; void *form; }; unsigned char type{}; };
struct Scripts {
    bool (__cdecl *call)(void *,void *,void *,Result *,unsigned char,...);
    void *params,*extract,*format,*callAlt;
    void *(__cdecl *compile)(const char *);
};
struct Console { unsigned version; bool (__cdecl *line)(const char *,void *); };

Scripts *scripts;
Messaging *messages;
unsigned pluginHandle;
const void *canteenForm=nullptr;
DWORD gameThread=0;
ULONGLONG lastRenderFit=0;
bool sawRenderFit=false;
Console *console;
void *syncItems, *checkDrawn, *putAway;
using State = canteen::EquipState;
canteen::Holster holster;
State previousState = State::unavailable;
ULONGLONG wristAfter = 0;
bool loaded=false, initialized=false, trace=false, gulp=false;
ULONGLONG nextInventory=0, nextPose=0, requestedAt=0;
char requested[96]{};
void *getDeadline, *putDeadlines, *clearOldAbilities, *setBlur, *motionMode;
bool legacyClean=false, presentedLean=false, driftTried=false;
float targetStrength=0, shownStrength=0;
int lastBlur=-1, playMode=0;
ULONGLONG lastMotion=0, nextMode=0, lastPresentation=0, nextBlur=0;
bool drinkingEnabled=true, timingLoaded=false;
int drinkInterval=canteen::defaultDrinkInterval;
ULONGLONG nextDrinkUpdate=0;
canteen::DrinkTiming timing;
canteen::Sip sip;
int drawnHand=-1;
ULONGLONG readGripAfter=0;

void log(const char *line) {
    if (FILE *f=std::fopen("Data/NVSE/Plugins/TTWVRPhysicalCanteen.log","a")) {
        std::fprintf(f,"%lu %s\n",GetTickCount(),line);
        std::fclose(f);
    }
}
int run(void *script) {
    Result result;
    return scripts->call(script,nullptr,nullptr,&result,0) && result.type==1 ? int(result.value) : -1;
}
constexpr char inventory[] = R"(begin Function {}
 ref tool
 int copies
 let tool := GetFormFromMod "TTWVRPhysicalCanteen.esp" "800"
 if eval !tool
  return
 endif
 let copies := player.GetItemCount tool
 if player.GetItemCount PreordVaultCanteen
  if copies == 0
   player.AddItem tool 1 1
  endif
 elseif copies > 0
  player.RemoveItem tool copies 1
 endif
 SetFunctionValue tool
end)";
constexpr char eligible[] = R"(begin Function {}
 int menu
 ref tool
 let menu := GetActiveMenuMode
 if eval (menu != 0 && menu != 1004) || player.GetDead
  return
 endif
 let tool := GetFormFromMod "TTWVRPhysicalCanteen.esp" "800"
 if eval tool && player.GetEquippedWeapon == tool && player.GetItemCount PreordVaultCanteen
  if player.IsWeaponOut
   SetFunctionValue 2
  elseif menu == 0
   SetFunctionValue 3
  endif
 else
  SetFunctionValue 1
 endif
end)";
// Recheck in the same script that performs the unequip. A new draw or
// another weapon selected since the sample belongs to the host.
constexpr char release[] = R"(begin Function {}
 ref tool
 let tool := GetFormFromMod "TTWVRPhysicalCanteen.esp" "800"
 if eval tool && GetActiveMenuMode == 0 && player.GetDead == 0 && player.IsWeaponOut == 0 && player.GetEquippedWeapon == tool
  player.UnequipItem tool 0 1
  SetFunctionValue 1
 endif
end)";

// JIP stores the two public player auxiliary values in the NVSE co-save.
// Absolute deadlines also expire while Fallout is closed.
constexpr char readDeadline[] = R"(int slot
begin Function {slot}
 SetFunctionValue (player.AuxVarGetFlt "_TTWVRPhysicalCanteenDeadlines" slot)
end)";
constexpr char writeDeadlines[] = R"(int readyAt
int soberAt
int ok
begin Function {readyAt soberAt}
 let ok := player.AuxVarSetFlt "_TTWVRPhysicalCanteenDeadlines" readyAt 0
 if ok
  SetFunctionValue (player.AuxVarSetFlt "_TTWVRPhysicalCanteenDeadlines" soberAt 1)
 endif
end)";
// 0.1.10 serialized its active abilities into the base save. Remove only our
// three private forms, even on a save loaded with drinking disabled.
constexpr char removeBeer[] = R"(begin Function {}
 ref ability
 let ability := GetFormFromMod "TTWVRPhysicalCanteen.esp" "802"
 if eval ability
  player.RemoveSpell ability
 endif
 let ability := GetFormFromMod "TTWVRPhysicalCanteen.esp" "803"
 if eval ability
  player.RemoveSpell ability
 endif
 let ability := GetFormFromMod "TTWVRPhysicalCanteen.esp" "804"
 if eval ability
  player.RemoveSpell ability
 endif
 SetFunctionValue 1
end)";
constexpr char blurScript[] = R"(int amount
ref modifier
float weight
begin Function {amount}
 let modifier := GetFormFromMod "TTWVRPhysicalCanteen.esp" "805"
 if eval !modifier
  return
 endif
 RemoveImageSpaceModifier modifier
 if amount > 0
  let weight := amount / 64.0
  ApplyImageSpaceModifier modifier weight
 endif
 SetFunctionValue 1
end)";
constexpr char checkMotionMode[] = R"(begin Function {}
 int menu
 int kind
 ref gun
 let menu := GetActiveMenuMode
 if eval (menu != 0 && menu != 1004) || player.GetDead
  SetFunctionValue 0
  return
 endif
 let gun := player.GetEquippedWeapon
 if eval gun && player.IsWeaponOut
  let kind := GetWeaponType gun
  if eval kind >= 3 && kind <= 9
   SetFunctionValue 2
   return
  endif
 endif
 SetFunctionValue 1
end)";

void initDrinking() {
    char path[MAX_PATH];
    const DWORD length=GetFullPathNameA("Data\\NVSE\\Plugins\\TTWVRPhysicalCanteen.ini",
                                        MAX_PATH,path,nullptr);
    if (length && length<MAX_PATH) {
        drinkingEnabled=GetPrivateProfileIntA("Drinking","Enabled",1,path)!=0;
        char minutes[64]{};
        GetPrivateProfileStringA("Drinking","CooldownMinutes","5",minutes,sizeof(minutes),path);
        drinkInterval=canteen::intervalSeconds(minutes);
    }
    getDeadline=scripts->compile(readDeadline);
    putDeadlines=scripts->compile(writeDeadlines);
    clearOldAbilities=scripts->compile(removeBeer);
    setBlur=scripts->compile(blurScript);
    motionMode=scripts->compile(checkMotionMode);
    if (!clearOldAbilities || !setBlur || !motionMode) log("Could not compile all canteen effect scripts");
    if (!getDeadline || !putDeadlines) {
        drinkingEnabled=false;
        log("Could not compile drinking scripts; using ordinary canteen saves");
        return;
    }
    char line[100];
    std::snprintf(line,sizeof(line),"canteen_drinking enabled=%d interval_seconds=%d",
                  int(drinkingEnabled),drinkInterval);
    log(line);
}

// Using 2020 as the epoch keeps deadlines within signed script integers past 2080.
int drinkClock() {
    FILETIME time;
    GetSystemTimeAsFileTime(&time);
    ULARGE_INTEGER ticks;
    ticks.HighPart=time.dwHighDateTime;
    ticks.LowPart=time.dwLowDateTime;
    constexpr long long epoch=13222310400LL;
    const auto elapsed=static_cast<long long>(ticks.QuadPart/10000000)-epoch;
    return static_cast<int>(std::clamp<long long>(elapsed,0,std::numeric_limits<int>::max()));
}

void storeTiming() {
    Result result;
    if (!scripts->call(putDeadlines,nullptr,nullptr,&result,2,timing.saveReady,timing.soberAt) ||
        result.type!=1 || result.value!=1) log("Could not persist canteen drinking deadlines");
}

void updateDrinking(int now) {
    if (!legacyClean && clearOldAbilities) legacyClean=run(clearOldAbilities)==1;
    if (!getDeadline || !putDeadlines) return;
    if (!timingLoaded) {
        int values[2]{};
        for (int slot=0;slot<2;++slot) {
            Result r;
            if (!scripts->call(getDeadline,nullptr,nullptr,&r,1,slot) || r.type!=1 ||
                !(r.value>=0 && r.value<=std::numeric_limits<int>::max())) return;
            values[slot]=static_cast<int>(r.value);
        }
        timing={values[0],values[1]};
        timingLoaded=true;
        if (!drinkingEnabled && (timing.saveReady || timing.soberAt)) {
            timing={};
            storeTiming();
        }
    }
    targetStrength=drinkingEnabled ? timing.intensity(now,drinkInterval) : 0.f;
}

void applyBlur(int amount) {
    Result r;
    if (setBlur && scripts->call(setBlur,nullptr,nullptr,&r,1,amount) && r.type==1 && r.value==1)
        lastBlur=amount;
}

void updateMotion(ULONGLONG now) {
    float step=lastMotion ? std::min(now-lastMotion,100ULL)*.0004f : 0.f;
    lastMotion=now;
    shownStrength+=std::clamp(targetStrength-shownStrength,-step,step);
    if (shownStrength>0 && now>=nextMode) {
        nextMode=now+60;
        playMode=motionMode ? run(motionMode) : 0;
        if (!driftTried) {
            driftTried=true;
            unsigned backends=canteen::installDrift();
            char line[80]; std::snprintf(line,sizeof(line),"drunk_aim openxr=%u openvr=%u",backends&1,(backends>>1)&1);
            log(line);
        }
    }
    if (shownStrength==0) playMode=0;
    float playing=playMode>0 ? shownStrength : 0.f;
    canteen::publishMotion(playing,playMode==2);
    int amount=static_cast<int>(std::lround(64.f*playing));
    if (amount!=lastBlur && now>=nextBlur) {
        applyBlur(amount);
        nextBlur=now+(amount==lastBlur ? 70 : 1000);
    }
}

void presentBody(ULONGLONG now) {
    float lean=playMode>0 ? canteen::rocking(now*.001,shownStrength).lean : 0.f;
    if (!lean && !presentedLean) return;
    auto *player=*reinterpret_cast<char **>(0x11dea3c);
    presentedLean=canteen::presentLean(canteen::worldRoot(player),lean) && lean!=0.f;
}

void countDrink(int now, bool saved) {
    timing.drink(now,drinkInterval,saved);
    storeTiming();
    char line[100];
    std::snprintf(line,sizeof(line),"canteen_drink saved=%d stage=%d next_save_seconds=%d",
                  int(saved),timing.effect(now,drinkInterval),timing.wait(now));
    log(line);
    nextDrinkUpdate=0;
}

void cooldownMessage(int remaining) {
    char cmd[120];
    std::snprintf(cmd,sizeof(cmd),"MessageExAlt 2 \"Canteen: next save in %d:%02d\"",
                  remaining/60,remaining%60);
    console->line(cmd,nullptr);
}

void logWrists(const char *when) {
    auto *player=*reinterpret_cast<char **>(0x11dea3c);
    auto *root=canteen::worldRoot(player);
    for (const char *name : {"Bip01 L Hand","Bip01 R Hand"}) {
        auto *w=canteen::find(root,name);
        if (!w || !w->parent) continue;
        const auto &r=w->local.rotation.m;
        const auto &up=w->world.rotation.m[2];
        char line[256];
        std::snprintf(line,sizeof(line),"wrist %s %s parent=%s local=%.3f,%.3f,%.3f/%.3f,%.3f,%.3f/%.3f,%.3f,%.3f world_up=%.3f,%.3f,%.3f",
            when,name,w->parent->name ? w->parent->name : "?",r[0][0],r[0][1],r[0][2],
            r[1][0],r[1][1],r[1][2],r[2][0],r[2][1],r[2][2],up[0],up[1],up[2]);
        log(line);
    }
}

void requestSave(ULONGLONG now) {
    SYSTEMTIME t; GetSystemTime(&t);
    std::snprintf(requested,sizeof(requested),"Canteen %04u-%02u-%02u %02u-%02u-%02u-%03u",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
    char cmd[128]; std::snprintf(cmd,sizeof(cmd),"SaveGame \"%s\"",requested);
    requestedAt=now;
    if (!console->line(cmd,nullptr)) { requested[0]=0; log("Game rejected the save command"); }
}
// Sample once after each draw or handoff so a headset test can distinguish
// the native canteen pose from TTWVR's fallback fist.
void reportGrip(const canteen::Measurement &m, ULONGLONG now) {
    const bool left=m.hand==canteen::Hand::left;
    if (drawnHand!=int(m.hand)) {
        drawnHand=int(m.hand);
        readGripAfter=now+350;
    }
    if (!readGripAfter || now<readGripAfter) return;
    readGripAfter=0;
    logWrists("held");
    auto *index=canteen::find(m.root,left ? "Bip01 L Finger11" : "Bip01 R Finger11");
    if (!index) return;
    const auto &r=index->local.rotation.m;
    const float cosine=std::fmax(-1.f,std::fmin(1.f,(r[0][0]+r[1][1]+r[2][2]-1.f)*.5f));
    char line[96]; std::snprintf(line,sizeof(line),"canteen_grip hand=%s index_bend_deg=%.1f",left?"left":"right",std::acos(cosine)*57.29578f); log(line);
}
// JohnnyGuitar 5.27+ broadcasts JG_OnRenderGameModeUpdate immediately
// before rendering the world. No scripts, input changes or saves run here.
void __cdecl renderMessage(Message *m) {
    constexpr unsigned renderGameMode = 10014;
    if (m->type!=renderGameMode || GetCurrentThreadId()!=gameThread || !loaded) return;
    lastPresentation=GetTickCount64();
    presentBody(lastPresentation);
    if (canteenForm && canteen::prepareDraw(canteenForm)) {
        lastRenderFit=GetTickCount64();
        sawRenderFit=true;
    }
}

void tick() {
    if (!loaded) return;
    if (!initialized) {
        initialized=true;
        gameThread=GetCurrentThreadId();
        if (!GetModuleHandleW(L"TTWVR.dll")) { log("TTWVR is required"); return; }
        syncItems=scripts->compile(inventory);
        checkDrawn=scripts->compile(eligible);
        putAway=scripts->compile(release);
        initDrinking();
        char value[3]{};
        trace=GetEnvironmentVariableA("TTWVR_CANTEEN_TRACE",value,sizeof(value))==1 && value[0]=='1';
        log(syncItems && checkDrawn && putAway ? "Physical Canteen 0.1.11 ready" : "Could not compile canteen scripts");
    }
    if (!syncItems || !checkDrawn || !putAway) return;
    if (gulp) {
        gulp=false;
        if (!console->line("PlaySound NPCHumanDrinkingBottleGulp",nullptr)) log("Bottle gulp unavailable");
    }
    auto now=GetTickCount64();
    if (now>=nextDrinkUpdate) {
        nextDrinkUpdate=now+1000;
        updateDrinking(drinkClock());
    }
    updateMotion(now);
    if (!lastPresentation || now-lastPresentation>100) presentBody(now);
    if (requested[0] && now-requestedAt>5000) { requested[0]=0; log("No save confirmation"); }
    // Runtimes that bypass the regular render event still get placement every
    // game frame, including the equipped-but-not-yet-drawn part of a draw.
    if (canteenForm && (!lastRenderFit || now-lastRenderFit>50)) canteen::prepareDraw(canteenForm);
    if (now<nextPose) return;
    const int status=run(checkDrawn);
    const State state=status>0 && status<=3 ? static_cast<State>(status) : State::unavailable;
    nextPose=now+(state==State::drawn ? 25 : 100);
    if (state!=State::unavailable && state!=previousState) {
        const bool leaving=previousState==State::drawn || previousState==State::sheathed;
        if (state==State::drawn) wristAfter=0;
        else if (leaving || state==State::sheathed || previousState==State::unavailable) {
            log(state==State::sheathed ? "canteen_state sheathed_equipped" : "canteen_state away");
            wristAfter=now+350;
        }
        previousState=state;
    }
    if (holster.shouldUnequip(state,now)) {
        log(run(putAway)==1 ? "canteen_release unequip_requested" : "canteen_release skipped");
        wristAfter=now+350;
    }
    if (wristAfter && now>=wristAfter && (state==State::away || state==State::sheathed)) {
        logWrists(state==State::away ? "away" : "sheathed_equipped");
        wristAfter=0;
    }
    if (status>0 && now>=nextInventory) {
        nextInventory=now+750;
        Result tool;
        if (scripts->call(syncItems,nullptr,nullptr,&tool,0) && tool.type==2)
            canteenForm=tool.form;
    }
    if (sawRenderFit) {
        static bool logged=false;
        if (!logged) { log("draw_placement pre_render active"); logged=true; }
        sawRenderFit=false;
    }
    canteen::Measurement m{};
    if (status!=2 || !canteen::measure(m)) { drawnHand=-1; sip.reset(); return; }
    reportGrip(m,now);
    if (trace) {
        static ULONGLONG nextTrace=0;
        if (now>=nextTrace) {
            nextTrace=now+125;
            char line[96]; std::snprintf(line,sizeof(line),"pose mouth_m=%.3f neck_up=%.3f",m.distance,m.up);log(line);
        }
    }
    if (!sip.sample(now,m.distance,m.up) || requested[0]) return;
    const int at=drinkClock();
    const int remaining=drinkingEnabled && timingLoaded ? timing.wait(at) : 0;
    if (remaining>0) {
        countDrink(at,false);
        gulp=true;
        cooldownMessage(remaining);
    } else {
        requestSave(now);
    }
}
void __cdecl message(Message *m) {
    switch (m->type) {
        case 0: // All NVSE plugins have loaded.
            messages->listen(pluginHandle,"JohnnyGuitarNVSE",renderMessage); break;
        case 1: case 2: case 6: case 7: case 8: case 14:
            if (loaded && (m->type==2 || m->type==6) && GetCurrentThreadId()==gameThread) {
                if (lastBlur>0) applyBlur(0);
                playMode=0;
                if (presentedLean) presentBody(GetTickCount64());
            }
            canteen::publishMotion(0,false);
            targetStrength=shownStrength=0; lastBlur=-1; playMode=0; legacyClean=presentedLean=false;
            lastMotion=nextMode=lastPresentation=nextBlur=0;
            loaded=m->type==14 || (m->type==8 && m->data);
            timing={}; timingLoaded=false; nextDrinkUpdate=0;
            canteenForm=nullptr; lastRenderFit=0; sawRenderFit=false;
            holster={}; previousState=State::unavailable; wristAfter=0;
            drawnHand=-1; nextInventory=nextPose=0; requested[0]=0; gulp=false; sip.reset(); break;
        case 4:
            if (requested[0] && m->data && std::strstr(static_cast<const char*>(m->data),requested)) {
                log(requested); requested[0]=0; gulp=true;
                // NVSE dispatches this before JIP writes the co-save. Only a
                // matching save can start a new cooldown.
                if (drinkingEnabled && timingLoaded) countDrink(drinkClock(),true);
            }
            break;
        case 20: tick(); break;
    }
}
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSE *n, Info *p) {
    if (!n || !p) return false;
    *p={1,"TTWVR Physical Canteen",12};
    return !n->isEditor && n->runtime==0x040020d0 && n->version>=6;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSE *n) {
    messages=static_cast<Messaging *>(n->query(2));
    pluginHandle=n->handle();
    console=static_cast<Console *>(n->query(1)); scripts=static_cast<Scripts *>(n->query(6));
    return console && console->version>=2 && console->line && scripts && scripts->compile && scripts->call &&
           messages && messages->listen(pluginHandle,"NVSE",message);
}
