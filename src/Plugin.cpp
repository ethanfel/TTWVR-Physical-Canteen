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
void prepareDrift();
unsigned installDrift();
void publishMotion(float amount,bool firearm,bool viewSway);
unsigned appliedViewBackends();
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
void *getDeadline, *putDeadlines, *clearOldAbilities, *setImageEffect, *motionMode;
void *readAlcohol, *clearPreviousImages;
bool hasCanteen=false, hasAlcohol=false, imagesClean=false;
bool legacyClean=false, presentedLean=false, driftTried=false;
float targetStrength=0, alcoholStrength=0, shownStrength=0;
enum class ImageEffect : int { blur, doubleVision };
struct ModifierState {
    ImageEffect effect;
    int lastWeight=-1;
    ULONGLONG nextUpdate=0;
};
ModifierState blur{ImageEffect::blur}, ghost{ImageEffect::doubleVision};
int playMode=0;
ULONGLONG lastMotion=0, nextMode=0, lastPresentation=0;
bool drinkingEnabled=true, viewSway=true, doubleVision=true, timingLoaded=false;
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
constexpr char enabledAddons[] = R"(begin Function {}
 int flags
 if IsModLoaded "TTWVRPhysicalCanteen.esp"
  let flags := 1
 endif
 if IsModLoaded "TTWVRAlcohol.esp"
  let flags += 2
 endif
 SetFunctionValue flags
end)";
constexpr char alcoholLevel[] = R"(begin Function {}
 float soberDay
 let soberDay := player.AuxVarGetFlt "_TTWVRAlcoholSoberDay" 0
 SetFunctionValue ((soberDay - GameDaysPassed) * 4.0)
end)";
constexpr char removePreviousImages[] = R"(begin Function {}
 ref modifier
 let modifier := GetFormFromMod "TTWVRPhysicalCanteen.esp" "805"
 if eval modifier
  RemoveImageSpaceModifier modifier
 endif
 let modifier := GetFormFromMod "TTWVRPhysicalCanteen.esp" "806"
 if eval modifier
  RemoveImageSpaceModifier modifier
 endif
 SetFunctionValue 1
end)";
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

// JIP stores both deadlines and the clock at save time in the NVSE co-save.
// Absolute deadlines also expire while Fallout is closed.
constexpr char readDeadline[] = R"(int slot
begin Function {slot}
 SetFunctionValue (player.AuxVarGetFlt "_TTWVRPhysicalCanteenDeadlines" slot)
