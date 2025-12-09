#include "resources/model/geometry_processor.hpp"

#include <glm/ext/vector_ulp.hpp>
#include <glm/gtx/optimum_pow.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <spdlog/spdlog.h>

static const std::vector<uint32_t> CUBE_INDICES {
    // Top
    2, 6, 7,
    2, 3, 7,

    // Bottom
    0, 4, 5,
    0, 1, 5,

    // Left
    0, 2, 6,
    0, 4, 6,

    // Right
    1, 3, 7,
    1, 5, 7,

    // Front
    0, 2, 3,
    0, 1, 3,

    // Back
    4, 6, 7,
    4, 5, 7
};

static const std::vector<glm::vec3> CUBE_VERTICES {
    glm::vec3(-1.0f, -1.0f, 1.0f),
    glm::vec3(1.0f, -1.0f, 1.0f),
    glm::vec3(-1.0f, 1.0f, 1.0f),
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(-1.0f, -1.0f, -1.0f),
    glm::vec3(1.0f, -1.0f, -1.0f),
    glm::vec3(-1.0f, 1.0f, -1.0f),
    glm::vec3(1.0f, 1.0f, -1.0f)
};

std::vector<Line> GenerateLines(const Mesh& mesh, const std::vector<Mesh::Vertex>& vertexBuffer, const std::vector<uint32_t>& indexBuffer)
{
    if (mesh.primitiveType != Mesh::PrimitiveType::eLines)
    {
        spdlog::error("[MODEL LOADING] Trying to generate line segments while mesh primitive type is not lines");
        return {};
    }

    std::vector<Line> lineSegments(mesh.indexCount / 2);
    uint32_t indexOffset = 0;

    for (Line& segment : lineSegments)
    {
        uint32_t startIndex = indexBuffer[mesh.firstIndex + indexOffset];
        uint32_t endIndex = indexBuffer[mesh.firstIndex + indexOffset + 1];

        segment.start = vertexBuffer[mesh.firstVertex + startIndex].position;
        segment.end = vertexBuffer[mesh.firstVertex + endIndex].position;
        indexOffset += 2;
    }

    return lineSegments;
}

std::vector<Line> MergeLines(const std::vector<Line>& lines)
{
    std::vector<Line> newLines {};

    uint32_t wantedLinesCount = lines.size() / 2;
    newLines.reserve(wantedLinesCount);

    for (uint32_t i = 0; i < wantedLinesCount; ++i)
    {
        uint32_t oldLineIndex = i * 2;
        const Line& oldLine1 = lines[oldLineIndex];

        // If only 1 line remaining, put it back into list
        if (oldLineIndex + 1 == lines.size())
        {
            newLines.push_back(oldLine1);
            break;
        }

        const Line& oldLine2 = lines[oldLineIndex + 1];

        // If lines are not connected, we can't merge them so put them back and continue
        if (oldLine1.end != oldLine2.start)
        {
            newLines.push_back(oldLine1);
            newLines.push_back(oldLine2);
            continue;
        }

        Line& newLine = newLines.emplace_back();
        newLine.start = oldLine1.start;
        newLine.end = oldLine2.end;
    }

    return newLines;
}

std::vector<Line> SplitLines(const std::vector<Line>& lines)
{
    std::vector<Line> newLines {};

    uint32_t wantedLinesCount = lines.size() * 2;
    newLines.reserve(wantedLinesCount);

    for (const Line& oldLine : lines)
    {
        glm::vec3 middlePoint = (oldLine.start + oldLine.end) * 0.5f;
        newLines.push_back({ oldLine.start, middlePoint });
        newLines.push_back({ middlePoint, oldLine.end });
    }

    return newLines;
}

