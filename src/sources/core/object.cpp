#include <core/object.hpp>

#include <cmath>

namespace core {

// Setters

void setSphere(Object& o, glm::vec3 center, float32 radius, int32 matIdx) {
    o.type           = SPHERE;
    o.data[0]        = center.x;
    o.data[1]        = center.y;
    o.data[2]        = center.z;
    o.data[3]        = radius;
    o.materialIndex  = matIdx;
}

void setTriangle(Object& o, glm::vec3 a, glm::vec3 b, glm::vec3 c, int32 matIdx) {
    o.type           = TRIANGLE;
    o.data[0]        = a.x; o.data[1] = a.y; o.data[2] = a.z;
    o.data[3]        = b.x; o.data[4] = b.y; o.data[5] = b.z;
    o.data[6]        = c.x; o.data[7] = c.y; o.data[8] = c.z;
    o.materialIndex  = matIdx;
}

void setQuad(Object& o, glm::vec3 center, glm::vec3 u, glm::vec3 v, int32 matIdx) {
    o.type           = QUAD;
    o.data[0]        = center.x; o.data[1] = center.y; o.data[2] = center.z;
    o.data[3]        = u.x;      o.data[4] = u.y;      o.data[5] = u.z;
    o.data[6]        = v.x;      o.data[7] = v.y;      o.data[8] = v.z;
    o.materialIndex  = matIdx;
}

// Intersection

bool hitSphere(const Ray& ray, HitRecord& hr, float32 tMin, float32 tMax, const float32 data[]) {
    glm::vec3 center(data[0], data[1], data[2]);
    float32   radius = data[3];

    glm::vec3 oc   = ray.org - center;
    float32   a    = glm::dot(ray.dir, ray.dir);
    float32   h    = glm::dot(oc, ray.dir);
    float32   c    = glm::dot(oc, oc) - radius * radius;
    float32   disc = h * h - a * c;

    if (disc < 0.0f) return false;

    float32 sqrtD = std::sqrt(disc);
    float32 root  = (-h - sqrtD) / a;
    if (root <= tMin || root >= tMax) {
        root = (-h + sqrtD) / a;
        if (root <= tMin || root >= tMax)
            return false;
    }

    hr.t             = root;
    hr.p             = rayAt(ray, root);
    hr.uv            = glm::vec2(0.0f);
    hr.materialIndex = 0;    // set by hitObject
    setFaceNormal(hr, ray, (hr.p - center) / radius);
    return true;
}

bool hitTriangle(const Ray&, HitRecord&, float32, float32, const float32[]) {
    return false;    // Phase 2
}

bool hitQuad(const Ray&, HitRecord&, float32, float32, const float32[]) {
    return false;    // Phase 2
}

bool hitObject(const Ray& ray, HitRecord& hr, float32 tMin, float32 tMax, const Object& obj) {
    bool hit = false;
    switch (obj.type) {
    case SPHERE:   hit = hitSphere  (ray, hr, tMin, tMax, obj.data); break;
    case TRIANGLE: hit = hitTriangle(ray, hr, tMin, tMax, obj.data); break;
    case QUAD:     hit = hitQuad    (ray, hr, tMin, tMax, obj.data); break;
    default:       break;
    }
    if (hit) hr.materialIndex = obj.materialIndex;
    return hit;
}

} // namespace core
