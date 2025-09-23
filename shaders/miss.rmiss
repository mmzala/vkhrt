#version 460
#extension GL_EXT_ray_tracing : enable

#include "ray.glsl"

layout(location = 0) rayPayloadInEXT HitPayload payload;

void main()
{
    vec3 clearValue = vec3(1.0);
    payload.hitValue = clearValue * 0.8;
}