std::vector<Curve> GenerateCurves(const std::vector<Line>& lines, float tension = 1.0f)
{
    std::vector<Curve> curves {};
    curves.reserve(lines.size());

    // Duplicate end-lines so Catmull–Rom works at the ends
    std::vector<Line> linesToProcess {};
    linesToProcess.reserve(lines.size() + 2);
    linesToProcess.push_back(lines.front()); // duplicate first
    linesToProcess.insert(linesToProcess.end(), lines.begin(), lines.end());
    linesToProcess.push_back(lines.back()); // duplicate last

    for (uint32_t i = 1; i < linesToProcess.size() - 1; ++i)
    {
        // Detect if the next and previous curve segments are on the same hair strand.
        // If they are, use them, if they aren't reuse the current line to generate
        // the curve to avoid generation based on unrelated hair strands
        uint32_t prevIndex = linesToProcess[i - 1].end == linesToProcess[i].start ? i - 1 : i;
        uint32_t nextIndex = linesToProcess[i].end == linesToProcess[i + 1].start ? i + 1 : i;

        glm::vec3 p0 = linesToProcess[prevIndex].start;
        glm::vec3 p1 = linesToProcess[i].start;
        glm::vec3 p2 = linesToProcess[i].end;
        glm::vec3 p3 = linesToProcess[nextIndex].end;

        Curve& curve = curves.emplace_back();
        curve.start = p1;
        curve.controlPoint1 = p1 + (p2 - p0) * (tension / 6.0f);
        curve.controlPoint2 = p2 - (p3 - p1) * (tension / 6.0f);
        curve.end = p2;
    }

    return curves;
}

std::vector<Curve> MergeCurvesFast(const std::vector<Curve>& curves)
{
    std::vector<Curve> newCurves {};

    uint32_t wantedCurvesCount = curves.size() / 2;
    newCurves.reserve(wantedCurvesCount);

    for (uint32_t i = 0; i < wantedCurvesCount; ++i)
    {
        uint32_t oldCurveIndex = i * 2;
        const Curve& oldCurve1 = curves[oldCurveIndex];

        // If only 1 curve remaining, put it back into list
        if (oldCurveIndex + 1 == curves.size())
        {
            newCurves.push_back(oldCurve1);
            break;
        }

        const Curve& oldCurve2 = curves[oldCurveIndex + 1];

        // If curves are not connected, we can't merge them so put them back and continue
        if (oldCurve1.end != oldCurve2.start)
        {
            newCurves.push_back(oldCurve1);
            newCurves.push_back(oldCurve2);
            continue;
        }

        Curve& newCuve = newCurves.emplace_back();
        newCuve.start = oldCurve1.start;
        newCuve.end = oldCurve2.end;

        glm::vec3 middlePoint = (oldCurve1.controlPoint2 + oldCurve2.controlPoint1) * 0.5f;
        newCuve.controlPoint1 = (oldCurve1.controlPoint1 + middlePoint) * 0.5f;
        newCuve.controlPoint2 = (middlePoint + oldCurve2.controlPoint2) * 0.5f;
    }

    return newCurves;
}

// Generate a vector that is orthogonal to the input vector.
// This can be used to invent a tangent frame for meshes that don't have real tangents/bitangents
inline glm::vec3 PerpStark(const glm::vec3& u)
{
    glm::vec3 a = abs(u);
    uint32_t uyx = (a.x - a.y) < 0 ? 1 : 0;
    uint32_t uzx = (a.x - a.z) < 0 ? 1 : 0;
    uint32_t uzy = (a.y - a.z) < 0 ? 1 : 0;
    uint32_t xm = uyx & uzx;
    uint32_t ym = (1 ^ xm) & uzy;
    uint32_t zm = 1 ^ (xm | ym); // 1 ^ (xm & ym)
    glm::vec3 v = glm::normalize(cross(u, glm::vec3(xm, ym, zm)));
    return v;
}

// Build a local frame from a unit normal vector
inline void BuildFrame(const glm::vec3& n, glm::vec3& t, glm::vec3& b)
{
    t = PerpStark(n);
    b = glm::cross(n, t);
}

