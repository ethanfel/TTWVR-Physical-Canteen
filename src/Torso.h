// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Balance.h"
#include "Scene.h"

namespace canteen {
inline Rotation transpose(Rotation a) {
    for (int i=0;i<3;++i) for (int j=0;j<i;++j) std::swap(a.m[i][j],a.m[j][i]);
    return a;
}
inline Rotation compose(Rotation a,Rotation b) {
    Rotation out{};
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) for (int k=0;k<3;++k)
        out.m[i][j]+=a.m[i][k]*b.m[k][j];
    return out;
}
inline Rotation matrix(Quaternion q) {
    Rotation result{};
    for (int j=0;j<3;++j) {
        Point axis{float(j==0),float(j==1),float(j==2)}, column=rotate(q,axis);
        result.m[0][j]=column.x; result.m[1][j]=column.y; result.m[2][j]=column.z;
    }
    return result;
}
inline Transform absolute(const Transform &parent,const Transform &local) {
    return {compose(parent.rotation,local.rotation),parent.point(local.position),parent.scale*local.scale};
}
inline Transform relative(const Transform &world,const Transform &parent) {
    auto inverse=transpose(parent.rotation);
    return {compose(inverse,world.rotation),inverse.apply(world.position-parent.position)*(1.f/parent.scale),
            world.scale/parent.scale};
}
inline Object *child(Object *p,const char *name) {
    if (!p) return nullptr;
    using AsNode=Node* (__thiscall *)(Object *);
    auto *n=reinterpret_cast<AsNode>(p->methods[3])(p);
    if (!n || !n->children || n->end>n->capacity) return nullptr;
    for (unsigned i=0;i<n->end;++i) {
        auto *c=n->children[i];
        if (c && c->parent==p && c->name && !std::strcmp(c->name,name)) return c;
    }
    return nullptr;
}
// Put the host's tracked palm back at its target after its shoulder moves.
// Bend at shoulder and elbow. Bone lengths and the host's wrist roll survive.
inline bool retargetArm(const Transform &shoulderParent,Transform &upper,Transform &fore,
                        Transform &hand,const Transform &goal) {
    auto shoulder=absolute(shoulderParent,upper), elbow=absolute(shoulder,fore), wrist=absolute(elbow,hand);
    Point a=elbow.position-shoulder.position, b=wrist.position-elbow.position, reach=goal.position-shoulder.position;
    float l=length(a), r=length(b), d=length(reach);
    if (!(l>.001f && r>.001f && d>.001f)) return false;
    Point axis=reach*(1.f/d);
    d=std::clamp(d,std::max(std::abs(l-r),.001f),l+r);
    Point guide=a-axis*dot(a,axis);
    if (length(guide)<.001f) {
        Point fallback=std::abs(axis.z)<.8f ? Point{0,0,1} : Point{0,1,0};
        guide=cross(axis,fallback);
    }
    float along=std::clamp((l*l-r*r+d*d)/(2*d),-l,l);
    Point newArm=axis*along+unit(guide)*std::sqrt(std::max(0.f,l*l-along*along));
    Rotation swing=matrix(between(a,newArm)), u=compose(swing,shoulder.rotation);
    Rotation f=compose(matrix(between(swing.apply(b),axis*d-newArm)),compose(swing,elbow.rotation));
    upper.rotation=compose(transpose(shoulderParent.rotation),u);
    fore.rotation=compose(transpose(u),f);
    hand.rotation=compose(transpose(f),goal.rotation);
    return true;
}
inline void propagatePose(Object *node) {
    using Update=void (__thiscall *)(Object *,const UpdateData &,unsigned);
    static const UpdateData update{};
    reinterpret_cast<Update>(node->methods[0xA4/4])(node,update,0);
}
// Alter the drawn avatar only. Restore every local transform immediately, so
// later animation and the other eye always start from the host's authored pose.
inline bool presentLean(Object *root,float radians) {
    auto *spine=find(root,"Bip01 Spine");
    if (!spine || !spine->parent) return false;
    if (radians==0) { propagatePose(spine); return true; }
    auto *mid=child(spine,"Bip01 Spine1"), *chest=child(mid,"Bip01 Spine2");
    auto *neck=child(chest,"Bip01 Neck"), *headBranch=child(neck,"Bip01 Neck1");
    if (!headBranch) return false;
    Object *nodes[8]{spine,headBranch};
    Object *clavicles[2];
    const char *names[2][4]={{"Bip01 L Clavicle","Bip01 L UpperArm","Bip01 L Forearm","Bip01 L Hand"},
                             {"Bip01 R Clavicle","Bip01 R UpperArm","Bip01 R Forearm","Bip01 R Hand"}};
    for (int i=0;i<2;++i) {
        Object *p=clavicles[i]=child(neck,names[i][0]);
        for (int j=0;j<3;++j) nodes[2+3*i+j]=p=child(p,names[i][1+j]);
        if (!p) return false;
    }
    Transform raw[8], shown[8];
    for (int i=0;i<8;++i) raw[i]=shown[i]=nodes[i]->local;
    const auto &parent=spine->parent->world;
    auto neckAt=[&](const Transform &base) {
        return absolute(absolute(absolute(absolute(parent,base),mid->local),chest->local),neck->local);
    };
    const auto before=neckAt(raw[0]);
    Point right=before.rotation.apply(clavicles[1]->local.position-clavicles[0]->local.position);
    Point forward=unit(cross(Point{0,0,1},right));
    if (!(parent.scale>.001f && before.scale>.001f && dot(forward,forward)>.9f)) return false;
    shown[0].rotation=compose(matrix(turn(transpose(parent.rotation).apply(forward),radians)),raw[0].rotation);
    const auto after=neckAt(shown[0]);
    shown[1]=relative(absolute(before,raw[1]),after);
    for (int i=0;i<2;++i) {
        int j=2+i*3;
        const auto clav=absolute(before,clavicles[i]->local);
        const auto goal=absolute(absolute(absolute(clav,raw[j]),raw[j+1]),raw[j+2]);
        if (!retargetArm(absolute(after,clavicles[i]->local),shown[j],shown[j+1],shown[j+2],goal)) return false;
    }
    for (int i=0;i<8;++i) nodes[i]->local=shown[i];
    propagatePose(spine);
    for (int i=0;i<8;++i) nodes[i]->local=raw[i];
    return true;
}
}
