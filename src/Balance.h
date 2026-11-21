// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Grip.h"
#include <algorithm>
#include <cstddef>

namespace canteen {
inline float dot(Point a, Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Point cross(Point a, Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Point unit(Point v) { float n=length(v); return n>1.e-6f ? v*(1.f/n) : Point{}; }
struct Quaternion { float x{},y{},z{},w{1}; };
inline Quaternion product(Quaternion a, Quaternion b) {
    Point av{a.x,a.y,a.z}, bv{b.x,b.y,b.z};
    Point v=bv*a.w+av*b.w+cross(av,bv);
    return {v.x,v.y,v.z,a.w*b.w-dot(av,bv)};
}
inline Quaternion turn(Point axis,float radians) {
    Point v=axis*std::sin(radians*.5f);
    return {v.x,v.y,v.z,std::cos(radians*.5f)};
}
inline Point rotate(Quaternion q,Point p) {
    Point axis{q.x,q.y,q.z}, v=cross(axis,p);
    return p+(v*q.w+cross(axis,v))*2.f;
}
inline Quaternion between(Point a,Point b) {
    a=unit(a); b=unit(b);
    float c=std::clamp(dot(a,b),-1.f,1.f);
    if (c<-.99999f) {
        Point axis=unit(cross(a,std::abs(a.x)<.8f ? Point{1,0,0} : Point{0,1,0}));
        return {axis.x,axis.y,axis.z,0};
    }
    Point v=cross(a,b);
    const float n=std::sqrt(dot(v,v)+(1+c)*(1+c));
    return {v.x/n,v.y/n,v.z/n,(1+c)/n};
}
struct Rock { float yaw,pitch,lean,viewRoll; };
inline Rock rocking(double seconds,float strength) {
    // Lean starts above the first extra sip and grows smoothly alongside aim drift.
    float u=std::clamp((strength-.35f)/.65f,0.f,1.f);
    float balance=u*u*(3.f-2.f*u);
    float wave=static_cast<float>(.8*std::sin(seconds*.89)+.2*std::sin(seconds*1.31+.6));
    constexpr float maxViewRoll=3.f*3.14159265f/180.f;
    return {float(.030*strength*(.72*std::sin(seconds*1.43)+.28*std::sin(seconds*.81))),
            float(.018*strength*std::sin(seconds*1.09+.8)),
            .046f*balance*wave, maxViewRoll*strength*(.4f+.6f*strength)*wave};
}

struct TrackedPose { Point position; Quaternion orientation; unsigned char valid,tracked,pad[2]; };
struct TrackingFrame {
    TrackedPose head;
    struct { TrackedPose aim,grip; } hands[2];
    struct { TrackedPose pose; float fov[4]; } eyes[2];
    float sticks[4], triggers[2], squeeze[2];
    unsigned char buttons[12], valid, pad[3];
};
static_assert(sizeof(TrackingFrame)==0x130 && offsetof(TrackingFrame,hands)==0x20 &&
              offsetof(TrackingFrame,eyes)==0xa0 && offsetof(TrackingFrame,valid)==0x12c);
inline bool validPose(const TrackedPose &p) {
    const auto &q=p.orientation;
    return p.valid && std::isfinite(dot(p.position,p.position)) &&
           std::abs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1.f)<.05f;
}
// Legacy OpenVR uses the host-side camera sample.
inline bool swayView(TrackingFrame &frame,float roll) {
    if (roll==0.f || !frame.valid || !validPose(frame.head) ||
        !validPose(frame.eyes[0].pose) || !validPose(frame.eyes[1].pose)) return false;
    const auto &head=frame.head;
    const Point pivot=head.position+rotate(head.orientation,{0,-.18f,0});
    const Quaternion tilt=turn(unit(rotate(head.orientation,{0,0,-1})),roll);
    for (auto *p:{&frame.head,&frame.eyes[0].pose,&frame.eyes[1].pose}) {
        p->position=pivot+rotate(tilt,p->position-pivot);
        p->orientation=product(tilt,p->orientation);
    }
    return true;
}
inline void driftHands(TrackingFrame &frame,Rock r,bool firearm) {
    if (!frame.valid || !validPose(frame.head)) return;
    Point up{0,1,0}, right=rotate(frame.head.orientation,{1,0,0});
    right.y=0; right=unit(right);
    if (dot(right,right)<.9f) return;
    Point waist=frame.head.position-up*.55f;
    Point center{}; int count=0;
    for (auto &h:frame.hands) if (validPose(h.grip) && h.grip.tracked) {center=center+h.grip.position; ++count;}
    if (!count) return;
    center=center*(1.f/count);
    const Quaternion aim=firearm ? product(turn(up,r.yaw),turn(right,r.pitch)) : Quaternion{};
    const Quaternion lean=turn(cross(up,right),r.lean);
    const Quaternion rotation=product(lean,aim);
    // Transform both ends of a two-hand grip together so the rig and shot aim
    // see the same orientation. Only hand poses in this fresh poll are changed.
    for (auto &h:frame.hands) for (auto *p:{&h.aim,&h.grip}) {
        if (!validPose(*p) || !p->tracked) continue;
        p->position=waist+rotate(lean,center-waist+rotate(aim,p->position-center));
        p->orientation=product(rotation,p->orientation);
    }
}
}
