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

#ifndef DASHING_H
#define DASHING_H

#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/random.hpp>
#include <boost/random/random_device.hpp>

struct PSMatrix {
    double a, b, c, d, e, f;

    PSMatrix inverse() const {
        auto i = 1. / (a * d - b * c);
        return PSMatrix{d*i, -b*i, -c*i, a*i, i*(c*f-e*d), i*(b*e-a*f)};
    }
};

inline PSMatrix Translation(double x, double y) {
    PSMatrix r{1.,0.,0.,1.,x,y};
    return r;
}

inline PSMatrix Rotation(double theta) {
    double c = cos(theta), s = sin(theta);
    PSMatrix r{c,s,-s,c,0.,0.};
    return r;
}

inline PSMatrix XSkew(double xk) {
    PSMatrix r{1.,0.,xk,1.,0.,0.};
    return r;
}

inline PSMatrix YScale(double ys) {
    PSMatrix r{1.,0.,0.,ys,0.,0.};
    return r;
}

struct Point { double x, y; };
Point operator*(const Point &p, const PSMatrix &m) {
    Point r{ p.x*m.a + p.y*m.c + m.e,
                  p.x*m.b + p.y*m.d + m.f };
    return r;
}

PSMatrix operator*(const PSMatrix &m1, const PSMatrix m2) {
    PSMatrix r{
            m2.a * m1.a + m2.b * m1.c,
            m2.a * m1.b + m2.b * m1.d,
            m2.c * m1.a + m2.d * m1.c,
            m2.c * m1.b + m2.d * m1.d,
            m2.e * m1.a + m2.f * m1.c + m1.e,
            m2.e * m1.b + m2.f * m1.d + m1.f};
    return r;
}

std::vector<double> parse_numbers(std::string line) {
    boost::algorithm::replace_all(line, ",", " ");
    std::istringstream fi(line);
    std::vector<double> result;
    double d;
    while((fi >> d)) result.push_back(d);
    return result;
}

double radians(double degrees) { return degrees * acos(0) / 90.; }

struct Dash {
    PSMatrix tr, tf;
    std::vector<double> dash, sum;

    template<class It>
    Dash(double th, double x0, double y0, double dx, double dy,
            It dbegin, It dend) : dash(dbegin, dend) {
        auto s = 0.;
        for(auto d : dash) { sum.push_back(s); s += fabs(d); }
        sum.push_back(s);

        tr = Translation(x0, y0) * Rotation(th) * XSkew(dx / dy) * YScale(dy);
        tf = tr.inverse();
    }

    static Dash FromString(const std::string &line, double scale) {
        std::vector<double> words = parse_numbers(line);
        if(words.size() < 5)
            throw std::invalid_argument("not a valid dash specification");
        for(auto i = words.begin() + 1; i != words.end(); i++) *i *= scale;
        return Dash(radians(words.at(0)), words.at(1), words.at(2), words.at(3),
            words.at(4), words.begin() + 5, words.end());        
    }
};

// "sort" a segment so that its first component has the lower y-value
struct Segment { Point p, q; };
void ysort(Segment &s) {
    if(s.p.y < s.q.y) return;
    std::swap(s.p, s.q);
}

double intceil(double x) { return int(ceil(x)); }
double intfloor(double x) { return int(floor(x)); }

double pythonmod(double a, double b) {
    auto r = a - floor(a / b) * b;
    if(r == b) return 0;
    return r;
}

double utoidx(const Dash &d, double u, double &o) {
    u = pythonmod(u, d.sum.back());
    for(size_t i = 1; i != d.sum.size(); i++) {
        if(u < d.sum[i]) { o = u - d.sum[i-1]; return i-1; }
    }
    abort(); // should be unreachable
}

template<class Cb>
void uvdraw(const Dash &pattern, double v, double u1, double u2, Cb cb) {
    if(pattern.dash.empty()) { cb(v, u1, u2); return;  }
    double o;
    auto i = utoidx(pattern, u1, o);
    for(auto u = u1; u < u2;) {
        auto pi = pattern.dash[i];
        if(pi >= 0) cb(v, u, std::min(u2, u+pi-o));
        u += fabs(pi)-o;
        o = 0;
        i = pythonmod(i+1, pattern.dash.size());
    }
}

template<class Cb>
void uvspans(const Dash &pattern, std::vector<Segment> && segments, Cb cb) {
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
                        return a.p.y < b.p.y; // sort in increasing q.y;
                })->q.y);

    // sweep-line algorithm to intersects spans with segments
    // "active" holds segments that may intersect with this span;
    // when v moves below an active segment, drop it from the active heap.
    // when v moves into a remaining segment, move it from segments to active.
    std::vector<double> uu;
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

        //std::cerr << "v=" << v << " heap size=" << active.size() << "\n";
        for(const auto &s : boost::make_iterator_range(heap_begin, heap_end)) {
            auto du = s.q.x - s.p.x;
            auto dv = s.q.y - s.p.y;
            if(dv) uu.push_back(s.p.x + du * (v - s.p.y) / dv);
            else { uu.push_back(s.p.x); uu.push_back(s.q.x); }
        }
        assert(uu.size() % 2 == 0);
        std::sort(uu.begin(), uu.end());
        for(size_t i=0; i<uu.size(); i+= 2) {
            uvdraw(pattern, v, uu[i], uu[i+1], cb);
        }
    }
}

struct HatchPattern {
    std::vector<Dash> d;
    static HatchPattern FromFile(std::istream &fi, double scale) {
        HatchPattern result;

        std::string line;
        while(getline(fi, line)) {
            auto i = line.find(";");
            if(i != line.npos) line.erase(i, line.npos);
            boost::algorithm::trim(line);
            boost::algorithm::trim(line);
            if(line.empty()) continue;
            if(line[0] == '*') continue;
            result.d.push_back(Dash::FromString(line, scale));
        }
        return result;
    }

    static HatchPattern FromFile(const char *filename, double scale) {
        std::ifstream fi(filename);
        return FromFile(fi, scale);
    }
};

template<class It, class Cb>
void xyhatch(const Dash &pattern, It start, It end, Cb cb) {
    std::vector<Segment> uvsegments;
    std::transform(start, end, std::back_inserter(uvsegments),
        [&](const Segment &s)
        { return Segment{s.p * pattern.tf, s.q * pattern.tf };
    });
    uvspans(pattern, std::move(uvsegments), [&](double v, double u1, double u2) {
        Point p{u1, v}, q{u2, v};
        Segment xy{ p * pattern.tr, q * pattern.tr };
        cb(xy);
    });
}

template<class It, class Cb>
void xyhatch(const HatchPattern &pattern, It start, It end, Cb cb) {
    for(const auto &i : pattern.d) xyhatch(i, start, end, cb);
}

template<class C, class Cb>
void xyhatch(const HatchPattern &pattern, const C &c, Cb cb) {
    for(const auto &i : pattern.d) xyhatch(i, c.begin(), c.end(), cb);
}

#endif
