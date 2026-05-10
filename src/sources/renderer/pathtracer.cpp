#include <renderer/pathtracer.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

#include <glm/gtc/constants.hpp>

// ---------------------------------------------------------------------------
// Shadow ray — returns true if anything blocks the path from p in direction d
// up to distance tMax.

static bool isOccluded(const glm::vec3& p, const glm::vec3& d, float32 tMax,
                       const SceneView& sv) {
    core::HitRecord shadow;
    return core::hitBVH({ p, d }, shadow, 0.001f, tMax - 0.001f,
                        sv.bvh, sv.primIndices, sv.objects);
}

// ---------------------------------------------------------------------------
// Emissive area light sampling

struct EmissiveSample {
    glm::vec3 dir;
    float32   dist;
    float32   pdf;    // solid-angle pdf
    glm::vec3 Le;
    bool      valid;
};

static EmissiveSample sampleEmissive(const glm::vec3& hitPos,
                                     const SceneView& sv, PCG32& rng) {
    EmissiveSample result{};
    if (sv.numEmissive == 0) return result;

    // Pick a random emissive object
    int32 ei = glm::clamp((int32)(rng.nextFloat() * (float32)sv.numEmissive),
                          0, sv.numEmissive - 1);
    int32 idx = sv.emissiveIds[ei];
    const core::Object& obj = sv.objects[idx];
    if (obj.type != core::TRIANGLE) return result;

    glm::vec3 A(obj.data[0], obj.data[1], obj.data[2]);
    glm::vec3 B(obj.data[3], obj.data[4], obj.data[5]);
    glm::vec3 C(obj.data[6], obj.data[7], obj.data[8]);
    glm::vec3 e1 = B - A, e2 = C - A;
    float32   area = 0.5f * glm::length(glm::cross(e1, e2));
    if (area < 1e-8f) return result;

    // Uniform barycentric sample
    float32   sqrtU = std::sqrt(rng.nextFloat());
    float32   u     = 1.0f - sqrtU;
    float32   v     = rng.nextFloat() * sqrtU;
    glm::vec3 point = (1.0f - u - v) * A + u * B + v * C;

    glm::vec3 diff = point - hitPos;
    float32   dist = glm::length(diff);
    if (dist < 1e-6f) return result;

    glm::vec3 dir      = diff / dist;
    glm::vec3 lightN   = glm::normalize(glm::cross(e1, e2));
    float32   cosLight = glm::abs(glm::dot(lightN, -dir));
    if (cosLight < 1e-6f) return result;

    const core::Material& mat = sv.materials[obj.materialIndex];
    glm::vec3 Le = glm::vec3(mat.data[0], mat.data[1], mat.data[2]) * mat.data[3];

    result.dir   = dir;
    result.dist  = dist;
    result.pdf   = dist * dist / (cosLight * area * (float32)sv.numEmissive);
    result.Le    = Le;
    result.valid = true;
    return result;
}

// Solid-angle pdf for the BRDF path arriving at an emissive triangle.
static float32 emissivePDF(const core::HitRecord& hr, const core::Ray& ray,
                           const SceneView& sv) {
    if (sv.numEmissive == 0 || hr.objectIndex < 0) return 0.0f;
    const core::Object& obj = sv.objects[hr.objectIndex];
    if (obj.type != core::TRIANGLE) return 0.0f;

    glm::vec3 e1(obj.data[3] - obj.data[0],
                 obj.data[4] - obj.data[1],
                 obj.data[5] - obj.data[2]);
    glm::vec3 e2(obj.data[6] - obj.data[0],
                 obj.data[7] - obj.data[1],
                 obj.data[8] - obj.data[2]);
    float32 area = 0.5f * glm::length(glm::cross(e1, e2));
    if (area < 1e-8f) return 0.0f;

    float32 cosLight = glm::abs(glm::dot(hr.n, -glm::normalize(ray.dir)));
    if (cosLight < 1e-6f) return 0.0f;

    return (hr.t * hr.t) / (cosLight * area * (float32)sv.numEmissive);
}

// ---------------------------------------------------------------------------
// Path tracer with NEE + MIS