end)";
constexpr char writeDeadlines[] = R"(int readyAt
int soberAt
int observedAt
int ok
begin Function {readyAt soberAt observedAt}
 let ok := player.AuxVarSetFlt "_TTWVRPhysicalCanteenDeadlines" readyAt 0
 if ok
  let ok := player.AuxVarSetFlt "_TTWVRPhysicalCanteenDeadlines" soberAt 1
  if ok
   SetFunctionValue (player.AuxVarSetFlt "_TTWVRPhysicalCanteenDeadlines" observedAt 2)
  endif
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
constexpr char imageEffectScript[] = R"(int amount
int doubleImage
int wasActive
ref modifier
float weight
begin Function {amount doubleImage wasActive}
 if doubleImage
  let modifier := GetFormFromMod "%s" "806"
 else
  let modifier := GetFormFromMod "%s" "805"
 endif
 if eval !modifier
  return
 endif
 if eval doubleImage && amount > 0
  let weight := amount / 10000.0
  SetImageSpaceModTrait modifier 46 weight
  if wasActive == 0
   RemoveImageSpaceModifier modifier
   ApplyImageSpaceModifier modifier 1.0
  endif
 else
  RemoveImageSpaceModifier modifier
  if amount > 0
   let weight := amount / 64.0
   ApplyImageSpaceModifier modifier weight
  endif
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
        viewSway=GetPrivateProfileIntA("Drinking","ViewSway",1,path)!=0;
        doubleVision=GetPrivateProfileIntA("Drinking","DoubleVision",1,path)!=0;
        char minutes[64]{};
        GetPrivateProfileStringA("Drinking","CooldownMinutes","5",minutes,sizeof(minutes),path);
        drinkInterval=canteen::intervalSeconds(minutes);
    }
    const char *effectPlugin=hasAlcohol ? "TTWVRAlcohol.esp" : "TTWVRPhysicalCanteen.esp";
    char effectSource[sizeof(imageEffectScript)+64];
    std::snprintf(effectSource,sizeof(effectSource),imageEffectScript,effectPlugin,effectPlugin);
    setImageEffect=scripts->compile(effectSource);
    motionMode=scripts->compile(checkMotionMode);
    if (hasAlcohol) {
        readAlcohol=scripts->compile(alcoholLevel);
        log(readAlcohol && setImageEffect && motionMode ? "regular_alcohol ready" : "Could not compile regular alcohol effects");
        if (hasCanteen) clearPreviousImages=scripts->compile(removePreviousImages);
    }
    if (!hasCanteen) return;
    getDeadline=scripts->compile(readDeadline);
    putDeadlines=scripts->compile(writeDeadlines);
    clearOldAbilities=scripts->compile(removeBeer);
    if (!clearOldAbilities || !setImageEffect || !motionMode) log("Could not compile all canteen effect scripts");
    if (!getDeadline || !putDeadlines) {
        drinkingEnabled=false;
        log("Could not compile drinking scripts; using ordinary canteen saves");
        return;
    }
    char line[100];
    std::snprintf(line,sizeof(line),"canteen_drinking enabled=%d interval_seconds=%d view_sway=%d double_vision=%d",
                  int(drinkingEnabled),drinkInterval,int(viewSway),int(doubleVision));
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
    if (!scripts->call(putDeadlines,nullptr,nullptr,&result,3,timing.saveReady,timing.soberAt,timing.observedAt) ||
        result.type!=1 || result.value!=1) log("Could not persist canteen drinking deadlines");
}

void observeDrinkClock(int now) {
    if (timing.observeClock(now,drinkInterval)) {
        char line[120];
        std::snprintf(line,sizeof(line),"canteen_clock_rebased cooldown_remaining=%d recovery_remaining=%d",
                      timing.wait(now),std::max(0,timing.soberAt-now));
        log(line);
    }
}

void updateDrinking(int now) {
    if (!legacyClean && clearOldAbilities) legacyClean=run(clearOldAbilities)==1;
    if (!getDeadline || !putDeadlines) return;
    if (!timingLoaded) {
        int values[3]{};
        for (int slot=0;slot<3;++slot) {
            Result r;
            if (!scripts->call(getDeadline,nullptr,nullptr,&r,1,slot) || r.type!=1 ||
                !(r.value>=0 && r.value<=std::numeric_limits<int>::max())) return;
            values[slot]=static_cast<int>(r.value);
        }
        timing={values[0],values[1],values[2]};
        timingLoaded=true;
        if (!drinkingEnabled && (timing.saveReady || timing.soberAt)) {
            timing={};
            storeTiming();
        }
    }
    if (drinkingEnabled) observeDrinkClock(now);
    targetStrength=drinkingEnabled ? timing.intensity(now,drinkInterval) : 0.f;
}

void updateModifier(ModifierState &state, int weight, ULONGLONG now) {
    const bool clearingActive=weight==0 && state.lastWeight>0;
    if (state.lastWeight==weight || (!clearingActive && now<state.nextUpdate)) return;
    Result result;
    if (setImageEffect && scripts->call(setImageEffect,nullptr,nullptr,&result,3,weight,
                                        static_cast<int>(state.effect),int(state.lastWeight>0)) && result.type==1 && result.value==1) {
        if (state.effect==ImageEffect::doubleVision && (state.lastWeight>0)!=(weight>0))
            log(weight>0 ? "double_vision native_modifier=on" : "double_vision native_modifier=off");
        state.lastWeight=weight;
        state.nextUpdate=now+(state.effect==ImageEffect::doubleVision ? 33 : 70);
    } else {
        state.lastWeight=-1;
        state.nextUpdate=now+1000;
    }
}

