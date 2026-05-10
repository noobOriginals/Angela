#include <core/object.hpp>

namespace core {

// Material

Material::Material(MaterialType type, const glm::vec3& albedo, float32 param) : type(type), albedo(albedo), param(param) {}

ScatterResult::ScatterResult(const Ray& ray, const glm::vec3& albedo, bool scattered) : ray(ray), albedo(albedo), scattered(scattered) {}

// Object helpers

void setSphereData(Object& obj, const glm::vec3& center, float32 radius) {
    obj.type = SPHERE;
    obj.data[0] = center.x;
    obj.data[1] = center.y;
    obj.data[2] = center.z;
    obj.data[3] = radius;
}

void setTriangleData(Object& obj, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    obj.type = TRIANGLE;
    obj.data[0] = a.x;
    obj.data[1] = a.y;
    obj.data[2] = a.z;
    obj.data[3] = b.x;
    obj.data[4] = b.y;
    obj.data[5] = b.z;
    obj.data[6] = c.x;
    obj.data[7] = c.y;
    obj.data[8] = c.z;
}

void setQuadData(Object& obj, const glm::vec3& center, const glm::vec3& u, const glm::vec3& v) {
    obj.type = QUAD;
    obj.data[0] = center.x;
    obj.data[1] = center.y;
    obj.data[2] = center.z;
    obj.data[3] = u.x;
    obj.data[4] = u.y;
    obj.data[5] = u.z;
    obj.data[6] = v.x;
    obj.data[7] = v.y;
    obj.data[8] = v.z;
}

void setMaterialData(Object& obj, MaterialType materialType, const glm::vec3& albedo, float32 param) {
    obj.materialType = materialType;
    obj.materialData[0] = albedo.x;
    obj.materialData[1] = albedo.y;
    obj.materialData[2] = albedo.z;
    obj.materialData[3] = param;
}

void setMaterialData(Object& obj, const Material& data) {
    setMaterialData(obj, data.type, data.albedo, data.param);
}

bool hitSphere(const Ray& ray, Hitpoint& hp, float32 minT, float32 maxT, const float32 data[OBJECT_DATA_SIZE]) {
    return false;
}

bool hitTriangle(const Ray& ray, Hitpoint& hp, float32 minT, float32 maxT, const float32 data[OBJECT_DATA_SIZE]) {
    return false;
}

bool hitQuad(const Ray& ray, Hitpoint& hp, float32 minT, float32 maxT, const float32 data[OBJECT_DATA_SIZE]) {
    return false;
}

bool hit(const Ray& ray, Hitpoint& hp, float32 minT, float32 maxT, const Object& obj) {
    switch (obj.type) {
    case SPHERE: {
        return hitSphere(ray, hp, minT, maxT, obj.data);
    }

    case TRIANGLE: {
        return hitTriangle(ray, hp, minT, maxT, obj.data);
    }

    case QUAD: {
        return hitQuad(ray, hp, minT, maxT, obj.data);
    }

    default: {
        return false;
    }
    }
}

ScatterResult scatterDiffuse(const Ray& ray, const Hitpoint& hp, const float32 data[MATERIAL_DATA_SIZE]) {
    return ScatterResult();
}

ScatterResult scatterMetal(const Ray& ray, const Hitpoint& hp, const float32 data[MATERIAL_DATA_SIZE]) {
    return ScatterResult();
}

ScatterResult scatterDielectric(const Ray& ray, const Hitpoint& hp, const float32 data[MATERIAL_DATA_SIZE]) {
    return ScatterResult();
}

ScatterResult scatter(const Ray& ray, const Hitpoint& hp, const Object& obj) {
    switch (obj.materialType) {
    case DIFFUSE: {
        return scatterDiffuse(ray, hp, obj.materialData);
    }

    case METAL: {
        return scatterMetal(ray, hp, obj.materialData);
    }

    case DIELECTRIC: {
        return scatterDielectric(ray, hp, obj.materialData);
    }

    default: {
        return ScatterResult();
    }
    }
}

} // namespace core