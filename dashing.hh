/*
Copyright (c) 2015 Jeff Epler

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgement in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <span>
#if defined(DASHING_OMP)
#include <omp.h>
#endif

#include "dashing_F.hh"

namespace dashing
{

struct PSMatrix {
    F a, b, c, d, e, f;

    PSMatrix inverse() const;
    F determinant() const { return a * d - b * c; }
};

PSMatrix Translation(F x, F y);
PSMatrix Rotation(F theta);
PSMatrix XSkew(F xk);
PSMatrix YScale(F ys);

struct Point { F x, y; };
inline Point operator*(const Point &p, const PSMatrix &m) {
    return Point{ p.x*m.a + p.y*m.c + m.e,
                  p.x*m.b + p.y*m.d + m.f };
}
inline Point operator*(const Point &p, F d)
{
    return Point{ p.x * d, p.y * d };
}
inline Point operator*(F d, const Point &p)
{
    return Point{ p.x * d, p.y * d };
}
inline Point operator+(const Point &p, const Point &q)
{
    return Point{ p.x + q.x, p.y * q.y };
}

PSMatrix operator*(const PSMatrix &m1, const PSMatrix m2);

struct Dash {
    PSMatrix tr, tf;
    std::vector<F> dash, sum;

    Dash(F th, F x0, F y0, F dx, F dy,
            const std::vector<F>::const_iterator dbegin,
            const std::vector<F>::const_iterator dend);

    static Dash FromString(const std::string &line, F scale);
};

struct Segment { Point p, q; bool swapped; };
struct Intersection { F u; bool positive; };
inline bool operator<(const Intersection &a, const Intersection &b)
{
    return a.u < b.u;
}

// "sort" a segment so that its first component has the lower y-value
inline void ysort(Segment &s) {
    if(s.p.y < s.q.y) return;
    s.swapped = ! s.swapped;
    std::swap(s.p, s.q);
}

inline int intceil(F x) { return int(ceil(x)); }
inline int intfloor(F x) { return int(floor(x)); }

inline F pythonmod(F a, F b) {
    auto r = a - floor(a / b) * b;
    if(r == b) return 0;
    return r;
}

inline size_t utoidx(const Dash &d, F u, F &o) {
    u = pythonmod(u, d.sum.back());
    for(size_t i = 1; i != d.sum.size(); i++) {
        if(u < d.sum[i]) { o = u - d.sum[i-1]; return i-1; }
    }
    abort(); // should be unreachable
}

template<class Cb>
void uvdraw(const Dash &pattern, F v, F u1, F u2, Cb cb) {
    if(pattern.dash.empty()) { cb(v, u1, u2); return;  }
    F o;
    auto i = utoidx(pattern, u1, o);
    const auto pi = pattern.dash[i];
    if(i % 2 == 0) { cb(v, u1, std::min(u2, u1+pi-o)); u1 += pi-o; }
    else { u1 -= pi+o; }
    i++;
    if(i % 2) {
        u1 += pattern.dash[i];
        i++;
    }
    for(auto u = u1; u < u2;) {
        if(i >= pattern.dash.size()) i = 0;
        const auto pi = pattern.dash[i];
        cb(v, u, std::min(u2, u+pi));
        u += pi;
        u += pattern.dash[i+1];
        i+=2;
    }
}

template<class Cb, class Wr>
void uvspans(const Dash &pattern, std::vector<Segment> & segments, Cb cb, std::vector<Intersection> &uu, Wr wr) {
    if(segments.empty()) return; // no segments

    for(auto &s : segments) ysort(s);
    std::sort(segments.begin(), segments.end(),
        [](const Segment &a, const Segment &b) {
            return a.p.y < b.p.y; // sort in increasing p.y
        });

    // we want to maintain the heap condition in such a way that we can always
    // quickly pop items that our span has moved past.
    // C++ heaps are max-heaps, so we need a decreasing sort.
    auto heapcmp = [](const Segment &a, const Segment &b) {
            return b.q.y < a.q.y; // sort in decreasing q.y;
        };

    auto segments_begin = segments.begin();
    auto heap_begin = segments.begin(), heap_end = segments.begin();

    auto vstart = intfloor(segments.front().p.y);
    auto vend = intceil(std::max_element(segments.begin(), segments.end(), 
                [](const Segment &a, const Segment &b) {
                        return a.q.y < b.q.y; // sort in increasing q.y;
                })->q.y);

    // sweep-line algorithm to intersects spans with segments
    // "active" holds segments that may intersect with this span;
    // when v moves below an active segment, drop it from the active heap.
    // when v moves into a remaining segment, move it from segments to active.
    for(auto v = vstart; v != vend; v++) {
        uu.clear();

        while(heap_begin != heap_end && heap_begin->q.y < v)
        {
            std::pop_heap(heap_begin, heap_end, heapcmp);
            heap_end --;
        }
        while(segments_begin != segments.end() && segments_begin->p.y < v) {
            const auto &s = *segments_begin;
            if(s.q.y >= v) {
                *heap_end++ = s;
                std::push_heap(heap_begin, heap_end, heapcmp);
            }
            segments_begin ++;
        }

        for(const auto &s : std::span(heap_begin, heap_end)) {
            auto du = s.q.x - s.p.x;
            auto dv = s.q.y - s.p.y;
            assert(dv);
            uu.push_back(
                    Intersection{s.p.x + du * (v - s.p.y) / dv,s.swapped});
        }
        std::sort(uu.begin(), uu.end());
        int winding = 0;
        F old_u = -std::numeric_limits<F>::infinity();
        for(const auto &isect : uu) {
            if(wr(winding)) uvdraw(pattern, v, old_u, isect.u, cb);
            winding += 2*isect.positive - 1;
            old_u = isect.u;
        }
    }
}

struct HatchPattern {
    std::vector<Dash> d;
    static HatchPattern FromFile(std::istream &fi, F scale) {
        HatchPattern result;

        std::string line;
        while(getline(fi, line)) {
            auto i = line.find(";");
            if(i != line.npos) line.erase(i, line.npos);
            while(!line.empty() && isspace(line.back())) {
                line.pop_back();
            }
            if(line.empty()) continue;
            if(line[0] == '*') continue;
            result.d.push_back(Dash::FromString(line, scale));
        }
        return result;
    }

    static HatchPattern FromFile(const char *filename, F scale) {
        std::ifstream fi(filename);
        return FromFile(fi, scale);
    }
};

template<class It, class Cb, class Wr>
void xyhatch(const Dash &pattern, It start, It end, Cb cb, Wr wr) {
    std::vector<Segment> uvsegments;
    uvsegments.reserve(end-start);

    std::vector<Intersection> uu;
    uu.reserve(8);

    bool swapped = pattern.tf.determinant() < 0;
    std::transform(start, end, std::back_inserter(uvsegments),
        [&](const Segment &s)
        { return Segment{s.p * pattern.tf, s.q * pattern.tf, swapped != s.swapped };
    });
    uvspans(pattern, uvsegments, [&](F v, F u1, F u2) {
        Point p{u1, v}, q{u2, v};
        cb(Segment{ p * pattern.tr, q * pattern.tr, false });
    }, uu, wr);
}

template<class It, class Cb, class Wr>
void xyhatch(const HatchPattern &pattern, It start, It end, Cb cb, Wr wr) {
    for(const auto &i : pattern.d) xyhatch(i, start, end, cb, wr);
}

template<class C, class Cb, class Wr>
void xyhatch(const HatchPattern &pattern, const C &c, Cb cb, Wr wr) {
    xyhatch(pattern, c.begin(), c.end(), cb, wr);
}

#if defined(DASHING_OMP)
template<class It, class Cb, class Wr>
void xyhatch_omp(const HatchPattern &pattern, It start, It end, Cb cb, Wr wr) {
    #pragma omp parallel for
    for(const auto &i : pattern.d) {
        int iam = omp_get_thread_num();
        xyhatch(i, start, end, [iam, &cb](Segment s) { cb(s, iam); }, wr);
    }
}
template<class C, class Cb, class Wr>
void xyhatch_omp(const HatchPattern &pattern, const C &c, Cb cb, Wr wr) {
    xyhatch_omp(pattern, c.begin(), c.end(), cb, wr);
}

#endif

}
