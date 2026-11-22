#pragma once

#include "QmlUITestBase.h"

/// The Auto Grid editor over the Fly View: Grid opens the panel and the map preview, the panel's buttons edit the
/// previewed grid, Start grid opens the confirm slider, and confirm flies exactly the edited grid.
class AutoGridEditorUITest : public QmlUITestBase
{
    Q_OBJECT

private slots:
    void _editAndConfirm();
    void _cancelClears();

private:
    /// Survey Detection (the only mode with Grid), take off, and tap Grid.
    void _openEditor(Vehicle* vehicle);
};
