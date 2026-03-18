/****************************************************************************
 *
 *   (c) 2009-2016 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "SettingsGroup.h"

class CustomSettings : public SettingsGroup
{
    Q_OBJECT

public:
    CustomSettings(QObject* parent = nullptr);

    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(takeoffAltitude)
    DEFINE_SETTINGFACT(divisions)
    DEFINE_SETTINGFACT(maxPulseStrength)
    DEFINE_SETTINGFACT(k)
    DEFINE_SETTINGFACT(falseAlarmPreset)
    DEFINE_SETTINGFACT(falseAlarmProbability)
    DEFINE_SETTINGFACT(gain)
    DEFINE_SETTINGFACT(detectionFlightMode)
    DEFINE_SETTINGFACT(antennaOffset)
    DEFINE_SETTINGFACT(antennaType)
    DEFINE_SETTINGFACT(rotationKWaitCount)
    DEFINE_SETTINGFACT(useSNRForPulseStrength)
    DEFINE_SETTINGFACT(allowMultiTagDetection)
    DEFINE_SETTINGFACT(rotationType)
    DEFINE_SETTINGFACT(detectionMode)

    enum AntennaType {
        OmniAntenna = 0,
        DirectionalAntenna = 1
    };
    Q_ENUM(AntennaType)

    enum FalseAlarmPreset {
        Aggressive = 0,
        Moderate = 1,
        Conservative = 2,
        Custom = 3
    };
    Q_ENUM(FalseAlarmPreset)

    enum DetectionFlightMode {
        Auto = 0,
        ManualRotation = 1,
        SurveyDetection = 2
    };
    Q_ENUM(DetectionFlightMode)

    static constexpr double AggressivePf  = 5e-2;  // 5%
    static constexpr double ModeratePf    = 1e-2;  // 1%
    static constexpr double ConservativePf = 1e-3;  // 0.1%
};
