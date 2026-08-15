# Changelog

All notable changes to this project are documented in this file.

## [0.1.0] — 2026-08-15

First public release.

### Added
- Serial debug mode: RX/TX panels, hex/ASCII, timestamps, file send, auto-send
- Serial terminal mode: basic VT100 handling and local echo
- ThemeManager: follow system / light / dark (View → Appearance)
- Brand accent color aligned with the logo (`#E42C2C`)
- Linux desktop icon install (`packaging/linux/install-local.sh`)
- Application version taken from CMake `PROJECT_VERSION` (window title included)

### Fixed
- Ubuntu/GNOME in-window menu bar visibility for appearance switching
- Accent focus ring on the receive/log pane removed for a cleaner layout
