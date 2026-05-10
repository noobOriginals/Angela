// Objective-C++ host code for the Metal path tracer.

#include <renderer/metal_pathtracer.hpp>

#ifdef __APPLE__

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <util/types.h>
#include <core/scene.hpp>
#include <core/camera.hpp>
#include <lib/image.hpp>

// ---------------------------------------------------------------------------
// Structs shared between C++ host and the Metal shader.
// These use only scalar types (int / float arrays) so layouts match exactly
// in both C++ and MSL without any padding surprises.

struct GPUTextureInfo {
    int32   width, height, channels;
    int32   byteOffset;   // byte offset into flat uint8 texture buffer
};

struct GPUUniforms {
    int32   width, height, spp, maxDepth;
    int32   numEmissive;
    int32   envWidth, envHeight;
    float32 envInvNormFactor;
    int32   hasEnvMap;
    int32   numTextures;
    int32   _pad[2];
    // Camera fields stored as flat float arrays (7×vec3 + lensRadius)
    float32 camOrigin[3],     _c0;
    float32 camLowerLeft[3],  _c1;
    float32 camHorizontal[3], _c2;
    float32 camVertical[3],   _c3;
    float32 camU[3],          _c4;
    float32 camV[3],          _c5;
    float32 camW[3];
    float32 camLensRadius;
};

// ---------------------------------------------------------------------------
// Metal shader source — embedded so no external .metallib step is needed.

static const char* kMetalSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

// ---- Shared struct layouts (must match C++ structs exactly) ----

struct Object {
    int   type;
    float data[15];
    int   materialIndex;
};

struct Material {
    int   type;
    float data[4];
    int   albedoTexture;
};

struct BVHNode {
    float aabbMin[3];
    float aabbMax[3];
    int   left, right, count, _pad;
};

struct GPUTextureInfo {
    int width, height, channels, byteOffset;
};

struct GPUUniforms {
    int   width, height, spp, maxDepth;
    int   numEmissive;
    int   envWidth, envHeight;
    float envInvNormFactor;
    int   hasEnvMap, numTextures;
    int   _pad[2];
    float camOrigin[3],     _c0;
    float camLowerLeft[3],  _c1;
    float camHorizontal[3], _c2;
    float camVertical[3],   _c3;
    float camU[3],          _c4;
    float camV[3],          _c5;
    float camW[3];
    float camLensRadius;
};

// ---- Object type constants ----
constant int OBJ_SPHERE   = 0;
constant int OBJ_TRIANGLE = 1;
constant int OBJ_QUAD     = 2;

// ---- Material type constants ----
constant int MAT_DIFFUSE    = 0;
constant int MAT_METAL      = 1;
constant int MAT_DIELECTRIC = 2;
constant int MAT_EMISSIVE   = 3;

// ============================================================
// PCG32
// ============================================================

struct PCG32 { ulong state, inc; };

PCG32 makePCG(ulong seed, ulong seq) {
    PCG32 r;
    r.state = 0; r.inc = (seq << 1u) | 1u;
    r.state = r.state * 6364136223846793005UL + r.inc;
    r.state += seed;
    r.state = r.state * 6364136223846793005UL + r.inc;
    return r;
}

uint pcgNext(thread PCG32& r) {
    ulong old = r.state;
    r.state = old * 6364136223846793005UL + r.inc;
    uint xs = (uint)(((old >> 18u) ^ old) >> 27u);
    uint rot = (uint)(old >> 59u);
    return (xs >> rot) | (xs << ((-rot) & 31u));
}

float pcgF(thread PCG32& r) {
    return (float)(pcgNext(r) >> 8) * (1.0f / (float)(1 << 24));
}

float3 pcgSphere(thread PCG32& r) {
    float3 p;
    do { p = float3(pcgF(r),pcgF(r),pcgF(r))*2.0f - 1.0f; } while (dot(p,p) >= 1.0f);
    return p;
}

float3 pcgUnit(thread PCG32& r)     { return normalize(pcgSphere(r)); }

float3 pcgHemi(thread PCG32& r, float3 n) {
    float3 v = pcgUnit(r);
    return dot(v,n) > 0.0f ? v : -v;
}

float2 pcgDisk(thread PCG32& r) {
    float2 p;
    do { p = float2(pcgF(r),pcgF(r))*2.0f-1.0f; } while (dot(p,p) >= 1.0f);
    return p;
}

// ============================================================
// Ray / HitRecord
// ============================================================

struct Ray { float3 org, dir; };

struct HitRecord {
    float  t;
    float3 p, n;
    float2 uv;
    int    materialIndex;
    int    objectIndex;
    bool   frontFace;
};

