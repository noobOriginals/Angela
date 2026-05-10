# Angela

A progressive CPU Path Tracer in C++17, designed from the ground up for eventual GPU acceleration.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./bin/Angela
```

## Libraries

All third-party headers live under `$OPENGL_PATH`:

| Library | Path | Include directive |
|---------|------|-------------------|
| GLM | `$OPENGL_PATH/glm/` | `#include <glm/glm.hpp>` |
| stb\_image | `$OPENGL_PATH/stb_image/` | `#include <stb_image/stb_image.h>` |
| stb\_image\_write | `$OPENGL_PATH/stb_image/` | `#include <stb_image/stb_image_write.h>` |
| tinyobjloader | `$OPENGL_PATH/tinyobjloader/` | `#include <tinyobjloader/tiny_obj_loader.h>` (Phase 3+) |

`src/sources/lib/stb_image_write.c` is the single translation unit that defines both `STB_IMAGE_IMPLEMENTATION` and `STB_IMAGE_WRITE_IMPLEMENTATION`. No other file defines these macros.

## Architecture

```
src/include/
  util/     — Shared CPU+GPU primitives: types.h (scalar aliases), random.hpp (PCG32 RNG)
  core/     — GPU-portable render primitives: Ray, HitRecord, Object, Material, Camera, Scene
  renderer/ — CPU-only render loop: traceRay(), CPUPathTracer (tile thread pool)
  lib/      — CPU-only utilities: Image (HDR accumulation + PNG output)
```

### GPU-Portability Rule

`core/` headers and their free functions must remain GPU-portable:

- No `std::vector`, `std::string`, or heap allocation
- No virtual functions — dispatch via `switch(type)` on integer enums
- All render-path state passed explicitly (no globals, no thread-local)
- `traceRay()` takes raw pointer+count, not STL containers

`renderer/` and `lib/` may use STL freely. When adding GPU support, only these layers change; `core/` promotes to device functions.

**Exception**: `core/scene.hpp` uses `std::vector` as the CPU-side owner of object/material arrays. It is excluded from the GPU-portability rule because `traceRay()` receives its `.data()`/`.size()` directly, not the `Scene` object.

### Object / Material Layout

Geometry and material properties are stored as flat `float32` arrays inside POD structs, dispatched by integer enums. This layout maps directly to GPU buffer bindings.

```cpp
struct Object   { int32 type; float32 data[9]; int32 materialIndex; };
struct Material { int32 type; float32 data[4]; };  // albedo.rgb + param
```

### Thread Pool

`CPUPathTracer` uses a `std::thread` worker pool with a mutex-protected tile queue. Each tile independently seeds a `PCG32` from its coordinates — renders are fully reproducible and independent of thread count.

---

## Render Verification

Never inspect rendered images directly (no pixel reading, no file introspection). The user describes how renders look. Only verify that the output file was produced (e.g. `ls render.png`). Inspect image content only when the user explicitly asks.

---

## Code Conventions

- **Primitive types**: use `types.h` aliases (`float32`, `int32`, `uint64`, …) everywhere — not `float`, `int`, etc.
- **Math types**: use GLM (`glm::vec2`, `glm::vec3`, `glm::mat4`) — never roll custom vector math.
- **Namespaces**: all `core/` types and free functions live in `namespace core`. `lib/` types (e.g. `Image`) and `renderer/` free functions have no namespace.
- **Naming**:
  - Structs / Classes: `PascalCase`
  - Free functions and methods: `camelCase`
  - Enum values and `constexpr` constants: `ALL_CAPS`
  - Local variables and fields: `camelCase`
- **Headers**: `#ifndef` include guards. Include order: std → lib → local.
- **Comments**: only when the *why* is non-obvious. No docstrings. No narration of what the code does.
- **Memory**: no raw `new`/`delete` outside `lib/` — use `std::vector` or stack allocation.

---

## Development Phases

Each phase leaves the project in a fully working, compilable state. No existing `core/` API is broken between phases — new primitives, BRDFs, and structures are added alongside existing ones.

| Phase | Scope | Status |
|-------|-------|--------|
| **1** | Sphere intersection · Lambertian BRDF · Perspective camera · PNG output · Tile thread pool | ✅ Complete |
| **2** | Metal + Dielectric BRDFs · Triangle + Quad primitives | ✅ Complete |
| **3** | OBJ loading (tinyobjloader) · UV coordinates · Texture maps (stb\_image read) | — |
| **4** | BVH acceleration structure (SAH) | — |
| **5** | Importance sampling · MIS · Environment maps · Emissive materials | — |
| **GPU** | CUDA/Metal/Vulkan compute backend — `core/` promotes to device functions; `Scene` arrays become device buffers | — |
