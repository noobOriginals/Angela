#ifndef CORE_HITTABLE_HPP
#define CORE_HITTABLE_HPP

// Std includes
#include <string>

// Lib includes
#include <glm/glm.hpp>

// Local includes
#include <util/types.h>
#include <core/ray.hpp>
#include <core/hitpoint.hpp>

namespace core {

constexpr uint64 OBJECT_DATA_SIZE = 9;
constexpr uint64 MATERIAL_DATA_SIZE = 4;

// Material

enum MaterialType {
    DIFFUSE,
    METAL,
    DIELECTRIC,
    EMMISIVE // TODO
};

struct Material {
    MaterialType type;
    glm::vec3 albedo;
    float32 param;

    Material() = default;
    Material(MaterialType type, const glm::vec3& albedo, float32 param);
};

struct ScatterResult {
    Ray ray;
    glm::vec3 albedo;
    bool scattered;

    ScatterResult() = default;
    ScatterResult(const Ray& ray, const glm::vec3& albedo, bool scattered);
};


// Object

enum ObjectType {
    SPHERE,
    TRIANGLE,
    QUAD,
    AABB, // TODO
    OBB // TODO
};

struct Object {
    int32 type;
    float32 data[OBJECT_DATA_SIZE];
    int32 materialType;
    float32 materialData[MATERIAL_DATA_SIZE];
};

// Object helpers

void setSphereData(Object& obj, const glm::vec3& center, float32 radius);
void setTriangleData(Object& obj, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);
void setQuadData(Object& obj, const glm::vec3& center, const glm::vec3& u, const glm::vec3& v);

void setMaterialData(Object& obj, MaterialType materialType, const glm::vec3& albedo, float32 param);
void setMaterialData(Object& obj, const Material& mat);

bool hitSphere(const Ray& ray, Hitpoint& hp, float32 minT, float32 maxT, const float32 data[OBJECT_DATA_SIZE]);
bool hitTriangle(const Ray& ray, Hitpoint& hp, float32 minT, float32 maxT, const float32 data[OBJECT_DATA_SIZE]);
bool hitQuad(const Ray& ray, Hitpoint& hp, float32 minT, float32 maxT, const float32 data[OBJECT_DATA_SIZE]);
bool hit(const Ray& ray, Hitpoint& hp, float32 minT, float32 maxT, const Object& obj);

ScatterResult scatterDiffuse(const Ray& ray, const Hitpoint& hp, const float32 data[MATERIAL_DATA_SIZE]);
ScatterResult scatterMetal(const Ray& ray, const Hitpoint& hp, const float32 data[MATERIAL_DATA_SIZE]);
ScatterResult scatterDielectric(const Ray& ray, const Hitpoint& hp, const float32 data[MATERIAL_DATA_SIZE]);
ScatterResult scatter(const Ray& ray, const Hitpoint& hp, const Object& obj);

} // namespace core

#endif // CORE_HITTABLE_HPP