#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include "texture_id.h"
#include "mesh.h"           // MeshId
#include "render_target.h"  // RenderTargetId

namespace Nothofagus
{

/**
 * @file render_snapshot.h
 * @brief POD projection of the scene that the render side consumes.
 *
 * Part of the sim/render thread split (see
 * notes/multithreading_design_discussion.md, option I). The simulation side
 * mutates the live `Bellota` scene and, once per frame, *commits* it into a
 * `RenderSnapshot` — a flat, trivially-copyable display list. The render side
 * draws only from the snapshot and never touches a `Bellota`.
 *
 * In Milestone 1 the snapshot is built and consumed back-to-back on the same
 * thread (`FrameRunner::buildSnapshot` → `renderSnapshot`), so it carries no
 * concurrency cost yet; it exists to establish the POD data boundary. At the
 * thread-flip milestone the snapshot becomes the double-buffered hand-off.
 *
 * Resources are referenced by id (`TextureId`/`MeshId`/`RenderTargetId`), never
 * by GPU handle (`DTexture`/`DMesh`): a freshly created texture has no GPU
 * handle at commit time, so ids are the only stable currency. The render side
 * resolves id → handle *after* the per-frame GPU upload.
 */

/// One sprite to draw. `bellotaTransform` is the bellota-local transform
/// (`bellota.transform().toMat3()`); the world transform is applied on the
/// render side, so the same item can be drawn into both the main canvas and an
/// RTT (which use different world transforms).
struct DrawItem
{
    glm::mat3   bellotaTransform;
    TextureId   texture;
    MeshId      mesh;
    int         layer;          ///< bellota.currentLayer()
    glm::vec3   tintColor;
    float       tintIntensity;
    float       opacity;
    std::int8_t depthOffset;    ///< retained for the depth sort / RTT gather
};

/// One render-to-texture pass: the display list to draw into `target`,
/// already visible-filtered and depth-sorted at commit time.
struct RttPass
{
    RenderTargetId        target;
    std::vector<DrawItem> draws;
};

/// The full per-frame display list the render side consumes.
struct RenderSnapshot
{
    std::uint64_t         commitSeq{0};   ///< monotonic commit counter; the deferred-free clock
    std::vector<DrawItem> draws;          ///< main pass, depth-sorted at commit
    std::vector<RttPass>  rttPasses;      ///< insertion order preserved (nested-RTT dependency)
    glm::vec3             clearColor{0.0f};
};

}
