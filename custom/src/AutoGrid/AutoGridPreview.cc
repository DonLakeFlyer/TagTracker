#include "AutoGridPreview.h"

#include <algorithm>
#include <cmath>

namespace {

// Lane count tolerance, so 100 / 20 is never 4.999…
constexpr double PREVIEW_LANE_EPS = 1e-9;

}  // namespace

AutoGridPreview::AutoGridPreview(QObject* parent)
    : QObject(parent)
{}

void AutoGridPreview::start(const GridGenerator::Params& latched, const Limits& limits)
{
    _stage = Stage::Editing;
    _params = latched;
    _limits = limits;
    _snapDepth();
    _regenerate();
}

bool AutoGridPreview::requestConfirm()
{
    if (!editing() || !valid()) {
        return false;
    }
    _stage = Stage::Confirming;
    emit changed();
    return true;
}

void AutoGridPreview::setConfirmed()
{
    if (active() && !confirmed()) {
        _stage = Stage::Confirmed;
        emit changed();
    }
}

void AutoGridPreview::clear()
{
    if (!active()) {
        return;
    }
    _stage = Stage::Idle;
    _params = GridGenerator::Params();
    _result = GridGenerator::Result();
    _limits = Limits();
    _path.clear();
    _outline.clear();
    emit changed();
}

void AutoGridPreview::adjustWidth(int steps)
{
    _adjust(_params.widthM, steps, WIDTH_STEP_M, widthMin(), widthMax());
}

void AutoGridPreview::adjustDepth(int steps)
{
    _adjust(_params.depthM, steps, _params.spacingM, depthMin(), depthMax());
}

void AutoGridPreview::adjustSpacing(int steps)
{
    if (!editing()) {
        return;
    }
    const double spacing =
        std::clamp(_params.spacingM + (steps * SPACING_STEP_M), spacingMin(), std::max(spacingMin(), spacingMax()));
    if (spacing != _params.spacingM) {
        _params.spacingM = spacing;
        _snapDepth();
        _regenerate();
    }
}

void AutoGridPreview::_snapDepth()
{
    if (_params.spacingM <= 0.0) {
        return;
    }
    const double lanes = std::round(_params.depthM / _params.spacingM);
    _params.depthM = std::clamp(lanes * _params.spacingM, depthMin(), std::max(depthMin(), depthMax()));
}

void AutoGridPreview::_adjust(double& value, int steps, double step, double low, double high)
{
    // Only the editor changes the plan: the slider shows, and confirm flies, what was edited
    if (!editing()) {
        return;
    }
    const double adjusted = std::clamp(value + (steps * step), low, std::max(low, high));
    if (adjusted != value) {
        value = adjusted;
        _regenerate();
    }
}

void AutoGridPreview::setDetectionRunning(bool running)
{
    if (_detectionRunning != running) {
        _detectionRunning = running;
        emit changed();
    }
}

bool AutoGridPreview::movedTooFar(const QGeoCoordinate& current) const
{
    if (!active() || !_params.origin.isValid() || !current.isValid()) {
        return true;
    }
    return _params.origin.distanceTo(current) > MAX_ORIGIN_MOVE_M;
}

int AutoGridPreview::laneCount() const
{
    if (!active() || (_params.spacingM <= 0.0)) {
        return 0;
    }
    return static_cast<int>(std::lround(_params.depthM / _params.spacingM)) + 1;
}

double AutoGridPreview::depthMin() const
{
    if (_params.spacingM <= 0.0) {
        return _limits.depthMin;
    }
    const double lanes = std::max(1.0, std::ceil(_limits.depthMin / _params.spacingM - PREVIEW_LANE_EPS));
    return lanes * _params.spacingM;
}

double AutoGridPreview::depthMax() const
{
    if (_params.spacingM <= 0.0) {
        return _limits.depthMax;
    }
    const double lanes = std::max(1.0, std::floor(_limits.depthMax / _params.spacingM + PREVIEW_LANE_EPS));
    return lanes * _params.spacingM;
}

double AutoGridPreview::spacingMax() const
{
    return std::max(std::min(_limits.spacingMax, _limits.depthMax), _limits.spacingMin);
}

QString AutoGridPreview::confirmMessage() const
{
    if (!active()) {
        return QString();
    }
    if (!_result.isValid()) {
        return tr("Can't start a grid: %1").arg(_result.error);
    }

    QString message = tr("%1. The cell and the return path must be clear.").arg(_result.summary);
    if (!_detectionRunning) {
        message += QStringLiteral("\n") + tr("Warning: detection is not running.");
    }
    return message;
}

void AutoGridPreview::_regenerate()
{
    _result = GridGenerator::generate(_params);
    _path.clear();
    _outline.clear();
    if (_result.isValid()) {
        for (const GridGenerator::CellPoint& point : _result.points) {
            _path.append(QVariant::fromValue(point.coordinate));
        }

        const double halfWidth = _params.widthM / 2.0;
        const double corners[][2] = {{-halfWidth, 0.0},
                                     {halfWidth, 0.0},
                                     {halfWidth, _params.depthM},
                                     {-halfWidth, _params.depthM},
                                     {-halfWidth, 0.0}};
        for (const auto& corner : corners) {
            _outline.append(QVariant::fromValue(
                GridGenerator::cellToCoordinate(_params.origin, _params.headingDeg, corner[0], corner[1])));
        }
    }
    emit changed();
}
