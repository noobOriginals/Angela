#include <core/bvh.hpp>

#include <algorithm>
#include <numeric>

namespace core {

// ---------------------------------------------------------------------------
// AABB — internal helper

struct AABB {
    float32 mn[3], mx[3];

    static AABB makeEmpty() {
        AABB a;
        a.mn[0] = a.mn[1] = a.mn[2] =  1e30f;
        a.mx[0] = a.mx[1] = a.mx[2] = -1e30f;
        return a;
    }

    void expand(const AABB& b) {
        for (int i = 0; i < 3; ++i) {
            if (b.mn[i] < mn[i]) mn[i] = b.mn[i];
            if (b.mx[i] > mx[i]) mx[i] = b.mx[i];
        }
    }

    void expandPoint(float32 x, float32 y, float32 z) {
        if (x < mn[0]) mn[0] = x; if (x > mx[0]) mx[0] = x;
        if (y < mn[1]) mn[1] = y; if (y > mx[1]) mx[1] = y;
        if (z < mn[2]) mn[2] = z; if (z > mx[2]) mx[2] = z;
    }

    float32 surfaceArea() const {
        float32 dx = mx[0] - mn[0];
        float32 dy = mx[1] - mn[1];
        float32 dz = mx[2] - mn[2];
        if (dx < 0.0f || dy < 0.0f || dz < 0.0f) return 0.0f;
        return 2.0f * (dx*dy + dy*dz + dz*dx);
    }

