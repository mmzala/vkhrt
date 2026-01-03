#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable

#include "bindless.glsl"
#include "ray.glsl"
#include "primitives.glsl"
#include "debug.glsl"

layout(location = 0) rayPayloadInEXT HitPayload payload;
hitAttributeEXT vec3 attribNormal;

void main()
{
    BLASInstance blasInstance = blasInstances[gl_InstanceCustomIndexEXT];
    GeometryNode geometryNode = geometryNodes[blasInstance.firstGeometryIndex + gl_GeometryIndexEXT];
    Material material = materials[nonuniformEXT(geometryNode.materialIndex)];

    vec3 normal = attribNormal;

    vec3 lightColor = vec3(0.4, 0.2, 0.1);
    vec3 lightDirection = vec3(0.0, -1.0, 0.0);
    vec3 ambientColor = vec3(0.3);

    vec3 color = abs(dot(normal, lightDirection)) * lightColor;
    color += ambientColor;

    payload.hitValue = GetDebugColor(gl_PrimitiveID);
}
