#pragma once
#include "model.hpp"

std::vector<Line> GenerateLines(const Mesh& mesh, const std::vector<Mesh::Vertex>& vertexBuffer, const std::vector<uint32_t>& indexBuffer);
Mesh GenerateMeshGeometryCubes(const std::vector<Line>& lines, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float cubeSize = 0.02f);
Mesh GenerateMeshGeometryCubes(const std::vector<Curve>& curves, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float cubeSize = 0.02f, uint32_t numSamplesPerCurve = 2);
Mesh GenerateMeshGeometryTubes(const std::vector<Curve>& curves, std::vector<Mesh::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, float radius = 0.2f, uint32_t numCurveSamples = 2, uint32_t numRadialSamples = 4);
std::vector<Curve> GenerateCurves(const std::vector<Line>& lines, float tension = 1.0f);
ModelCreation GenerateHairMeshesFromHairModel(const ModelCreation& modelCreation);
