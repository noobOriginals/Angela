#include <core/hitpoint.hpp>

using namespace glm;

namespace core {

// Hitpoint

void Hitpoint::setNormal(const vec3& dir, const vec3& normal) {
    if (dot(dir, normal) > 0) {
        exit = true;
        n = -normal;
    } else {
        exit = false;
        n = normal;
    }
}

} // namespace core
