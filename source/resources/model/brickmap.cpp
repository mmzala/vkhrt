#include "resources/model/brickmap.hpp"
#include <glm/vec3.hpp>

uint32_t Flatten3D(uint32_t x, uint32_t y, uint32_t z, uint32_t resolutionX, uint32_t resolutionY)
{
    return x + y * resolutionX + z * resolutionX * resolutionY;
}

Brickmap::Brickmap(uint32_t voxelCountX, uint32_t voxelCountY, uint32_t voxelCountZ)
    : _voxelCountX(voxelCountX)
    , _voxelCountY(voxelCountY)
    , _voxelCountZ(voxelCountZ)
{
    _brickCountX = (_voxelCountX + BRICK_SIZE - 1) / BRICK_SIZE;
    _brickCountY = (_voxelCountY + BRICK_SIZE - 1) / BRICK_SIZE;
    _brickCountZ = (_voxelCountZ + BRICK_SIZE - 1) / BRICK_SIZE;

    _brickIndices.resize(_brickCountX * _brickCountY * _brickCountZ, INVALID_BRICK);
}

void Brickmap::Build(const std::vector<bool>& voxels)
{
    _bricks.clear();

    for (uint32_t bz = 0; bz < _brickCountX; bz++)
    for (uint32_t by = 0; by < _brickCountY; by++)
    for (uint32_t bx = 0; bx < _brickCountZ; bx++)
    {
        Brick brick {};
        bool hasData = false;

        for (uint32_t z = 0; z < BRICK_SIZE; z++)
        for (uint32_t y = 0; y < BRICK_SIZE; y++)
        for (uint32_t x = 0; x < BRICK_SIZE; x++)
        {
            uint32_t vx = bx * BRICK_SIZE + x;
            uint32_t vy = by * BRICK_SIZE + y;
            uint32_t vz = bz * BRICK_SIZE + z;

            if (vx >= _voxelCountX || vy >= _voxelCountY || vz >= _voxelCountZ)
            {
                continue;
            }

            uint32_t voxelIdx = Flatten3D(vx, vy, vz, _voxelCountX, _voxelCountY);
            if (voxels[voxelIdx] == false)
            {
                continue;
            }

            hasData = true;

            uint32_t bitIdx = x + y * BRICK_SIZE + z * BRICK_SIZE * BRICK_SIZE;
            brick.mask[bitIdx >> 6] |= (1ull << (bitIdx & 63));
        }

        if (hasData)
        {
            uint32_t brickID = static_cast<uint32_t>(_bricks.size());
            _bricks.push_back(brick);

            uint32_t gridIdx = Flatten3D(bx, by, bz, _brickCountX, _brickCountY);
            _brickIndices[gridIdx] = brickID;
        }
    }
}