Mesh GenerateDisjointOrthogonalTriangleStrips(const std::vector<Line>& lines, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float radius = 0.02f)
{
    Mesh mesh {};
    mesh.firstIndex = indexBuffer.size();
    mesh.firstVertex = vertexBuffer.size();

    const uint32_t numVerticesPerSegment = 4 * 3; // 4 triangles (3 vertices each)
    const uint32_t numIndices = lines.size() * numVerticesPerSegment;
    const uint32_t numVertices = numIndices;

    mesh.indexCount = numIndices;

    indexBuffer.resize(indexBuffer.size() + numIndices);
    vertexBuffer.resize(vertexBuffer.size() + numVertices);
    uint32_t indexOffset = 0;

    for (const Line& line : lines)
    {
        // Build the initial frame
        glm::vec3 fwd, s, t;
        fwd = glm::normalize(line.end - line.start);
        BuildFrame(fwd, s, t);

        glm::vec3 v[2] = { s, t };

        for (uint32_t face = 0; face < 2; ++face)
        {
            // Generate indices
            const uint32_t baseIndex = mesh.firstIndex + indexOffset + face * 6;

            indexBuffer[baseIndex] = baseIndex;
            indexBuffer[baseIndex + 1] = baseIndex + 1;
            indexBuffer[baseIndex + 2] = baseIndex + 2;
            indexBuffer[baseIndex + 3] = baseIndex + 3;
            indexBuffer[baseIndex + 4] = baseIndex + 4;
            indexBuffer[baseIndex + 5] = baseIndex + 5;

            // Generate vertices
            vertexBuffer[baseIndex] = { line.start + v[face] * radius };
            vertexBuffer[baseIndex + 1] = { line.end - v[face] * radius };
            vertexBuffer[baseIndex + 2] = { line.end + v[face] * radius };
            vertexBuffer[baseIndex + 3] = { line.start + v[face] * radius };
            vertexBuffer[baseIndex + 4] = { line.start - v[face] * radius };
            vertexBuffer[baseIndex + 5] = { line.end - v[face] * radius };
        }

        indexOffset += numVerticesPerSegment;
    }

    return mesh;
}

LSSMesh GenerateLinearSweptSpheres(const std::vector<Line>& lines, std::vector<glm::vec3>& positionBuffer, std::vector<float>& radiusBuffer, float radius = 0.02f)
{
    LSSMesh mesh {};
    mesh.firstVertex = positionBuffer.size(); // Should be the same for radius buffer, so we only track positions buffer

    uint32_t numVertices = lines.size() * 2;
    mesh.vertexCount = numVertices;

    positionBuffer.resize(positionBuffer.size() + numVertices);
    radiusBuffer.resize(radiusBuffer.size() + numVertices);

    uint32_t indexOffset = 0;
    float lssRadius = glm::max(radius, 0.001f);

    for (const Line& line : lines)
    {
        positionBuffer[indexOffset] = line.start;
        positionBuffer[indexOffset + 1] = line.end;
        radiusBuffer[indexOffset] = lssRadius;
        radiusBuffer[indexOffset + 1] = lssRadius;

        indexOffset += 2;
    }

    return mesh;
}

Mesh GenerateMeshGeometryCubes(const std::vector<Line>& lines, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float cubeSize = 0.02f)
{
    Mesh mesh {};
    mesh.firstIndex = indexBuffer.size();
    mesh.firstVertex = vertexBuffer.size();

    for (const Line& line : lines)
    {
        const uint32_t firstCubeVertex = vertexBuffer.size();

        for (const uint32_t index : CUBE_INDICES)
        {
            indexBuffer.push_back(index + firstCubeVertex);
        }

        for (const glm::vec3& pos : CUBE_VERTICES)
        {
            vertexBuffer.push_back({ pos * cubeSize + line.start });
        }

        mesh.indexCount += CUBE_INDICES.size();
    }

    return mesh;
}

Mesh GenerateMeshGeometryCubes(const std::vector<Curve>& curves, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float cubeSize = 0.02f, uint32_t numSamplesPerCurve = 2)
{
    Mesh mesh {};
    mesh.firstIndex = indexBuffer.size();
    mesh.firstVertex = vertexBuffer.size();

    for (const Curve& curve : curves)
    {
        for (uint32_t sample = 0; sample < numSamplesPerCurve; ++sample)
        {
            const float t = static_cast<float>(sample) / static_cast<float>(numSamplesPerCurve - 1);
            const glm::vec3 point = curve.Sample(t);
            const uint32_t firstCubeVertex = vertexBuffer.size();

            for (const uint32_t index : CUBE_INDICES)
            {
                indexBuffer.push_back(index + firstCubeVertex);
            }

            for (const glm::vec3& pos : CUBE_VERTICES)
            {
                vertexBuffer.push_back({ pos * cubeSize + point });
            }

            mesh.indexCount += CUBE_INDICES.size();
        }
    }

    return mesh;
}

