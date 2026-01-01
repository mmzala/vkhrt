#version 460
#extension GL_EXT_ray_tracing : enable

#include "bindless.glsl"
#include "ray.glsl"

layout(location = 0) rayPayloadInEXT HitPayload payload;

layout(push_constant) uniform PushConstants
{
    uint environmentMapIndex;
};

#define PI 3.1415926538

vec2 DirectionToEquirectUV(vec3 dir)
{
    float phi = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));

    vec2 uv;
    uv.x = (phi + PI) / (2.0 * PI);
    uv.y = theta / PI;

    return uv;
}

void main()
{
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    vec2 uv = DirectionToEquirectUV(dir);

    vec4 environmentColor = texture(textures[nonuniformEXT(environmentMapIndex)], uv);
    payload.hitValue = environmentColor.rgb;
}
