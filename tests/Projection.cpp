// SPDX-License-Identifier: GPL-3.0-only
#include "../src/Projection.h"
#include <cassert>
#include <cstring>

using namespace canteen;
static const XrFrameEndInfo *original;
static bool expectTilt;
static float expectedAngle;
static XrResult runtimeResult=XR_SUCCESS;
static unsigned calls;
static Quaternion orientation(const XrPosef &p) {
    return {p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w};
}
static Point location(const XrPosef &p) { return {p.position.x,p.position.y,p.position.z}; }
static XrResult XRAPI_CALL receive(XrSession session,const XrFrameEndInfo *frame) {
    ++calls;
    assert(session==XR_NULL_HANDLE);
    if (!expectTilt) {
        assert(frame==original);
        return runtimeResult;
    }
    assert(frame!=original && frame->layerCount==original->layerCount && frame->displayTime==original->displayTime);
    for (unsigned i=0;i<frame->layerCount;++i) {
        if (i!=1) { assert(frame->layers[i]==original->layers[i]); continue; }
        const auto &world=*reinterpret_cast<const XrCompositionLayerProjection *>(frame->layers[i]);
        const auto &raw=*reinterpret_cast<const XrCompositionLayerProjection *>(original->layers[i]);
        assert(world.views!=raw.views && world.space==raw.space && world.next==raw.next);
        Point center=(location(raw.views[0].pose)+location(raw.views[1].pose))*.5f;
        Point forward=unit(rotate(orientation(raw.views[0].pose),{0,0,-1})+
                           rotate(orientation(raw.views[1].pose),{0,0,-1}));
        for (int eye=0;eye<2;++eye) {
            const auto &pose=world.views[eye].pose;
            const auto &before=raw.views[eye].pose;
            const auto expected=turn(forward,-expectedAngle);
            assert(length(location(pose)-center-rotate(expected,location(before)-center))<.00001f);
            Point oldUp=rotate(orientation(before),{0,1,0}), up=rotate(orientation(pose),{0,1,0});
            assert(length(up-rotate(expected,oldUp))<.00001f);
            assert(std::abs(std::atan2(dot(cross(oldUp,up),forward),dot(oldUp,up))+expectedAngle)<.0001f);
            assert(world.views[eye].next==raw.views[eye].next &&
                   !std::memcmp(&world.views[eye].fov,&raw.views[eye].fov,sizeof(XrFovf)) &&
                   !std::memcmp(&world.views[eye].subImage,&raw.views[eye].subImage,sizeof(XrSwapchainSubImage)));
        }
    }
    return runtimeResult;
}

int main() {
    XrCompositionLayerProjectionView eyes[2]{};
    int extension;
    const auto head=product(turn({0,1,0},1.2f),turn({1,0,0},.3f));
    for (int i=0;i<2;++i) {
        float side=i ? 1.f : -1.f;
        auto q=product(head,turn({0,1,0},.04f*side));
        auto p=Point{1,1.7f,.4f}+rotate(head,{side*.032f,0,0});
        eyes[i].type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        eyes[i].next=&extension; eyes[i].pose={{q.x,q.y,q.z,q.w},{p.x,p.y,p.z}};
        eyes[i].fov={-.8f,.85f,.9f,-.9f}; eyes[i].subImage.imageArrayIndex=i;
    }
    XrCompositionLayerProjection world{};
    world.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION; world.viewCount=2; world.views=eyes;
    XrCompositionLayerQuad overlay{};
    overlay.type=XR_TYPE_COMPOSITION_LAYER_QUAD;
    const XrCompositionLayerBaseHeader *layers[20];
    std::fill_n(layers,20,reinterpret_cast<const XrCompositionLayerBaseHeader *>(&overlay));
    layers[1]=reinterpret_cast<const XrCompositionLayerBaseHeader *>(&world);
    XrFrameEndInfo frame{}; frame.type=XR_TYPE_FRAME_END_INFO; frame.layers=layers; original=&frame;
    auto check=[&](float angle,bool shouldTilt) {
        expectedAngle=angle; expectTilt=shouldTilt;
        unsigned prior=calls;
        XrCompositionLayerProjectionView saved[]{eyes[0],eyes[1]};
        bool applied;
        assert(submitView(receive,XR_NULL_HANDLE,&frame,angle,applied)==runtimeResult);
        assert(applied==(shouldTilt && runtimeResult==XR_SUCCESS));
        assert(calls==prior+1 && !std::memcmp(saved,eyes,sizeof(saved)));
    };
    for (unsigned n:{2u,20u}) {
        frame.layerCount=n;
        for (int level=1;level<=3;++level) for (int i=0;i<120;++i)
            check(rocking(i*.1,level/3.f).viewRoll,true);
        check(0,false);
    }
    runtimeResult=XR_ERROR_SESSION_LOST; check(.05f,true); runtimeResult=XR_SUCCESS;
    eyes[1].pose.orientation.w=99; check(.04f,false);
    world.viewCount=1; check(.02f,false);
    frame.layerCount=1; check(.03f,false);
    frame.layerCount=0; check(.04f,false);
}
