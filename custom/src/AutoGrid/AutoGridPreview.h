#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtPositioning/QGeoCoordinate>

#include "GridGenerator.h"

/// The Auto Grid preview (docs/AUTO_GRID_SPEC.md §6 step 3): from tapping Grid until the grid machine takes over.
///
/// Tap latches the pose and opens the editor panel (editing). Start grid opens the confirm slider (confirming).
/// Sliding hands the params to the grid machine (confirmed), and the preview stays drawn until QGC draws the uploaded
/// mission. Cancel at any point clears it. Width, depth and lane spacing change for this grid only, and every change
/// regenerates the path, outline and confirm text. Depth is kept a whole number of lane spacings, so the last lane is
/// never a clamped short one. Pure: no vehicle, settings or UI access, and nothing is sent.
class AutoGridPreview : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY changed)
    Q_PROPERTY(bool confirming READ confirming NOTIFY changed)
    Q_PROPERTY(bool confirmed READ confirmed NOTIFY changed)
    Q_PROPERTY(bool valid READ valid NOTIFY changed)
    Q_PROPERTY(double width READ width NOTIFY changed)
    Q_PROPERTY(double depth READ depth NOTIFY changed)
    Q_PROPERTY(int laneCount READ laneCount NOTIFY changed)
    Q_PROPERTY(double spacing READ spacing NOTIFY changed)
    Q_PROPERTY(double widthMin READ widthMin NOTIFY changed)
    Q_PROPERTY(double widthMax READ widthMax NOTIFY changed)
    Q_PROPERTY(double depthMin READ depthMin NOTIFY changed)
    Q_PROPERTY(double depthMax READ depthMax NOTIFY changed)
    Q_PROPERTY(double spacingMin READ spacingMin NOTIFY changed)
    Q_PROPERTY(double spacingMax READ spacingMax NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString confirmMessage READ confirmMessage NOTIFY changed)
    Q_PROPERTY(QGeoCoordinate origin READ origin NOTIFY changed)
    Q_PROPERTY(QVariantList path READ path NOTIFY changed)
    Q_PROPERTY(QVariantList outline READ outline NOTIFY changed)

public:
    /// The settings' min/max for the three adjustable values
    struct Limits
    {
        double widthMin = 0.0;
        double widthMax = 0.0;
        double depthMin = 0.0;
        double depthMax = 0.0;
        double spacingMin = 0.0;
        double spacingMax = 0.0;
    };

    /// Width is the lane length, so it steps independently of the spacing. Depth steps by one lane spacing.
    static constexpr double WIDTH_STEP_M = 20.0;
    static constexpr double SPACING_STEP_M = 5.0;
    /// Confirm refuses when the vehicle is further than this from the latched origin
    static constexpr double MAX_ORIGIN_MOVE_M = 10.0;

    explicit AutoGridPreview(QObject* parent = nullptr);

    /// Tap: latches the params (pose plus the settings' values), snaps the depth to whole lanes, generates the grid
    /// and opens the editor. Other invalid values from the settings show their error until they're adjusted.
    void start(const GridGenerator::Params& latched, const Limits& limits);

    /// Start grid: closes the editor for the confirm slider. False (and no change) unless editing a valid grid.
    Q_INVOKABLE bool requestConfirm();

    /// Slid to confirm: the grid machine has the params. The preview stays drawn until clear().
    void setConfirmed();

    /// Ends the preview (Cancel, slider cancelled or closed, refused confirm, or the uploaded mission is drawn).
    Q_INVOKABLE void clear();

    /// While editing: steps the value by `steps` × its step (depth: × the lane spacing), clamped to the limits, then
    /// regenerates. A spacing change re-snaps the depth to the nearest whole number of lanes within the limits.
    Q_INVOKABLE void adjustWidth(int steps);
    Q_INVOKABLE void adjustDepth(int steps);
    Q_INVOKABLE void adjustSpacing(int steps);

    /// Adds the "detection is not running" warning to the confirm text when false.
    void setDetectionRunning(bool running);

    /// True when `current` is more than MAX_ORIGIN_MOVE_M from the latched origin (or either position is invalid).
    [[nodiscard]] bool movedTooFar(const QGeoCoordinate& current) const;

    bool active() const { return _stage != Stage::Idle; }

    bool editing() const { return _stage == Stage::Editing; }

    bool confirming() const { return _stage == Stage::Confirming; }

    bool confirmed() const { return _stage == Stage::Confirmed; }

    bool valid() const { return active() && _result.isValid(); }

    double width() const { return _params.widthM; }

    double depth() const { return _params.depthM; }

    double spacing() const { return _params.spacingM; }

    double widthMin() const { return _limits.widthMin; }

    double widthMax() const { return _limits.widthMax; }

    /// Lanes flown, including the one at the origin: depth / spacing + 1
    int laneCount() const;

    /// Effective: the smallest whole number of lanes (at least one spacing) at or above the settings' minimum
    double depthMin() const;

    /// Effective: the largest whole number of lanes at or below the settings' maximum
    double depthMax() const;

    double spacingMin() const { return _limits.spacingMin; }

    /// Effective: one spacing must fit within the settings' depth maximum
    double spacingMax() const;

    QString summary() const { return valid() ? _result.summary : QString(); }

    /// The validate() error, empty when valid or inactive
    QString error() const { return active() ? _result.error : QString(); }

    QString confirmMessage() const;

    QGeoCoordinate origin() const { return active() ? _params.origin : QGeoCoordinate(); }

    QVariantList path() const { return _path; }

    QVariantList outline() const { return _outline; }

    const GridGenerator::Params& params() const { return _params; }

    const GridGenerator::Result& result() const { return _result; }

signals:
    void changed();

private:
    enum class Stage
    {
        Idle,
        Editing,
        Confirming,
        Confirmed,
    };

    void _adjust(double& value, int steps, double step, double low, double high);
    /// Depth to the nearest whole number of lane spacings within depthMin()..depthMax()
    void _snapDepth();
    void _regenerate();

    Stage _stage = Stage::Idle;
    bool _detectionRunning = false;
    GridGenerator::Params _params;
    GridGenerator::Result _result;
    Limits _limits;
    QVariantList _path;
    QVariantList _outline;
};
