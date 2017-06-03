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

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtx/transform.hpp>
#include <SDL2/SDL.h>

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

struct AppState
{
    double jitter, scale;
    const char *rule;
    GLint attribute_aPos, attribute_aColor, uniform_uXf;
    glm::fmat4x4 mat;
    Contours c;
    HatchPattern h;
};

AppState st;

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

#define SHADER_SOURCE(...) #__VA_ARGS__
void setup()
{
    glewInit();

    GLint compile_ok = GL_FALSE, link_ok = GL_FALSE;

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const char *vs_source =
        "#version 300 es\n"
        SHADER_SOURCE(
            uniform highp mat4 uXf;
            in highp vec2 aPos;
            in mediump vec3 aColor;
            out vec4 vColor;
            void main(void)
            {
                gl_Position = uXf * vec4(aPos, 0., 1.);
                vColor = vec4(aColor, 1.);
            }
        );
    printf("shader source = \n%s\n", vs_source);
    glShaderSource(vs, 1, &vs_source, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &compile_ok);
    if (!compile_ok) {
        std::cerr << "Error in vertex shader\n";
	abort();
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const char *fs_source =
        "#version 300 es\n"
        SHADER_SOURCE(
                in mediump vec4 vColor;
                void main(void)
                {
                    gl_FragColor = vColor;
                }
        );
    glShaderSource(fs, 1, &fs_source, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &compile_ok);
    if (!compile_ok) {
        std::cerr << "Error in fragment shader\n";
	abort();
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
    if (!link_ok) {
        std::cerr << "Error in glLinkProgram\n";
        abort();
    }

    glUseProgram(program);
    glClearColor(1., 1., 1., 1.);

    st.attribute_aPos = glGetAttribLocation(program, "aPos");
    st.attribute_aColor = glGetAttribLocation(program, "aColor");
    st.uniform_uXf = glGetUniformLocation(program, "uXf");

    glEnableVertexAttribArray(st.attribute_aPos);
    glUniformMatrix4fv(st.uniform_uXf, 1, false, &st.mat[0][0]);
}

size_t render(SDL_Window *window __attribute__((unused)))
{
    glClear(GL_COLOR_BUFFER_BIT);

    static Segments s;
    ContoursToSegments(s, st.c, st.jitter);

    static std::vector<Point> p;
    p.clear();
    for(auto && si : s)
    {
        p.push_back(si.p);
        p.push_back(si.q);
    }

    // draw outline of shape
    glVertexAttrib3f(st.attribute_aColor, 0., 0., 0.);
    glVertexAttribPointer(st.attribute_aPos, 2, GL_DOUBLE, GL_FALSE, 
            sizeof(p[0]), &p[0]);
    glDrawArrays(GL_LINES, 0, p.size());

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
    xyhatch(h, s,
            [](const Segment &s) { p.push_back(s.p); p.push_back(s.q); },
            st.rule);

    // draw dashes of shape
    glVertexAttrib3f(st.attribute_aColor, 0., 0., 1.);
    glVertexAttribPointer(st.attribute_aPos, 2, GL_DOUBLE, GL_FALSE, 
            sizeof(p[0]), &p[0]);
    glDrawArrays(GL_LINES, 0, p.size());
    
    SDL_GL_SwapWindow(window);
    return p.size();
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
