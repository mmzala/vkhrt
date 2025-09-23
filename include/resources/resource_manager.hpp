#pragma once

#include <memory>
#include <vector>

constexpr uint32_t NULL_RESOURCE_INDEX_VALUE = 0xFFFF;

template<typename T>
struct ResourceHandle
{
    bool operator==(const ResourceHandle<T>& other) const { return handle == other.handle; }
    static ResourceHandle<T> Null() { return ResourceHandle<T> {}; }
    [[nodiscard]] bool IsNull() const { return handle == NULL_RESOURCE_INDEX_VALUE; }

    uint32_t handle = NULL_RESOURCE_INDEX_VALUE;
};

template<typename T>
class ResourceManager
{
public:
    ResourceManager() = default;
    const T& Get(ResourceHandle<T> handle) const { return _resources[handle.handle]; }
    const std::vector<T>& GetAll() const { return _resources; }

protected:
    ResourceHandle<T> Create(T&& resource)
    {
        uint32_t index = _resources.size();
        _resources.push_back(std::move(resource));
        return ResourceHandle<T>{ index };
    }

private:
    std::vector<T> _resources {};
};
