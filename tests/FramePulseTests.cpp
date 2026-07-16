#include <enbcore/runtime/FramePulse.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void expect(const bool condition, const char* expression, const char* file, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n';
    ++failures;
}

#define EXPECT(expression) expect((expression), #expression, __FILE__, __LINE__)

using namespace enbcore::runtime;

[[nodiscard]] FramePulseSample valid_sample()
{
    FramePulseSample sample{};
    sample.delta_seconds = 1.0F / 60.0F;
    sample.output_width = 2560U;
    sample.output_height = 1440U;
    sample.render_info_available = true;
    return sample;
}

[[nodiscard]] bool payload_is_inactive(const FramePulsePayload& payload)
{
    return payload.frame == 0.0F && payload.delta_seconds == 0.0F && payload.width == 0.0F
        && payload.height == 0.0F;
}

void stable_codes_are_explicit()
{
    EXPECT(static_cast<std::uint32_t>(FramePulseStatus::Published) == 0U);
    EXPECT(static_cast<std::uint32_t>(FramePulseStatus::Withheld) == 1U);
    EXPECT(static_cast<std::uint32_t>(FramePulseStatus::Rejected) == 2U);
    EXPECT(static_cast<std::uint32_t>(FramePulseDiagnostic::None) == 0U);
    EXPECT(static_cast<std::uint32_t>(FramePulseDiagnostic::RenderInfoUnavailable) == 100U);
    EXPECT(static_cast<std::uint32_t>(FramePulseDiagnostic::DeltaSecondsNonFinite) == 110U);
    EXPECT(static_cast<std::uint32_t>(FramePulseDiagnostic::DeltaSecondsOutOfRange) == 111U);
    EXPECT(static_cast<std::uint32_t>(FramePulseDiagnostic::DimensionOutOfRange) == 120U);
}

void first_valid_publish_is_frame_one_and_active()
{
    FramePulseState state{};
    FramePulsePayload payload{};

    const FramePulseResult result = PublishFramePulse(state, payload, valid_sample());

    EXPECT(result.status == FramePulseStatus::Published);
    EXPECT(result.diagnostic == FramePulseDiagnostic::None);
    EXPECT(payload.frame == 1.0F);
    EXPECT(payload.frame > 0.0F);
    EXPECT(payload.delta_seconds == 1.0F / 60.0F);
    EXPECT(payload.width == 2560.0F);
    EXPECT(payload.height == 1440.0F);
    EXPECT(state.published_frames == 1U);
}

void consecutive_publishes_increment_exactly_once_each()
{
    FramePulseState state{};
    FramePulsePayload payload{};

    static_cast<void>(PublishFramePulse(state, payload, valid_sample()));
    static_cast<void>(PublishFramePulse(state, payload, valid_sample()));
    const FramePulseResult third = PublishFramePulse(state, payload, valid_sample());

    EXPECT(third.status == FramePulseStatus::Published);
    EXPECT(payload.frame == 3.0F);
    EXPECT(state.published_frames == 3U);
}

void missing_render_info_withholds_with_inactive_payload()
{
    FramePulseState state{};
    FramePulsePayload payload{};
    static_cast<void>(PublishFramePulse(state, payload, valid_sample()));

    FramePulseSample sample = valid_sample();
    sample.render_info_available = false;

    const FramePulseResult result = PublishFramePulse(state, payload, sample);

    EXPECT(result.status == FramePulseStatus::Withheld);
    EXPECT(result.diagnostic == FramePulseDiagnostic::RenderInfoUnavailable);
    EXPECT(payload_is_inactive(payload));
    EXPECT(state.published_frames == 1U);
}

void non_finite_delta_rejects_with_inactive_payload()
{
    FramePulseState state{};
    FramePulsePayload payload{};
    static_cast<void>(PublishFramePulse(state, payload, valid_sample()));

    FramePulseSample sample = valid_sample();
    sample.delta_seconds = std::numeric_limits<float>::quiet_NaN();

    const FramePulseResult result = PublishFramePulse(state, payload, sample);

    EXPECT(result.status == FramePulseStatus::Rejected);
    EXPECT(result.diagnostic == FramePulseDiagnostic::DeltaSecondsNonFinite);
    EXPECT(payload_is_inactive(payload));
    EXPECT(state.published_frames == 1U);
}

