#include "dashing.hh"
#include "contours_and_segments.hh"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtx/transform.hpp>

void setup();

struct AppState
{
    double jitter, scale;
    const char *rule;
    int attribute_aPos, attribute_aColor, uniform_uXf, uniform_uTex;
    unsigned texture;
    glm::fmat4x4 mat;
    dashing::Contours c;
    dashing::HatchPattern h;
};

extern AppState st;


