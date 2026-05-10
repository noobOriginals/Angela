#include <core/envmap.hpp>

#include <cmath>
#include <glm/gtc/constants.hpp>

namespace core {

// ---------------------------------------------------------------------------
// Helpers

static float32 luminance(const glm::vec3& c) {
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

static glm::vec3 pixelAt(const float32* pixels, int32 w, int32 x, int32 y) {
    const float32* p = pixels + (size_t)(y * w + x) * 3;
    return glm::vec3(p[0], p[1], p[2]);
}

// Equirectangular: u = φ/(2π), v = θ/π, θ=0 at north pole (+y axis)
static glm::vec3 uvToDir(float32 u, float32 v) {
    float32 phi   = u * 2.0f * glm::pi<float>() ;
    float32 theta = v * glm::pi<float>();
    float32 sinT  = std::sin(theta);
    return glm::vec3(sinT * std::cos(phi), std::cos(theta), sinT * std::sin(phi));
}

static glm::vec2 dirToUV(const glm::vec3& dir) {
    glm::vec3 d   = glm::normalize(dir);
    float32   phi = std::atan2(d.z, d.x);
    float32   u   = (phi + glm::pi<float>()) / (2.0f * glm::pi<float>());
    float32   v   = std::acos(glm::clamp(d.y, -1.0f, 1.0f)) / glm::pi<float>();
    return glm::vec2(u, v);
}

// Binary search: smallest index where cdf[i] >= xi
static int32 binarySearch(const float32* cdf, int32 n, float32 xi) {
    int32 lo = 0, hi = n - 1;
    while (lo < hi) {
        int32 mid = (lo + hi) / 2;
        if (cdf[mid] < xi) lo = mid + 1;
        else               hi = mid;
    }
    return lo;
}

// Analytical sky gradient
static glm::vec3 skyGradient(const glm::vec3& dir) {
    glm::vec3 unit = glm::normalize(dir);
    float32   t    = 0.5f * (unit.y + 1.0f);
    return glm::mix(glm::vec3(1.0f), glm::vec3(0.5f, 0.7f, 1.0f), t);
}

// ---------------------------------------------------------------------------
// CDF build

float32 buildEnvMapCDF(const float32* pixels, int32 width, int32 height,
                       std::vector<float32>& marginalCDF,
                       std::vector<float32>& conditionalCDF) {
    marginalCDF.resize((size_t)height);
    conditionalCDF.resize((size_t)height * (size_t)width);

    float32 totalWeight = 0.0f;

    for (int32 y = 0; y < height; ++y) {
        float32 sinTheta = std::sin((y + 0.5f) * glm::pi<float>() / (float32)height);
        float32 rowSum   = 0.0f;

        for (int32 x = 0; x < width; ++x) {
            float32 w = luminance(pixelAt(pixels, width, x, y)) * sinTheta;
            rowSum   += w;
            conditionalCDF[(size_t)y * width + x] = rowSum;
        }

        // Normalize conditional CDF for this row
        if (rowSum > 0.0f) {
            float32 invRow = 1.0f / rowSum;
            for (int32 x = 0; x < width; ++x)
                conditionalCDF[(size_t)y * width + x] *= invRow;
        }

        totalWeight         += rowSum;
        marginalCDF[y]       = totalWeight;
    }

    // Normalize marginal CDF
    if (totalWeight > 0.0f) {
        float32 inv = 1.0f / totalWeight;
        for (int32 y = 0; y < height; ++y)
            marginalCDF[y] *= inv;
    }

    // pdf_sa(dir) = lum(dir) * invNormFactor
    // invNormFactor = W*H / (2π² * totalWeight)
    float32 invNorm = (float32)(width * height)
                    / (2.0f * glm::pi<float>() * glm::pi<float>() * totalWeight);
    return invNorm;
}

// ---------------------------------------------------------------------------
// Public API

glm::vec3 sampleEnvMap(const EnvMap& env, PCG32& rng,
                       glm::vec3& outDir, float32& outPdf) {
    float32 xi1 = rng.nextFloat(), xi2 = rng.nextFloat();

    int32 y = binarySearch(env.marginalCDF, env.height, xi1);
    int32 x = binarySearch(env.conditionalCDF + (size_t)y * env.width, env.width, xi2);

    float32 u = (x + 0.5f) / (float32)env.width;
    float32 v = (y + 0.5f) / (float32)env.height;
    outDir    = uvToDir(u, v);

    glm::vec3 Le  = pixelAt(env.pixels, env.width, x, y);
    float32   lum = luminance(Le);
    outPdf        = lum * env.invNormFactor;
    if (outPdf < 1e-8f) outPdf = 1e-8f;

    return Le;
}

glm::vec3 evalEnvMap(const EnvMap& env, const glm::vec3& dir) {
    if (!env.pixels) return skyGradient(dir);

    glm::vec2 uv = dirToUV(dir);
    int32 x = glm::clamp((int32)(uv.x * (float32)env.width),  0, env.width  - 1);
    int32 y = glm::clamp((int32)(uv.y * (float32)env.height), 0, env.height - 1);
    return pixelAt(env.pixels, env.width, x, y);
}

float32 envMapPDF(const EnvMap& env, const glm::vec3& dir) {
    if (!env.pixels)
        return 1.0f / (2.0f * glm::pi<float>());  // uniform hemisphere

    glm::vec2 uv = dirToUV(dir);
    int32 x = glm::clamp((int32)(uv.x * (float32)env.width),  0, env.width  - 1);
    int32 y = glm::clamp((int32)(uv.y * (float32)env.height), 0, env.height - 1);
    return luminance(pixelAt(env.pixels, env.width, x, y)) * env.invNormFactor;
}

} // namespace core
