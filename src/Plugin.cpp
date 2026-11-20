// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "Scene.h"

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
struct Result { double value{}; unsigned char type{}; };
struct Scripts {
    bool (__cdecl *call)(void *,void *,void *,Result *,unsigned char,...);
    void *params,*extract,*format,*callAlt;
    void *(__cdecl *compile)(const char *);
};
struct Console { unsigned version; bool (__cdecl *line)(const char *,void *); };

Scripts *scripts;
Console *console;
void *syncItems, *checkDrawn;
bool loaded=false, initialized=false, trace=false, gulp=false;
ULONGLONG nextInventory=0, nextPose=0, requestedAt=0;
char requested[96]{};
canteen::Sip sip;

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
 SetFunctionValue 1
end)";
constexpr char eligible[] = R"(begin Function {}
 int menu
 ref tool
 let menu := GetActiveMenuMode
 if eval (menu != 0 && menu != 1004) || player.GetDead
  return
 endif
 let tool := GetFormFromMod "TTWVRPhysicalCanteen.esp" "800"
 if eval tool && player.GetEquippedWeapon == tool && player.IsWeaponOut && player.GetItemCount PreordVaultCanteen
  SetFunctionValue 2
 else
  SetFunctionValue 1
 endif
end)";
void requestSave(ULONGLONG now) {
    SYSTEMTIME t; GetSystemTime(&t);
    std::snprintf(requested,sizeof(requested),"Canteen %04u-%02u-%02u %02u-%02u-%02u-%03u",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
    char cmd[128]; std::snprintf(cmd,sizeof(cmd),"SaveGame \"%s\"",requested);
    requestedAt=now;
    if (!console->line(cmd,nullptr)) { requested[0]=0; log("Game rejected the save command"); }
}
void tick() {
    if (!loaded) return;
    if (!initialized) {
        initialized=true;
        if (!GetModuleHandleW(L"TTWVR.dll")) { log("TTWVR is required"); return; }
        syncItems=scripts->compile(inventory);
        checkDrawn=scripts->compile(eligible);
        char value[3]{};
        trace=GetEnvironmentVariableA("TTWVR_CANTEEN_TRACE",value,sizeof(value))==1 && value[0]=='1';
        log(syncItems && checkDrawn ? "Physical Canteen 0.1.2 ready" : "Could not compile canteen scripts");
    }
    if (!syncItems || !checkDrawn) return;
    if (gulp) {
        gulp=false;
        if (!console->line("PlaySound NPCHumanDrinkingBottleGulp",nullptr)) log("Bottle gulp unavailable");
    }
    auto now=GetTickCount64();
    if (requested[0] && now-requestedAt>5000) { requested[0]=0; log("No save confirmation"); }
    if (now<nextPose) return;
    const int status=run(checkDrawn);
    nextPose=now+(status==2 ? 25 : 100);
    if (status>0 && now>=nextInventory) { nextInventory=now+750; run(syncItems); }
    canteen::Measurement m{};
    if (status!=2 || !canteen::measure(m)) { sip.reset(); return; }
    if (trace) {
        static ULONGLONG nextTrace=0;
        if (now>=nextTrace) {
            nextTrace=now+125;
            char line[96]; std::snprintf(line,sizeof(line),"pose mouth_m=%.3f neck_up=%.3f",m.distance,m.up);log(line);
        }
    }
    if (sip.sample(now,m.distance,m.up) && !requested[0]) requestSave(now);
}
void __cdecl message(Message *m) {
    switch (m->type) {
        case 1: case 2: case 6: case 7: case 8: case 14:
            loaded=m->type==14 || (m->type==8 && m->data);
            nextInventory=nextPose=0; requested[0]=0; gulp=false; sip.reset(); break;
        case 4:
            if (requested[0] && m->data && std::strstr(static_cast<const char*>(m->data),requested)) {
                log(requested); requested[0]=0; gulp=true;
            }
            break;
        case 20: tick(); break;
    }
}
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSE *n, Info *p) {
    if (!n || !p) return false;
    *p={1,"TTWVR Physical Canteen",3};
    return !n->isEditor && n->runtime==0x040020d0 && n->version>=6;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSE *n) {
    auto *messages=static_cast<Messaging *>(n->query(2));
    console=static_cast<Console *>(n->query(1)); scripts=static_cast<Scripts *>(n->query(6));
    return console && console->version>=2 && console->line && scripts && scripts->compile && scripts->call &&
           messages && messages->listen(n->handle(),"NVSE",message);
}
