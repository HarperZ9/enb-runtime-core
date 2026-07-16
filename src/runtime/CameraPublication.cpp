#include <enbcore/runtime/CameraPublication.hpp>

#include <cmath>

namespace enbcore::runtime {
namespace {

[[nodiscard]] CameraPublicationResult Disable(
    CameraPayload& payload,
    const CameraPublicationStatus status,
    const CameraPublicationDiagnostic diagnostic) noexcept
{
    payload = DisabledCameraPayload();
    return CameraPublicationResult{status, diagnostic};
}

[[nodiscard]] bool AllFinite(const float* values, const std::size_t count) noexcept
{
    for (std::size_t index = 0U; index < count; ++index) {
        if (!std::isfinite(values[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] CameraVector RowAt(const std::array<float, 16>& matrix, const std::size_t row) noexcept
{
    const std::size_t base = row * 4U;
    return CameraVector{matrix[base], matrix[base + 1U], matrix[base + 2U], matrix[base + 3U]};
}

}  // namespace

CameraPublicationResult PublishCameraParameters(CameraPublicationState& state,
                                                CameraPayload& payload,
                                                const CameraReading& reading) noexcept
{
    if (!reading.reading_valid) {
        return Disable(payload, CameraPublicationStatus::Withheld,
                       CameraPublicationDiagnostic::ReadingInvalid);
    }

    if (!AllFinite(reading.inverse_view_projection.data(),
                   reading.inverse_view_projection.size())) {
        return Disable(payload, CameraPublicationStatus::Rejected,
                       CameraPublicationDiagnostic::MatrixNonFinite);
    }

    if (!AllFinite(reading.camera_world.data(), reading.camera_world.size())) {
        return Disable(payload, CameraPublicationStatus::Rejected,
                       CameraPublicationDiagnostic::CameraNonFinite);
    }

    const float scale = reading.engine_world_units_per_aurora_unit;
    if (!std::isfinite(scale) || scale <= kCameraMinimumScaleExclusive
        || scale > kCameraMaximumScale) {
        return Disable(payload, CameraPublicationStatus::Rejected,
                       CameraPublicationDiagnostic::ScaleOutOfRange);
    }

    const std::uint64_t next_generation = state.published_generations + 1U;

    CameraPayload candidate;
    candidate.inverse_view_projection_row0 = RowAt(reading.inverse_view_projection, 0U);
    candidate.inverse_view_projection_row1 = RowAt(reading.inverse_view_projection, 1U);
    candidate.inverse_view_projection_row2 = RowAt(reading.inverse_view_projection, 2U);
    candidate.inverse_view_projection_row3 = RowAt(reading.inverse_view_projection, 3U);
    candidate.camera_world =
        CameraVector{reading.camera_world[0], reading.camera_world[1], reading.camera_world[2], 0.0F};
    candidate.status = CameraVector{kCameraProtocolVersion, 1.0F,
                                    static_cast<float>(next_generation), scale};

    state.published_generations = next_generation;
    payload = candidate;
    return CameraPublicationResult{CameraPublicationStatus::Published,
                                   CameraPublicationDiagnostic::None};
}

}  // namespace enbcore::runtime
