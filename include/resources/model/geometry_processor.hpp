#pragma once
#include "model.hpp"

std::vector<Line> GenerateLines(const Mesh& mesh, const std::vector<Mesh::Vertex>& vertexBuffer, const std::vector<uint32_t>& indexBuffer);
Mesh GenerateMeshGeometryCubesFromLines(const std::vector<Line>& lines, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer);
ModelCreation GenerateHairMeshesFromHairModel(const ModelCreation& modelCreation);
