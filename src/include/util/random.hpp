#ifndef UTIL_RANDOM_HPP
#define UTIL_RANDOM_HPP

#include <glm/glm.hpp>
#include <util/types.h>

struct PCG32 {
    uint64 state, inc;

    PCG32() : PCG32(12345ULL, 54321ULL) {}

    PCG32(uint64 seed, uint64 seq) {
        state = 0;
        inc   = (seq << 1u) | 1u;
        next();
        state += seed;
        next();
    }

    uint32 next() {
        uint64 old      = state;
        state           = old * 6364136223846793005ULL + inc;
        uint32 xorshift = (uint32)(((old >> 18u) ^ old) >> 27u);
        uint32 rot      = (uint32)(old >> 59u);
        return (xorshift >> rot) | (xorshift << ((uint32)(-(int32)rot) & 31u));
    }

    float32 nextFloat() {
        return (float32)(next() >> 8) * (1.0f / (float32)(1 << 24));
    }

    glm::vec2 nextDisk() {
        glm::vec2 p;
        do {
            p = glm::vec2(nextFloat() * 2.0f - 1.0f, nextFloat() * 2.0f - 1.0f);
        } while (glm::dot(p, p) >= 1.0f);
        return p;
    }

    glm::vec3 nextUnitSphere() {
        glm::vec3 p;
        do {
            p = glm::vec3(nextFloat(), nextFloat(), nextFloat()) * 2.0f - glm::vec3(1.0f);
        } while (glm::dot(p, p) >= 1.0f);
        return p;
    }

    glm::vec3 nextUnitVector() {
        return glm::normalize(nextUnitSphere());
    }

    glm::vec3 nextHemisphere(const glm::vec3& normal) {
        glm::vec3 v = nextUnitSphere();
        return glm::dot(v, normal) > 0.0f ? v : -v;
    }

    // Uniform random direction on the hemisphere above normal.
    glm::vec3 nextUnitHemisphere(const glm::vec3& normal) {
        return glm::normalize(nextHemisphere(normal));
    }
};

#endif // UTIL_RANDOM_HPP