void setFaceNormal(thread HitRecord& hr, float3 rayDir, float3 outN) {
    hr.frontFace = dot(rayDir, outN) < 0.0f;
    hr.n = hr.frontFace ? outN : -outN;
}

float3 at(Ray r, float t) { return r.org + r.dir * t; }

// ============================================================
// Intersection
// ============================================================

bool hitSphere(Ray r, thread HitRecord& hr, float tMin, float tMax,
               device const float* d) {
    float3 cen = float3(d[0],d[1],d[2]);
    float  rad = d[3];
    float3 oc  = r.org - cen;
    float  a   = dot(r.dir,r.dir);
    float  h   = dot(oc,r.dir);
    float  c   = dot(oc,oc) - rad*rad;
    float  dis = h*h - a*c;
    if (dis < 0.0f) return false;
    float sq = sqrt(dis);
    float t  = (-h - sq) / a;
    if (t <= tMin || t >= tMax) {
        t = (-h + sq) / a;
        if (t <= tMin || t >= tMax) return false;
    }
    hr.t = t; hr.p = at(r,t);
    float3 n = (hr.p - cen) / rad;
    setFaceNormal(hr, r.dir, n);
    float theta = acos(clamp(-n.y,-1.0f,1.0f));
    float phi   = atan2(-n.z, n.x) + M_PI_F;
    hr.uv = float2(phi/(2.0f*M_PI_F), theta/M_PI_F);
    return true;
}

bool hitTriangle(Ray r, thread HitRecord& hr, float tMin, float tMax,
                 device const float* d) {
    float3 a = float3(d[0],d[1],d[2]);
    float3 b = float3(d[3],d[4],d[5]);
    float3 c = float3(d[6],d[7],d[8]);
    float3 e1 = b-a, e2 = c-a;
    float3 pv = cross(r.dir, e2);
    float  det = dot(e1, pv);
    if (abs(det) < 1e-8f) return false;
    float invD = 1.0f / det;
    float3 tv = r.org - a;
    float  u  = dot(tv, pv) * invD;
    if (u < 0.0f || u > 1.0f) return false;
    float3 qv = cross(tv, e1);
    float  v  = dot(r.dir, qv) * invD;
    if (v < 0.0f || u+v > 1.0f) return false;
    float t = dot(e2, qv) * invD;
    if (t <= tMin || t >= tMax) return false;
    hr.t = t; hr.p = at(r,t);
    setFaceNormal(hr, r.dir, normalize(cross(e1,e2)));
    float w = 1.0f - u - v;
    hr.uv = float2(w*d[9] + u*d[11] + v*d[13],
                   w*d[10]+ u*d[12] + v*d[14]);
    return true;
}

bool hitQuad(Ray r, thread HitRecord& hr, float tMin, float tMax,
             device const float* d) {
    float3 cen = float3(d[0],d[1],d[2]);
    float3 u   = float3(d[3],d[4],d[5]);
    float3 v   = float3(d[6],d[7],d[8]);
    float3 nor = normalize(cross(u,v));
    float den = dot(r.dir, nor);
    if (abs(den) < 1e-8f) return false;
    float t = dot(cen - r.org, nor) / den;
    if (t <= tMin || t >= tMax) return false;
    float3 p = at(r,t), diff = p - cen;
    float  s = dot(diff,u)/dot(u,u);
    float  q = dot(diff,v)/dot(v,v);
    if (s < -0.5f || s > 0.5f || q < -0.5f || q > 0.5f) return false;
    hr.t = t; hr.p = p; hr.uv = float2(s+0.5f, q+0.5f);
    setFaceNormal(hr, r.dir, nor);
    return true;
}

bool hitObject(Ray r, thread HitRecord& hr, float tMin, float tMax,
               device const Object& obj) {
    bool hit = false;
    switch (obj.type) {
    case OBJ_SPHERE:   hit = hitSphere  (r,hr,tMin,tMax,obj.data); break;
    case OBJ_TRIANGLE: hit = hitTriangle(r,hr,tMin,tMax,obj.data); break;
    case OBJ_QUAD:     hit = hitQuad    (r,hr,tMin,tMax,obj.data); break;
    }
    if (hit) hr.materialIndex = obj.materialIndex;
    return hit;
}

// ============================================================
// BVH traversal
// ============================================================

bool aabbHit(Ray r, device const float* mn, device const float* mx,
             float tMin, float tMax) {
    for (int i = 0; i < 3; ++i) {
        float invD = 1.0f / r.dir[i];
        float t0 = (mn[i] - r.org[i]) * invD;
        float t1 = (mx[i] - r.org[i]) * invD;
        if (invD < 0.0f) { float tmp=t0; t0=t1; t1=tmp; }
        if (t0 > tMin) tMin = t0;
        if (t1 < tMax) tMax = t1;
        if (tMax <= tMin) return false;
    }
    return true;
}

