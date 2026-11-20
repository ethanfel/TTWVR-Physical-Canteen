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
            Point edge{-5.049f,.8f,4.7f}, palm{-2.448f,-.15f,-.950f};
            if (hand == Hand::left) { edge=reflectMesh(edge); palm=reflectMount(palm); }
            assert(length(placed(g,edge,scale)-palm) < .0001f);
        }
    }
}
