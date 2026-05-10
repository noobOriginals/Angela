#include <renderer/pathtracer.hpp>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

// ---------------------------------------------------------------------------
// Sky

static glm::vec3 skyColor(const core::Ray& ray) {
    glm::vec3 unit = glm::normalize(ray.dir);
    float32   t    = 0.5f * (unit.y + 1.0f);
    return glm::mix(glm::vec3(1.0f), glm::vec3(0.5f, 0.7f, 1.0f), t);
}

// ---------------------------------------------------------------------------
// traceRay — GPU-portable core

glm::vec3 traceRay(const core::Ray& ray,
                   const core::Object*   objects,
                   const core::Material* materials,
                   const core::Texture*  textures,
                   const core::BVHNode*  bvh,
                   const int32*          primIndices,
                   PCG32& rng, int32 maxDepth) {
    core::Ray current    = ray;
    glm::vec3 throughput = glm::vec3(1.0f);

    for (int32 depth = 0; depth < maxDepth; ++depth) {
        core::HitRecord hr;
        if (!core::hitBVH(current, hr, 0.001f, 1e30f, bvh, primIndices, objects))
            return throughput * skyColor(current);

        const core::Material& mat = materials[hr.materialIndex];

        if (mat.type == core::EMISSIVE) {
            glm::vec3 emit(mat.data[0], mat.data[1], mat.data[2]);
            return throughput * emit * mat.data[3];
        }

        core::ScatterResult sr = core::scatter(current, hr, mat, rng, textures);
        if (!sr.scattered)
            return glm::vec3(0.0f);

        throughput *= sr.attenuation;
        current     = sr.ray;

        // Russian roulette — stochastically terminate low-energy rays
        if (depth >= 3) {
            float32 pSurvive = glm::max(throughput.r, glm::max(throughput.g, throughput.b));
            pSurvive = glm::clamp(pSurvive, 0.0f, 0.95f);
            if (rng.nextFloat() >= pSurvive)
                return glm::vec3(0.0f);
            throughput /= pSurvive;
        }
    }

    return glm::vec3(0.0f);
}

// ---------------------------------------------------------------------------
// CPUPathTracer

void CPUPathTracer::renderTile(const core::Scene& scene, const core::Camera& cam,
                               const RenderConfig& cfg, Image& out,
                               int32 tileX, int32 tileY) {
    int32 x0 = tileX * cfg.tileSize;
    int32 y0 = tileY * cfg.tileSize;
    int32 x1 = std::min(x0 + cfg.tileSize, cfg.width);
    int32 y1 = std::min(y0 + cfg.tileSize, cfg.height);

    float32 invW = 1.0f / (float32)(cfg.width  - 1);
    float32 invH = 1.0f / (float32)(cfg.height - 1);

    const core::Object*   objects     = scene.objects.data();
    const core::Material* materials   = scene.materials.data();
    const core::Texture*  textures    = scene.textures.empty() ? nullptr : scene.textures.data();
    const core::BVHNode*  bvh         = scene.bvhNodes.data();
    const int32*          primIndices = scene.primIndices.data();

    for (int32 y = y0; y < y1; ++y) {
        for (int32 x = x0; x < x1; ++x) {
            uint64 seed = (uint64)y * (uint64)cfg.width + (uint64)x;
            PCG32  rng(seed, 0ULL);

            for (int32 s = 0; s < cfg.spp; ++s) {
                float32   u   = (x + rng.nextFloat()) * invW;
                float32   v   = (y + rng.nextFloat()) * invH;
                core::Ray r   = core::generateRay(cam, u, v, rng);
                glm::vec3 col = traceRay(r, objects, materials, textures,
                                         bvh, primIndices, rng, cfg.maxDepth);
                out.accumulate(x, y, col);
            }
        }
    }
}

void CPUPathTracer::render(const core::Scene& scene, const core::Camera& cam,
                           const RenderConfig& cfg, Image& out) {
    int32 tilesX = (cfg.width  + cfg.tileSize - 1) / cfg.tileSize;
    int32 tilesY = (cfg.height + cfg.tileSize - 1) / cfg.tileSize;
    int32 totalTiles = tilesX * tilesY;

    std::queue<std::pair<int32, int32>> queue;
    for (int32 ty = 0; ty < tilesY; ++ty)
        for (int32 tx = 0; tx < tilesX; ++tx)
            queue.push({ tx, ty });

    std::mutex            queueMx;
    std::atomic<int32>    done{0};
    int32 reportEvery = std::max(1, totalTiles / 20);

    int32 numThreads = cfg.numThreads > 0
        ? cfg.numThreads
        : (int32)std::thread::hardware_concurrency();

    auto worker = [&]() {
        while (true) {
            std::pair<int32, int32> tile;
            {
                std::lock_guard<std::mutex> lock(queueMx);
                if (queue.empty()) return;
                tile = queue.front();
                queue.pop();
            }
            renderTile(scene, cam, cfg, out, tile.first, tile.second);

            int32 n = ++done;
            if (n % reportEvery == 0 || n == totalTiles)
                std::cout << "\r  " << n << "/" << totalTiles << " tiles" << std::flush;
        }
    };

    std::vector<std::thread> threads;
    threads.reserve((size_t)numThreads);
    for (int32 i = 0; i < numThreads; ++i)
        threads.emplace_back(worker);
    for (auto& t : threads)
        t.join();

    std::cout << "\n";
}
