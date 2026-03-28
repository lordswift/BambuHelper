# BambuHelper v2.4 Release Notes

## OTA firmware update (NEW)

- **Web-based OTA updates** - upload new firmware directly from the web UI, no USB cable needed
- Upload a .bin file in the "WiFi & System" section, progress bar shows upload status
- All settings preserved after update (WiFi, printers, cloud tokens, display config)
- Client-side validation (file type, size) + server-side ESP32 magic byte check
- MQTT disconnects during upload to free memory, device restarts automatically after success
- Current firmware version shown in web UI

## Date format fix

- **Locale-aware date format** - clock screens (standard and Pong) now use DD.MM.YYYY in 24h mode and MM/DD/YYYY in 12h mode (previously always DD.MM.YYYY regardless of setting)
- **ETA date format** - print ETA on the dashboard now uses MM/DD format in 12h mode (was DD.MM)

## Screen wakeup fix

- **Wake from screen off on print end** - when display auto-off was active and printer left FINISH state, the screen now properly wakes up to IDLE instead of staying off until button press
- SCREEN_CLOCK remains sticky (only button or new print exits it)

## Night mode (NEW)

- **Scheduled display dimming** - automatically dim the screen during set hours (e.g. 22:00-07:00), with a separate brightness slider for the night period
- Supports wrap-around schedules (across midnight)
- Night brightness respects all wake-up paths (button press, print start, screen off exit)
- Requires NTP time sync to activate
- Configurable in the Display section of the web UI

## Live brightness preview

- **Instant brightness feedback** - moving the brightness slider (normal or night) now updates the display in real-time without pressing Save
- Lightweight `/brightness` endpoint - no NVS writes until Apply is clicked

## H2 dual nozzle - right nozzle filament display (FIX)

- **Right nozzle filament now shown** - previously only the left nozzle's filament was displayed (tray_now only reports left nozzle). Now uses per-nozzle `snow` field from `extruder.info[]` to determine the active tray for each nozzle independently
- Filament color and name display correctly regardless of which nozzle is printing
- Works with any AMS configuration across both nozzles

## Door open acknowledge after print (NEW)

- **Wait for door open before screen timeout** - on printers with a door sensor (H2 series), the finish screen stays active until the enclosure door is opened, treating it as acknowledgment that the print was removed
- After door opens, the normal display timeout starts (configurable minutes to clock/off)
- "Open door to dismiss" indicator shown in orange on the finish screen while waiting
- Optional - enable via checkbox in Display settings ("Wait for door open after print")
- Printers without a door sensor ignore this setting and use normal timeout behavior
- Door sensor state parsed from MQTT `stat` field (hex, bit 0x00800000)

## Bug fixes

- **MQTT parser buffer overread** - `deserializeJson()` for extruder and vt_tray sections was called without a length limit, allowing reads past the MQTT payload buffer. Now bounded by payload size.
- **Timezone migration never persisted** - NVS Preferences were opened read-only during `loadSettings()`, so the old-to-new timezone format migration wrote correctly to RAM but silently failed to save to flash. Migration re-ran on every boot. Now properly reopens in write mode for migration.
- **Cloud printer userId self-heal broken** - `isPrinterConfigured()` required `cloudUserId` for cloud slots, but `initBambuMqtt()` only tried userId extraction when `isPrinterConfigured()` was true AND `cloudUserId` was empty - a logical contradiction. Cloud slots with token+serial but missing userId could never recover without manual re-save.
- **Stale cloud data showing wrong screen** - when cloud printer (H2C) stopped sending MQTT data for 5+ minutes, the display fell back to 2-gauge idle screen with "RUNNING" text instead of the 6-gauge printing dashboard. Stale timeout now properly resets both the printing flag and gcode state.
- **Idle/blank screen never triggered with offline printers** - when both printers were off or unreachable at startup, the display stayed on the "Connecting to Printer" screen forever and never transitioned to clock or blank screen. Idle timeout now also applies to the connecting screen.
- **No path from idle to screen off** - when "Show clock after finish" was disabled, the idle screen had no timeout path to screen off (only idle-to-clock existed). Both clock and off transitions now work from idle and connecting screens.

## ESP32-2432S028 (CYD) support

- **New build target** - ESP32-2432S028 ("Cheap Yellow Display") with ILI9341 2.8" 320x240 in portrait mode (240x320)
- Separate PlatformIO environment (`pio run -e cyd`), does not affect the existing S3+ST7789 build
- Layout system with per-display profile constants - adding future displays requires only a new `layout_*.h` file
- **XPT2046 touchscreen** as button replacement - tap anywhere to wake display or cycle printers
- New button type "Touchscreen (XPT2046)" in web UI, auto-enabled on CYD builds
- `merge_bins.py` supports `--board cyd` for CYD firmware packaging (bootloader offset 0x1000)
- **Full landscape support** - rotation 90/270 now works correctly on CYD (320x240 actual). Idle, finished, and printing screens use runtime dimensions instead of compile-time portrait constants. No more off-screen elements.
- **AMS in both orientations** - AMS filament visualization renders in portrait (horizontal strip) and landscape (vertical sidebar). Previously landscape was blocked by a `!land` guard despite the renderer being complete.
- **CYD Extra Area Mode (NEW)** - new setting in Display section of web UI: choose between "AMS Filament" or "Extra Gauges (Chamber + Heatbreak Fan)" for the extra CYD screen space. In portrait, extra gauges appear between the gauge rows and ETA. In landscape, they appear in the right sidebar. Setting has no effect on 240x240 displays.

## Build stats

- **ESP32-S3** (lolin_s3_mini): Flash 90.3% (1184KB), RAM 15.7% (51KB)
- **CYD** (esp32dev): Flash 94.9% (1244KB), RAM 16.1% (52KB)
