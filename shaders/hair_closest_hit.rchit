#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable

#include "bindless.glsl"
#include "ray.glsl"
#include "primitives.glsl"
#include "shading.glsl"
#include "debug.glsl"

layout(location = 0) rayPayloadInEXT HitPayload payload;
hitAttributeEXT vec3 attribNormal;

void main()
{
    BLASInstance blasInstance = blasInstances[gl_InstanceCustomIndexEXT];
    GeometryNode geometryNode = geometryNodes[blasInstance.firstGeometryIndex + gl_GeometryIndexEXT];
    Material material = materials[nonuniformEXT(geometryNode.materialIndex)];

    vec3 normal = attribNormal;
    vec3 color = Shade(normal);

    payload.hitValue = GetDebugColor(gl_PrimitiveID);
}
