// SPDX-License-Identifier: GPL-3.0-only
#include "../src/Grip.h"
#include <cassert>
#include <initializer_list>

using namespace canteen;

static float dot(Point a, Point b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Point reflectMount(Point p) { return {p.x,p.y,-p.z}; }
static Point reflectMesh(Point p) { return {-p.x,p.y,p.z}; }
static Point placed(const Grip &g, Point p, float scale) { return g.position + g.rotation.apply(p)*scale; }

int main() {
    // Right wrist in the drawn Weapon frame and flask in the receiving wrist frame.
    // Keep the side grip close to the captured contact while clearing the finger pads.
    const Rotation wrist{{{.991f,-.051f,-.123f},{.125f,.035f,.992f},{-.046f,-.998f,.041f}}};
    const Rotation inWrist{{{.49023f,-.87145f,-.01582f},{.86887f,.49005f,-.07007f},
                            {.06882f,.02061f,.99742f}}};
    const Point edge{-5.835f,1.48f,4.7f};
    const Point measured=wristInMount(Hand::right)+wrist.apply(Point{12.21546f,6.25163f,-3.04732f}+inWrist.apply(edge));
    const Grip fitted=sideGrip(Hand::right,1.f);
    assert(length(placed(fitted,edge,1.f)-measured)<.26f);
    for (Point axis : {Point{1,0,0},Point{0,1,0},Point{0,0,1}})
        assert(dot(fitted.rotation.apply(axis),wrist.apply(inWrist.apply(axis)))>.9958f);
    for (float scale : {.8f,1.f,1.2f}) {
        const Grip left = sideGrip(Hand::left,scale), right = sideGrip(Hand::right,scale);
        for (Point p : {capCenter, Point{0,0,0}, Point{-5.835f,1.48f,4.7f}, Point{0,1.3f,6}}) {
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
            assert(dot(upAtLips,concave)<-.87f && dot(upAtLips,neck)<0.f);
            assert(dot(upWhenRaised,neck)>.90f);
            Point edge{-5.835f,1.48f,4.7f}, palm{1.893276f,.573977f,.201163f};
            if (hand == Hand::left) { edge=reflectMesh(edge); palm=reflectMount(palm); }
            assert(length(placed(g,edge,scale)-palm) < .0001f);
        }
    }
}
