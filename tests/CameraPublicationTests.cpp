#include <enbcore/runtime/CameraPublication.hpp>

#include <array>
#include <bit>
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

[[nodiscard]] CameraReading valid_reading()
{
    CameraReading reading{};
    // A plain, finite row-major inverse view-projection (identity here is fine;
    // the publication path does not interpret it, only guards finiteness).
    reading.inverse_view_projection = {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F};
    reading.camera_world = {128.0F, 256.0F, 512.0F};
    reading.engine_world_units_per_aurora_unit = 1.0F;
    reading.reading_valid = true;
    return reading;
}

[[nodiscard]] bool same_bits(const float lhs, const float rhs)
{
    return std::bit_cast<std::uint32_t>(lhs) == std::bit_cast<std::uint32_t>(rhs);
}

[[nodiscard]] bool payload_is_disabled(const CameraPayload& payload)
{
    // A disabled payload publishes a zero status vector so the world-space path
    // stays off, matching the shader-side default.
    return payload.status.x == 0.0F && payload.status.y == 0.0F
        && payload.status.z == 0.0F && payload.status.w == 0.0F;
}

void stable_codes_are_explicit()
{
    EXPECT(static_cast<std::uint32_t>(CameraPublicationStatus::Published) == 0U);
    EXPECT(static_cast<std::uint32_t>(CameraPublicationStatus::Withheld) == 1U);
    EXPECT(static_cast<std::uint32_t>(CameraPublicationStatus::Rejected) == 2U);
    EXPECT(static_cast<std::uint32_t>(CameraPublicationDiagnostic::None) == 0U);
    EXPECT(static_cast<std::uint32_t>(CameraPublicationDiagnostic::ReadingInvalid) == 100U);
    EXPECT(static_cast<std::uint32_t>(CameraPublicationDiagnostic::MatrixNonFinite) == 110U);
    EXPECT(static_cast<std::uint32_t>(CameraPublicationDiagnostic::CameraNonFinite) == 120U);
    EXPECT(static_cast<std::uint32_t>(CameraPublicationDiagnostic::ScaleOutOfRange) == 130U);
    // Protocol version 1 is fixed for the six-vector runtime ABI.
    EXPECT(kCameraProtocolVersion == 1.0F);
}

void valid_reading_publishes_rows_camera_and_active_status()
{
    CameraPublicationState state{};
    CameraPayload payload{};

    const CameraPublicationResult result = PublishCameraParameters(state, payload, valid_reading());

    EXPECT(result.status == CameraPublicationStatus::Published);
    EXPECT(result.diagnostic == CameraPublicationDiagnostic::None);

    // Rows carry the row-major inverse view-projection.
    EXPECT(payload.inverse_view_projection_row0.x == 1.0F);
    EXPECT(payload.inverse_view_projection_row1.y == 1.0F);
    EXPECT(payload.inverse_view_projection_row2.z == 1.0F);
    EXPECT(payload.inverse_view_projection_row3.w == 1.0F);
    EXPECT(payload.camera_world.x == 128.0F);
    EXPECT(payload.camera_world.y == 256.0F);
    EXPECT(payload.camera_world.z == 512.0F);

    // Status: version, valid flag, generation 1, scale.
    EXPECT(payload.status.x == kCameraProtocolVersion);
    EXPECT(payload.status.y == 1.0F);
    EXPECT(payload.status.z == 1.0F);
    EXPECT(payload.status.w == 1.0F);
    EXPECT(state.published_generations == 1U);
}

void generation_advances_once_per_publish()
{
    CameraPublicationState state{};
    CameraPayload payload{};

    static_cast<void>(PublishCameraParameters(state, payload, valid_reading()));
    static_cast<void>(PublishCameraParameters(state, payload, valid_reading()));
    const CameraPublicationResult third =
        PublishCameraParameters(state, payload, valid_reading());

    EXPECT(third.status == CameraPublicationStatus::Published);
    EXPECT(payload.status.z == 3.0F);
    EXPECT(state.published_generations == 3U);
}

void an_invalid_reading_withholds_and_disables()
{
    // The adapter reports it could not resolve/validate the engine read; the
    // world-space path must fail closed, not publish stale matrices.
    CameraPublicationState state{};
    CameraPayload payload{};
    static_cast<void>(PublishCameraParameters(state, payload, valid_reading()));

    CameraReading reading = valid_reading();
    reading.reading_valid = false;

    const CameraPublicationResult result = PublishCameraParameters(state, payload, reading);

    EXPECT(result.status == CameraPublicationStatus::Withheld);
    EXPECT(result.diagnostic == CameraPublicationDiagnostic::ReadingInvalid);
    EXPECT(payload_is_disabled(payload));
    EXPECT(state.published_generations == 1U);  // not advanced
}

void a_non_finite_matrix_is_rejected_and_disabled()
{
    CameraPublicationState state{};
    CameraPayload payload{};

    CameraReading reading = valid_reading();
    reading.inverse_view_projection[5] = std::numeric_limits<float>::quiet_NaN();

    const CameraPublicationResult result = PublishCameraParameters(state, payload, reading);

    EXPECT(result.status == CameraPublicationStatus::Rejected);
    EXPECT(result.diagnostic == CameraPublicationDiagnostic::MatrixNonFinite);
    EXPECT(payload_is_disabled(payload));
    EXPECT(state.published_generations == 0U);
}

void a_non_finite_camera_is_rejected_and_disabled()
{
    CameraPublicationState state{};
    CameraPayload payload{};

    CameraReading reading = valid_reading();
    reading.camera_world[2] = std::numeric_limits<float>::infinity();

    const CameraPublicationResult result = PublishCameraParameters(state, payload, reading);

    EXPECT(result.status == CameraPublicationStatus::Rejected);
    EXPECT(result.diagnostic == CameraPublicationDiagnostic::CameraNonFinite);
    EXPECT(payload_is_disabled(payload));
}

void an_out_of_range_scale_is_rejected()
{
    CameraPublicationState state{};
    CameraPayload payload{};

    CameraReading reading = valid_reading();
    reading.engine_world_units_per_aurora_unit = 0.0F;  // must be strictly positive

    const CameraPublicationResult result = PublishCameraParameters(state, payload, reading);

    EXPECT(result.status == CameraPublicationStatus::Rejected);
    EXPECT(result.diagnostic == CameraPublicationDiagnostic::ScaleOutOfRange);
    EXPECT(payload_is_disabled(payload));
}

void a_withheld_publish_preserves_no_stale_active_flag()
{
    // Even starting from an active payload, a withheld publish must clear the
    // valid flag so a shader never reads a stale "valid" status.
    CameraPublicationState state{};
    CameraPayload payload{};
    static_cast<void>(PublishCameraParameters(state, payload, valid_reading()));
    EXPECT(payload.status.y == 1.0F);

    CameraReading reading = valid_reading();
    reading.reading_valid = false;
    static_cast<void>(PublishCameraParameters(state, payload, reading));

    EXPECT(same_bits(payload.status.y, 0.0F));
}

}  // namespace

int main()
{
    stable_codes_are_explicit();
    valid_reading_publishes_rows_camera_and_active_status();
    generation_advances_once_per_publish();
    an_invalid_reading_withholds_and_disables();
    a_non_finite_matrix_is_rejected_and_disabled();
    a_non_finite_camera_is_rejected_and_disabled();
    an_out_of_range_scale_is_rejected();
    a_withheld_publish_preserves_no_stale_active_flag();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "CameraPublication tests passed\n";
    return 0;
}