Mesh GenerateMeshGeometryTubes(const std::vector<Curve>& curves, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float radius = 0.2f, uint32_t numCurveSamples = 2, uint32_t numRadialSamples = 4)
{
    Mesh mesh {};
    mesh.firstIndex = indexBuffer.size();
    mesh.firstVertex = vertexBuffer.size();

    // For stable frames, pick an up vector as a starting hint
    glm::vec3 globalUp(0.0f, 1.0f, 0.0f);

    for (const Curve& curve : curves)
    {
        for (uint32_t sample = 0; sample < numCurveSamples; ++sample)
        {
            const float t = static_cast<float>(sample) / static_cast<float>(numCurveSamples - 1);
            const glm::vec3 point = curve.Sample(t);
            glm::vec3 tangent = glm::normalize(curve.SampleDerivitive(t));

            // Build an orthonormal frame
            glm::vec3 n = glm::normalize(globalUp - tangent * glm::dot(globalUp, tangent));
            if (glm::length(n) < 1e-4f)
            {
                n = glm::normalize(glm::cross(tangent, glm::vec3(1, 0, 0)));
            }
            glm::vec3 b = glm::normalize(glm::cross(tangent, n));

            // Generate circle of vertices
            for (uint32_t j = 0; j < numRadialSamples; j++)
            {
                float theta = (2.0f * glm::pi<float>() * j) / static_cast<float>(numRadialSamples);
                glm::vec3 offset = radius * (glm::cos(theta) * n + glm::sin(theta) * b);
                glm::vec3 vertex = point + offset;

                vertexBuffer.push_back({ vertex });
            }

            // Generate triangle indices
            uint32_t ringSize = numRadialSamples;
            const uint32_t firstVertex = vertexBuffer.size();

            for (uint32_t i = 0; i < numCurveSamples; i++)
            {
                uint32_t ringStart = i * ringSize;
                uint32_t nextRingStart = (i + 1) * ringSize;

                for (uint32_t j = 0; j < numRadialSamples; j++)
                {
                    uint32_t nextJ = (j + 1) % numRadialSamples;

                    indexBuffer.push_back(firstVertex + ringStart + j);
                    indexBuffer.push_back(firstVertex + nextRingStart + j);
                    indexBuffer.push_back(firstVertex + nextRingStart + nextJ);

                    indexBuffer.push_back(firstVertex + ringStart + j);
                    indexBuffer.push_back(firstVertex + nextRingStart + nextJ);
                    indexBuffer.push_back(firstVertex + ringStart + nextJ);

                    mesh.indexCount += 6;
                }
            }
        }
    }

    return mesh;
}

std::vector<AABB> GenerateAABBs(const std::vector<Curve>& curves, float curveRadius)
{
    std::vector<AABB> aabbs(curves.size());

    for (uint32_t i = 0; i < curves.size(); ++i)
    {
        const Curve& curve = curves[i];
        AABB& aabb = aabbs[i];

        aabb.min = glm::min(glm::min(curve.start, curve.end), glm::min(curve.controlPoint1, curve.controlPoint2)) - curveRadius;
        aabb.max = glm::max(glm::max(curve.start, curve.end), glm::max(curve.controlPoint1, curve.controlPoint2)) + curveRadius;
    }

    return aabbs;
}

glm::vec3 GetVoxelWorldPosition(uint32_t voxelIndex1D, const glm::vec3& voxelGridOrigin, const glm::ivec3& voxelGridResolution, float voxelSize)
{
    // Unflatten index
    glm::vec3 voxelIndex3D {};
    voxelIndex3D.z = voxelIndex1D / (voxelGridResolution.x * voxelGridResolution.y);
    uint32_t remainder = voxelIndex1D % (voxelGridResolution.x * voxelGridResolution.y);
    voxelIndex3D.y = remainder / voxelGridResolution.x;
    voxelIndex3D.x = remainder % voxelGridResolution.x;

    // Convert to world space
    return voxelGridOrigin + voxelIndex3D * voxelSize;
}

