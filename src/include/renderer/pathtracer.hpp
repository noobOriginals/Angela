#ifndef RENDERER_PATHTRACER_HPP
#define RENDERER_PATHTRACER_HPP

#include <glm/glm.hpp>
#include <util/types.h>
#include <util/random.hpp>
#include <core/object.hpp>
#include <core/material.hpp>
#include <core/texture.hpp>
#include <core/bvh.hpp>
#include <core/scene.hpp>
#include <core/camera.hpp>
#include <lib/image.hpp>

// GPU-portable core — raw pointers, no STL containers
glm::vec3 traceRay(const core::Ray& ray,
                   const core::Object*   objects,
                   const core::Material* materials,
                   const core::Texture*  textures,
                   const core::BVHNode*  bvh,
                   const int32*          primIndices,
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