void updateMotion(ULONGLONG now) {
    float step=lastMotion ? std::min(now-lastMotion,100ULL)*.0004f : 0.f;
    lastMotion=now;
    float strength=std::max(targetStrength,alcoholStrength);
    shownStrength+=std::clamp(strength-shownStrength,-step,step);
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
    canteen::publishMotion(playing,playMode==2,viewSway);
    static unsigned reportedViews=0;
    const unsigned applied=canteen::appliedViewBackends();
    if (applied!=reportedViews) {
        if ((applied&1) && !(reportedViews&1)) log("view_sway backend=OpenXR stage=xrEndFrame eyes=2 result=XR_SUCCESS");
        if ((applied&2) && !(reportedViews&2)) log("view_sway backend=OpenVR stage=tracking_sample eyes=2");
        reportedViews=applied;
    }
    int amount=static_cast<int>(std::lround(64.f*playing));
    updateModifier(blur,amount,now);
    updateModifier(ghost,canteen::doubleVisionAmount(playing,doubleVision,now),now);
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
        auto *enabled=scripts->compile(enabledAddons);
        int flags=enabled ? run(enabled) : -1;
        if (flags<=0) return;
        hasCanteen=(flags&1)!=0;
        hasAlcohol=(flags&2)!=0;
        if (hasCanteen) {
            syncItems=scripts->compile(inventory);
            checkDrawn=scripts->compile(eligible);
            putAway=scripts->compile(release);
            log(syncItems && checkDrawn && putAway ? "Physical Canteen 1.1.8-r1 ready" : "Could not compile canteen scripts");
        }
        canteen::prepareDrift();
        initDrinking();
        char value[3]{};
        trace=GetEnvironmentVariableA("TTWVR_CANTEEN_TRACE",value,sizeof(value))==1 && value[0]=='1';
    }
    if (!hasCanteen && !hasAlcohol) return;
    if (gulp) {
        gulp=false;
        if (!console->line("PlaySound NPCHumanDrinkingBottleGulp",nullptr)) log("Bottle gulp unavailable");
    }
    auto now=GetTickCount64();
    if (now>=nextDrinkUpdate) {
        nextDrinkUpdate=now+1000;
        if (hasCanteen) updateDrinking(drinkClock());
        if (clearPreviousImages && !imagesClean) imagesClean=run(clearPreviousImages)==1;
        if (readAlcohol) {
            Result result;
            if (scripts->call(readAlcohol,nullptr,nullptr,&result,0) && result.type==1) {
                const float current=static_cast<float>(std::clamp(result.value,0.0,1.0));
                if (current>alcoholStrength+.05f) {
                    char line[64];
                    std::snprintf(line,sizeof(line),"regular_alcohol intensity=%.3f",current);
                    log(line);
                }
                alcoholStrength=current;
            }
        }
    }
    updateMotion(now);
    if (!lastPresentation || now-lastPresentation>100) presentBody(now);
    if (!syncItems || !checkDrawn || !putAway) return;
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
    if (drinkingEnabled && timingLoaded) observeDrinkClock(at);
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
                const auto now=GetTickCount64();
                updateModifier(blur,0,now);
                updateModifier(ghost,0,now);
                playMode=0;
                if (presentedLean) presentBody(GetTickCount64());
            }
            canteen::publishMotion(0,false,false);
            blur={ImageEffect::blur}; ghost={ImageEffect::doubleVision};
            targetStrength=alcoholStrength=shownStrength=0; playMode=0;
            legacyClean=imagesClean=presentedLean=false;
            lastMotion=nextMode=lastPresentation=0;
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
            } else if (loaded && drinkingEnabled && timingLoaded && (timing.saveReady || timing.soberAt)) {
                // Stamp ordinary saves too, including ones made after recovery.
                observeDrinkClock(drinkClock());
                storeTiming();
            }
            break;
        case 20: tick(); break;
    }
}
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSE *n, Info *p) {
    if (!n || !p) return false;
    *p={1,"TTWVR Physical Canteen",1010801};
    return !n->isEditor && n->runtime==0x040020d0 && n->version>=6;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSE *n) {
    messages=static_cast<Messaging *>(n->query(2));
    pluginHandle=n->handle();
    console=static_cast<Console *>(n->query(1)); scripts=static_cast<Scripts *>(n->query(6));
    return console && console->version>=2 && console->line && scripts && scripts->compile && scripts->call &&
           messages && messages->listen(pluginHandle,"NVSE",message);
}