bool hitBVH(Ray r, thread HitRecord& hr, float tMin, float tMax,
            device const BVHNode*  nodes,
            device const int*      primIdx,
            device const Object*   objects) {
    bool      hitAny  = false;
    float     closest = tMax;
    HitRecord tmp;
    int       stack[64];
    int       top = 0;
    stack[top++] = 0;
    while (top > 0) {
        device const BVHNode& nd = nodes[stack[--top]];
        if (!aabbHit(r, nd.aabbMin, nd.aabbMax, tMin, closest)) continue;
        if (nd.count > 0) {
            for (int i = 0; i < nd.count; ++i) {
                int idx = primIdx[nd.left + i];
                if (hitObject(r, tmp, tMin, closest, objects[idx])) {
                    hitAny = true; closest = tmp.t; hr = tmp; hr.objectIndex = idx;
                }
            }
        } else {
            stack[top++] = nd.right;
            stack[top++] = nd.left;
        }
    }
    return hitAny;
}

// Shadow ray — just needs a bool result
bool isOccluded(float3 p, float3 dir, float dist,
                device const BVHNode*  nodes,
                device const int*      primIdx,
                device const Object*   objects) {
    Ray sr = { p, dir };
    HitRecord sh;
    return hitBVH(sr, sh, 0.001f, dist - 0.001f, nodes, primIdx, objects);
}

// ============================================================
// Texture sampling
// ============================================================

float3 sampleTex(int texIdx, float2 uv,
                 device const uchar*           texData,
                 device const GPUTextureInfo*  infos) {
    device const GPUTextureInfo& info = infos[texIdx];
    // Wrap + flip V (image origin top-left, UV origin bottom-left)
    float u = uv.x - floor(uv.x);
    float v = 1.0f - (uv.y - floor(uv.y));
    int px = clamp((int)(u * (float)info.width),  0, info.width  - 1);
    int py = clamp((int)(v * (float)info.height), 0, info.height - 1);
    int off = info.byteOffset + (py * info.width + px) * info.channels;
    float r = (float)texData[off] / 255.0f;
    float g = info.channels >= 2 ? (float)texData[off+1]/255.0f : r;
    float b = info.channels >= 3 ? (float)texData[off+2]/255.0f : r;
    return float3(r,g,b);
}

float3 albedo(device const Material& mat, float2 uv,
              device const uchar*           texData,
              device const GPUTextureInfo*  infos) {
    if (mat.albedoTexture >= 0)
        return sampleTex(mat.albedoTexture, uv, texData, infos);
    return float3(mat.data[0], mat.data[1], mat.data[2]);
}

// ============================================================
// Environment map / sky
// ============================================================

float3 skyGrad(float3 dir) {
    float3 u = normalize(dir);
    float  t = 0.5f * (u.y + 1.0f);
    return mix(float3(1.0f), float3(0.5f,0.7f,1.0f), t);
}

float3 evalEnv(float3 dir,
               int hasEnv, int envW, int envH,
               device const float* envPx) {
    if (!hasEnv) return skyGrad(dir);
    float3 d = normalize(dir);
    float  u = (atan2(d.z, d.x) + M_PI_F) / (2.0f*M_PI_F);
    float  v = acos(clamp(d.y,-1.0f,1.0f)) / M_PI_F;
    int    x = clamp((int)(u*(float)envW), 0, envW-1);
    int    y = clamp((int)(v*(float)envH), 0, envH-1);
    int    i = (y*envW + x)*3;
    return float3(envPx[i], envPx[i+1], envPx[i+2]);
}

float envPDF(float3 dir,
             int hasEnv, int envW, int envH,
             float invNorm, device const float* envPx) {
    if (!hasEnv) return 1.0f / (2.0f * M_PI_F);
    float3 d = normalize(dir);
    float  u = (atan2(d.z, d.x) + M_PI_F) / (2.0f*M_PI_F);
    float  v = acos(clamp(d.y,-1.0f,1.0f)) / M_PI_F;
    int    x = clamp((int)(u*(float)envW), 0, envW-1);
    int    y = clamp((int)(v*(float)envH), 0, envH-1);
    int    i = (y*envW + x)*3;
    float  lum = 0.2126f*envPx[i] + 0.7152f*envPx[i+1] + 0.0722f*envPx[i+2];
    return lum * invNorm;
}

int cdfSearch(device const float* cdf, int n, float xi) {
    int lo = 0, hi = n-1;
    while (lo < hi) { int mid=(lo+hi)/2; if(cdf[mid]<xi) lo=mid+1; else hi=mid; }
    return lo;
}