std::vector<AABB> GenerateAABBs(const VoxelMesh& voxelMesh, const std::vector<bool>& voxels, float voxelSize)
{
    std::vector<AABB> aabbs(voxels.size());

    for (uint32_t i = 0; i < voxels.size(); ++i)
    {
        if (!voxels[i])
        {
            continue;
        }

        glm::vec3 voxelWorldPosition = GetVoxelWorldPosition(i, voxelMesh.boundingBox.min, voxelMesh.voxelGridResolution, voxelSize);

        AABB& aabb = aabbs[i];
        aabb.min = voxelWorldPosition;
        aabb.max = voxelWorldPosition + voxelSize;
    }

    return aabbs;
}

template <typename T, typename B>
T NextDivisible(const T& dividend, const B divisor)
{
    return glm::ceil(dividend / divisor) * divisor;
}

glm::vec3 GetVoxelWorldPosition(const glm::vec3& worldPosition, float voxelSize)
{
    return glm::floor(worldPosition / voxelSize);
}

glm::ivec3 GetVoxelIndex3D(const glm::vec3& worldPosition, const glm::vec3& voxelGridOrigin, float voxelSize)
{
    return glm::floor((worldPosition - voxelGridOrigin) / voxelSize);
}

uint32_t GetVoxelIndex1D(const glm::ivec3& voxelIndex3D, const glm::ivec3& voxelGridResolution)
{
    return voxelIndex3D.x + voxelIndex3D.y * voxelGridResolution.x + voxelIndex3D.z * (voxelGridResolution.x * voxelGridResolution.y);
}

std::array<uint8_t, 3> GetMajorAxes(const glm::vec3& v)
{
    glm::vec3 av = glm::abs(v);
    std::array<uint8_t, 3> axes = { 0, 1, 2 };
    std::sort(axes.begin(), axes.end(), [&](uint8_t a, uint8_t b)
        { return av[a] > av[b]; });
    return axes;
}

void FillVoxel(const glm::ivec3& index3D, VoxelMesh& mesh, std::vector<bool>& voxels)
{
    uint32_t index1D = GetVoxelIndex1D(index3D, mesh.voxelGridResolution);

    if (index1D >= voxels.size())
    {
        return; // TODO: Look at this case as this should never happen
    }

    voxels[mesh.firstVoxel + index1D] = true;
    mesh.filledVoxelCount++;
}

