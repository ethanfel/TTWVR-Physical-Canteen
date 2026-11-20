// SPDX-License-Identifier: GPL-3.0-only
#include "../src/Fingers.h"
#include <cassert>
#include <initializer_list>

using namespace canteen;

static float dot(Point a, Point b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Point reflectMount(Point p) { return {p.x,p.y,-p.z}; }
static Point reflectMesh(Point p) { return {-p.x,p.y,p.z}; }
static Point placed(const Grip &g, Point p, float scale) { return g.position + g.rotation.apply(p)*scale; }

// Measured on the stock TTWVR hand. Follow each actual finger chain to its pad,
// then check which face of the drawn flask it lands on, for both controllers.
static void checkContact(Hand hand) {
    const Point joints[5][4] = {
        {{6.968f,-.254f,2.710f},{3.783f,.001f,-.043f},{1.945f,0,0},{1.959f,.018f,-.083f}},
        {{7.151f,-.625f,.883f},{3.935f,.001f,0},{2.115f,0,0},{1.648f,-.049f,-.023f}},
        {{6.993f,-.129f,-.738f},{3.531f,0,0},{1.968f,0,0},{1.659f,-.118f,.018f}},
        {{6.799f,.363f,-2.198f},{2.974f,0,0},{1.512f,0,0},{1.494f,-.056f,-.023f}},
        {{.547f,.791f,1.513f},{5.514f,0,.001f},{2.936f,0,0},{1.949f,-.114f,-.094f}},
    };
    const Rotation wrist{{{.991f,-.051f,-.123f},{.125f,.035f,.992f},{-.046f,-.998f,.041f}}};
    auto reflected=[hand](Point p) { return hand==Hand::left ? reflectMount(p) : p; };
    const Grip g=sideGrip(hand,1.f);
    for (int digit=0;digit<5;++digit) {
        Point pad=reflected(joints[digit][3]);
        for (int j=2;j>=0;--j) {
            const auto bend=fingerRotation(grasp[digit*3+j].angles,hand);
            pad=reflected(joints[digit][j])+bend.apply(pad);
        }
        Point mount=wristInMount(hand)+reflected(wrist.apply(reflected(pad)));
        Point offset=mount-g.position;
        const float width=dot(offset,g.rotation.apply({1,0,0}));
        const float depth=dot(offset,g.rotation.apply({0,1,0}));
        const float height=dot(offset,g.rotation.apply({0,0,1}));
        assert(std::fabs(width)<2.f && height>1.8f && height<9.5f);
        if (digit==4) assert(depth>1.55f && depth<2.25f);
        else assert(depth< -1.35f && depth> -2.15f);
    }
}

int main() {
    checkContact(Hand::right);
    checkContact(Hand::left);
    for (float scale : {.8f,1.f,1.2f}) {
        const Grip left = sideGrip(Hand::left,scale), right = sideGrip(Hand::right,scale);
        for (Point p : {capCenter, Point{0,0,0}, Point{-5.049f,.8f,4.7f}, Point{0,1.3f,6}}) {
            assert(length(placed(left,reflectMesh(p),scale) - reflectMount(placed(right,p,scale))) < .0001f);
        }
        for (Hand hand : {Hand::left,Hand::right}) {
            const Grip g = sideGrip(hand,scale);
            const Point width = g.rotation.apply({1,0,0});
            const Point concave = g.rotation.apply({0,1,0});
            const Point neck = g.rotation.apply({0,0,1});
            const Point cross{width.y*concave.z-width.z*concave.y,
                              width.z*concave.x-width.x*concave.z,
                              width.x*concave.y-width.y*concave.x};
            for (Point v : {width,concave,neck}) assert(std::fabs(length(v)-1.f) < .0001f);
            assert(length(cross-neck)<.0001f);
            // World up expressed in the mount frame at the two recorded poses.
            const Point upAtLips{1,0,0}, upWhenRaised{-.09f,.996f,0};
            assert(dot(upAtLips,concave)<-.90f && dot(upAtLips,neck)<0.f);
            assert(dot(upWhenRaised,neck)>.90f);
            Point edge{-1.949f,2.1f,4.7f}, palm{-2.448f,-.15f,-.950f};
            if (hand == Hand::left) { edge=reflectMesh(edge); palm=reflectMount(palm); }
            assert(length(placed(g,edge,scale)-palm) < .0001f);
        }
    }
}
