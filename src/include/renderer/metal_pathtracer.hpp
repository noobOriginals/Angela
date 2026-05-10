#ifndef RENDERER_METAL_PATHTRACER_HPP
#define RENDERER_METAL_PATHTRACER_HPP

#ifdef __APPLE__

#include <util/types.h>
#include <core/scene.hpp>
#include <core/camera.hpp>
#include <renderer/pathtracer.hpp>
#include <lib/image.hpp>

class MetalPathTracer {
public:
    MetalPathTracer();
    ~MetalPathTracer();

    bool isAvailable() const;

    void render(const core::Scene&, const core::Camera&,
                const RenderConfig&, Image& out);

private:
    struct Impl;
    Impl* impl;
};

#endif // __APPLE__
#endif // RENDERER_METAL_PATHTRACER_HPP
