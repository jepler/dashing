#include <GL/glew.h>
#include <cstdio>
#include <cmath>
#include <iostream>
#include "gldashing.hh"

int ifloor(double x) { return int(floor(x)); }
int iceil(double x) { return int(ceil(x)); }
int iround(double x) { return int(round(x)); }

#define SHADER_SOURCE(...) #__VA_ARGS__
void setup()
{
    glewInit();

    GLint compile_ok = GL_FALSE, link_ok = GL_FALSE;

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const char *vs_source =
        "#version 300 es\n"
        "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
        "# define maxfragp highp\n"
        "#else\n"
        "# define maxfragp medp\n"
        "#endif\n"
        SHADER_SOURCE(
            uniform maxfragp mat4 uXf;
            in maxfragp vec4 aPos;
            in mediump vec3 aColor;
            out vec4 vColor;
            out vec2 vTexCoord;
            void main(void)
            {
                gl_Position = uXf * vec4(aPos.xy, 0., 1.);
                vColor = vec4(aColor, 1.);
                vTexCoord = aPos.zw;
            }
        );
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
            in mediump vec2 vTexCoord;
            out mediump vec4 fragColor;
            uniform mediump sampler2DArray uTex;
            void main(void) {
                mediump float coverage =
                        texture(uTex, vec3(vTexCoord.x, 0, vTexCoord.y)).r;
                coverage = (vTexCoord.y < 0.) ? 1. : coverage;
                fragColor = vec4(vColor.rgb * coverage, coverage);
            }
        );
    glShaderSource(fs, 1, &fs_source, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &compile_ok);
    if (!compile_ok) {
        std::cerr << "Error in fragment shader\n";
        printf("%s\n", fs_source);
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
    st.uniform_uTex = glGetUniformLocation(program, "uTex");

    glEnableVertexAttribArray(st.attribute_aPos);
    glUniformMatrix4fv(st.uniform_uXf, 1, false, &st.mat[0][0]);
    glGenTextures(1, &st.texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, st.texture);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R8, 256, 256, st.h.d.size());
    glUniform1i(st.uniform_uTex, 0);

    for(size_t i=0; i<st.h.d.size(); i++) {
        auto &th = st.h.d[i];
        auto total = th.sum.back();
        float coverage[256];
        auto np = 256;
        std::fill(coverage, coverage+np, 0.f);
        for(size_t j=0; j<th.dash.size(); j++) {
            auto di = th.dash[j];
            if(di < 0) continue;

            auto lo = th.sum[j], lof = lo * np/total;
            auto lop = ifloor(lof);
            auto loc = (lop + 1) - lof;

            if(di == 0) { coverage[lop] += 256. / np; continue; }

            auto hi = th.sum[j+1], hif = hi * np/total;
            auto hip = iceil(hif);
            auto hic = 1 - (hip - hif);

            coverage[lop] += loc;
            for(int k = lop+1; k < hip-1; k++) coverage[k] = 1.;
            coverage[hip-1] += hic;
        }
        for(auto j=0; j<np; j++) coverage[j] = std::min(1.f, std::max(0.f, coverage[j]));

        for(auto j=0; j<np; j++)
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, j, i, np, 1, 1, GL_RED, GL_FLOAT, coverage);

    }
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glEnable(GL_BLEND);
    auto i = glGetError();
    if(i) { printf("gl error: %d\n", (int)i); abort(); }
}
