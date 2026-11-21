// SPDX-License-Identifier: GPL-3.0-only
#ifndef _WIN32
#if defined(__i386__)
#define __thiscall __attribute__((thiscall))
#else
#define __thiscall
#endif
#endif
#include "../src/Torso.h"
#include <cassert>
#include <array>
#include <vector>

using namespace canteen;
static bool near(Point a,Point b,float eps=.0002f) { return length(a-b)<eps; }
static void samePose(const Transform &a,const Transform &b) {
    assert(near(a.position,b.position));
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) assert(std::abs(a.rotation.m[i][j]-b.rotation.m[i][j])<.0002f);
    assert(std::abs(a.scale-b.scale)<.00001f);
}
static Node * __thiscall node(Object *obj) { return static_cast<Node *>(obj); }
static void __thiscall update(Object *obj,const UpdateData &data,unsigned flags) {
    auto *n=node(obj);
    n->world=n->parent ? absolute(n->parent->world,n->local) : n->local;
    for (unsigned i=0;i<n->end;++i) update(n->children[i],data,flags);
}

static void testSpine() {
    enum {root,hips,spine,mid,chest,neck,neck1,head,left,lu,lf,lh,right,ru,rf,rh,foot,camera,count};
    const char *names[]={"Scene Root","Bip01 Pelvis","Bip01 Spine","Bip01 Spine1","Bip01 Spine2","Bip01 Neck",
        "Bip01 Neck1","Bip01 Head","Bip01 L Clavicle","Bip01 L UpperArm","Bip01 L Forearm","Bip01 L Hand",
        "Bip01 R Clavicle","Bip01 R UpperArm","Bip01 R Forearm","Bip01 R Hand","Foot","Camera3rd"};
    const int parents[]={-1,root,root,spine,mid,chest,neck,neck1,neck,left,lu,lf,neck,right,ru,rf,hips,root};
    const Point offsets[]={{30,42,80},{0,0,1},{0,0,2},{0,0,12},{0,0,10},{0,0,10},{0,0,4},{0,0,4},
        {-3,0,0},{-7,0,0},{-6,4,-7},{-3,9,-1},{3,0,0},{7,0,0},{6,4,-7},{3,9,-1},{0,0,-20},{0,0,55}};
    std::array<Node,count> rig{};
    std::vector<Object *> children[count];
    void *methods[0xA4/4+1]{};
    methods[3]=reinterpret_cast<void *>(node);
    methods[0xA4/4]=reinterpret_cast<void *>(update);
    for (int i=0;i<count;++i) {
        rig[i].methods=methods; rig[i].name=names[i];
        rig[i].local={matrix(Quaternion{}),offsets[i],1.f};
        if (i==root) rig[i].local.rotation=matrix(turn({0,0,1},.85f));
        if (parents[i]>=0) {
            rig[i].parent=&rig[parents[i]];
            children[parents[i]].push_back(&rig[i]);
        }
    }
    for (int i=0;i<count;++i) {
        rig[i].children=children[i].data();
        rig[i].end=rig[i].capacity=static_cast<unsigned short>(children[i].size());
    }
    propagatePose(&rig[root]);
    auto rest=rig;
    for (int pass=0;pass<40;pass++) {
        assert(presentLean(&rig[root],pass<20 ? .04f : -.04f));
        for (int i=0;i<count;++i) samePose(rest[i].local,rig[i].local);
        for (int i:{hips,foot,camera,head,lh,rh}) samePose(rest[i].world,rig[i].world);
        for (int i:{chest,lu,ru}) assert(length(rig[i].world.position-rest[i].world.position)>.3f);
    }
    assert(presentLean(&rig[root],0.f));
    for (int i=0;i<count;++i) samePose(rest[i].world,rig[i].world);
    rig[rf].name="missing";
    assert(!presentLean(&rig[root],.04f));
    for (int i=0;i<count;++i) samePose(rest[i].local,rig[i].local);
}