VoxelMesh GenerateVoxelMesh(const std::vector<Line>& lines, const AABB& meshBounds, float hairRadius, float voxelSize, std::vector<bool>& voxels)
{
    VoxelMesh voxelMesh {};
    voxelMesh.firstVoxel = voxels.size();
    voxelMesh.boundingBox.min = meshBounds.min;
    voxelMesh.boundingBox.max = meshBounds.max;

    // Expand grid bounds until we can fit whole voxels inside
    glm::vec3 voxelGridAreaDistance = voxelMesh.boundingBox.max - voxelMesh.boundingBox.min;
    voxelGridAreaDistance = NextDivisible(voxelGridAreaDistance, voxelSize);
    voxelMesh.boundingBox.max = voxelMesh.boundingBox.min + voxelGridAreaDistance;

    // Set grid resolution
    voxelMesh.voxelGridResolution = voxelGridAreaDistance / voxelSize;
    const uint32_t numVoxels = voxelMesh.voxelGridResolution.x * voxelMesh.voxelGridResolution.y * voxelMesh.voxelGridResolution.z;
    voxels.resize(voxels.size() + numVoxels);

    spdlog::info("voxel grid size: {} with grid distance {}, {}, {}", voxels.size(), voxelGridAreaDistance.x, voxelGridAreaDistance.y, voxelGridAreaDistance.z);
    spdlog::info("voxel grid res {}, {}, {}", voxelMesh.voxelGridResolution.x, voxelMesh.voxelGridResolution.y, voxelMesh.voxelGridResolution.z);
    spdlog::info("voxel bounds {}, {}, {} to {}, {}, {}", voxelMesh.boundingBox.min.x, voxelMesh.boundingBox.min.y, voxelMesh.boundingBox.min.z, voxelMesh.boundingBox.max.x, voxelMesh.boundingBox.max.y, voxelMesh.boundingBox.max.z);

    // Voxelize polylines as capsules
    // Algorithm taken from 'Real-Time Rendering of Dynamic Line Sets using Voxel Ray Tracing' paper: https://arxiv.org/pdf/2510.09081
    for (const Line& line : lines)
    {
        glm::vec3 d = line.end - line.start;
        std::array<uint8_t, 3> a = GetMajorAxes(d);

        // Make sure the major axis is always positive by swapping points
        glm::vec3 v0 = d[a[0]] < 0.0f ? line.end : line.start;
        glm::vec3 v1 = d[a[0]] < 0.0f ? line.start : line.end;

        // Step vector from one major axis voxel boundary to the next
        glm::vec3 s = d / d[a[0]];

        // Get extended line segments to capture capsule ends
        glm::vec3 sr = s * hairRadius;
        glm::vec3 vr0 = v0 - sr;
        glm::vec3 vr1 = v1 + sr;

        // Get projected capsule radius on both minor axes
        float dn = glm::length(d);
        float r1 = hairRadius / glm::sqrt(1.0f - glm::pow2(d[a[1]] / dn));
        float r2 = hairRadius / glm::sqrt(1.0f - glm::pow2(d[a[2]] / dn));

        // Setup starting point for
        float tmin = vr0[a[0]];
        float tmax = vr1[a[0]];
        float t0 = tmin;
        glm::vec3 p0 = vr0;

        while (t0 < tmax)
        {
            // Compute next intersection point
            float t1 = glm::min(tmax, t0 + voxelSize); // TODO: Check if step is correct (voxelsize should be multiplied by s?)
            glm::vec3 p1 = vr0 + s * (t1 - tmin);

            // Define box to voxelize
            glm::vec3 worldMin = glm::min(p0, p1);
            glm::vec3 worldMax = glm::max(p0, p1);

            worldMin[a[1]] -= r1;
            worldMin[a[2]] -= r2;

            worldMax[a[1]] += r1;
            worldMax[a[2]] += r2;

            glm::ivec3 minIndex = GetVoxelIndex3D(worldMin, voxelMesh.boundingBox.min, voxelSize);
            glm::ivec3 maxIndex = GetVoxelIndex3D(worldMax, voxelMesh.boundingBox.min, voxelSize);

            // Visit all voxels within box
            for (int32_t j = minIndex.y; j <= maxIndex.y; ++j)
            {
                for (int32_t k = minIndex.z; k <= maxIndex.z; ++k)
                {
                    glm::ivec3 index = glm::ivec3(minIndex.x, j, k);
                    FillVoxel(index, voxelMesh, voxels);
                }
            }

            // Move to next intersection point
            t0 = t1;
            p0 = p1;
        }
    }

    return voxelMesh;
}

ModelCreation ProcessHairCurves(const ModelCreation& modelCreation)
{
    const auto it = std::find_if(modelCreation.sceneGraph->meshes.begin(), modelCreation.sceneGraph->meshes.end(), [](const Mesh& mesh)
        { return mesh.primitiveType != Mesh::PrimitiveType::eLines; });
    if (it != modelCreation.sceneGraph->meshes.end())
    {
        spdlog::error("[GEOMETRY PROCESSOR] Model \"{}\" contains multiple different mesh primitive types while trying to generate hair model!", modelCreation.sceneGraph->sceneName);
        return modelCreation;
    }

    ModelCreation newModelCreation {};
    newModelCreation.sceneGraph = modelCreation.sceneGraph;
    SceneGraph& sceneGraph = *newModelCreation.sceneGraph;

    for (int meshIndex = 0; meshIndex < sceneGraph.meshes.size(); ++meshIndex)
    {
        const Mesh& oldMesh = sceneGraph.meshes[meshIndex];

        Hair& hair = sceneGraph.hairs.emplace_back();
        hair.material = oldMesh.material;
        hair.firstCurve = newModelCreation.curveBuffer.size();
        hair.firstAabb = newModelCreation.aabbBuffer.size();

        // Create line segments from hair lines
        const std::vector<Line> lines = GenerateLines(oldMesh, modelCreation.vertexBuffer, modelCreation.indexBuffer);

        // Create curves from lines
        const std::vector<Curve> curves = GenerateCurves(lines);
        newModelCreation.curveBuffer.insert(newModelCreation.curveBuffer.end(), curves.begin(), curves.end());

        // Create aabb's from curves
        constexpr float hairCurveRadius = 0.02f;
        const std::vector<AABB> aabbs = GenerateAABBs(curves, hairCurveRadius);
        newModelCreation.aabbBuffer.insert(newModelCreation.aabbBuffer.end(), aabbs.begin(), aabbs.end());

        // Update hair information
        hair.curveCount = curves.size();
        hair.aabbCount = aabbs.size();
    }

    // Update scene graph to use hair
    sceneGraph.meshes.clear();

    for (Node& node : sceneGraph.nodes)
    {
        node.hairs = node.meshes; // Since we don't support models with mixed hair and mesh geometry for now, we just copy the information
        node.meshes.clear();
    }

    return newModelCreation;
}

