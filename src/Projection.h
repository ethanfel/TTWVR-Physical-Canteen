// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Balance.h"
#include <openxr.h>
#include <array>
#include <memory>
#include <new>

namespace canteen {
// Work on the world projection after TTWVR has rendered it. The cameras and
// the runtime's physical tracking stay independent of the displayed lean.
inline bool tiltProjection(XrCompositionLayerProjectionView (&eyes)[2],float angle) {
    Quaternion orientations[2];
    Point positions[2];
    for (unsigned i=0;i<2;++i) {
        const auto &q=eyes[i].pose.orientation;
        const auto &p=eyes[i].pose.position;
        orientations[i]={q.x,q.y,q.z,q.w};
        positions[i]={p.x,p.y,p.z};
        if (eyes[i].type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW ||
            !validPose({positions[i],orientations[i],1,0,{}})) return false;
    }
    const Point forward=unit(rotate(orientations[0],{0,0,-1})+rotate(orientations[1],{0,0,-1}));
    if (dot(forward,forward)<.9f) return false;
    const auto tilt=turn(forward,-angle);
    const auto center=(positions[0]+positions[1])*.5f;
    for (unsigned i=0;i<2;++i) {
        const auto q=product(tilt,orientations[i]);
        const auto p=center+rotate(tilt,positions[i]-center);
        eyes[i].pose={{q.x,q.y,q.z,q.w},{p.x,p.y,p.z}};
    }
    return true;
}

inline XrResult submitView(PFN_xrEndFrame next,XrSession session,const XrFrameEndInfo *frame,
                           float angle,bool &applied) {
    applied=false;
    if (!frame || angle==0.f || !frame->layers) return next(session,frame);
    for (unsigned i=0;i<frame->layerCount;++i) {
        const auto *base=frame->layers[i];
        if (!base || base->type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION) continue;
        auto world=*reinterpret_cast<const XrCompositionLayerProjection *>(base);
        if (world.viewCount!=2 || !world.views) continue;
        XrCompositionLayerProjectionView eyes[]{world.views[0],world.views[1]};
        if (!tiltProjection(eyes,angle)) return next(session,frame);
        world.views=eyes;
        std::array<const XrCompositionLayerBaseHeader *,XR_MIN_COMPOSITION_LAYERS_SUPPORTED> local;
        using Layer = const XrCompositionLayerBaseHeader *;
        std::unique_ptr<Layer[]> extra;
        auto **layers=local.data();
        if (frame->layerCount>local.size()) {
            extra.reset(new (std::nothrow) Layer[frame->layerCount]);
            if (!extra) return next(session,frame);
            layers=extra.get();
        }
        std::copy_n(frame->layers,frame->layerCount,layers);
        layers[i]=reinterpret_cast<const XrCompositionLayerBaseHeader *>(&world);
        auto submission=*frame;
        submission.layers=layers;
        const XrResult result=next(session,&submission);
        applied=result==XR_SUCCESS;
        return result;
    }
    return next(session,frame);
}
}
