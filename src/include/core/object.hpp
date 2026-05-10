#ifndef CORE_OBJECT_HPP
#define CORE_OBJECT_HPP

#include <glm/glm.hpp>
#include <util/types.h>
#include <core/ray.hpp>
#include <core/hitpoint.hpp>

namespace core {

// data layout per type:
//   SPHERE:   [0..2] center.xyz, [3] radius
//   TRIANGLE: [0..2] a.xyz, [3..5] b.xyz, [6..8] c.xyz, [9..10] uva, [11..12] uvb, [13..14] uvc
//   QUAD:     [0..2] center.xyz, [3..5] u.xyz, [6..8] v.xyz
constexpr uint32 OBJECT_DATA_SIZE = 15;

enum ObjectType : int32 {
    SPHERE,
    TRIANGLE,
    QUAD
};

struct Object {
    int32   type;
    float32 data[OBJECT_DATA_SIZE];
    int32   materialIndex;
};

void setSphere  (Object& o, glm::vec3 center, float32 radius, int32 matIdx);
void setTriangle(Object& o, glm::vec3 a, glm::vec3 b, glm::vec3 c, int32 matIdx);
void setTriangle(Object& o, glm::vec3 a, glm::vec3 b, glm::vec3 c,
                            glm::vec2 uva, glm::vec2 uvb, glm::vec2 uvc, int32 matIdx);
void setQuad    (Object& o, glm::vec3 center, glm::vec3 u, glm::vec3 v, int32 matIdx);

bool hitSphere  (const Ray&, HitRecord&, float32 tMin, float32 tMax, const float32 data[]);
bool hitTriangle(const Ray&, HitRecord&, float32 tMin, float32 tMax, const float32 data[]);
bool hitQuad    (const Ray&, HitRecord&, float32 tMin, float32 tMax, const float32 data[]);
bool hitObject  (const Ray&, HitRecord&, float32 tMin, float32 tMax, const Object&);

} // namespace core

#endif // CORE_OBJECT_HPP
