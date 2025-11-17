#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "bindless.glsl"
#include "ray.glsl"
#include "primitives.glsl"
#include "debug.glsl"

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertices { Vertex vertices[]; };
layout(buffer_reference, scalar) readonly buffer Indices { uint indices[]; };

layout(location = 0) rayPayloadInEXT HitPayload payload;
hitAttributeEXT vec2 attribs;

Triangle UnpackGeometry(GeometryNode geometryNode)
{
    Vertices vertices = Vertices(geometryNode.primitiveBufferDeviceAddress);
    Indices indices = Indices(geometryNode.indexBufferDeviceAddress);

    Triangle triangle;
    const uint indexOffset = gl_PrimitiveID * 3;

    for (uint i = 0; i < 3; ++i)
    {
        const uint offset = indices.indices[indexOffset + i];
        triangle.vertices[i] = vertices.vertices[offset];
    }

    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    triangle.position = triangle.vertices[0].position * barycentricCoords.x + triangle.vertices[1].position * barycentricCoords.y + triangle.vertices[2].position * barycentricCoords.z;
    triangle.normal = normalize(triangle.vertices[0].normal * barycentricCoords.x + triangle.vertices[1].normal * barycentricCoords.y + triangle.vertices[2].normal * barycentricCoords.z);
    triangle.texCoord = triangle.vertices[0].texCoord * barycentricCoords.x + triangle.vertices[1].texCoord * barycentricCoords.y + triangle.vertices[2].texCoord * barycentricCoords.z;

    return triangle;
}

void main()
{
    BLASInstance blasInstance = blasInstances[gl_InstanceCustomIndexEXT];
    GeometryNode geometryNode = geometryNodes[blasInstance.firstGeometryIndex + gl_GeometryIndexEXT];
    Material material = materials[nonuniformEXT(geometryNode.materialIndex)];
    Triangle triangle = UnpackGeometry(geometryNode);

    vec4 albedo = material.albedoFactor;
    if (material.useAlbedoMap)
    {
        albedo *= texture(textures[nonuniformEXT(material.albedoMapIndex)], triangle.texCoord);
    }

    payload.hitValue = GetDebugColor(gl_PrimitiveID);
}
