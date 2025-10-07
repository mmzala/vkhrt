#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "bindless.glsl"
#include "ray.glsl"

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 texCoord;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertices { Vertex vertices[]; };
layout(buffer_reference, scalar) readonly buffer Indices { uint indices[]; };

layout(location = 0) rayPayloadInEXT HitPayload payload;
hitAttributeEXT vec2 attribs;

void main()
{
    BLASInstance blasInstance = blasInstances[gl_InstanceCustomIndexEXT];
    GeometryNode geometryNode = geometryNodes[blasInstance.firstGeometryIndex + gl_GeometryIndexEXT];
    Material material = materials[nonuniformEXT(geometryNode.materialIndex)];

    payload.hitValue = vec3(0.0, 1.0, 0.0);
}
