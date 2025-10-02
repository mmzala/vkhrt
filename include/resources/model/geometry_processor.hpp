#pragma once
#include "model.hpp"

std::vector<Line> GenerateLines(const Mesh& mesh, const std::vector<Mesh::Vertex>& vertexBuffer, const std::vector<uint32_t>& indexBuffer);
Mesh GenerateMeshGeometryCubesFromLines(const std::vector<Line>& lines, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer);
std::vector<Curve> GenerateCurvesFromLines(const std::vector<Line>& lines, float tension = 1.0f);
ModelCreation GenerateHairMeshesFromHairModel(const ModelCreation& modelCreation);
