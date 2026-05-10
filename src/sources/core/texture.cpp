#include <core/texture.hpp>

namespace core {

glm::vec3 sampleTexture(const Texture& tex, glm::vec2 uv) {
    if (tex.type == TEX_SOLID)
        return glm::vec3(tex.solid[0], tex.solid[1], tex.solid[2]);

    // Wrap and flip V (image origin top-left, UV origin bottom-left)
    float32 u = uv.x - glm::floor(uv.x);
    float32 v = 1.0f - (uv.y - glm::floor(uv.y));

    int32 px = glm::clamp((int32)(u * (float32)tex.width),  0, tex.width  - 1);
    int32 py = glm::clamp((int32)(v * (float32)tex.height), 0, tex.height - 1);

    const uint8* p = tex.pixels + (py * tex.width + px) * tex.channels;
    return glm::vec3(
        p[0] / 255.0f,
        tex.channels >= 2 ? p[1] / 255.0f : p[0] / 255.0f,
        tex.channels >= 3 ? p[2] / 255.0f : p[0] / 255.0f
    );
}

} // namespace core
