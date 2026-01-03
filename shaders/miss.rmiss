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
#define M_1_OVER_PI 0.3183098861837

vec2 DirectionToUV(vec3 v)
{
    float gamma = asin(v.y);
    float theta = atan(v.x, -v.z);

    return vec2(theta * M_1_OVER_PI * 0.5, gamma * M_1_OVER_PI) + 0.5;
}

void main()
{
    vec3 dir = normalize(-gl_WorldRayDirectionEXT); // Invert environment map
    vec2 uv = DirectionToUV(dir);

    const float gamma = 2.2;
    const float exposure = 1.0;

    vec3 environmentColor = texture(textures[nonuniformEXT(environmentMapIndex)], uv).rgb;
    environmentColor = vec3(1.0) - exp(-environmentColor * exposure);
    environmentColor = pow(environmentColor, vec3(1.0 / gamma));

    payload.hitValue = environmentColor;
}
