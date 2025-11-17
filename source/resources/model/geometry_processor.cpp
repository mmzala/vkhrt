#include "resources/model/geometry_processor.hpp"
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
                glm::vec3 offset = radius * (cos(theta) * n + sin(theta) * b);
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

std::vector<AABB> GenerateAABBs(const std::vector<Curve>& curves, float curveRadius = 0.05f)
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
    std::vector<Mesh::Vertex> newVertexBuffer {};
    std::vector<uint32_t> newIndexBuffer {};

    for (int meshIndex = 0; meshIndex < sceneGraph.meshes.size(); ++meshIndex)
    {
        const Mesh& oldMesh = sceneGraph.meshes[meshIndex];

        // Create line segments from hair lines
        const std::vector<Line> lines = GenerateLines(oldMesh, modelCreation.vertexBuffer, modelCreation.indexBuffer);

        // Create DOTS mesh from line segments
        Mesh& newMesh = newMeshes[meshIndex];
        newMesh = GenerateDisjointOrthogonalTriangleStrips(lines, newVertexBuffer, newIndexBuffer, 0.02f);
        newMesh.material = oldMesh.material;
    }

    // Update geometry information in the model
    sceneGraph.meshes = newMeshes;
    newModelCreation.vertexBuffer = newVertexBuffer;
    newModelCreation.indexBuffer = newIndexBuffer;
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
    std::vector<Mesh::Vertex> newVertexBuffer {};
    std::vector<uint32_t> newIndexBuffer {};

    for (int meshIndex = 0; meshIndex < sceneGraph.meshes.size(); ++meshIndex)
    {
        const Mesh& oldMesh = sceneGraph.meshes[meshIndex];

        // Create line segments from hair lines
        const std::vector<Line> lines = GenerateLines(oldMesh, modelCreation.vertexBuffer, modelCreation.indexBuffer);

        // Create curves from lines
        const std::vector<Curve> curves = GenerateCurves(lines);

        // Create mesh from curve segments
        Mesh& newMesh = newMeshes[meshIndex];
        newMesh = GenerateMeshGeometryTubes(curves, newVertexBuffer, newIndexBuffer, 0.02f, 3, 3);
        newMesh.material = oldMesh.material;
    }

    // Update geometry information in the model
    sceneGraph.meshes = newMeshes;
    newModelCreation.vertexBuffer = newVertexBuffer;
    newModelCreation.indexBuffer = newIndexBuffer;
    return newModelCreation;
}
