#ifndef CORE_MATERIAL_HPP
#define CORE_MATERIAL_HPP

#include <glm/glm.hpp>
#include <util/types.h>
#include <util/random.hpp>
#include <core/ray.hpp>
#include <core/hitpoint.hpp>
#include <core/texture.hpp>

namespace core {

constexpr uint32 MATERIAL_DATA_SIZE = 4;

enum MaterialType : int32 {
    DIFFUSE,
    METAL,
    DIELECTRIC,
    EMISSIVE
};

struct Material {
    int32   type;
    float32 data[MATERIAL_DATA_SIZE];   // [0..2] = albedo rgb, [3] = param (fuzz / ior / intensity)
    int32   albedoTexture;              // -1 = use data[0..2], >=0 = index into Scene::textures
};

void setDiffuse   (Material& m, glm::vec3 albedo);
void setMetal     (Material& m, glm::vec3 albedo, float32 fuzz);
void setDielectric(Material& m, float32 ior);
void setEmissive  (Material& m, glm::vec3 color, float32 intensity);

struct ScatterResult {
    Ray       ray;
    glm::vec3 attenuation;
    bool      scattered;
};

ScatterResult scatter(const Ray&, const HitRecord&, const Material&, PCG32& rng,
                      const Texture* textures = nullptr);

} // namespace core

#endif // CORE_MATERIAL_HPP
