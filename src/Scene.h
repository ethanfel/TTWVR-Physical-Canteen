// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Sip.h"
#include <cstddef>
#include <cstring>

namespace canteen {
struct Rotation {
    float m[3][3];
    Point apply(Point v) const {
        return {m[0][0]*v.x+m[0][1]*v.y+m[0][2]*v.z,
                m[1][0]*v.x+m[1][1]*v.y+m[1][2]*v.z,
                m[2][0]*v.x+m[2][1]*v.y+m[2][2]*v.z};
    }
};
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

struct Measurement { float distance, up; };
inline Object *fitCanteen(Object *root) {
    auto *mesh = find(find(root,"Weapon"),"canteen:0");
    if (!mesh) return nullptr;
    // Classic Pack's local +Z runs from the base to the cap. Map it into the
    // weapon grip frame, and move the middle of the flask to the grip origin.
    constexpr Rotation fit{{{0.f,.5f,-.8660254f},{0.f,.8660254f,.5f},{1.f,0.f,0.f}}};
    mesh->local.rotation = fit;
    mesh->local.position = fit.apply({0.f,-.3f,-5.5f});
    return mesh;
}

inline bool measure(Measurement &out) {
    // TTWVR poses both Weapon nodes together while drawn. Full Body displays the
    // third-person instance. Resolve both afresh on the game thread after loads
    // and weapon swaps, and only fit the named canteen under the weapon mount.
    auto *player = *reinterpret_cast<char **>(0x11dea3c);
    if (!player) return false;
    auto *loaded = *reinterpret_cast<char **>(player+0x64);
    auto *third = loaded ? fitCanteen(*reinterpret_cast<Object **>(loaded+0x14)) : nullptr;
    auto *first = fitCanteen(*reinterpret_cast<Object **>(player+0x694));
    auto *mesh = first ? first : third;
    auto *scene = *reinterpret_cast<char **>(0x11dea0c);
    auto *camera = scene ? *reinterpret_cast<Object **>(scene+0xa0) : nullptr;
    if (!mesh || !camera || !(mesh->world.scale>0.f) || !(camera->world.scale>0.f)) return false;
    // Use the last propagated transform, exactly where the game placed the mesh.
    const auto &pose=mesh->world;
    Point cap=pose.point({0.f,.32f,13.34f});
    const auto &view=camera->world;
    // Camera +Y points forward, +Z up. The mouth is below and slightly ahead of the eyes.
    constexpr float units=70.f;
    Point mouth=view.position+view.rotation.apply({0.f,.048f*units,-.091f*units});
    out={length(cap-mouth)/units,pose.rotation.m[2][2]};
    return std::isfinite(out.distance) && std::isfinite(out.up);
}
}
