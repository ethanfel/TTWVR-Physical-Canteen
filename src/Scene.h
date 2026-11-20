// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Grip.h"
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
#if defined(_WIN32) || UINTPTR_MAX==UINT32_MAX
static_assert(sizeof(void *)==4);
static_assert(offsetof(Object,local)==0x34 && offsetof(Object,world)==0x68 && sizeof(Object)==0x9c);
#endif

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

struct Measurement { float distance, up; Hand hand; Object *root; };
struct DrawnRig {
    Object *root = nullptr;
    Object *mesh = nullptr;
    Hand hand = Hand::right;
    float error = std::numeric_limits<float>::infinity();
};

inline DrawnRig drawnRig(Object *root) {
    DrawnRig rig;
    rig.root=root;
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

// Same leaf update used by JIP LN after changing a NIF block transform.
// Controllers stay off. Gamebryo propagates the mesh transform and bounds.
struct UpdateData {
    float timePassed{};
    unsigned char options[8]{};
};
static_assert(sizeof(UpdateData)==12);
inline void place(Object *mesh, Hand hand) {
    if (!mesh || !(mesh->local.scale>0.f)) return;
    const Grip fit = sideGrip(hand, mesh->local.scale);
    if (!std::memcmp(&fit.rotation,&mesh->local.rotation,sizeof(Rotation)) &&
        !std::memcmp(&fit.position,&mesh->local.position,sizeof(Point))) return;
    mesh->local.rotation = fit.rotation;
    mesh->local.position = fit.position;
    using Update = void (__thiscall *)(Object *,const UpdateData &,unsigned);
    static const UpdateData update{};
    reinterpret_cast<Update>(mesh->methods[0xA4/4])(mesh,update,0);
}

inline Object *worldRoot(char *player) {
    auto *loaded=player ? *reinterpret_cast<char **>(player+0x64) : nullptr;
    return loaded ? *reinterpret_cast<Object **>(loaded+0x14) : nullptr;
}

inline DrawnRig fitRoots(Object *thirdRoot, Object *firstRoot, bool beforeDraw) {
    DrawnRig third=drawnRig(thirdRoot);
    DrawnRig first=firstRoot==thirdRoot ? third : drawnRig(firstRoot);
    const auto &tracked = third.error <= first.error ? third : first;
    // During an equip the Weapon mount can still be on the body. The score
    // sums the squared wrist and knuckle errors in unscaled game units.
    if (!std::isfinite(tracked.error) || (beforeDraw && tracked.error>4.f)) return {};
    place(third.mesh,tracked.hand);
    if (first.mesh!=third.mesh) place(first.mesh,tracked.hand);
    return tracked;
}

inline Object *firstPersonRoot(char *player) {
    return *reinterpret_cast<Object **>(player+0x694);
}

// During equip the actor process and the two biped slots need not update on
// the same frame. Look for this exact form before searching the player's 3D.
inline bool hasCanteen(char *player, const void *form) {
    if (!player || !form) return false;
    for (unsigned offset : {0x1B4u, 0x68Cu}) {
        auto *biped=*reinterpret_cast<char **>(player+offset);
        // BipedAnim::slotData[5].item, FalloutNV 1.4.0.525.
        if (biped && *reinterpret_cast<void **>(biped+0x2C+5*0x10)==form) return true;
    }
    // Actor::GetEquippedWeapon on FalloutNV 1.4.0.525.
    auto *process=*reinterpret_cast<char **>(player+0x68);
    if (!process || static_cast<unsigned char>(process[0x28])>1) return false;
    auto *entry=*reinterpret_cast<char **>(process+0x114);
    return entry && *reinterpret_cast<void **>(entry+8)==form;
}

inline bool prepareDraw(const void *form) {
    auto *player=*reinterpret_cast<char **>(0x11dea3c);
    if (!hasCanteen(player,form)) return false;
    return fitRoots(worldRoot(player),firstPersonRoot(player),true).mesh!=nullptr;
}

inline bool measure(Measurement &out) {
    auto *player=*reinterpret_cast<char **>(0x11dea3c);
    if (!player) return false;
    DrawnRig tracked=fitRoots(worldRoot(player),firstPersonRoot(player),false);
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
    out={length(cap-mouth)/units,pose.rotation.m[2][2],tracked.hand,tracked.root};
    return std::isfinite(out.distance) && std::isfinite(out.up);
}
}
