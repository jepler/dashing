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

// demo and benchmark program for dashing

#include "dashing.hh"
#include "contours_and_segments.hh"

#include <SDL2/SDL.h>
#include "gldashing.hh"
using namespace dashing;

AppState st;

const char *argv0 = "dashing";

#ifdef __GNUC__
void usage() __attribute__((noreturn));
#endif

void usage() {
    fprintf(stderr,
        "Usage: %s [-b] [-s scale] [-j jitter] [-r rulename] patfile segfile\n",
        argv0);
    exit(1);
}

template<class C, class Cb>
void xyhatch(const HatchPattern &pattern, const C &c, Cb cb, const char *arg) {
    if(!strcmp(arg, "odd"))
        return xyhatch(pattern, c, cb, [](int i) { return i % 2 != 0; });
    if(!strcmp(arg, "nonzero"))
        return xyhatch(pattern, c, cb, [](int i) { return i != 0; });
    if(!strcmp(arg, "positive"))
        return xyhatch(pattern, c, cb, [](int i) { return i > 0; });
    if(!strcmp(arg, "negative"))
        return xyhatch(pattern, c, cb, [](int i) { return i < 0; });
    if(!strcmp(arg, "abs_geq_two"))
        return xyhatch(pattern, c, cb, [](int i) { return abs(i) >= 2; });
    fprintf(stderr, "Unrecognized winding rule '%s'\n", arg);
    fprintf(stderr, "Rules are: odd nonzero positive negative abs_geq_two\n");
    usage();
}



template<class Cb, class Wr>
void glspans(std::vector<Segment> && segments, Cb cb, std::vector<Intersection> &uu, Wr wr) {
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

        for(const auto &s : boost::make_iterator_range(heap_begin, heap_end)) {
            auto du = s.q.x - s.p.x;
            auto dv = s.q.y - s.p.y;
            assert(dv);
            if(dv) uu.push_back(
                    Intersection{s.p.x + du * (v - s.p.y) / dv,s.swapped});
        }
        std::sort(uu.begin(), uu.end());
        int winding = 0;
        double old_u = -std::numeric_limits<double>::infinity();
        for(const auto &isect : uu) {
            if(wr(winding)) cb(v, old_u, isect.u);
            winding += 2*isect.positive - 1;
            old_u = isect.u;
        }
    }
}

template<class It, class Cb>
void glhatch(int i, const Dash &pattern, It start, It end, Cb cb, std::vector<Segment> &uvsegments, std::vector<Intersection> &uu) {
    uvsegments.clear();
    bool swapped = pattern.tf.determinant() < 0;
    std::transform(start, end, std::back_inserter(uvsegments),
        [&](const Segment &s)
        { return Segment{s.p * pattern.tf, s.q * pattern.tf, swapped != s.swapped };
    });
    auto recip = 1./pattern.sum.back();
    glspans(std::move(uvsegments), [&](double v, double u1, double u2) {
        auto p = Point{u1, v} * pattern.tr;
        auto q = Point{u2, v} * pattern.tr;
        cb(i, p.x, p.y, u1*recip, q.x, q.y, u2*recip);
    }, uu, [](int i) { return !!i; } );
}

template<class It, class Cb>
void glhatch(const HatchPattern &pattern, It start, It end, Cb cb) {
    std::vector<Segment> uvsegments;
    uvsegments.reserve(end-start);
    std::vector<Intersection> uu;
    uu.reserve(8);
    for(size_t i=0; i<pattern.d.size(); i++)
        glhatch(i, pattern.d[i], start, end, cb, uvsegments, uu);
}

template<class C, class Cb>
void glhatch(const HatchPattern &pattern, const C &c, Cb cb) {
    glhatch(pattern, std::begin(c), std::end(c), cb);
}

