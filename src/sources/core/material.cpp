#include <core/material.hpp>

#include <cmath>
#include <glm/gtc/constants.hpp>

namespace core {

void setDiffuse(Material& m, glm::vec3 albedo) {
    m.type           = DIFFUSE;
    m.data[0]        = albedo.r; m.data[1] = albedo.g; m.data[2] = albedo.b;
    m.data[3]        = 0.0f;
    m.albedoTexture  = -1;
}

void setMetal(Material& m, glm::vec3 albedo, float32 fuzz) {
    m.type           = METAL;
    m.data[0]        = albedo.r; m.data[1] = albedo.g; m.data[2] = albedo.b;
    m.data[3]        = fuzz;
    m.albedoTexture  = -1;
}

void setDielectric(Material& m, float32 ior) {
    m.type           = DIELECTRIC;
    m.data[0]        = 1.0f; m.data[1] = 1.0f; m.data[2] = 1.0f;
    m.data[3]        = ior;
    m.albedoTexture  = -1;
}

void setEmissive(Material& m, glm::vec3 color, float32 intensity) {
    m.type           = EMISSIVE;
    m.data[0]        = color.r; m.data[1] = color.g; m.data[2] = color.b;
    m.data[3]        = intensity;
    m.albedoTexture  = -1;
}

glm::vec3 getMaterialAlbedo(const Material& mat, const HitRecord& hr,
                             const Texture* textures) {
    if (textures && mat.albedoTexture >= 0)
        return sampleTexture(textures[mat.albedoTexture], hr.uv);
    return glm::vec3(mat.data[0], mat.data[1], mat.data[2]);
}

float32 scatterPDF(const HitRecord& hr, const Material& mat, const glm::vec3& scatterDir) {
    if (mat.type == DIFFUSE) {
        float32 cosTheta = glm::max(0.0f, glm::dot(hr.n, glm::normalize(scatterDir)));
        return cosTheta / glm::pi<float32>();
    }
    return 0.0f;  // delta-function BSDFs (metal, dielectric)
}

static ScatterResult scatterDiffuse(const HitRecord& hr, glm::vec3 albedo, PCG32& rng) {
    glm::vec3 dir = hr.n + rng.nextUnitVector();
    if (glm::dot(dir, dir) < 1e-8f) dir = hr.n;
    glm::vec3 normDir  = glm::normalize(dir);
    float32   cosTheta = glm::max(0.0f, glm::dot(hr.n, normDir));
    float32   pdf      = cosTheta / glm::pi<float32>();
    return { Ray(hr.p, normDir), albedo, pdf, true };
}

static ScatterResult scatterMetal(const Ray& ray, const HitRecord& hr,
                                  glm::vec3 albedo, float32 fuzz, PCG32& rng) {
    glm::vec3 reflected = glm::reflect(glm::normalize(ray.dir), hr.n);
    glm::vec3 dir       = reflected + fuzz * rng.nextUnitSphere();
    bool      scattered = glm::dot(dir, hr.n) > 0.0f;
    return { Ray(hr.p, glm::normalize(dir)), albedo, 0.0f, scattered };
}

static float32 schlick(float32 cosine, float32 ratio) {
    float32 r0 = (1.0f - ratio) / (1.0f + ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * std::pow(1.0f - cosine, 5.0f);
}

static ScatterResult scatterDielectric(const Ray& ray, const HitRecord& hr,
                                       float32 ior, PCG32& rng) {
    float32   ratio    = hr.frontFace ? (1.0f / ior) : ior;
    glm::vec3 unit     = glm::normalize(ray.dir);
    float32   cosTheta = glm::min(glm::dot(-unit, hr.n), 1.0f);
    float32   sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

    glm::vec3 dir;
    if (ratio * sinTheta > 1.0f || schlick(cosTheta, ratio) > rng.nextFloat())
        dir = glm::reflect(unit, hr.n);
    else
        dir = glm::refract(unit, hr.n, ratio);

    return { Ray(hr.p, dir), glm::vec3(1.0f), 0.0f, true };
}

ScatterResult scatter(const Ray& ray, const HitRecord& hr, const Material& mat,
                      PCG32& rng, const Texture* textures) {
    glm::vec3 albedo = getMaterialAlbedo(mat, hr, textures);

    switch (mat.type) {
    case DIFFUSE:    return scatterDiffuse   (hr, albedo, rng);
    case METAL:      return scatterMetal     (ray, hr, albedo, mat.data[3], rng);
    case DIELECTRIC: return scatterDielectric(ray, hr, mat.data[3], rng);
    default:         return { ray, glm::vec3(0.0f), 0.0f, false };
    }
}

} // namespace core