float3 sampleEnvMap(int envW, int envH, float invNorm,
                    device const float* envPx,
                    device const float* margCDF,
                    device const float* condCDF,
                    thread PCG32& rng,
                    thread float3& outDir, thread float& outPdf) {
    float xi1 = pcgF(rng), xi2 = pcgF(rng);
    int y = cdfSearch(margCDF, envH, xi1);
    int x = cdfSearch(condCDF + y*envW, envW, xi2);
    float u = (x+0.5f)/(float)envW;
    float v = (y+0.5f)/(float)envH;
    float phi = u*2.0f*M_PI_F, theta = v*M_PI_F;
    float sinT = sin(theta);
    outDir = float3(sinT*cos(phi), cos(theta), sinT*sin(phi));
    int   i  = (y*envW+x)*3;
    float3 Le = float3(envPx[i], envPx[i+1], envPx[i+2]);
    float lum = 0.2126f*Le.r + 0.7152f*Le.g + 0.0722f*Le.b;
    outPdf = max(lum * invNorm, 1e-8f);
    return Le;
}

// ============================================================
// Emissive area light sampling
// ============================================================

struct EmissiveSample { float3 dir; float dist, pdf; float3 Le; bool valid; };

EmissiveSample sampleEmissive(float3 hitPos,
                               device const Object*   objects,
                               device const int*      emIds, int numEm,
                               device const Material* mats,
                               thread PCG32& rng) {
    EmissiveSample s; s.valid = false;
    if (numEm == 0) return s;
    int ei  = clamp((int)(pcgF(rng)*(float)numEm), 0, numEm-1);
    int idx = emIds[ei];
    device const Object& obj = objects[idx];
    if (obj.type != OBJ_TRIANGLE) return s;
    float3 A = float3(obj.data[0],obj.data[1],obj.data[2]);
    float3 B = float3(obj.data[3],obj.data[4],obj.data[5]);
    float3 C = float3(obj.data[6],obj.data[7],obj.data[8]);
    float3 e1 = B-A, e2 = C-A;
    float  area = 0.5f * length(cross(e1,e2));
    if (area < 1e-8f) return s;
    float sq  = sqrt(pcgF(rng));
    float u   = 1.0f - sq;
    float v   = pcgF(rng) * sq;
    float3 pt = (1.0f-u-v)*A + u*B + v*C;
    float3 df = pt - hitPos;
    float  dist = length(df);
    if (dist < 1e-6f) return s;
    float3 dir  = df / dist;
    float3 lN   = normalize(cross(e1,e2));
    float  cosL = abs(dot(lN,-dir));
    if (cosL < 1e-6f) return s;
    device const Material& mat = mats[obj.materialIndex];
    s.dir  = dir;
    s.dist = dist;
    s.pdf  = dist*dist / (cosL * area * (float)numEm);
    s.Le   = float3(mat.data[0],mat.data[1],mat.data[2]) * mat.data[3];
    s.valid = true;
    return s;
}

float emissivePDF(HitRecord hr, float3 rayDir,
                  device const Object* objects, int numEm) {
    if (numEm == 0 || hr.objectIndex < 0) return 0.0f;
    device const Object& obj = objects[hr.objectIndex];
    if (obj.type != OBJ_TRIANGLE) return 0.0f;
    float3 e1 = float3(obj.data[3]-obj.data[0], obj.data[4]-obj.data[1], obj.data[5]-obj.data[2]);
    float3 e2 = float3(obj.data[6]-obj.data[0], obj.data[7]-obj.data[1], obj.data[8]-obj.data[2]);
    float  area = 0.5f * length(cross(e1,e2));
    if (area < 1e-8f) return 0.0f;
    float cosL = abs(dot(hr.n, -normalize(rayDir)));
    if (cosL < 1e-6f) return 0.0f;
    return hr.t*hr.t / (cosL * area * (float)numEm);
}

// ============================================================
// Scatter
// ============================================================

struct ScatterResult { Ray ray; float3 atten; float pdf; bool scattered; };

float schlick(float cos, float ratio) {
    float r0 = (1.0f-ratio)/(1.0f+ratio); r0 *= r0;
    return r0 + (1.0f-r0)*pow(1.0f-cos, 5.0f);
}

