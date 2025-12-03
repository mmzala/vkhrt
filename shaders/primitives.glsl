struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 texCoord;
};

struct Triangle
{
    Vertex vertices[3];
};

#define GeometrySample Vertex

#include "curve.glsl"
#include "cone.glsl"
#include "cylinder.glsl"
