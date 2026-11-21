// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstring>
#include "Balance.h"

namespace canteen {
// The same 1.1.7 controller-frame layout and verification used by the Night Vision addon.
static std::atomic<unsigned> motion{0};
static std::atomic<unsigned> viewApplied{0};
unsigned appliedViewBackends() { return viewApplied.load(std::memory_order_relaxed); }
void publishMotion(float amount,bool firearm,bool viewSway) {
    const unsigned amplitude=static_cast<unsigned>(std::lround(amount*4096));
    motion.store(amplitude ? amplitude | (firearm ? 8192u : 0u) | (viewSway ? 16384u : 0u) : 0u,
                 std::memory_order_relaxed);
}

using Poll = bool (__thiscall *)(void *,TrackingFrame *);
template<int Backend> struct Filter {
    static inline Poll previous=nullptr;
    static bool __fastcall poll(void *self,void *,TrackingFrame *f) {
        bool ok=previous(self,f);
        unsigned state=motion.load(std::memory_order_relaxed);
        if (ok && f && state) {
            const Rock rock=rocking(GetTickCount64()*.001,(state&8191)/4096.f);
            driftHands(*f,rock,state&8192);
            if ((state&16384) && swayView(*f,rock.viewRoll) && std::abs(rock.viewRoll)>.004f) {
                constexpr unsigned backendBit=1u<<Backend;
                if (!(viewApplied.load(std::memory_order_relaxed)&backendBit))
                    viewApplied.fetch_or(backendBit,std::memory_order_relaxed);
            }
        }
        return ok;
    }
    static bool install(BYTE *host,unsigned table) {
        void **slot=reinterpret_cast<void **>(host+table)+6;
        void *next=*slot;
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(next,&region,sizeof(region)) || region.State!=MEM_COMMIT ||
            !(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))
            return false;
        DWORD protection;
        if (!VirtualProtect(slot,sizeof(void *),PAGE_READWRITE,&protection)) return false;
        previous=reinterpret_cast<Poll>(next);
        bool ok=InterlockedCompareExchangePointer(slot,reinterpret_cast<void *>(poll),next)==next;
        DWORD ignored;
        VirtualProtect(slot,sizeof(void *),protection,&ignored);
        return ok;
    }
};

unsigned installDrift() {
    auto *host=reinterpret_cast<BYTE *>(GetModuleHandleW(L"TTWVR.dll"));
    if (!host) return 0;
    auto *dos=reinterpret_cast<IMAGE_DOS_HEADER *>(host);
    auto *nt=reinterpret_cast<IMAGE_NT_HEADERS32 *>(host+dos->e_lfanew);
    const BYTE xr[]{0x88,0x47,0x25,0x8d,0x4c,0x24,0x28}, vr[]{0x88,0x41,0x20,0x8d,0x4c,0x24};
    if (nt->FileHeader.TimeDateStamp!=0x6afa9b4f || nt->OptionalHeader.SizeOfImage!=0x1d84000 ||
        std::memcmp(host+0x6e986d,xr,sizeof(xr)) || std::memcmp(host+0x6c8954,vr,sizeof(vr))) return 0;
    unsigned result=Filter<0>::install(host,0xaf3568) ? 1u : 0u;
    if (Filter<1>::install(host,0xaf36d0)) result|=2;
    return result;
}
}
