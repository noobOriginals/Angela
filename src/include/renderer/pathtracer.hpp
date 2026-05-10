#ifndef RENDERER_PATHTRACER_HPP
#define RENDERER_PATHTRACER_HPP

#include <glm/glm.hpp>
#include <util/types.h>
#include <util/random.hpp>
#include <core/object.hpp>
#include <core/material.hpp>
#include <core/texture.hpp>
#include <core/bvh.hpp>
#include <core/envmap.hpp>
#include <core/scene.hpp>
#include <core/camera.hpp>
#include <lib/image.hpp>

// Bundles the raw-pointer scene data passed into traceRay.
// GPU-portable: only POD types and raw pointers, no STL.
struct SceneView {
    const core::Object*   objects;
    const core::Material* materials;
    const core::Texture*  textures;
    const core::BVHNode*  bvh;
    const int32*          primIndices;
    const core::EnvMap*   envMap;      // always valid; pixels==nullptr → analytical sky
    const int32*          emissiveIds;
    int32                 numEmissive;
};

// MIS power heuristic (β=2): p² / (p² + q²)
inline float32 powerHeuristic(float32 p, float32 q) {
    p *= p; q *= q;
    return (p + q > 0.0f) ? p / (p + q) : 0.0f;
}

// GPU-portable path tracer core — no STL, no heap allocation.
glm::vec3 traceRay(const core::Ray& ray, const SceneView& sv,
                   PCG32& rng, int32 maxDepth);

struct RenderConfig {
    int32 width, height, spp, maxDepth;
    int32 tileSize   = 32;
    int32 numThreads = 0;    // 0 → std::thread::hardware_concurrency()
};

class CPUPathTracer {
public:
    void render(const core::Scene&, const core::Camera&,
                const RenderConfig&, Image& out);

private:
    void renderTile(const core::Scene&, const core::Camera&,
                    const RenderConfig&, Image&, int32 tileX, int32 tileY);
};

#endif // RENDERER_PATHTRACER_HPP
