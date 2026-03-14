# BambuHelper v2.0 Release Notes

## Bambu Cloud support (NEW)

- **Cloud mode for all printers** — connect any Bambu printer (H2C, H2D, H2S, P2S, P1, X1, A1...) via Bambu Cloud MQTT, no LAN mode required
- **Token-based authentication** — paste your access token from bambulab.com cookies, no email/password stored on device
- **Region support** — Americas (US), Europe (EU), China (CN) server selection
- **Python helper script** (`tools/get_token.py`) — get your token and printer serial via command line

## MQTT improvements

- **Proper memory cleanup** — TLS + MQTT clients (~40KB) are freed on disconnect, preventing heap exhaustion across reconnects
- **Cloud pushall backoff** — reduces polling when printer is idle, goes fully passive after 10 minutes (relies on broker's delta stream)
- **Longer stale timeout for cloud** — cloud sends less frequently, timeout adjusted to avoid false disconnects
- **Heap check before connect** — skips TLS allocation if free heap is below 40KB

## Web interface

- **Simplified cloud setup** — token paste + serial number entry, inline instructions for extracting token from browser
- **Server region dropdown** — select US/EU/CN to match your Bambu account
- **Collapsible sections** — Printer, Display, Diagnostics, WiFi & System
- **Diagnostics panel** — MQTT connection status, attempt count, messages received, free heap, uptime
- **Verbose debug logging toggle** — enable/disable detailed serial output from web UI

## Display

- **Clock mode** — after a print finishes, the display shows a digital clock with date instead of turning off (configurable in Display settings, enabled by default)
- Removed redundant "Enable Monitoring" checkbox — monitoring is always active when a printer is configured

## Other

- Flash usage: 80.5% (down from 80.6% in v1.1)
- RAM usage: 14.8% (unchanged)
