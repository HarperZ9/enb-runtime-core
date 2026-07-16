#pragma once

#include <array>
#include <cstdint>

namespace enbcore::runtime {

// Camera-parameter publication (parity-map capability #2). The neutral runtime
// turns a validated camera reading into the six-vector ENB shader payload a
// product consumes for its world-space path: four row-major inverse
// view-projection rows, camera world position, and a status vector
// (protocol version, valid flag, generation, engine-units-per-artist-unit).
//
// Scope boundary: this is the *publication* path only. It does not read engine
// memory. Resolving the inverse view-projection and camera position from the
// running game requires a validated Address Library relocation descriptor for
// the exact executable, which the Skyrim engine adapter owns. `reading_valid`
// is that adapter's verdict: false means the adapter could not resolve or
// validate the read, and this path then fails closed so the world-space path
// stays disabled rather than publishing stale matrices.

inline constexpr float kCameraProtocolVersion = 1.0F;
inline constexpr float kCameraMinimumScaleExclusive = 0.0F;
inline constexpr float kCameraMaximumScale = 1'000'000.0F;

struct CameraReading {
    std::array<float, 16> inverse_view_projection;  // row-major
    std::array<float, 3> camera_world;
    float engine_world_units_per_aurora_unit;
    bool reading_valid;
};

struct CameraPublicationState {
    std::uint64_t published_generations;
};

struct CameraVector {
    float x;
    float y;
    float z;
    float w;
};

// Six-vector ENB payload. Matches the product-side runtime protocol: rows 0-3
// carry the row-major inverse view-projection, camera_world carries the world
// position, and status is (version, valid, generation, scale). An all-zero
// status keeps the shader-side world-space path disabled.
struct CameraPayload {
    CameraVector inverse_view_projection_row0;
    CameraVector inverse_view_projection_row1;
    CameraVector inverse_view_projection_row2;
    CameraVector inverse_view_projection_row3;
    CameraVector camera_world;
    CameraVector status;
};

enum class CameraPublicationStatus : std::uint32_t {
    Published = 0U,
    Withheld = 1U,
    Rejected = 2U,
};

enum class CameraPublicationDiagnostic : std::uint32_t {
    None = 0U,
    ReadingInvalid = 100U,
    MatrixNonFinite = 110U,
    CameraNonFinite = 120U,
    ScaleOutOfRange = 130U,
};

struct CameraPublicationResult {
    CameraPublicationStatus status;
    CameraPublicationDiagnostic diagnostic;
};

[[nodiscard]] constexpr CameraPayload DisabledCameraPayload() noexcept
{
    return CameraPayload{};
}

// Publish the camera payload when the adapter reports a valid, finite reading;
// otherwise write the disabled payload (zero status) and do not advance the
// generation. A withheld or rejected publish never leaves a stale valid flag.
[[nodiscard]] CameraPublicationResult PublishCameraParameters(
    CameraPublicationState& state,
    CameraPayload& payload,
    const CameraReading& reading) noexcept;

}  // namespace enbcore::runtime