ScatterResult scatter(Ray r, HitRecord hr, device const Material& mat,
                      thread PCG32& rng,
                      device const uchar*           texData,
                      device const GPUTextureInfo*  infos) {
    ScatterResult sr; sr.scattered = false;
    switch (mat.type) {
    case MAT_DIFFUSE: {
        float3 alb = albedo(mat, hr.uv, texData, infos);
        float3 dir = hr.n + pcgUnit(rng);
        if (dot(dir,dir) < 1e-8f) dir = hr.n;
        dir = normalize(dir);
        float cosT = max(0.0f, dot(hr.n, dir));
        sr.ray = {hr.p, dir};
        sr.atten = alb;
        sr.pdf   = cosT / M_PI_F;
        sr.scattered = true;
        break;
    }
    case MAT_METAL: {
        float3 alb = float3(mat.data[0],mat.data[1],mat.data[2]);
        float  fz  = mat.data[3];
        float3 ref = reflect(normalize(r.dir), hr.n);
        float3 dir = ref + fz * pcgSphere(rng);
        sr.ray = {hr.p, normalize(dir)};
        sr.atten = alb; sr.pdf = 0.0f;
        sr.scattered = dot(dir, hr.n) > 0.0f;
        break;
    }
    case MAT_DIELECTRIC: {
        float  ior   = mat.data[3];
        float  ratio = hr.frontFace ? (1.0f/ior) : ior;
        float3 unit  = normalize(r.dir);
        float  cosT  = min(dot(-unit, hr.n), 1.0f);
        float  sinT  = sqrt(1.0f - cosT*cosT);
        float3 dir   = (ratio*sinT > 1.0f || schlick(cosT,ratio) > pcgF(rng))
                       ? reflect(unit, hr.n)
                       : refract(unit, hr.n, ratio);
        sr.ray = {hr.p, dir}; sr.atten = float3(1.0f);
        sr.pdf = 0.0f; sr.scattered = true;
        break;
    }
    }
    return sr;
}

// ============================================================
// MIS power heuristic
// ============================================================

float mis(float p, float q) {
    p *= p; q *= q;
    return (p+q > 0.0f) ? p/(p+q) : 0.0f;
}

// ============================================================
// Scene context (groups per-scene device pointers)
// ============================================================

struct SceneCtx {
    device const Object*          objects;
    device const Material*        materials;
    device const BVHNode*         bvh;
    device const int*             primIdx;
    device const int*             emIds;
    int                           numEm;
    device const uchar*           texData;
    device const GPUTextureInfo*  texInfos;
    device const float*           envPx;
    device const float*           margCDF;
    device const float*           condCDF;
    int   hasEnv, envW, envH;
    float envInvNorm;
};

// ============================================================
// traceRay — NEE + MIS, same algorithm as CPU
// ============================================================

float3 traceRay(Ray ray, SceneCtx ctx, thread PCG32& rng, int maxDepth) {
    Ray   cur  = ray;
    float3 thr = float3(1.0f);
    float3 acc = float3(0.0f);
    float  brdfPdf     = 0.0f;
    bool   specBounce  = true;

    for (int depth = 0; depth < maxDepth; ++depth) {
        HitRecord hr;
        if (!hitBVH(cur, hr, 0.001f, 1e30f, ctx.bvh, ctx.primIdx, ctx.objects)) {
            float3 Le = evalEnv(cur.dir, ctx.hasEnv, ctx.envW, ctx.envH, ctx.envPx);
            if (specBounce) {
                acc += thr * Le;
            } else {
                float ePdf = envPDF(cur.dir, ctx.hasEnv, ctx.envW, ctx.envH, ctx.envInvNorm, ctx.envPx);
                acc += thr * Le * mis(brdfPdf, ePdf);
            }
            break;
        }

        device const Material& mat = ctx.materials[hr.materialIndex];

        if (mat.type == MAT_EMISSIVE) {
            float3 Le = float3(mat.data[0],mat.data[1],mat.data[2]) * mat.data[3];
            if (specBounce) {
                acc += thr * Le;
            } else {
                float ePdf = emissivePDF(hr, cur.dir, ctx.objects, ctx.numEm);
                acc += thr * Le * mis(brdfPdf, ePdf);
            }
            break;
        }

        if (mat.type == MAT_DIFFUSE) {
            float3 alb = albedo(mat, hr.uv, ctx.texData, ctx.texInfos);

            // NEE — environment
            {
                float3 lDir; float lPdf; float3 Le;
                if (ctx.hasEnv) {
                    Le = sampleEnvMap(ctx.envW, ctx.envH, ctx.envInvNorm,
                                      ctx.envPx, ctx.margCDF, ctx.condCDF,
                                      rng, lDir, lPdf);
                } else {
                    lDir = pcgHemi(rng, hr.n);
                    lPdf = 1.0f / (2.0f * M_PI_F);
                    Le   = skyGrad(lDir);
                }
                float cosL = max(0.0f, dot(hr.n, lDir));
                if (lPdf > 0.0f && cosL > 0.0f &&
                    !isOccluded(hr.p, lDir, 1e30f, ctx.bvh, ctx.primIdx, ctx.objects)) {
                    float bPdf = cosL / M_PI_F;
                    float w    = mis(lPdf, bPdf);
                    acc += thr * Le * alb * (cosL / (M_PI_F * lPdf)) * w;
                }
            }

            // NEE — emissive area lights
            if (ctx.numEm > 0) {
                EmissiveSample es = sampleEmissive(hr.p, ctx.objects,
                                                   ctx.emIds, ctx.numEm,
                                                   ctx.materials, rng);
                if (es.valid) {
                    float cosL = max(0.0f, dot(hr.n, es.dir));
                    if (cosL > 0.0f &&
                        !isOccluded(hr.p, es.dir, es.dist, ctx.bvh, ctx.primIdx, ctx.objects)) {
                        float bPdf = cosL / M_PI_F;
                        float w    = mis(es.pdf, bPdf);
                        acc += thr * es.Le * alb * (cosL / (M_PI_F * es.pdf)) * w;
                    }
                }
            }
        }

        // BRDF scatter
        ScatterResult sr = scatter(cur, hr, mat, rng, ctx.texData, ctx.texInfos);
        if (!sr.scattered) break;

        specBounce = (mat.type == MAT_METAL || mat.type == MAT_DIELECTRIC);
        brdfPdf    = sr.pdf;
        thr       *= sr.atten;
        cur        = sr.ray;

        // Russian roulette
        if (depth >= 3) {
            float p = clamp(max(thr.r, max(thr.g, thr.b)), 0.0f, 0.95f);
            if (pcgF(rng) >= p) break;
            thr /= p;
        }
    }
    return acc;
}

