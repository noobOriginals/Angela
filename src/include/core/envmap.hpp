#ifndef CORE_ENVMAP_HPP
#define CORE_ENVMAP_HPP

#include <vector>
#include <util/types.h>
#include <util/random.hpp>
#include <glm/glm.hpp>

namespace core {

// GPU-portable representation of an environment map.
// pixels == nullptr → analytical sky gradient is used.
// CDF arrays are owned by Scene; this struct holds non-owning pointers.
struct EnvMap {
    int32          width, height;
    const float32* pixels;           // float32 RGB HDR pixels; nullptr = analytical sky
    const float32* marginalCDF;      // [height]       cumulative row probabilities
    const float32* conditionalCDF;   // [height*width] cumulative column probabilities per row
    float32        invNormFactor;    // pdf_sa(dir) = lum(dir) * invNormFactor
};

// Build the 2D CDF tables for an HDR environment map.
// Fills marginalCDF and conditionalCDF and returns invNormFactor.
float32 buildEnvMapCDF(const float32* pixels, int32 width, int32 height,
                       std::vector<float32>& marginalCDF,
                       std::vector<float32>& conditionalCDF);

// Sample a direction from the environment map using CDF importance sampling.
// Returns the radiance in that direction; outPdf is the solid-angle pdf.
// Only valid when env.pixels != nullptr.
glm::vec3 sampleEnvMap(const EnvMap& env, PCG32& rng,
                       glm::vec3& outDir, float32& outPdf);

// Evaluate radiance for a given direction.
// Falls back to the analytical sky gradient when env.pixels == nullptr.
glm::vec3 evalEnvMap(const EnvMap& env, const glm::vec3& dir);

// Evaluate the solid-angle sampling pdf for a given direction.
// For analytical sky (pixels==nullptr) returns 1/(2π) (uniform hemisphere).
float32 envMapPDF(const EnvMap& env, const glm::vec3& dir);

} // namespace core

#endif // CORE_ENVMAP_HPP
