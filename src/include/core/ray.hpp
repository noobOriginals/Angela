#ifndef CORE_RAY_HPP
#define CORE_RAY_HPP

#include <glm/glm.hpp>
#include <util/types.h>

namespace core {

struct Ray {
    glm::vec3 org, dir;

    Ray() = default;
    Ray(const glm::vec3& org, const glm::vec3& dir) : org(org), dir(dir) {}
};

inline glm::vec3 rayAt(const Ray& r, float32 t) {
    return r.org + r.dir * t;
}

} // namespace core

#endif // CORE_RAY_HPP