// ============================================================
// Camera ray
// ============================================================

Ray cameraRay(constant GPUUniforms& u, float2 uv, thread PCG32& rng) {
    float3 org  = float3(u.camOrigin[0],    u.camOrigin[1],    u.camOrigin[2]);
    float3 ll   = float3(u.camLowerLeft[0], u.camLowerLeft[1], u.camLowerLeft[2]);
    float3 horiz = float3(u.camHorizontal[0], u.camHorizontal[1], u.camHorizontal[2]);
    float3 vert  = float3(u.camVertical[0],   u.camVertical[1],   u.camVertical[2]);
    float3 cu   = float3(u.camU[0], u.camU[1], u.camU[2]);
    float3 cv   = float3(u.camV[0], u.camV[1], u.camV[2]);
    float3 rayOrg = org;
    if (u.camLensRadius > 0.0f) {
        float2 rd = u.camLensRadius * pcgDisk(rng);
        rayOrg += cu*rd.x + cv*rd.y;
    }
    float3 dir = ll + uv.x*horiz + uv.y*vert - rayOrg;
    return { rayOrg, normalize(dir) };
}

// ============================================================
// Kernel entry point
// ============================================================

kernel void pathTrace(
    device const Object*          objects    [[buffer(0)]],
    device const Material*        materials  [[buffer(1)]],
    device const BVHNode*         bvh        [[buffer(2)]],
    device const int*             primIdx    [[buffer(3)]],
    device const int*             emIds      [[buffer(4)]],
    device const uchar*           texData    [[buffer(5)]],
    device const GPUTextureInfo*  texInfos   [[buffer(6)]],
    device const float*           envPx      [[buffer(7)]],
    device const float*           margCDF    [[buffer(8)]],
    device const float*           condCDF    [[buffer(9)]],
    device       float4*          accumBuf   [[buffer(10)]],
    constant     GPUUniforms&     u          [[buffer(11)]],
    uint2 gid [[thread_position_in_grid]])
{
    if ((int)gid.x >= u.width || (int)gid.y >= u.height) return;

    int pixIdx = (int)gid.y * u.width + (int)gid.x;

    SceneCtx ctx;
    ctx.objects   = objects;   ctx.materials = materials;
    ctx.bvh       = bvh;       ctx.primIdx   = primIdx;
    ctx.emIds     = emIds;     ctx.numEm     = u.numEmissive;
    ctx.texData   = texData;   ctx.texInfos  = texInfos;
    ctx.envPx     = (u.hasEnvMap) ? envPx : nullptr;
    ctx.margCDF   = margCDF;   ctx.condCDF   = condCDF;
    ctx.hasEnv    = u.hasEnvMap;
    ctx.envW      = u.envWidth; ctx.envH = u.envHeight;
    ctx.envInvNorm= u.envInvNormFactor;

    float3 acc = float3(0.0f);
    for (int s = 0; s < u.spp; ++s) {
        // Unique seed per pixel × sample
        ulong seed = (ulong)pixIdx + (ulong)s * (ulong)(u.width * u.height);
        PCG32 rng = makePCG(seed, 42UL);

        float pu = ((float)gid.x + pcgF(rng)) / (float)(u.width  - 1);
        float pv = ((float)gid.y + pcgF(rng)) / (float)(u.height - 1);
        Ray   r  = cameraRay(u, float2(pu, pv), rng);
        acc += traceRay(r, ctx, rng, u.maxDepth);
    }
    accumBuf[pixIdx] = float4(acc, 1.0f);
}
)METAL";