    glm::vec3 centroid() const {
        return glm::vec3(0.5f*(mn[0]+mx[0]),
                         0.5f*(mn[1]+mx[1]),
                         0.5f*(mn[2]+mx[2]));
    }
};

// ---------------------------------------------------------------------------
// Per-object AABB

static AABB objectAABB(const Object& obj) {
    AABB box = AABB::makeEmpty();
    constexpr float32 EPS = 1e-4f;

    switch (obj.type) {
    case SPHERE: {
        float32 r = obj.data[3] + EPS;
        box.mn[0] = obj.data[0] - r;  box.mx[0] = obj.data[0] + r;
        box.mn[1] = obj.data[1] - r;  box.mx[1] = obj.data[1] + r;
        box.mn[2] = obj.data[2] - r;  box.mx[2] = obj.data[2] + r;
        break;
    }
    case TRIANGLE: {
        for (int v = 0; v < 3; ++v)
            box.expandPoint(obj.data[v*3], obj.data[v*3+1], obj.data[v*3+2]);
        for (int i = 0; i < 3; ++i) { box.mn[i] -= EPS; box.mx[i] += EPS; }
        break;
    }
    case QUAD: {
        // Four corners: center ± 0.5*u ± 0.5*v
        glm::vec3 c(obj.data[0], obj.data[1], obj.data[2]);
        glm::vec3 u(obj.data[3], obj.data[4], obj.data[5]);
        glm::vec3 v(obj.data[6], obj.data[7], obj.data[8]);
        for (int su = -1; su <= 1; su += 2)
            for (int sv = -1; sv <= 1; sv += 2) {
                glm::vec3 corner = c + 0.5f*(float32)su*u + 0.5f*(float32)sv*v;
                box.expandPoint(corner.x, corner.y, corner.z);
            }
        for (int i = 0; i < 3; ++i) { box.mn[i] -= EPS; box.mx[i] += EPS; }
        break;
    }
    default: break;
    }
    return box;
}

// ---------------------------------------------------------------------------
// SAH binned build

constexpr int32 NUM_BINS = 8;
constexpr int32 LEAF_MAX = 4;

static int32 buildNode(std::vector<BVHNode>& nodes,
                       std::vector<int32>&   primIndices,
                       const Object*         objects,
                       int32 start, int32 count) {
    int32 nodeIdx = (int32)nodes.size();
    nodes.push_back({});

    // Compute bounds and centroid bounds for the range
    AABB bounds    = AABB::makeEmpty();
    AABB centroids = AABB::makeEmpty();
    for (int32 i = start; i < start + count; ++i) {
        AABB b = objectAABB(objects[primIndices[i]]);
        bounds.expand(b);
        glm::vec3 cen = b.centroid();
        centroids.expandPoint(cen.x, cen.y, cen.z);
    }

    for (int i = 0; i < 3; ++i) {
        nodes[nodeIdx].aabbMin[i] = bounds.mn[i];
        nodes[nodeIdx].aabbMax[i] = bounds.mx[i];
    }
    nodes[nodeIdx]._pad = 0;

    if (count <= LEAF_MAX) {
        nodes[nodeIdx].left  = start;
        nodes[nodeIdx].right = 0;
        nodes[nodeIdx].count = count;
        return nodeIdx;
    }

    // SAH: try all 3 axes with NUM_BINS bins each
    int32   bestAxis = -1;
    int32   bestBin  = -1;
    float32 bestCost = 1e30f;

    for (int32 axis = 0; axis < 3; ++axis) {
        float32 axisLen = centroids.mx[axis] - centroids.mn[axis];
        if (axisLen < 1e-6f) continue;

        struct Bin { AABB box; int32 cnt; };
        Bin bins[NUM_BINS];
        for (int b = 0; b < NUM_BINS; ++b) { bins[b].box = AABB::makeEmpty(); bins[b].cnt = 0; }

        float32 scale = (float32)NUM_BINS / axisLen;
        for (int32 i = start; i < start + count; ++i) {
            AABB      b   = objectAABB(objects[primIndices[i]]);
            glm::vec3 cen = b.centroid();
            int32 binIdx = (int32)((cen[axis] - centroids.mn[axis]) * scale);
            if (binIdx < 0)         binIdx = 0;
            if (binIdx >= NUM_BINS) binIdx = NUM_BINS - 1;
            bins[binIdx].box.expand(b);
            bins[binIdx].cnt++;
        }

        // Left prefix sweep
        float32 leftSA[NUM_BINS - 1]; int32 leftN[NUM_BINS - 1];
        AABB leftBox = AABB::makeEmpty(); int32 leftCnt = 0;
        for (int32 b = 0; b < NUM_BINS - 1; ++b) {
            leftBox.expand(bins[b].box); leftCnt += bins[b].cnt;
            leftSA[b] = leftBox.surfaceArea(); leftN[b] = leftCnt;
        }

        // Right suffix sweep
        AABB rightBox = AABB::makeEmpty(); int32 rightCnt = 0;
        for (int32 b = NUM_BINS - 1; b > 0; --b) {
            rightBox.expand(bins[b].box); rightCnt += bins[b].cnt;
            float32 cost = leftSA[b-1] * (float32)leftN[b-1]
                         + rightBox.surfaceArea() * (float32)rightCnt;
            if (cost < bestCost) {
                bestCost = cost; bestAxis = axis; bestBin = b;
            }
        }
    }

    // Partition
    int32 mid;
    if (bestAxis < 0) {
        // All centroids coincide on every axis — can't split meaningfully
        nodes[nodeIdx].left  = start;
        nodes[nodeIdx].right = 0;
        nodes[nodeIdx].count = count;
        return nodeIdx;
    }

    float32 axisLen  = centroids.mx[bestAxis] - centroids.mn[bestAxis];
    float32 splitPos = centroids.mn[bestAxis] + axisLen * (float32)bestBin / (float32)NUM_BINS;

    auto it = std::partition(
        primIndices.begin() + start,
        primIndices.begin() + start + count,
        [&](int32 idx) {
            AABB b = objectAABB(objects[idx]);
            return b.centroid()[bestAxis] < splitPos;
        });
    mid = (int32)(it - primIndices.begin());

    // Guard against degenerate splits
    if (mid == start || mid == start + count)
        mid = start + count / 2;

    nodes[nodeIdx].count = 0;  // mark as internal before recursing
    int32 leftIdx  = buildNode(nodes, primIndices, objects, start, mid - start);
    int32 rightIdx = buildNode(nodes, primIndices, objects, mid,   start + count - mid);
    // nodes may have reallocated — re-index through nodeIdx
    nodes[nodeIdx].left  = leftIdx;
    nodes[nodeIdx].right = rightIdx;
    return nodeIdx;
}

void buildBVH(const Object* objects, int32 numObjects,
              std::vector<BVHNode>& nodes,
              std::vector<int32>&   primIndices) {
    nodes.clear();
    primIndices.resize((size_t)numObjects);
    std::iota(primIndices.begin(), primIndices.end(), 0);
    if (numObjects == 0) return;
    nodes.reserve((size_t)numObjects * 2);
    buildNode(nodes, primIndices, objects, 0, numObjects);
}

// ---------------------------------------------------------------------------
// AABB ray test — slab method

static bool hitAABB(const Ray& ray,
                    const float32 mn[3], const float32 mx[3],
                    float32 tMin, float32 tMax) {
    for (int i = 0; i < 3; ++i) {
        float32 invD = 1.0f / ray.dir[i];
        float32 t0   = (mn[i] - ray.org[i]) * invD;
        float32 t1   = (mx[i] - ray.org[i]) * invD;
        if (invD < 0.0f) { float32 tmp = t0; t0 = t1; t1 = tmp; }
        if (t0 > tMin) tMin = t0;
        if (t1 < tMax) tMax = t1;
        if (tMax <= tMin) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Iterative BVH traversal

bool hitBVH(const Ray& ray, HitRecord& hr,
            float32 tMin, float32 tMax,
            const BVHNode* nodes,
            const int32*   primIndices,
            const Object*  objects) {
    bool      hitAny  = false;
    float32   closest = tMax;
    HitRecord temp;

    constexpr int32 STACK_SIZE = 64;
    int32 stack[STACK_SIZE];
    int32 top = 0;
    stack[top++] = 0;

    while (top > 0) {
        const BVHNode& node = nodes[stack[--top]];

        if (!hitAABB(ray, node.aabbMin, node.aabbMax, tMin, closest))
            continue;

        if (node.count > 0) {
            for (int32 i = 0; i < node.count; ++i) {
                int32 idx = primIndices[node.left + i];
                if (hitObject(ray, temp, tMin, closest, objects[idx])) {
                    hitAny          = true;
                    closest         = temp.t;
                    hr              = temp;
                    hr.objectIndex  = idx;
                }
            }
        } else {
            // Push right first so left is popped (tested) first
            stack[top++] = node.right;
            stack[top++] = node.left;
        }
    }
    return hitAny;
}

} // namespace core
