#include <lib/image.hpp>

#include <stb_image/stb_image_write.h>

Image::Image(int32 w, int32 h)
    : buffer((size_t)(w * h), glm::vec3(0.0f)), width(w), height(h) {}

void Image::accumulate(int32 x, int32 y, const glm::vec3& sample) {
    buffer[(size_t)(y * width + x)] += sample;
}

void Image::savePNG(const std::string& path, int32 spp) const {
    std::vector<uint8> pixels((size_t)(width * height * 3));
    float32 invSpp = 1.0f / (float32)spp;
    float32 gamma  = 1.0f / 2.2f;

    for (int32 i = 0; i < width * height; ++i) {
        glm::vec3 c = glm::pow(glm::clamp(buffer[(size_t)i] * invSpp, 0.0f, 1.0f),
                               glm::vec3(gamma));
        pixels[(size_t)(i * 3 + 0)] = (uint8)(c.r * 255.99f);
        pixels[(size_t)(i * 3 + 1)] = (uint8)(c.g * 255.99f);
        pixels[(size_t)(i * 3 + 2)] = (uint8)(c.b * 255.99f);
    }

    // stb_image_write expects top-left origin; flip vertically so y=0 is bottom
    stbi_flip_vertically_on_write(1);
    stbi_write_png(path.c_str(), width, height, 3, pixels.data(), width * 3);
}

int32 Image::getWidth()  const { return width;  }
int32 Image::getHeight() const { return height; }