// ---------------------------------------------------------------------------
// Host-side implementation

struct MetalPathTracer::Impl {
    id<MTLDevice>                 device    = nil;
    id<MTLCommandQueue>           queue     = nil;
    id<MTLComputePipelineState>   pipeline  = nil;
    bool                          available = false;
};

// Helper — create a shared MTLBuffer. Always at least 1 byte so Metal doesn't
// complain about zero-length buffers.
static id<MTLBuffer> makeBuffer(id<MTLDevice> dev,
                                 const void* data, size_t len) {
    if (len == 0) {
        uint8_t dummy = 0;
        return [dev newBufferWithBytes:&dummy length:1
                               options:MTLResourceStorageModeShared];
    }
    return [dev newBufferWithBytes:data length:len
                           options:MTLResourceStorageModeShared];
}

MetalPathTracer::MetalPathTracer() {
    impl = new Impl();
    impl->device = MTLCreateSystemDefaultDevice();
    if (!impl->device) {
        std::cerr << "[Metal] No Metal device found.\n";
        return;
    }
    impl->queue = [impl->device newCommandQueue];

    NSError* err = nil;
    NSString* src = [NSString stringWithUTF8String:kMetalSource];
    MTLCompileOptions* opts = [[MTLCompileOptions alloc] init];
    opts.languageVersion = MTLLanguageVersion2_4;

    id<MTLLibrary> lib = [impl->device newLibraryWithSource:src
                                                     options:opts
                                                       error:&err];
    if (!lib) {
        std::cerr << "[Metal] Shader compile error: "
                  << [[err localizedDescription] UTF8String] << "\n";
        return;
    }

    id<MTLFunction> fn = [lib newFunctionWithName:@"pathTrace"];
    if (!fn) {
        std::cerr << "[Metal] Function 'pathTrace' not found.\n";
        return;
    }

    impl->pipeline = [impl->device newComputePipelineStateWithFunction:fn
                                                                 error:&err];
    if (!impl->pipeline) {
        std::cerr << "[Metal] Pipeline error: "
                  << [[err localizedDescription] UTF8String] << "\n";
        return;
    }

    impl->available = true;
    std::cout << "[Metal] Initialised on: "
              << [impl->device.name UTF8String] << "\n";
}

MetalPathTracer::~MetalPathTracer() { delete impl; }

bool MetalPathTracer::isAvailable() const { return impl->available; }

