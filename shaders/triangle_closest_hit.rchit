#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_NV_linear_swept_spheres : enable

#include "bindless.glsl"
#include "ray.glsl"
#include "primitives.glsl"
#include "debug.glsl"

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertices { Vertex vertices[]; };
layout(buffer_reference, scalar) readonly buffer Indices { uint indices[]; };

layout(location = 0) rayPayloadInEXT HitPayload payload;
hitAttributeEXT vec2 attribs;

GeometrySample UnpackTriangleGeometry(GeometryNode geometryNode)
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

    GeometrySample geometry;
    geometry.position = triangle.vertices[0].position * barycentricCoords.x + triangle.vertices[1].position * barycentricCoords.y + triangle.vertices[2].position * barycentricCoords.z;
    geometry.normal = normalize(triangle.vertices[0].normal * barycentricCoords.x + triangle.vertices[1].normal * barycentricCoords.y + triangle.vertices[2].normal * barycentricCoords.z);
    geometry.texCoord = triangle.vertices[0].texCoord * barycentricCoords.x + triangle.vertices[1].texCoord * barycentricCoords.y + triangle.vertices[2].texCoord * barycentricCoords.z;

    return geometry;
}

GeometrySample UnpackLSSGeometry()
{
    GeometrySample geometry;

    vec3[2] positions = gl_HitLSSPositionsNV;
    vec3 p0 = positions[0].xyz;
    vec3 p1 = positions[1].xyz;
    geometry.position = mix(p0, p1, attribs.x);

    vec3 objectSpacePosition = gl_ObjectRayOriginEXT + gl_HitTEXT * gl_ObjectRayDirectionEXT;
    geometry.normal = objectSpacePosition - geometry.position;
    geometry.normal = (gl_ObjectToWorldEXT * vec4(geometry.normal, 0.0)).xyz;
    geometry.normal = normalize(geometry.normal);

    return geometry;
}

GeometrySample UnpackGeometry(GeometryNode geometryNode)
{
    if (gl_HitIsLSSNV)
    {
        return UnpackLSSGeometry();
    }

    return UnpackTriangleGeometry(geometryNode);
}

void main()
{
    BLASInstance blasInstance = blasInstances[gl_InstanceCustomIndexEXT];
    GeometryNode geometryNode = geometryNodes[blasInstance.firstGeometryIndex + gl_GeometryIndexEXT];
    Material material = materials[nonuniformEXT(geometryNode.materialIndex)];
    GeometrySample geometry = UnpackGeometry(geometryNode);

    vec4 albedo = material.albedoFactor;
    if (material.useAlbedoMap)
    {
        albedo *= texture(textures[nonuniformEXT(material.albedoMapIndex)], geometry.texCoord);
    }

    payload.hitValue = GetDebugColor(gl_PrimitiveID);
}
