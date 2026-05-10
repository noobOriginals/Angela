#include <renderer/pathtracer.hpp>

#include <algorithm>
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
                   const core::Object* objects, int32 numObjects,
                   const core::Material* materials,
                   PCG32& rng, int32 maxDepth) {
    core::Ray current    = ray;
    glm::vec3 throughput = glm::vec3(1.0f);

    for (int32 depth = 0; depth < maxDepth; ++depth) {
        core::HitRecord hr;
        bool    hitAny  = false;
        float32 closest = 1e30f;

        for (int32 i = 0; i < numObjects; ++i) {
            core::HitRecord temp;
            if (core::hitObject(current, temp, 0.001f, closest, objects[i])) {
                hitAny  = true;
                closest = temp.t;
                hr      = temp;
            }
        }

        if (!hitAny)
            return throughput * skyColor(current);

        const core::Material& mat = materials[hr.materialIndex];

        if (mat.type == core::EMISSIVE) {
            glm::vec3 emit(mat.data[0], mat.data[1], mat.data[2]);
            return throughput * emit * mat.data[3];
        }

        core::ScatterResult sr = core::scatter(current, hr, mat, rng);
        if (!sr.scattered)
            return glm::vec3(0.0f);

        throughput *= sr.attenuation;
        current     = sr.ray;
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

    for (int32 y = y0; y < y1; ++y) {
        for (int32 x = x0; x < x1; ++x) {
            // Seed from pixel coordinates for reproducibility
            uint64 seed = (uint64)y * (uint64)cfg.width + (uint64)x;
            PCG32  rng(seed, 0ULL);

            for (int32 s = 0; s < cfg.spp; ++s) {
                float32    u   = (x + rng.nextFloat()) * invW;
                float32    v   = (y + rng.nextFloat()) * invH;
                core::Ray  r   = core::generateRay(cam, u, v, rng);
                glm::vec3  col = traceRay(r,
                    scene.objects.data(),   (int32)scene.objects.size(),
                    scene.materials.data(),
                    rng, cfg.maxDepth);
                out.accumulate(x, y, col);
            }
        }
    }
}

void CPUPathTracer::render(const core::Scene& scene, const core::Camera& cam,
                           const RenderConfig& cfg, Image& out) {
    int32 tilesX = (cfg.width  + cfg.tileSize - 1) / cfg.tileSize;
    int32 tilesY = (cfg.height + cfg.tileSize - 1) / cfg.tileSize;

    std::queue<std::pair<int32, int32>> queue;
    for (int32 ty = 0; ty < tilesY; ++ty)
        for (int32 tx = 0; tx < tilesX; ++tx)
            queue.push({ tx, ty });

    std::mutex mx;
    int32 numThreads = cfg.numThreads > 0
        ? cfg.numThreads
        : (int32)std::thread::hardware_concurrency();

    auto worker = [&]() {
        while (true) {
            std::pair<int32, int32> tile;
            {
                std::lock_guard<std::mutex> lock(mx);
                if (queue.empty()) return;
                tile = queue.front();
                queue.pop();
            }
            renderTile(scene, cam, cfg, out, tile.first, tile.second);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve((size_t)numThreads);
    for (int32 i = 0; i < numThreads; ++i)
        threads.emplace_back(worker);
    for (auto& t : threads)
        t.join();
}