void out_of_range_delta_rejects()
{
    FramePulseState state{};
    FramePulsePayload payload{};

    FramePulseSample zero_delta = valid_sample();
    zero_delta.delta_seconds = 0.0F;
    const FramePulseResult zero_result = PublishFramePulse(state, payload, zero_delta);

    EXPECT(zero_result.status == FramePulseStatus::Rejected);
    EXPECT(zero_result.diagnostic == FramePulseDiagnostic::DeltaSecondsOutOfRange);

    FramePulseSample huge_delta = valid_sample();
    huge_delta.delta_seconds = kFramePulseMaximumDeltaSeconds * 2.0F;
    const FramePulseResult huge_result = PublishFramePulse(state, payload, huge_delta);

    EXPECT(huge_result.status == FramePulseStatus::Rejected);
    EXPECT(huge_result.diagnostic == FramePulseDiagnostic::DeltaSecondsOutOfRange);
    EXPECT(state.published_frames == 0U);
}

void out_of_range_dimensions_reject()
{
    FramePulseState state{};
    FramePulsePayload payload{};

    FramePulseSample zero_width = valid_sample();
    zero_width.output_width = 0U;
    const FramePulseResult zero_result = PublishFramePulse(state, payload, zero_width);

    EXPECT(zero_result.status == FramePulseStatus::Rejected);
    EXPECT(zero_result.diagnostic == FramePulseDiagnostic::DimensionOutOfRange);

    FramePulseSample huge_height = valid_sample();
    huge_height.output_height = kFramePulseMaximumDimension + 1U;
    const FramePulseResult huge_result = PublishFramePulse(state, payload, huge_height);

    EXPECT(huge_result.status == FramePulseStatus::Rejected);
    EXPECT(huge_result.diagnostic == FramePulseDiagnostic::DimensionOutOfRange);
    EXPECT(state.published_frames == 0U);
}

void frame_counter_wraps_inside_float_exact_range()
{
    FramePulseState state{};
    state.published_frames = kFramePulseFrameWrapModulus - 1U;
    FramePulsePayload payload{};

    const FramePulseResult at_limit = PublishFramePulse(state, payload, valid_sample());

    EXPECT(at_limit.status == FramePulseStatus::Published);
    EXPECT(payload.frame == static_cast<float>(kFramePulseFrameWrapModulus));
    EXPECT(state.published_frames == kFramePulseFrameWrapModulus);

    const FramePulseResult wrapped = PublishFramePulse(state, payload, valid_sample());

    EXPECT(wrapped.status == FramePulseStatus::Published);
    EXPECT(payload.frame == 1.0F);
    EXPECT(payload.frame > 0.0F);
    EXPECT(state.published_frames == kFramePulseFrameWrapModulus + 1U);
}

void published_frame_is_always_float_exact_and_positive()
{
    FramePulseState state{};
    state.published_frames = 12'345'678U;
    FramePulsePayload payload{};

    const FramePulseResult result = PublishFramePulse(state, payload, valid_sample());

    EXPECT(result.status == FramePulseStatus::Published);
    const float frame = payload.frame;
    EXPECT(frame > 0.0F);
    EXPECT(frame <= static_cast<float>(kFramePulseFrameWrapModulus));
    EXPECT(std::nearbyint(frame) == frame);
}

}  // namespace

int main()
{
    stable_codes_are_explicit();
    first_valid_publish_is_frame_one_and_active();
    consecutive_publishes_increment_exactly_once_each();
    missing_render_info_withholds_with_inactive_payload();
    non_finite_delta_rejects_with_inactive_payload();
    out_of_range_delta_rejects();
    out_of_range_dimensions_reject();
    frame_counter_wraps_inside_float_exact_range();
    published_frame_is_always_float_exact_and_positive();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "FramePulse tests passed\n";
    return 0;
}
