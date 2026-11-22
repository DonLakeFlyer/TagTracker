#include "AutoGridEditorUITest.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtQuick/QQuickItem>
#include <QtTest/QTest>

#include "AutoGridPreview.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "Fact.h"
#include "GridGenerator.h"
#include "MissionItem.h"
#include "MissionManager.h"
#include "MockLink.h"
#include "Vehicle.h"

UT_REGISTER_TEST(AutoGridEditorUITest, TestLabel::Integration)

namespace {

CustomPlugin* editorTestPlugin()
{
    return qobject_cast<CustomPlugin*>(CustomPlugin::instance());
}

}  // namespace

void AutoGridEditorUITest::_openEditor(Vehicle* vehicle)
{
    // The airborne transition creates QGCPressure, which warns on hosts without a pressure backend
    ignoreLogMessage("Utilities.QGCSensors", QtWarningMsg, QRegularExpression(QStringLiteral("[Pp]ressure")));
    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_NAV_TAKEOFF, false /* showError */, 0.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, 30.0f /* altitude */);
    QVERIFY_TRUE_WAIT(vehicle->airborne(), TestTimeout::longMs());
    // Grids need at least GridGenerator::MIN_GRID_ALTITUDE_M
    QVERIFY_TRUE_WAIT(vehicle->altitudeRelative()->rawValue().toDouble() > 15.0, TestTimeout::longMs());

    QVERIFY(clickButton(QStringLiteral("flyViewToolStrip_gridButton")));
    QVERIFY(verifyVisibility(QStringLiteral("autoGridEditorPanel"), true, QStringLiteral("after tapping Grid")));
}

void AutoGridEditorUITest::_editAndConfirm()
{
    CustomSettings* settings = editorTestPlugin()->customSettings();
    Fact* const modeFact = settings->detectionFlightMode();
    const QVariant savedMode = modeFact->rawValue();
    const auto restoreMode = qScopeGuard([modeFact, savedMode] { modeFact->setRawValue(savedMode); });
    modeFact->setRawValue(CustomSettings::SurveyDetection);

    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [this, settings](QPointer<MockLink>, Vehicle* vehicle) {
            AutoGridPreview* preview = editorTestPlugin()->gridPreview();
            _openEditor(vehicle);
            if (QTest::currentTestFailed()) {
                return;
            }
            QVERIFY(preview->editing());
            QVERIFY(!preview->path().isEmpty());

            // The panel's buttons edit this grid only
            const double width = settings->gridWidth()->rawValue().toDouble();
            QVERIFY(clickButton(QStringLiteral("autoGridWidthPlus")));
            QCOMPARE(preview->width(), width + AutoGridPreview::WIDTH_STEP_M);
            QCOMPARE(settings->gridWidth()->rawValue().toDouble(), width);
            QVERIFY(verifyText(QStringLiteral("autoGridSummaryLabel"), preview->confirmMessage(),
                               QStringLiteral("summary after Width +")));
            QVERIFY(verifyEnabled(QStringLiteral("autoGridStartButton"), true, QStringLiteral("valid grid")));

            // Start grid: the panel closes and the confirm slider opens; the preview stays
            const GridGenerator::Result previewed = preview->result();
            QVERIFY(clickButton(QStringLiteral("autoGridStartButton")));
            QVERIFY(verifyVisibility(QStringLiteral("autoGridEditorPanel"), false, QStringLiteral("after Start grid")));
            QVERIFY(preview->confirming());
            QQuickItem* const confirmButton =
                findVisibleItem(_rootItem, QStringLiteral("guidedActionConfirmButton"), 3000);
            QVERIFY2(confirmButton, "Confirm slider never opened");

            // Slide to confirm: the edited grid is uploaded and the preview hands over to QGC's mission drawing
            QVERIFY(QMetaObject::invokeMethod(confirmButton, "activated"));
            QVERIFY(editorTestPlugin()->gridInProgress());
            QVERIFY_TRUE_WAIT(!preview->active(), TestTimeout::longMs());
            QCOMPARE(vehicle->missionManager()->missionItems().count(), previewed.items.count());

            emit vehicle->flightModeChanged(QStringLiteral("Position"));
            QVERIFY_TRUE_WAIT(!editorTestPlugin()->gridInProgress(), TestTimeout::longMs());
        });
}

void AutoGridEditorUITest::_cancelClears()
{
    Fact* const modeFact = editorTestPlugin()->customSettings()->detectionFlightMode();
    const QVariant savedMode = modeFact->rawValue();
    const auto restoreMode = qScopeGuard([modeFact, savedMode] { modeFact->setRawValue(savedMode); });
    modeFact->setRawValue(CustomSettings::SurveyDetection);

    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [this](QPointer<MockLink> mockLink, Vehicle* vehicle) {
            AutoGridPreview* preview = editorTestPlugin()->gridPreview();
            _openEditor(vehicle);
            if (QTest::currentTestFailed()) {
                return;
            }
            mockLink->clearReceivedMavlinkMessageCounts();

            QVERIFY(clickButton(QStringLiteral("autoGridCancelButton")));
            QVERIFY(verifyVisibility(QStringLiteral("autoGridEditorPanel"), false, QStringLiteral("after Cancel")));
            QVERIFY(!preview->active());

            // Start grid then cancelling the slider also clears it
            QVERIFY(clickButton(QStringLiteral("flyViewToolStrip_gridButton")));
            QVERIFY(clickButton(QStringLiteral("autoGridStartButton")));
            QVERIFY(preview->confirming());
            // The slider's close button: GuidedActionConfirm > its row > the confirm button
            QQuickItem* const confirmButton =
                findVisibleItem(_rootItem, QStringLiteral("guidedActionConfirmButton"), 3000);
            QVERIFY(confirmButton && confirmButton->parentItem());
            QVERIFY(QMetaObject::invokeMethod(confirmButton->parentItem()->parentItem(), "confirmCancelled",
                                              Q_ARG(QVariant, QVariant())));
            QVERIFY_TRUE_WAIT(!preview->active(), TestTimeout::shortMs());

            QCOMPARE(mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_MISSION_COUNT), 0);
            QVERIFY(!editorTestPlugin()->gridInProgress());
        });
}