ModelCreation ProcessHairDOTS(const ModelCreation& modelCreation)
{
    const auto it = std::find_if(modelCreation.sceneGraph->meshes.begin(), modelCreation.sceneGraph->meshes.end(), [](const Mesh& mesh)
        { return mesh.primitiveType != Mesh::PrimitiveType::eLines; });
    if (it != modelCreation.sceneGraph->meshes.end())
    {
        spdlog::error("[GEOMETRY PROCESSOR] Model \"{}\" contains multiple different mesh primitive types while trying to generate hair model!", modelCreation.sceneGraph->sceneName);
        return modelCreation;
    }

    ModelCreation newModelCreation {};
    newModelCreation.sceneGraph = modelCreation.sceneGraph;
    SceneGraph& sceneGraph = *newModelCreation.sceneGraph;

    std::vector<Mesh> newMeshes(sceneGraph.meshes.size());

    for (uint32_t meshIndex = 0; meshIndex < sceneGraph.meshes.size(); ++meshIndex)
    {
        const Mesh& oldMesh = sceneGraph.meshes[meshIndex];

        // Create line segments from hair lines
        const std::vector<Line> lines = GenerateLines(oldMesh, modelCreation.vertexBuffer, modelCreation.indexBuffer);

        // Create DOTS mesh from line segments
        Mesh& newMesh = newMeshes[meshIndex];
        newMesh = GenerateDisjointOrthogonalTriangleStrips(lines, newModelCreation.vertexBuffer, newModelCreation.indexBuffer, 0.02f);
        newMesh.material = oldMesh.material;
    }

    // Update geometry information in the model
    sceneGraph.meshes = newMeshes;
    return newModelCreation;
}

ModelCreation ProcessHairVoxels(const ModelCreation& modelCreation)
{
    const auto it = std::find_if(modelCreation.sceneGraph->meshes.begin(), modelCreation.sceneGraph->meshes.end(), [](const Mesh& mesh)
        { return mesh.primitiveType != Mesh::PrimitiveType::eLines; });
    if (it != modelCreation.sceneGraph->meshes.end())
    {
        spdlog::error("[GEOMETRY PROCESSOR] Model \"{}\" contains multiple different mesh primitive types while trying to generate hair model!", modelCreation.sceneGraph->sceneName);
        return modelCreation;
    }

    ModelCreation newModelCreation {};
    newModelCreation.sceneGraph = modelCreation.sceneGraph;
    SceneGraph& sceneGraph = *newModelCreation.sceneGraph;

    for (int meshIndex = 0; meshIndex < sceneGraph.meshes.size(); ++meshIndex)
    {
        const Mesh& oldMesh = sceneGraph.meshes[meshIndex];

        // Create line segments from hair lines
        const std::vector<Line> lines = GenerateLines(oldMesh, modelCreation.vertexBuffer, modelCreation.indexBuffer);

        // Voxelize mesh
        constexpr float voxelSize = 0.1f;
        constexpr float hairRadius = 0.02f;

        VoxelMesh& newMesh = sceneGraph.voxelMeshes.emplace_back();
        newMesh = GenerateVoxelMesh(lines, oldMesh.boundingBox, hairRadius, voxelSize, newModelCreation.voxelGridBuffer);
        newMesh.material = oldMesh.material;
        newMesh.firstAabb = newModelCreation.aabbBuffer.size();

        // Generate debug AABBs
        const std::vector<AABB> aabbs = GenerateAABBs(newMesh, newModelCreation.voxelGridBuffer, voxelSize);
        newModelCreation.aabbBuffer.insert(newModelCreation.aabbBuffer.end(), aabbs.begin(), aabbs.end());

        // Update hair information
        newMesh.aabbCount = aabbs.size();
    }

    // Update scene graph to use hair
    sceneGraph.meshes.clear();

    for (Node& node : sceneGraph.nodes)
    {
        node.voxelMeshes = node.meshes; // Since we don't support models with mixed voxel meshes and mesh geometry for now, we just copy the information
        node.meshes.clear();
    }

    return newModelCreation;
}

