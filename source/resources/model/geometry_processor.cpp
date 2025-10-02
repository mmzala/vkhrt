#include "resources/model/geometry_processor.hpp"
#include <spdlog/spdlog.h>

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

Mesh GenerateMeshGeometryCubesFromLines(const std::vector<Line>& lines, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer)
{
    Mesh mesh {};
    mesh.firstIndex = indexBuffer.size();
    mesh.firstVertex = vertexBuffer.size();

    constexpr float boxSize = 0.02f;

    for (const Line& line : lines)
    {
        static const std::vector<uint32_t> indices {
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

        static const std::vector<glm::vec3> vertices {
            glm::vec3(-1.0f, -1.0f, 1.0f),
            glm::vec3(1.0f, -1.0f, 1.0f),
            glm::vec3(-1.0f, 1.0f, 1.0f),
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(-1.0f, -1.0f, -1.0f),
            glm::vec3(1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f, 1.0f, -1.0f),
            glm::vec3(1.0f, 1.0f, -1.0f)
        };

        const uint32_t firstCubeVertex = vertexBuffer.size();

        for (const uint32_t index : indices)
        {
            indexBuffer.push_back(index + firstCubeVertex);
        }

        for (const glm::vec3& pos : vertices)
        {
            vertexBuffer.push_back({ pos * boxSize + line.start });
        }

        mesh.indexCount += indices.size();
    }

    return mesh;
}

std::vector<Curve> GenerateCurvesFromLines(const std::vector<Line>& lines, float tension)
{
    std::vector<Curve> curves(lines.size());

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

        // Create mesh from line segments
        Mesh& newMesh = newMeshes[meshIndex];
        newMesh = GenerateMeshGeometryCubesFromLines(lines, newVertexBuffer, newIndexBuffer);
        newMesh.material = oldMesh.material;
    }

    // Update geometry information in the model
    sceneGraph.meshes = newMeshes;
    newModelCreation.vertexBuffer = newVertexBuffer;
    newModelCreation.indexBuffer = newIndexBuffer;
    return newModelCreation;
}