void MetalPathTracer::render(const core::Scene& scene,
                              const core::Camera& cam,
                              const RenderConfig& cfg,
                              Image& out) {
    id<MTLDevice> dev = impl->device;

    // ---- Texture data: pack all textures into one flat uint8 buffer ----
    std::vector<uint8_t>      allTexData;
    std::vector<GPUTextureInfo> texInfoVec;

    for (const auto& tex : scene.textures) {
        GPUTextureInfo info{};
        if (tex.type == core::TEX_IMAGE && tex.pixels) {
            info.width      = tex.width;
            info.height     = tex.height;
            info.channels   = tex.channels;
            info.byteOffset = (int32_t)allTexData.size();
            size_t bytes    = (size_t)tex.width * tex.height * tex.channels;
            allTexData.insert(allTexData.end(), tex.pixels, tex.pixels + bytes);
        } else {
            // Solid-colour fallback: 1×1 RGB pixel
            info.width = info.height = 1; info.channels = 3;
            info.byteOffset = (int32_t)allTexData.size();
            uint8_t px[3] = {
                (uint8_t)(tex.solid[0]*255.f),
                (uint8_t)(tex.solid[1]*255.f),
                (uint8_t)(tex.solid[2]*255.f)
            };
            allTexData.insert(allTexData.end(), px, px+3);
        }
        texInfoVec.push_back(info);
    }

    // ---- Build GPUUniforms ----
    GPUUniforms u{};
    u.width    = cfg.width;   u.height   = cfg.height;
    u.spp      = cfg.spp;     u.maxDepth = cfg.maxDepth;
    u.numEmissive = (int32_t)scene.emissiveIds.size();
    u.hasEnvMap   = scene.envMap.pixels ? 1 : 0;
    u.envWidth    = scene.envMap.width;
    u.envHeight   = scene.envMap.height;
    u.envInvNormFactor = scene.envMap.invNormFactor;
    u.numTextures = (int32_t)texInfoVec.size();

    auto cp3 = [&](float32* dst, const glm::vec3& v) {
        dst[0]=v.x; dst[1]=v.y; dst[2]=v.z;
    };
    cp3(u.camOrigin,     cam.origin);
    cp3(u.camLowerLeft,  cam.lowerLeft);
    cp3(u.camHorizontal, cam.horizontal);
    cp3(u.camVertical,   cam.vertical);
    cp3(u.camU, cam.u); cp3(u.camV, cam.v); cp3(u.camW, cam.w);
    u.camLensRadius = cam.lensRadius;

    // ---- GPU buffers ----
    id<MTLBuffer> objBuf   = makeBuffer(dev, scene.objects.data(),
                                         scene.objects.size() * sizeof(core::Object));
    id<MTLBuffer> matBuf   = makeBuffer(dev, scene.materials.data(),
                                         scene.materials.size() * sizeof(core::Material));
    id<MTLBuffer> bvhBuf   = makeBuffer(dev, scene.bvhNodes.data(),
                                         scene.bvhNodes.size() * sizeof(core::BVHNode));
    id<MTLBuffer> primBuf  = makeBuffer(dev, scene.primIndices.data(),
                                         scene.primIndices.size() * sizeof(int32_t));
    id<MTLBuffer> emBuf    = makeBuffer(dev,
                                         scene.emissiveIds.empty() ? nullptr : scene.emissiveIds.data(),
                                         scene.emissiveIds.size() * sizeof(int32_t));
    id<MTLBuffer> texBuf   = makeBuffer(dev, allTexData.empty() ? nullptr : allTexData.data(),
                                         allTexData.size());
    id<MTLBuffer> texInfoBuf = makeBuffer(dev, texInfoVec.empty() ? nullptr : texInfoVec.data(),
                                           texInfoVec.size() * sizeof(GPUTextureInfo));

    size_t envPxLen  = scene.envMap.pixels
                       ? (size_t)scene.envMap.width * scene.envMap.height * 3 * sizeof(float)
                       : 0;
    size_t margLen = scene.envMap.pixels
                     ? (size_t)scene.envMap.height * sizeof(float32) : 0;
    size_t condLen = scene.envMap.pixels
                     ? (size_t)scene.envMap.width * scene.envMap.height * sizeof(float32) : 0;

    id<MTLBuffer> envPxBuf = makeBuffer(dev, scene.envMap.pixels,         envPxLen);
    id<MTLBuffer> margBuf  = makeBuffer(dev, scene.envMap.marginalCDF,    margLen);
    id<MTLBuffer> condBuf  = makeBuffer(dev, scene.envMap.conditionalCDF, condLen);

    size_t accumBytes = (size_t)cfg.width * cfg.height * sizeof(float) * 4;
    id<MTLBuffer> accumBuf = [dev newBufferWithLength:accumBytes
                                              options:MTLResourceStorageModeShared];

    id<MTLBuffer> uniformBuf = makeBuffer(dev, &u, sizeof(u));

    // ---- Dispatch ----
    id<MTLCommandBuffer>        cmd = [impl->queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    [enc setComputePipelineState:impl->pipeline];

    auto setB = [&](id<MTLBuffer> b, NSUInteger i) {
        [enc setBuffer:b offset:0 atIndex:i];
    };
    setB(objBuf,    0); setB(matBuf,    1); setB(bvhBuf,  2); setB(primBuf, 3);
    setB(emBuf,     4); setB(texBuf,    5); setB(texInfoBuf, 6);
    setB(envPxBuf,  7); setB(margBuf,   8); setB(condBuf, 9);
    setB(accumBuf, 10); setB(uniformBuf, 11);

    MTLSize tgSize = MTLSizeMake(8, 8, 1);
    [enc dispatchThreads:MTLSizeMake(cfg.width, cfg.height, 1)
       threadsPerThreadgroup:tgSize];
    [enc endEncoding];

    [cmd commit];
    [cmd waitUntilCompleted];

    if (cmd.error) {
        std::cerr << "[Metal] Render error: "
                  << [[cmd.error localizedDescription] UTF8String] << "\n";
        return;
    }

    // ---- Read back ----
    const float* data = static_cast<const float*>([accumBuf contents]);
    for (int32_t y = 0; y < cfg.height; ++y) {
        for (int32_t x = 0; x < cfg.width; ++x) {
            int32_t idx = y * cfg.width + x;
            out.accumulate(x, y, glm::vec3(data[idx*4], data[idx*4+1], data[idx*4+2]));
        }
    }
}

#endif // __APPLE__
