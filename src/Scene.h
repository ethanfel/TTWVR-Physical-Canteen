// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Fingers.h"
#include <array>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <limits>
#include <initializer_list>

namespace canteen {
struct Transform {
    Rotation rotation; Point position; float scale;
    Point point(Point p) const { return position + rotation.apply(p)*scale; }
};
// NetImmerse layouts for FalloutNV 1.4.0.525 (x86). Used only on the game thread.
struct Object {
    void **methods;
    unsigned refs;
    const char *name;
    char beforeParent[12];
    Object *parent;
    char beforeFlags[20];
    unsigned flags;
    Transform local, world;
};
struct Node : Object {
    void *allocator;
    Object **children;
    unsigned short capacity, end;
};
static_assert(offsetof(Object,local)==0x34 && offsetof(Object,world)==0x68 && sizeof(Object)==0x9c);

inline Object *find(Object *object, const char *name, unsigned depth=0) {
    if (!object || depth>40) return nullptr;
    if (object->name && std::strcmp(object->name,name)==0) return object;
    using AsNode = Node* (__thiscall *)(Object *);
    Node *n = reinterpret_cast<AsNode>(object->methods[3])(object);
    if (n && n->children && n->end<=n->capacity)
        for (unsigned i=0;i<n->end;++i)
            if (auto *found=find(n->children[i],name,depth+1)) return found;
    return nullptr;
}

inline Object *handNode(Object *root, Hand side) {
    return find(root, side == Hand::left ? "Bip01 L Hand" : "Bip01 R Hand");
}
inline void updateHand(Object *wrist) {
    struct Update { float time; unsigned flags, unknown; } update{};
    using Fn = void (__thiscall *)(Object *,const Update &,unsigned);
    reinterpret_cast<Fn>(wrist->methods[0xa4/sizeof(void *)])(wrist,update,0);
}
struct HandPose {
    Object *wrist{};
    Hand side{};
    struct Joint { char name[32]{}; Object *bone{}; Rotation saved{}, posed{}; };
    std::array<Joint,std::size(grasp)> joints{};

    void release(Object *root) {
        if (!wrist) return;
        if (handNode(root,side)==wrist) {
            bool changed=false;
            for (unsigned i=0;i<joints.size();++i) {
                const auto &j=joints[i];
                auto *bone=find(wrist,j.name);
                if (bone && j.bone==bone && std::memcmp(&bone->local.rotation,&j.posed,sizeof(Rotation))==0) {
                    bone->local.rotation=j.saved;
                    changed=true;
                }
            }
            if (changed) updateHand(wrist);
        }
        *this={};
    }
    void hold(Object *root, Hand hand) {
        Object *current=handNode(root,hand);
        if (current==wrist && hand==side) return;
        release(root);
        wrist=current;
        side=hand;
        if (!wrist) return;
        for (unsigned i=0;i<joints.size();++i) {
            auto &j=joints[i];
            std::snprintf(j.name,sizeof(j.name),"Bip01 %c %s",hand==Hand::left ? 'L' : 'R',grasp[i].name);
            j.posed=fingerRotation(grasp[i].angles,hand);
        }
    }
    void apply() {
        for (auto &j : joints) {
            auto *bone=find(wrist,j.name);
            if (!bone) { j.bone=nullptr; continue; }
            if (bone!=j.bone || std::memcmp(&bone->local.rotation,&j.posed,sizeof(Rotation))!=0)
                j.saved=bone->local.rotation;
            j.bone=bone;
            bone->local.rotation=j.posed;
        }
    }
};
struct Roots { Object *third, *first; };
inline Roots playerRoots() {
    auto *player=*reinterpret_cast<char **>(0x11dea3c);
    if (!player) return {};
    auto *loaded=*reinterpret_cast<char **>(player+0x64);
    return {loaded ? *reinterpret_cast<Object **>(loaded+0x14) : nullptr,
            *reinterpret_cast<Object **>(player+0x694)};
}
inline HandPose poses[2];
inline void releaseGrip() {
    if (!poses[0].wrist && !poses[1].wrist) return;
    const auto roots=playerRoots();
    poses[0].release(roots.third);
    poses[1].release(roots.first);
}

struct Measurement { float distance, up; };
struct DrawnRig {
    Object *mesh = nullptr;
    Hand hand = Hand::right;
    float error = std::numeric_limits<float>::infinity();
};

inline DrawnRig drawnRig(Object *root) {
    DrawnRig rig;
    auto *mount = find(root,"Weapon");
    rig.mesh = find(mount,"canteen:0");
    if (!rig.mesh || !(mount->world.scale > 0.f)) return rig;

    // TTWVR moves the same Weapon mount to whichever controller draws. Match
    // wrist and index knuckle to that mount, including when the hands cross.
    for (Hand hand : {Hand::left, Hand::right}) {
        auto *wrist = find(root, hand == Hand::left ? "Bip01 L Hand" : "Bip01 R Hand");
        auto *index = find(wrist, hand == Hand::left ? "Bip01 L Finger1" : "Bip01 R Finger1");
        if (!index) continue;
        const auto &pose = mount->world;
        float error = (squaredLength(wrist->world.position - pose.point(wristInMount(hand))) +
                       squaredLength(index->world.position - pose.point(indexInMount(hand)))) /
                      (pose.scale * pose.scale);
        if (error < rig.error) { rig.hand = hand; rig.error = error; }
    }
    return rig;
}

inline void place(Object *mesh, Hand hand, bool fitted) {
    if (!mesh) return;
    const Grip fit = sideGrip(hand, mesh->local.scale, fitted);
    mesh->local.rotation = fit.rotation;
    mesh->local.position = fit.position;
}

inline bool measure(Measurement &out, bool fitted) {
    const auto roots=playerRoots();
    DrawnRig third=drawnRig(roots.third), first=drawnRig(roots.first);
    const auto &tracked = third.error <= first.error ? third : first;
    if (!std::isfinite(tracked.error)) return false;
    place(third.mesh, tracked.hand, fitted);
    place(first.mesh, tracked.hand, fitted);
    if (fitted && third.mesh) poses[0].hold(roots.third,tracked.hand);
    else poses[0].release(roots.third);
    if (fitted && first.mesh && roots.first!=roots.third) poses[1].hold(roots.first,tracked.hand);
    else poses[1].release(roots.first);
    auto *mesh = tracked.mesh;
    auto *scene = *reinterpret_cast<char **>(0x11dea0c);
    auto *camera = scene ? *reinterpret_cast<Object **>(scene+0xa0) : nullptr;
    if (!mesh || !camera || !(mesh->world.scale>0.f) || !(camera->world.scale>0.f)) return false;
    // Use the last propagated transform, exactly where the game placed the mesh.
    const auto &pose=mesh->world;
    Point cap=pose.point(capCenter);
    const auto &view=camera->world;
    // Camera +Y points forward, +Z up. The mouth is below and slightly ahead of the eyes.
    constexpr float units=70.f;
    Point mouth=view.position+view.rotation.apply({0.f,.048f*units,-.091f*units});
    out={length(cap-mouth)/units,pose.rotation.m[2][2]};
    return std::isfinite(out.distance) && std::isfinite(out.up);
}
}
