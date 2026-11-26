// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstring>
#include "Projection.h"
#include "Host.h"

namespace canteen {
static std::atomic<unsigned> motion{0};
static std::atomic<unsigned> viewApplied{0};
static std::atomic<PFN_xrEndFrame> runtimeEndFrame{nullptr};
static BYTE **loaderInstance=nullptr;
unsigned appliedViewBackends() { return viewApplied.load(std::memory_order_relaxed); }
void publishMotion(float amount,bool firearm,bool viewSway) {
    const unsigned amplitude=static_cast<unsigned>(std::lround(amount*4096));
    motion.store(amplitude ? amplitude | (firearm ? 8192u : 0u) | (viewSway ? 16384u : 0u) : 0u,
                 std::memory_order_relaxed);
}

using Poll = bool (__thiscall *)(void *,TrackingFrame *);
static XrResult XRAPI_CALL endFrame(XrSession session,const XrFrameEndInfo *frame) {
    const unsigned state=motion.load(std::memory_order_relaxed);
    const float angle=(state&16384) ? rocking(GetTickCount64()*.001,(state&8191)/4096.f).viewRoll : 0.f;
    bool applied=false;
    auto result=submitView(runtimeEndFrame.load(),session,frame,angle,applied);
    if (applied && std::abs(angle)>.004f && !(viewApplied.load(std::memory_order_relaxed)&1u))
        viewApplied.fetch_or(1u,std::memory_order_relaxed);
    return result;
}

// Called during a successful OpenXR poll with a live instance. TTWVR's
// embedded loader reads this heap table even if a different addon wraps the
// game's xrEndFrame call site. A recreated table can use the same downstream.
static void bindViewSubmission() {
    if (!loaderInstance || !*loaderInstance) return;
    auto *table=*reinterpret_cast<BYTE **>(*loaderInstance+0x2c);
    if (!table) return;
    auto *slot=reinterpret_cast<void *volatile *>(table+0x8c);
    auto current=reinterpret_cast<PFN_xrEndFrame>(*slot);
    if (!current || current==endFrame) return;
    PFN_xrEndFrame previous=nullptr;
    if (!runtimeEndFrame.compare_exchange_strong(previous,current) && previous!=current) return;
    InterlockedCompareExchangePointer(slot,reinterpret_cast<void *>(endFrame),reinterpret_cast<void *>(current));
}

template<int Backend> struct Filter {
    static inline Poll previous=nullptr;
    static bool __fastcall poll(void *self,void *,TrackingFrame *f) {
        bool ok=previous(self,f);
        unsigned state=motion.load(std::memory_order_relaxed);
        if (ok && f && state) {
            const Rock rock=rocking(GetTickCount64()*.001,(state&8191)/4096.f);
            driftHands(*f,rock,state&8192);
            if constexpr (Backend==0) {
                if (state&16384) bindViewSubmission();
            } else if ((state&16384) && swayView(*f,rock.viewRoll) && std::abs(rock.viewRoll)>.004f) {
                constexpr unsigned backendBit=1u<<Backend;
                if (!(viewApplied.load(std::memory_order_relaxed)&backendBit))
                    viewApplied.fetch_or(backendBit,std::memory_order_relaxed);
            }
        }
        return ok;
    }
    static bool install(void **slot) {
        if (!slot) return false;
        if (previous) return true;
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
        if (!ok) previous=nullptr;
        return ok;
    }
};

static void **xrPollSlot=nullptr, **vrPollSlot=nullptr;

void prepareDrift() {
    // Discover once during addon initialization. Install on first use so the
    // initial sip never has to scan the image and later addons can wrap first.
    HMODULE module=GetModuleHandleW(L"TTWVR.dll");
    if (!module) return;
    host::Image image(module);
    loaderInstance=image.xrInstance();
    xrPollSlot=image.pollSlot(".?AVOpenXRBackend@nvvr@@");
    vrPollSlot=image.pollSlot(".?AVOpenVRBackend@nvvr@@");
}

unsigned installDrift() {
    unsigned result=Filter<0>::install(xrPollSlot) ? 1u : 0u;
    if (Filter<1>::install(vrPollSlot)) result|=2;
    return result;
}
}
