#ifndef IMAGE_HPP
#define IMAGE_HPP

// Std includes
#include <string>

// Lib includes
#include <glm/glm.hpp>

// Local includes
#include <types.h>

struct Pixel {
    uint8 r, g, b;
    Pixel();
    Pixel(uint8 r, uint8 g, uint8 b);
    Pixel(glm::vec3 v);
};

class Image {
public:
    Image() = default;
    Image(const Image& other);
    Image(int32 width, int32 height);
    ~Image();

    Pixel get(int32 x, int32 y) const;

    void set(int32 x, int32 y, Pixel p);
    void set(int32 i, Pixel p);
    void setPixels(int32 size, int32 offset, Pixel* pixels);

    void save(std::string filename) const;

    const int32& getWidth() const;
    const int32& getHeight() const;
    const int32& getSize() const;
    Pixel* getPixels(int32* size) const;

    void operator=(const Image& other);

private:
    Pixel* pixels;
    int32 width, height, size;
};

#endif // IMAGE_HPP