ModelCreation ProcessHairLSS(const ModelCreation& modelCreation)
{
    const auto it = std::find_if(modelCreation.sceneGraph->meshes.begin(), modelCreation.sceneGraph->meshes.end(), [](const Mesh& mesh)
        { return mesh.primitiveType != Mesh::PrimitiveType::eLines; });
    if (it != modelCreation.sceneGraph->meshes.end())
    {
        spdlog::error("[GEOMETRY PROCESSOR] Model \"{}\" contains multiple different mesh primitive types while trying to generate hair model!", modelCreation.sceneGraph->sceneName);
        return modelCreation;
    }

    ModelCreation newModelCreation {};
    newModelCreation.sceneGraph = modelCreation.sceneGraph;
    SceneGraph& sceneGraph = *newModelCreation.sceneGraph;

    sceneGraph.lssMeshes.resize(sceneGraph.meshes.size());

    for (uint32_t meshIndex = 0; meshIndex < sceneGraph.meshes.size(); ++meshIndex)
    {
        const Mesh& oldMesh = sceneGraph.meshes[meshIndex];

        // Create line segments from hair lines
        const std::vector<Line> lines = GenerateLines(oldMesh, modelCreation.vertexBuffer, modelCreation.indexBuffer);

        // Create LSS mesh from line segments
        LSSMesh& lssMesh = sceneGraph.lssMeshes[meshIndex];
        lssMesh = GenerateLinearSweptSpheres(lines, newModelCreation.lssPositionBuffer, newModelCreation.lssRadiusBuffer, 0.02f);
        lssMesh.material = oldMesh.material;
    }

    // Update scene graph to use lss
    sceneGraph.meshes.clear();

    for (Node& node : sceneGraph.nodes)
    {
        node.lssMeshes = node.meshes; // Since we don't support models with mixed hair and mesh geometry for now, we just copy the information
        node.meshes.clear();
    }

    return newModelCreation;
}

ModelCreation ProcessHairDebugMesh(const ModelCreation& modelCreation)
{
    const auto it = std::find_if(modelCreation.sceneGraph->meshes.begin(), modelCreation.sceneGraph->meshes.end(), [](const Mesh& mesh)
        { return mesh.primitiveType != Mesh::PrimitiveType::eLines; });
    if (it != modelCreation.sceneGraph->meshes.end())
    {
        spdlog::error("[GEOMETRY PROCESSOR] Model \"{}\" contains multiple different mesh primitive types while trying to generate hair model!", modelCreation.sceneGraph->sceneName);
        return modelCreation;
    }

    ModelCreation newModelCreation {};
    newModelCreation.sceneGraph = modelCreation.sceneGraph;
    SceneGraph& sceneGraph = *newModelCreation.sceneGraph;

    std::vector<Mesh> newMeshes(sceneGraph.meshes.size());

    for (int meshIndex = 0; meshIndex < sceneGraph.meshes.size(); ++meshIndex)
    {
        const Mesh& oldMesh = sceneGraph.meshes[meshIndex];

        // Create line segments from hair lines
        const std::vector<Line> lines = GenerateLines(oldMesh, modelCreation.vertexBuffer, modelCreation.indexBuffer);

        // Create curves from lines
        const std::vector<Curve> curves = GenerateCurves(lines);

        // Create mesh from curve segments
        Mesh& newMesh = newMeshes[meshIndex];
        newMesh = GenerateMeshGeometryTubes(curves, newModelCreation.vertexBuffer, newModelCreation.indexBuffer, 0.02f, 3, 3);
        newMesh.material = oldMesh.material;
    }

    // Update geometry information in the model
    sceneGraph.meshes = newMeshes;
    return newModelCreation;
}