size_t render(SDL_Window *window __attribute__((unused)))
{
    glClear(GL_COLOR_BUFFER_BIT);

    static Segments s;
    ContoursToSegments(s, st.c, st.jitter);

    static std::vector<float> p;
    p.clear();
    for(auto && si : s)
    {
        p.push_back(si.p.x);
        p.push_back(si.p.y);
        p.push_back(0.f);
        p.push_back(-1);
        p.push_back(si.q.x);
        p.push_back(si.q.y);
        p.push_back(0.f);
        p.push_back(-1);
    }

    // draw outline of shape
    glVertexAttrib3f(st.attribute_aColor, 0., 0., 0.);
    glVertexAttribPointer(st.attribute_aPos, 4, GL_FLOAT, GL_FALSE, 
            0, &p[0]);
    glDrawArrays(GL_LINES, 0, p.size()/4);

    p.clear();
    auto scale = pow(1.25, st.scale);
    auto sf = PSMatrix{scale, 0., 0., scale, 0., 0.};
    auto sr = PSMatrix{1./scale, 0., 0., 1./scale, 0., 0.};

    auto h = st.h;
    for(auto & i : h.d)
    {
        i.tf = i.tf * sf;
        i.tr = sr * i.tr;
    }

    double frac = 1.*h.d.size();
    glhatch(h, s, [frac](int i, double x1, double y1, double u1, double x2, double y2, double u2) {
        p.push_back(x1);
        p.push_back(y1);
        p.push_back(u1);
        p.push_back(i);
        p.push_back(x2);
        p.push_back(y2);
        p.push_back(u2);
        p.push_back(i);
    });

    // draw dashes of shape
    glVertexAttrib3f(st.attribute_aColor, 0., 0., 1.);
    glVertexAttribPointer(st.attribute_aPos, 4, GL_FLOAT, GL_FALSE, 
            0, &p[0]);
    glDrawArrays(GL_LINES, 0, p.size()/4);
    
    SDL_GL_SwapWindow(window);
    return p.size() / 16;
}

double now()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void mainloop(SDL_Window *window)
{
    SDL_Event ev;
    auto t0 = now();
    while(1) {
        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_QUIT) return;
            if(ev.type == SDL_KEYDOWN)
            {
                if(ev.key.keysym.sym == SDLK_LEFT) st.scale -= 1;
                if(ev.key.keysym.sym == SDLK_RIGHT) st.scale += 1;
            }
        }
        auto n = render(window);
        auto t1 = now();
        printf("scale = %5.2f frame time = %6.4f [est fps=%4.1f] segments=%zd [est Mseg/s=%5.1f]\n", pow(1.25, -st.scale), t1 - t0, 1/(t1-t0), n, n/(t1-t0)/1e6);
        t0 = t1;
    }
}

int main(int argc, char **argv) {
    st.jitter = 0.;
    st.rule = "odd";
    int c;
    while((c = getopt(argc, argv, "s:j:r:")) > 0) {
        switch(c) {
        case 'r': st.rule = optarg; break;
        case 's': st.scale = atof(optarg); break;
        case 'j': st.jitter = atof(optarg); break;
        default:
            usage();
        }
    }
    auto nargs = argc - optind;
    if(nargs != 2) usage();

    auto patfile = argv[optind];
    auto segfile = argv[optind+1];

    st.h = HatchPattern::FromFile(patfile, 1.);
    st.c = ContoursFromFile(segfile);

    Segments s;
    ContoursToSegments(s, st.c);

    static auto cmp_x = [](const Segment &a, const Segment & b)
            { return a.p.x < b.p.x; };
    static auto cmp_y = [](const Segment &a, const Segment & b)
            { return a.p.y < b.p.y; };

    float min_x = std::min_element(s.begin(), s.end(), cmp_x)->p.x * 1.1;
    float max_x = std::max_element(s.begin(), s.end(), cmp_x)->p.x * 1.1;
    float min_y = -std::max_element(s.begin(), s.end(), cmp_y)->p.y * 1.1;
    float max_y = -std::min_element(s.begin(), s.end(), cmp_y)->p.y * 1.1;
    float d_x = max_x - min_x;
    float d_y = max_y - min_y;

    // set up view matrix
    st.mat =
        glm::translate(glm::fvec3{-1.f, -1.f, 0.f}) *
        glm::scale(glm::fvec3{2.f/d_x, 2.f/d_y, 1.f}) *
        glm::translate(glm::fvec3{-min_x, -min_y, 0.f});


    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("GL Dashing",
            0, 0, 1600, 900, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    SDL_GL_CreateContext(window);

    setup();
    mainloop(window);
return 0;
}
