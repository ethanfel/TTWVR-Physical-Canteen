// SPDX-License-Identifier: GPL-3.0-only
#ifndef _WIN32
#if defined(__i386__)
#define __thiscall __attribute__((thiscall))
#else
#define __thiscall
#endif
#endif
#include "../src/Scene.h"
#include <cassert>

using namespace canteen;
static int propagated;
static Node * __thiscall asNode(Object *obj) { return static_cast<Node *>(obj); }
static void __thiscall propagate(Object *obj, const UpdateData &, unsigned flags) {
    assert(flags==0);
    obj->world=obj->local; // The test mount has an identity world transform.
    ++propagated;
}

struct PlayerRig {
    void *methods[0xA4/4+1]{};
    Node root{}, mount{}, flask{}, left{}, right{}, leftIndex{}, rightIndex{};
    Object *rootChildren[3]{&mount,&left,&right};
    Object *mountChild=&flask, *leftChild=&leftIndex, *rightChild=&rightIndex;

    PlayerRig() {
        methods[3]=reinterpret_cast<void *>(asNode);
        methods[0xA4/4]=reinterpret_cast<void *>(propagate);
        for (auto *n : {&root,&mount,&flask,&left,&right,&leftIndex,&rightIndex}) {
            n->methods=methods;
            n->local.scale=n->world.scale=1.f;
            for (int i=0;i<3;i++) n->local.rotation.m[i][i]=n->world.rotation.m[i][i]=1.f;
        }
        root.children=rootChildren; root.capacity=root.end=3;
        mount.name="Weapon"; mount.children=&mountChild; mount.capacity=mount.end=1;
        flask.name="canteen:0";
        left.name="Bip01 L Hand"; left.children=&leftChild; left.capacity=left.end=1;
        right.name="Bip01 R Hand"; right.children=&rightChild; right.capacity=right.end=1;
        leftIndex.name="Bip01 L Finger1"; rightIndex.name="Bip01 R Finger1";
        left.world.position=right.world.position={50.f,0.f,0.f};
    }
    void hold(Hand hand) {
        left.world.position=right.world.position={50.f,0.f,0.f};
        (hand==Hand::left?left:right).world.position=wristInMount(hand);
        (hand==Hand::left?leftIndex:rightIndex).world.position=indexInMount(hand);
    }
    void check(Hand hand) const {
        auto fit=sideGrip(hand,1.f);
        assert(length(flask.world.position-fit.position)<.0001f);
        assert(!std::memcmp(&flask.world.rotation,&fit.rotation,sizeof(Rotation)));
    }
};

int main() {
#if UINTPTR_MAX==UINT32_MAX
    alignas(void *) char player[0x698]{}, biped[0x160]{}, process[0x118]{}, entry[12]{};
    int flaskForm, other;
    assert(!hasCanteen(player,&flaskForm));
    *reinterpret_cast<char **>(player+0x68)=process;
    *reinterpret_cast<char **>(process+0x114)=entry;
    *reinterpret_cast<void **>(entry+8)=&flaskForm;
    assert(hasCanteen(player,&flaskForm));
    process[0x28]=2;
    assert(!hasCanteen(player,&flaskForm));
    for (unsigned offset : {0x1B4u,0x68Cu}) {
        *reinterpret_cast<char **>(player+offset)=biped;
        *reinterpret_cast<void **>(biped+0x7C)=&flaskForm;
        assert(hasCanteen(player,&flaskForm) && !hasCanteen(player,&other));
        *reinterpret_cast<char **>(player+offset)=nullptr;
    }
#endif
    PlayerRig third,first;
    // Neither hand owns the weapon on the body's holster mount.
    assert(!fitRoots(&third.root,&first.root,true).mesh && !propagated);
    // First frame of a left draw, even before the scripted weapon-out flag.
    third.hold(Hand::left);
    assert(fitRoots(&third.root,&first.root,true).hand==Hand::left);
    third.check(Hand::left); first.check(Hand::left); assert(propagated==2);
    fitRoots(&third.root,&first.root,true); assert(propagated==2);
    // The other camera rig can locate the right hand first on the next draw.
    third.left.world.position={50.f,0.f,0.f}; first.hold(Hand::right);
    assert(fitRoots(&third.root,&first.root,true).hand==Hand::right);
    third.check(Hand::right); first.check(Hand::right); assert(propagated==4);
    first.hold(Hand::left); fitRoots(&first.root,&first.root,true);
    first.check(Hand::left); assert(propagated==5);
    first.flask.name="other";
    assert(!fitRoots(nullptr,&first.root,true).mesh && propagated==5);
}
