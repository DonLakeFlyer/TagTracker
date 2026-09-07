/****************************************************************************
 *
 *   (c) 2009-2016 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtQmlIntegration/QtQmlIntegration>

#include "SettingsGroup.h"

/// Registered at runtime via CustomPlugin::registerCustomSettings so QML resolves it as
/// QGroundControl.settingsManager.customSettings.<factName>.
class CustomSettings : public SettingsGroup
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

public:
    CustomSettings(QObject* parent = nullptr);

    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(takeoffAltitude)
    DEFINE_SETTINGFACT(divisions)
    DEFINE_SETTINGFACT(maxPulseStrength)
    DEFINE_SETTINGFACT(k)
    DEFINE_SETTINGFACT(falseAlarmProbability)
    DEFINE_SETTINGFACT(gain)
    DEFINE_SETTINGFACT(detectionFlightMode)
    DEFINE_SETTINGFACT(antennaOffset)
    DEFINE_SETTINGFACT(antennaType)
    DEFINE_SETTINGFACT(useSNRForPulseStrength)
    DEFINE_SETTINGFACT(allowMultiTagDetection)
    DEFINE_SETTINGFACT(detectionMargin)
    DEFINE_SETTINGFACT(confidenceRatio)
    DEFINE_SETTINGFACT(debugDetector)
    DEFINE_SETTINGFACT(pythonPreLockK)
    DEFINE_SETTINGFACT(pythonPostLockK)
    DEFINE_SETTINGFACT(pythonFalseAlarmMode)
    DEFINE_SETTINGFACT(pythonFalseAlarmProbability)

    enum AntennaType {
        OmniAntenna = 0,
        DirectionalAntenna = 1
    };
    Q_ENUM(AntennaType)

    enum FalseAlarmMode {
        Normal = 0,
        StressTest = 1,
        Custom = 2
    };
    Q_ENUM(FalseAlarmMode)

    // Rotation modes use the Python detector; Survey Detection uses uavrt_detection.
    enum DetectionFlightMode {
        Auto = 0,
        ManualRotation = 1,
        SurveyDetection = 2
    };
    Q_ENUM(DetectionFlightMode)

    bool isPythonMode() { return detectionFlightMode()->rawValue().toUInt() != SurveyDetection; }

    // pf only sets acquisition range; false locks are rejected downstream. See
    // MavlinkTagController2 DETECTOR_AMPLITUDE_ANALYSIS.md section 8.
    static constexpr double NormalPf     = 5e-2;
    static constexpr double StressTestPf = 0.25;
};
