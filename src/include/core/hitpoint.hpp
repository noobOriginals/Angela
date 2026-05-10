#ifndef CORE_HITPOINT_HPP
#define CORE_HITPOINT_HPP

#include <glm/glm.hpp>
#include <util/types.h>
#include <core/ray.hpp>

namespace core {

struct HitRecord {
    float32   t;
    glm::vec3 p, n;
    glm::vec2 uv;
    int32     materialIndex;
    bool      frontFace;
};

void setFaceNormal(HitRecord& hr, const Ray& r, const glm::vec3& outwardN);

} // namespace core

#endif // CORE_HITPOINT_HPP
