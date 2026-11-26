// SPDX-License-Identifier: GPL-3.0-only
// Pass the path to an installed TTWVR.dll. No game, save or headset is used.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/Host.h"
#include "../src/Projection.h"
#include <cassert>
#include <cstdio>

namespace canteen {
void prepareDrift();
unsigned installDrift();
void publishMotion(float,bool,bool);
}

static unsigned polls, submits;
static XrCompositionLayerProjectionView eyes[2]{};
static bool __fastcall device(void *,void *,canteen::TrackingFrame *frame) {
    ++polls;
    *frame={};
    frame->valid=1;
    frame->head.valid=frame->head.tracked=1;
    for (unsigned i=0;i<2;++i) {
        frame->eyes[i].pose=frame->head;
        frame->eyes[i].pose.position.x=i ? .03f : -.03f;
        frame->hands[i].aim=frame->hands[i].grip=frame->eyes[i].pose;
    }
    return true;
}
static XrResult XRAPI_CALL submit(XrSession,const XrFrameEndInfo *frame) {
    auto *world=reinterpret_cast<const XrCompositionLayerProjection *>(frame->layers[0]);
    assert(world->viewCount==2 && world->views!=eyes);
    assert(world->views[0].pose.orientation.z!=0 && world->views[1].pose.orientation.z!=0);
    ++submits;
    return XR_SUCCESS;
}
static void replace(void **slot,void *method) {
    DWORD old,unused;
    assert(VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&old));
    *slot=method;
    assert(VirtualProtect(slot,sizeof(*slot),old,&unused));
}

int wmain(int argc,wchar_t **argv) {
    assert(argc==2);
    // Map the actual PE with relocations without initializing its game plugin.
    HMODULE module=LoadLibraryExW(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    assert(module && GetModuleHandleW(L"TTWVR.dll")==module);
    canteen::host::Image image(module);
    void **xr=image.pollSlot(".?AVOpenXRBackend@nvvr@@");
    void **vr=image.pollSlot(".?AVOpenVRBackend@nvvr@@");
    BYTE **instance=image.xrInstance();
    assert(xr && vr && xr!=vr && instance);
    assert(!image.pollSlot(".?AVUnknownBackend@nvvr@@"));
    replace(xr,reinterpret_cast<void *>(device));
    replace(vr,reinterpret_cast<void *>(device));
    assert(image.pollMethod(image.originalMethod(xr)));
    assert(image.pollMethod(image.originalMethod(vr)));
    assert(image.pollSlot(".?AVOpenXRBackend@nvvr@@")==xr);
    assert(image.pollSlot(".?AVOpenVRBackend@nvvr@@")==vr);
    canteen::prepareDrift();
    const auto first=canteen::installDrift(), second=canteen::installDrift();
    assert(first==3 && second==3);

    void *dispatch[64]{}, *loader[16]{};
    dispatch[0x8c/sizeof(void*)]=reinterpret_cast<void *>(submit);
    loader[0x2c/sizeof(void*)]=dispatch;
    *instance=reinterpret_cast<BYTE *>(loader);
    using Poll=bool (__thiscall *)(void *,canteen::TrackingFrame *);
    canteen::TrackingFrame frame;
    assert(reinterpret_cast<Poll>(*vr)(nullptr,&frame) && frame.eyes[0].pose.orientation.z==0);
    canteen::publishMotion(1.f,true,true);
    assert(reinterpret_cast<Poll>(*vr)(nullptr,&frame) && frame.eyes[0].pose.orientation.z!=0);
    assert(reinterpret_cast<Poll>(*xr)(nullptr,&frame) && (frame.hands[0].aim.orientation.x!=0.f || frame.hands[0].aim.orientation.y!=0.f));
    XrCompositionLayerProjection world{};
    world.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    world.views=eyes;
    world.viewCount=2;
    for (auto &eye:eyes) {
        eye.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        eye.pose.orientation.w=1.f;
    }
    const auto *layer=reinterpret_cast<const XrCompositionLayerBaseHeader *>(&world);
    XrFrameEndInfo end{};
    end.layerCount=1;
    end.layers=&layer;
    auto fn=reinterpret_cast<PFN_xrEndFrame>(dispatch[0x8c/sizeof(void*)]);
    assert(fn!=submit && fn(XR_NULL_HANDLE,&end)==XR_SUCCESS);
    assert(polls==3 && submits==1 && eyes[0].pose.orientation.z==0);
    // Reject an unrelated frame shape for only the affected backend. All
    // writes here are private to the test process's mapped copy of the DLL.
    auto *method=image.body(image.originalMethod(xr));
    const BYTE frameCount[]{0xb9,0x4c,0,0,0};
    auto *count=canteen::host::Span{method,0x300}.unique(frameCount,sizeof(frameCount));
    DWORD old,unused;
    assert(count && VirtualProtect(count+1,1,PAGE_EXECUTE_READWRITE,&old));
    count[1]=0x4d;
    assert(!image.pollSlot(".?AVOpenXRBackend@nvvr@@") && image.pollSlot(".?AVOpenVRBackend@nvvr@@")==vr);
    count[1]=0x4c;
    assert(VirtualProtect(count+1,1,old,&unused));
    auto *command=image.ro.unique("xrEndFrame",sizeof("xrEndFrame"));
    assert(command && VirtualProtect(command,1,PAGE_READWRITE,&old));
    command[0]='?';
    assert(!image.xrInstance());
    command[0]='x';
    assert(VirtualProtect(command,1,old,&unused));
    std::puts("PASS backend discovery, existing controller hooks, aim drift, OpenVR sway and stereo OpenXR submission");
    return 0;
}
