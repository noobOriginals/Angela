#ifndef LIB_IMAGE_HPP
#define LIB_IMAGE_HPP

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <util/types.h>

class Image {
public:
    Image(int32 w, int32 h);

    void  accumulate(int32 x, int32 y, const glm::vec3& sample);
    void  savePNG(const std::string& path, int32 samplesPerPixel) const;

    int32 getWidth()  const;
    int32 getHeight() const;

private:
    std::vector<glm::vec3> buffer;
    int32 width, height;
};

#endif // LIB_IMAGE_HPP
