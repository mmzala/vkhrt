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

Mesh GenerateMeshGeometryCubes(const std::vector<Line>& lines, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float cubeSize)
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

Mesh GenerateMeshGeometryCubes(const std::vector<Curve>& curves, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float cubeSize, uint32_t numSamplesPerCurve)
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

Mesh GenerateMeshGeometryTubes(const std::vector<Curve>& curves, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float radius, uint32_t numCurveSamples, uint32_t numRadialSamples)
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
            const uint32_t firstCubeVertex = vertexBuffer.size();

            for (uint32_t i = 0; i < numCurveSamples; i++)
            {
                uint32_t ringStart = i * ringSize;
                uint32_t nextRingStart = (i + 1) * ringSize;

                for (uint32_t j = 0; j < numRadialSamples; j++)
                {
                    uint32_t nextJ = (j + 1) % numRadialSamples;

                    indexBuffer.push_back(firstCubeVertex + ringStart + j);
                    indexBuffer.push_back(firstCubeVertex + nextRingStart + j);
                    indexBuffer.push_back(firstCubeVertex + nextRingStart + nextJ);

                    indexBuffer.push_back(firstCubeVertex + ringStart + j);
                    indexBuffer.push_back(firstCubeVertex + nextRingStart + nextJ);
                    indexBuffer.push_back(firstCubeVertex + ringStart + nextJ);

                    mesh.indexCount += 6;
                }
            }
        }
    }

    return mesh;
}

std::vector<Curve> GenerateCurves(const std::vector<Line>& lines, float tension)
{
    std::vector<Curve> curves(lines.size());

    // TODO: Handle line array that has multiple hair strands

    // Duplicate end-lines so Catmull–Rom works at the ends
    std::vector<Line> linesToProcess {};
    linesToProcess.reserve(lines.size() + 2);
    linesToProcess.push_back(lines.front()); // duplicate first
    linesToProcess.insert(linesToProcess.end(), lines.begin(), lines.end());
    linesToProcess.push_back(lines.back()); // duplicate last

    for (uint32_t i = 1; i < linesToProcess.size() - 1; ++i)
    {
        Curve& curve = curves[i];

        glm::vec3 p0 = linesToProcess[i - 1].start;
        glm::vec3 p1 = linesToProcess[i].start;
        glm::vec3 p2 = linesToProcess[i].end;
        glm::vec3 p3 = linesToProcess[i + 1].end;

        curve.start = p1;
        curve.controlPoint1 = p1 + (p2 - p0) * (tension / 6.0f);
        curve.controlPoint2 = p2 - (p3 - p1) * (tension / 6.0f);
        curve.end = p2;
    }

    return curves;
}

ModelCreation GenerateHairMeshesFromHairModel(const ModelCreation& modelCreation)
{
    const auto it = std::find_if(modelCreation.sceneGraph->meshes.begin(), modelCreation.sceneGraph->meshes.end(), [](const Mesh& mesh){ return mesh.primitiveType != Mesh::PrimitiveType::eLines; });
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

        // Create mesh from line segments
        Mesh& newMesh = newMeshes[meshIndex];
        newMesh = GenerateMeshGeometryTubes(curves, newVertexBuffer, newIndexBuffer, 0.02f, 3, 4);
        newMesh.material = oldMesh.material;
    }

    // Update geometry information in the model
    sceneGraph.meshes = newMeshes;
    newModelCreation.vertexBuffer = newVertexBuffer;
    newModelCreation.indexBuffer = newIndexBuffer;
    return newModelCreation;
}
