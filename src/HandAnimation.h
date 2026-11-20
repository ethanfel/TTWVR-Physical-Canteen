// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Scene.h"
#include <windows.h>
#include <cstdint>

namespace canteen {
using RestoreHand = void (__cdecl *)(Object *);
inline RestoreHand restoreHostHand;
inline DWORD animationThread;

// TTWVR supplies this callback to its hand update. The stock callback first
// restores the weapon's fingers. Supply the canteen bends before skinning.
inline void __cdecl restoreDrawnHand(Object *wrist) {
    restoreHostHand(wrist);
    if (GetCurrentThreadId()!=animationThread) return;
    for (auto &pose : poses)
        if (wrist && pose.wrist==wrist) pose.apply();
}

inline bool installHandAnimation(HMODULE host) {
    auto *base=reinterpret_cast<unsigned char *>(host);
    const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS *>(base+dos->e_lfanew);
    // Only the verified TTWVR 1.1.7 x86 animation layout uses this call site.
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp!=0x6afa9b4f || nt->OptionalHeader.SizeOfImage<=0x218c8e) return false;
    auto *site=base+0x204f1e;
    auto *original=reinterpret_cast<RestoreHand>(base+0x218c40);
    constexpr unsigned char args[]={0x51,0xf3,0x0f,0x11,0x0c,0x24,0x8d,0x44,0x24,0x44,0x56,0x50};
    if (site[0]!=0x68 || std::memcmp(site+1,&original,sizeof(original)) ||
        std::memcmp(site+5,args,sizeof(args))) return false;
    DWORD protection;
    if (!VirtualProtect(site+1,sizeof(original),PAGE_EXECUTE_READWRITE,&protection)) return false;
    restoreHostHand=original;
    animationThread=GetCurrentThreadId();
    RestoreHand fitted=restoreDrawnHand;
    std::memcpy(site+1,&fitted,sizeof(fitted));
    FlushInstructionCache(GetCurrentProcess(),site+1,sizeof(fitted));
    VirtualProtect(site+1,sizeof(fitted),protection,&protection);
    return true;
}
}
