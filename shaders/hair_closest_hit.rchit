#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "bindless.glsl"
#include "ray.glsl"
#include "primitives.glsl"

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertices { Vertex vertices[]; };
layout(buffer_reference, scalar) readonly buffer Indices { uint indices[]; };

layout(location = 0) rayPayloadInEXT HitPayload payload;
hitAttributeEXT vec3 attribNormal;

vec3 colors[6] = vec3[](vec3(1.0, 0.0, 0.3), vec3(0.8, 0.2, 0.3), vec3(0.6, 0.4, 0.3), vec3(0.4, 0.6, 0.3), vec3(0.2, 0.8, 0.3), vec3(0.0, 1.0, 0.3));

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

    // int index = gl_PrimitiveID % 6;
    // vec3 color = colors[index];

    payload.hitValue = color;
}
