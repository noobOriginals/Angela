#include <core/camera.hpp>

#include <glm/gtc/constants.hpp>

namespace core {

Camera makeCamera(glm::vec3 lookFrom, glm::vec3 lookAt, glm::vec3 up,
                  float32 vfov, float32 aspect,
                  float32 aperture, float32 focusDist) {
    float32 theta    = glm::radians(vfov);
    float32 h        = glm::tan(theta * 0.5f);
    float32 vpHeight = 2.0f * h;
    float32 vpWidth  = aspect * vpHeight;

    Camera cam;
    cam.w          = glm::normalize(lookFrom - lookAt);
    cam.u          = glm::normalize(glm::cross(up, cam.w));
    cam.v          = glm::cross(cam.w, cam.u);
    cam.origin     = lookFrom;
    cam.horizontal = focusDist * vpWidth  * cam.u;
    cam.vertical   = focusDist * vpHeight * cam.v;
    cam.lowerLeft  = cam.origin - cam.horizontal * 0.5f - cam.vertical * 0.5f - focusDist * cam.w;
    cam.lensRadius = aperture * 0.5f;
    return cam;
}

Ray generateRay(const Camera& cam, float32 s, float32 t, PCG32& rng) {
    glm::vec2 rd     = cam.lensRadius * rng.nextDisk();
    glm::vec3 offset = cam.u * rd.x + cam.v * rd.y;
    glm::vec3 dir    = cam.lowerLeft + s * cam.horizontal + t * cam.vertical - cam.origin - offset;
    return Ray(cam.origin + offset, glm::normalize(dir));
}

} // namespace core