glm::vec3 traceRay(const core::Ray& ray, const SceneView& sv,
                   PCG32& rng, int32 maxDepth) {
    core::Ray current    = ray;
    glm::vec3 throughput = glm::vec3(1.0f);
    glm::vec3 accum      = glm::vec3(0.0f);

    // brdfPdf from the previous scatter, needed to compute MIS weight when the
    // BRDF path hits an environment or emissive.  Initialised to 0 so that the
    // very first bounce (camera ray) always adds env/emissive contribution in
    // full (specularBounce == true handles this too, but belt-and-suspenders).
    float32 brdfPdf       = 0.0f;
    bool    specularBounce = true;  // camera ray or previous bounce was specular

    for (int32 depth = 0; depth < maxDepth; ++depth) {
        core::HitRecord hr;
        if (!core::hitBVH(current, hr, 0.001f, 1e30f, sv.bvh, sv.primIndices, sv.objects)) {
            // Missed all geometry — environment contribution
            glm::vec3 Le = core::evalEnvMap(*sv.envMap, current.dir);
            if (specularBounce) {
                accum += throughput * Le;
            } else {
                float32 ePdf = core::envMapPDF(*sv.envMap, current.dir);
                accum += throughput * Le * powerHeuristic(brdfPdf, ePdf);
            }
            break;
        }

        const core::Material& mat = sv.materials[hr.materialIndex];

        // Emissive surface — add contribution and terminate
        if (mat.type == core::EMISSIVE) {
            glm::vec3 Le = glm::vec3(mat.data[0], mat.data[1], mat.data[2]) * mat.data[3];
            if (specularBounce) {
                accum += throughput * Le;
            } else {
                float32 ePdf = emissivePDF(hr, current, sv);
                accum += throughput * Le * powerHeuristic(brdfPdf, ePdf);
            }
            break;
        }

        // ---------------------------------------------------------------
        // NEE — only for non-specular (diffuse) surfaces
        if (mat.type == core::DIFFUSE) {
            glm::vec3 albedo = core::getMaterialAlbedo(mat, hr, sv.textures);

            // --- NEE: environment map / analytical sky ---
            {
                glm::vec3 lightDir; float32 lPdf; glm::vec3 Le;

                if (sv.envMap->pixels) {
                    // Importance-sampled env map
                    Le = core::sampleEnvMap(*sv.envMap, rng, lightDir, lPdf);
                } else {
                    // Analytical sky: sample uniform hemisphere
                    lightDir = rng.nextUnitHemisphere(hr.n);
                    lPdf     = 1.0f / (2.0f * glm::pi<float>());
                    Le       = core::evalEnvMap(*sv.envMap, lightDir);
                }

                float32 cosL = glm::max(0.0f, glm::dot(hr.n, lightDir));
                if (lPdf > 0.0f && cosL > 0.0f &&
                    !isOccluded(hr.p, lightDir, 1e30f, sv)) {
                    float32 bPdf = cosL / glm::pi<float>();
                    float32 w    = powerHeuristic(lPdf, bPdf);
                    // f = albedo/π, contribution = Le * f * cosL / lPdf * w
                    accum += throughput * Le * albedo * (cosL / (glm::pi<float>() * lPdf)) * w;
                }
            }

            // --- NEE: emissive area lights ---
            if (sv.numEmissive > 0) {
                EmissiveSample es = sampleEmissive(hr.p, sv, rng);
                if (es.valid) {
                    float32 cosL = glm::max(0.0f, glm::dot(hr.n, es.dir));
                    if (cosL > 0.0f && !isOccluded(hr.p, es.dir, es.dist, sv)) {
                        float32 bPdf = cosL / glm::pi<float>();
                        float32 w    = powerHeuristic(es.pdf, bPdf);
                        accum += throughput * es.Le * albedo
                               * (cosL / (glm::pi<float>() * es.pdf)) * w;
                    }
                }
            }
        }

        // ---------------------------------------------------------------
        // BRDF scatter — continue path
        core::ScatterResult sr = core::scatter(current, hr, mat, rng, sv.textures);
        if (!sr.scattered) break;

        specularBounce = (mat.type == core::METAL || mat.type == core::DIELECTRIC);
        brdfPdf        = sr.pdf;
        throughput    *= sr.attenuation;
        current        = sr.ray;

        // Russian roulette
        if (depth >= 3) {
            float32 p = glm::clamp(glm::max(throughput.r,
                                   glm::max(throughput.g, throughput.b)),
                                   0.0f, 0.95f);
            if (rng.nextFloat() >= p) break;
            throughput /= p;
        }
    }

    return accum;
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

    SceneView sv;
    sv.objects     = scene.objects.data();
    sv.materials   = scene.materials.data();
    sv.textures    = scene.textures.empty() ? nullptr : scene.textures.data();
    sv.bvh         = scene.bvhNodes.data();
    sv.primIndices = scene.primIndices.data();
    sv.envMap      = &scene.envMap;
    sv.emissiveIds = scene.emissiveIds.empty() ? nullptr : scene.emissiveIds.data();
    sv.numEmissive = (int32)scene.emissiveIds.size();

    for (int32 y = y0; y < y1; ++y) {
        for (int32 x = x0; x < x1; ++x) {
            uint64 seed = (uint64)y * (uint64)cfg.width + (uint64)x;
            PCG32  rng(seed, 0ULL);

            for (int32 s = 0; s < cfg.spp; ++s) {
                float32   u   = (x + rng.nextFloat()) * invW;
                float32   v   = (y + rng.nextFloat()) * invH;
                core::Ray r   = core::generateRay(cam, u, v, rng);
                glm::vec3 col = traceRay(r, sv, rng, cfg.maxDepth);
                out.accumulate(x, y, col);
            }
        }
    }
}

void CPUPathTracer::render(const core::Scene& scene, const core::Camera& cam,
                           const RenderConfig& cfg, Image& out) {
    int32 tilesX     = (cfg.width  + cfg.tileSize - 1) / cfg.tileSize;
    int32 tilesY     = (cfg.height + cfg.tileSize - 1) / cfg.tileSize;
    int32 totalTiles = tilesX * tilesY;

    std::queue<std::pair<int32, int32>> queue;
    for (int32 ty = 0; ty < tilesY; ++ty)
        for (int32 tx = 0; tx < tilesX; ++tx)
            queue.push({ tx, ty });

    std::mutex         queueMx;
    std::atomic<int32> done{0};
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
