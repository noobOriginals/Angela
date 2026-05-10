#ifndef CORE_BVH_HPP
#define CORE_BVH_HPP

#include <vector>
#include <util/types.h>
#include <core/ray.hpp>
#include <core/hitpoint.hpp>
#include <core/object.hpp>

namespace core {

// 32-byte flat node — maps directly to a GPU buffer.
// Internal: count == 0, left/right are child node indices.
// Leaf:     count  > 0, left is the start offset in primIndices.
struct BVHNode {
    float32 aabbMin[3];
    float32 aabbMax[3];
    int32   left;
    int32   right;
    int32   count;
    int32   _pad;
};

// Build a flat BVH over objects[0..numObjects). Outputs nodes and a
// permuted primIndices array so that leaves reference contiguous ranges.
void buildBVH(const Object* objects, int32 numObjects,
              std::vector<BVHNode>& nodes,
              std::vector<int32>&   primIndices);

// Traverse the BVH; returns true and fills hr with the closest hit.
bool hitBVH(const Ray& ray, HitRecord& hr,
            float32 tMin, float32 tMax,
            const BVHNode* nodes,
            const int32*   primIndices,
            const Object*  objects);

} // namespace core

#endif // CORE_BVH_HPP
