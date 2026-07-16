#include <enbcore/runtime/FramePulse.hpp>

#include <cmath>

namespace enbcore::runtime {
namespace {

[[nodiscard]] FramePulseResult Withhold(FramePulsePayload& payload,
                                        const FramePulseDiagnostic diagnostic) noexcept
{
    payload = InactiveFramePulsePayload();
    return FramePulseResult{FramePulseStatus::Withheld, diagnostic};
}

[[nodiscard]] FramePulseResult Reject(FramePulsePayload& payload,
                                      const FramePulseDiagnostic diagnostic) noexcept
{
    payload = InactiveFramePulsePayload();
    return FramePulseResult{FramePulseStatus::Rejected, diagnostic};
}

[[nodiscard]] bool DimensionInRange(const std::uint32_t dimension) noexcept
{
    return dimension >= kFramePulseMinimumDimension && dimension <= kFramePulseMaximumDimension;
}

}  // namespace

FramePulseResult PublishFramePulse(FramePulseState& state,
                                   FramePulsePayload& payload,
                                   const FramePulseSample& sample) noexcept
{
    if (!sample.render_info_available) {
        return Withhold(payload, FramePulseDiagnostic::RenderInfoUnavailable);
    }

    if (!std::isfinite(sample.delta_seconds)) {
        return Reject(payload, FramePulseDiagnostic::DeltaSecondsNonFinite);
    }

    if (sample.delta_seconds <= kFramePulseMinimumDeltaSecondsExclusive
        || sample.delta_seconds > kFramePulseMaximumDeltaSeconds) {
        return Reject(payload, FramePulseDiagnostic::DeltaSecondsOutOfRange);
    }

    if (!DimensionInRange(sample.output_width) || !DimensionInRange(sample.output_height)) {
        return Reject(payload, FramePulseDiagnostic::DimensionOutOfRange);
    }

    const std::uint64_t next_published = state.published_frames + 1U;
    const std::uint64_t wrapped_frame = ((next_published - 1U) % kFramePulseFrameWrapModulus) + 1U;

    FramePulsePayload candidate;
    candidate.frame = static_cast<float>(wrapped_frame);
    candidate.delta_seconds = sample.delta_seconds;
    candidate.width = static_cast<float>(sample.output_width);
    candidate.height = static_cast<float>(sample.output_height);

    state.published_frames = next_published;
    payload = candidate;
    return FramePulseResult{FramePulseStatus::Published, FramePulseDiagnostic::None};
}

}  // namespace enbcore::runtime