static void testViewSway() {
    TrackingFrame raw{};
    raw.valid=1;
    raw.head={{.4f,1.65f,-.7f},product(turn({0,1,0},1.3f),turn({1,0,0},-.4f)),1,0,{}};
    for (int eye=0;eye<2;++eye) {
        const float side=eye ? 1.f : -1.f;
        auto &v=raw.eyes[eye];
        v.pose=raw.head;
        v.pose.position=v.pose.position+rotate(raw.head.orientation,{side*.032f,.004f,-.01f});
        v.pose.orientation=product(raw.head.orientation,turn({0,1,0},side*.06f));
        v.fov[0]=-.9f+side*.1f;
    }
    auto invalid=raw;
    invalid.eyes[1].pose.valid=0;
    auto unchanged=invalid;
    assert(!swayView(invalid,.05f) && !std::memcmp(&invalid,&unchanged,sizeof(raw)));
    auto sober=raw;
    assert(!swayView(sober,rocking(1,0).viewRoll) && !std::memcmp(&raw,&sober,sizeof(raw)));

    float maximum[3]{};
    for (int level=1;level<=3;++level) for (int tick=0;tick<900;++tick) {
        auto sample=raw; // Each poll starts with fresh physical tracking, including with hands offline.
        const float angle=rocking(tick/90.,level/3.f).viewRoll;
        maximum[level-1]=std::max(maximum[level-1],std::abs(angle));
        assert(swayView(sample,angle));
        Quaternion localRoll=turn({0,0,-1},angle);
        Quaternion expected=product(raw.head.orientation,localRoll);
        assert(near(rotate(sample.head.orientation,{0,1,0}),rotate(expected,{0,1,0})));
        for (int eye=0;eye<2;++eye) {
            auto toHead=[](const TrackedPose &head,Point world) {
                Quaternion inverse{-head.orientation.x,-head.orientation.y,-head.orientation.z,head.orientation.w};
                return rotate(inverse,world);
            };
            const auto &v=sample.eyes[eye].pose, &before=raw.eyes[eye].pose;
            assert(near(toHead(sample.head,v.position-sample.head.position),
                        toHead(raw.head,before.position-raw.head.position)));
            assert(near(toHead(sample.head,rotate(v.orientation,{0,0,-1})),
                        toHead(raw.head,rotate(before.orientation,{0,0,-1}))));
            assert(!std::memcmp(sample.eyes[eye].fov,raw.eyes[eye].fov,sizeof(sample.eyes[eye].fov)));
        }
        assert(!std::memcmp(sample.hands,raw.hands,sizeof(raw.hands)));
        assert(!std::memcmp(sample.sticks,raw.sticks,sizeof(raw)-offsetof(TrackingFrame,sticks)));
    }
    assert(maximum[0]>.008f && maximum[0]<maximum[1] && maximum[1]<maximum[2]);
    assert(maximum[2]>.045f && maximum[2]<=.05236f);
}

int main() {
    testViewSway();
    TrackingFrame sample{};
    sample.valid=1;
    sample.head={{0,1.7f,0},{},1,0,{}};
    for(int i=0;i<2;++i) {
        sample.hands[i].grip={{i ? .2f : -.15f,1.3f,-.35f}, {},1,1,{}};
        sample.hands[i].aim=sample.hands[i].grip;
    }
    auto sober=sample;
    driftHands(sober,rocking(1.2,0),true);
    assert(near(sober.hands[0].grip.position,sample.hands[0].grip.position));
    auto low=rocking(1.2,.33f), high=rocking(1.2,1.f);
    assert(low.lean==0.f && std::abs(high.lean)>.01f && std::abs(low.yaw)<std::abs(high.yaw));
    auto armed=sample; driftHands(armed,low,true);
    assert(std::abs(armed.hands[0].aim.orientation.y)>.0001f);
    auto idle=sample; driftHands(idle,low,false);
    assert(near(idle.hands[0].aim.position,sample.hands[0].aim.position));
    auto both=sample; driftHands(both,high,true);
    assert(!std::memcmp(&both.head,&sample.head,sizeof(sample.head)));
    assert(!std::memcmp(&both.eyes,&sample.eyes,sizeof(TrackingFrame)-offsetof(TrackingFrame,eyes)));
    Point delta=sample.hands[1].grip.position-sample.hands[0].grip.position;
    assert(near(both.hands[1].grip.position-both.hands[0].grip.position,rotate(both.hands[0].grip.orientation,delta)));
    auto lost=sample;lost.hands[0].aim.tracked=lost.hands[0].grip.tracked=0;
    auto before=lost;driftHands(lost,high,true);
    assert(!std::memcmp(&before.hands[0],&lost.hands[0],sizeof(lost.hands[0])));
    testSpine();
}
