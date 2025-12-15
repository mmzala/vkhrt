#pragma once
#include <cstdint>
#include <vector>

constexpr uint32_t BRICK_SIZE = 8;
constexpr uint32_t BRICK_VOLUME = BRICK_SIZE * BRICK_SIZE * BRICK_SIZE;
constexpr uint32_t INVALID_BRICK = 0xFFFFFFFF;

// One brick = 512 bits (8×8×8 occupancy)
struct Brick
{
    uint64_t mask[8];
};

class Brickmap
{
public:
    Brickmap(uint32_t voxelCountX, uint32_t voxelCountY, uint32_t voxelCountZ);
    void Build(const std::vector<bool>& voxels);

    [[nodiscard]] const std::vector<uint32_t>& GetBrickIndices() const { return _brickIndices; }
    [[nodiscard]] const std::vector<Brick>& GetBricks() const { return _bricks; }

private:
    uint32_t _voxelCountX {}, _voxelCountY {}, _voxelCountZ {};
    uint32_t _brickCountX {}, _brickCountY {}, _brickCountZ {};

    std::vector<uint32_t> _brickIndices {};
    std::vector<Brick> _bricks {};
};