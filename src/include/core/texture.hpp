#ifndef CORE_TEXTURE_HPP
#define CORE_TEXTURE_HPP

#include <glm/glm.hpp>
#include <util/types.h>

namespace core {

enum TextureType : int32 { TEX_SOLID, TEX_IMAGE };

struct Texture {
    int32        type;
    float32      solid[3];           // TEX_SOLID: rgb color
    int32        width, height, channels;
    const uint8* pixels;             // TEX_IMAGE: non-owning pointer into Scene's storage
};

glm::vec3 sampleTexture(const Texture& tex, glm::vec2 uv);

} // namespace core

#endif // CORE_TEXTURE_HPP
