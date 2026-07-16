#pragma once

#include <cstdint>

namespace enbcore::runtime {

// Frame-pulse publication: the neutral per-frame liveness value a product
// writes as one ENB shader parameter. A shader treats `frame > 0` as "the
// native bridge is live this frame"; the all-zero payload is the explicit
// inactive state. No engine memory is read: the delta and output dimensions
// come from the host's render information, and the counter is adapter-owned.
//
// The counter wraps inside the float-exact integer range so the published
// frame value is always exactly representable and always strictly positive
// while live. Withheld and rejected publications force the inactive payload
// instead of leaving a stale live payload visible to shaders.

inline constexpr float kFramePulseMinimumDeltaSecondsExclusive = 0.0F;
inline constexpr float kFramePulseMaximumDeltaSeconds = 10.0F;
inline constexpr std::uint32_t kFramePulseMinimumDimension = 1U;
inline constexpr std::uint32_t kFramePulseMaximumDimension = 16'384U;
inline constexpr std::uint64_t kFramePulseFrameWrapModulus = 16'777'216ULL;  // 2^24

struct FramePulseSample {
    float delta_seconds;
    std::uint32_t output_width;
    std::uint32_t output_height;
    bool render_info_available;
};

struct FramePulseState {
    std::uint64_t published_frames;
};

// Shader payload layout: x = wrapped 1-based frame (0 while inactive),
// y = delta seconds, z = output width, w = output height.
struct FramePulsePayload {
    float frame;
    float delta_seconds;
    float width;
    float height;
};

enum class FramePulseStatus : std::uint32_t {
    Published = 0U,
    Withheld = 1U,
    Rejected = 2U,
};

enum class FramePulseDiagnostic : std::uint32_t {
    None = 0U,
    RenderInfoUnavailable = 100U,
    DeltaSecondsNonFinite = 110U,
    DeltaSecondsOutOfRange = 111U,
    DimensionOutOfRange = 120U,
};

struct FramePulseResult {
    FramePulseStatus status;
    FramePulseDiagnostic diagnostic;
};

[[nodiscard]] constexpr FramePulsePayload InactiveFramePulsePayload() noexcept
{
    return FramePulsePayload{0.0F, 0.0F, 0.0F, 0.0F};
}

// Validates the sample, then either publishes the next live payload and
// advances the counter exactly once, or forces the inactive payload without
// advancing it. The state is never modified on a withheld or rejected pulse.
[[nodiscard]] FramePulseResult PublishFramePulse(FramePulseState& state,
                                                 FramePulsePayload& payload,
                                                 const FramePulseSample& sample) noexcept;

}  // namespace enbcore::runtime
