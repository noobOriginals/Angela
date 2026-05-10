#include <core/hitpoint.hpp>

namespace core {

void setFaceNormal(HitRecord& hr, const Ray& r, const glm::vec3& outwardN) {
    hr.frontFace = glm::dot(r.dir, outwardN) < 0.0f;
    hr.n = hr.frontFace ? outwardN : -outwardN;
}

} // namespace core
