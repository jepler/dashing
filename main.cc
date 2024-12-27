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

#include <new>
#include <atomic>
#include <cstring>
#if defined(DASHING_OMP)
#include <omp.h>
#endif
#include "dashing.hh"
#include "contours_and_segments.hh"

using namespace dashing;

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

typedef bool(*winding_rule)(int);

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

int main(int argc, char **argv) {
    auto scale = 1., jitter = 0.;
    auto rule = "odd";
    bool bench = false, xit = false;
    int c;
    while((c = getopt(argc, argv, "bxs:j:r:")) > 0) {
        switch(c) {
        case 'r': rule = optarg; break;
        case 'b': bench = !bench; break;
        case 'x': xit = !xit; break;
        case 's': scale = atof(optarg); break;
        case 'j': jitter = atof(optarg); break;
        default:
            usage();
        }
    }
    auto nargs = argc - optind;
    if(nargs != 2) usage();

    auto patfile = argv[optind];
    auto segfile = argv[optind+1];

    auto h = HatchPattern::FromFile(patfile, scale);
    auto s = SegmentsFromFile(segfile, jitter);

    if(xit) return 0;

    if(bench) {
#if defined(DASHING_OMP)
        struct per_thread_count_type {
            alignas(std::hardware_destructive_interference_size) size_t n;
        };
        auto max_threads = omp_get_max_threads();
        std::vector<per_thread_count_type> thread_nseg(max_threads, per_thread_count_type{0});
        auto count_seg = [&thread_nseg](const Segment &s, int iam) { (void)s; thread_nseg[iam].n++; };
        xyhatch_omp(h, s, count_seg, [](int i) { return i != 0; } );
        size_t nseg = std::accumulate(thread_nseg.begin(), thread_nseg.end(), size_t{}, [](size_t a, const per_thread_count_type &b) { return a + b.n; });
#else
        size_t nseg{};
        auto count_seg = [&nseg](const Segment &s) { (void)s; nseg ++; };
        xyhatch(h, s, count_seg, [](int i) { return i != 0; } );
#endif
        std::cout << nseg << "\n";
        return 0;
    }

    auto cmp_x = [](const Segment &a, const Segment & b)
            { return a.p.x < b.p.x; };
    auto cmp_y = [](const Segment &a, const Segment & b)
            { return a.p.y < b.p.y; };
    auto min_x = std::min_element(s.begin(), s.end(), cmp_x)->p.x;
    auto max_x = std::max_element(s.begin(), s.end(), cmp_x)->p.x;
    auto min_y = -std::max_element(s.begin(), s.end(), cmp_y)->p.y;
    auto max_y = -std::min_element(s.begin(), s.end(), cmp_y)->p.y;
    auto d_x = max_x - min_x;
    auto d_y = max_y - min_y;

    std::cout <<
        "<svg width=\"100%%\" height=\"100%%\" viewBox=\""
            << (min_x - .05 * d_x) << " " << (min_y - .05 * d_x) << " "
            << (d_x * 1.1)         << " " << (d_y * 1.1) << "\" "
      "preserveAspectRatio=\"xMidyMid\" "
      "xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
      "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
      "<path stroke=\"green\" stroke-dasharray=\"20 20\" d=\"";

    auto print_seg = [](const Segment &s) {
        std::cout << "M" << s.p.x << " " << -s.p.y
                  << "L" << s.q.x << " " << -s.q.y << "\n";
    };

    Segment xaxis {{-2 * d_x, 0}, {2 * d_x, 0}, false};
    Segment yaxis {{0, -2 * d_y}, {0, 2 * d_y}, false};
    print_seg(xaxis);
    print_seg(yaxis);
    std::cout << "\"/>";

    std::cout << "<path fill=\"none\" stroke=\"black\" "
                 "stroke-linecap=\"round\"  d=\"";
    for(const auto i : s) print_seg(i);
    std::cout << "\"/>";

    std::vector<Segment> segs;
    std::cout << "<path fill=\"none\" stroke=\"blue\" stroke-opacity=\".8\" "
                 "stroke-linecap=\"round\"  d=\"";
    xyhatch(h, s, print_seg, rule);
    std::cout << "\"/>";

    std::cout << "</svg>";
    return 0;
}
