#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 0, binding = 0) uniform sampler2D textures[];

struct Material
{
    vec4 albedoFactor;

    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;

    vec3 emissiveFactor;
    bool useEmissiveMap;

    // TODO: Use flags to check usage
    bool useAlbedoMap;
    bool useMRMap;
    bool useNormalMap;
    bool useOcclusionMap;

    uint albedoMapIndex;
    uint mrMapIndex;
    uint normalMapIndex;
    uint occlusionMapIndex;

    uint emissiveMapIndex;
    float transparency;
    float ior;
};
layout (std140, set = 0, binding = 1) uniform Materials
{
    Material materials[1024];
};

struct GeometryNode
{
    uint64_t primitiveBufferDeviceAddress;
    uint64_t indexBufferDeviceAddress;
    uint materialIndex;
};
layout (std140, set = 0, binding = 2) buffer GeometryNodes
{
    GeometryNode geometryNodes[];
};

struct BLASInstance
{
    uint firstGeometryIndex;
};
layout (set = 0, binding = 3) buffer BLASInstances
{
    BLASInstance blasInstances[];
};
