# Auto Detection

Auto Detection is a one-button operation that performs an automated detection flight: takeoff, rotate while capturing tag pulses, then return to launch. This guide covers how to select tags and run an Auto Detection.

## Selecting Tags

Before starting Auto Detection you must select at least one tag to track. Tags are selected from the Controller Indicator in the toolbar.

1. Tap the **Controller Indicator** in the top toolbar. This is the bar that shows signal strength and tag labels for each connected detector.
2. The indicator popup displays all configured tags as a list with checkboxes. Each row shows the tag name and frequency in MHz.
3. Check the box next to each tag you want to track. If multi-tag detection is disabled, selecting a tag automatically deselects any previously selected tag.
4. Close the popup. Your tag selection is saved automatically.

## Starting Auto Detection

When the Detection Flight Mode is set to **Auto**, the Fly View toolbar displays a simplified set of actions with a **Go** button.

1. Ensure the vehicle is armed but **not flying**.
2. Verify the Controller Indicator shows a green status bar (controller heartbeat is active and at least one detector is connected).
3. Tap the **Go** button in the Fly View toolbar on the left side of the screen.
4. A confirmation dialog appears with the message: *"Takeoff, rotate, return."*
5. Tap **Confirm** to begin the automated sequence. Tap **Cancel** to abort.

## What Happens During Auto Detection

Once confirmed, TagTracker runs through the following sequence automatically:

1. **Audio announcement** — "Starting auto detection" is spoken aloud.
2. **Airspy status check** — The SDR hardware status is verified.
3. **Start detection** — Pulse detection begins for all selected tags.
4. **Takeoff** — The vehicle climbs to the configured takeoff altitude.
5. **Rotate and capture** — The vehicle performs a full 360° rotation in place, capturing pulses at each rotation division. Detected pulses appear on the map in real time.
6. **Stop detection** — Pulse detection is stopped.
7. **Audio announcement** — "Auto detection complete. Returning" is spoken.
8. **Return to launch** — The vehicle automatically returns to its launch point and lands.

## During the Flight

While the auto detection sequence is running:

- The **Controller Indicator** in the toolbar shows live signal strength for each detector. A green bar indicates active pulses, yellow means the last pulse is stale, and red means the detector heartbeat is lost.
- Detected pulses are plotted on the **map** as they are received, showing the estimated bearing and strength for each tag.
- You can tap **Pause** to pause the vehicle or **RTL** to abort and return home at any time.

## After Detection

When the sequence completes and the vehicle has landed, review the pulse locations plotted on the map. Detection logs can be saved or downloaded using the **Save Logs** and **Download** actions available in the Fly View additional actions menu.
