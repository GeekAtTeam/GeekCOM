# Changelog

All notable changes to this project are documented in this file.

## [0.3.0] — 2026-10-22

GeekCOM `main` is now a Tauri-only desktop project. Historical Qt sources remain
on the `qt` branch for comparison and rollback.

### Added
- Windows NSIS packaging that bundles `WebView2Loader.dll` so MinGW builds start after install

### Changed
- Removed Qt/C++ sources, CMake, and Qt packaging from `main`; recovery path is the `qt` branch
- Replaced the theme select with a sun/moon toggle and shared menu selects for config controls
- Unified UI and monospace font stacks; tightened tabs and segmented control selection
- Brightened the light theme and raised dark-theme contrast for chrome, status, and log surfaces
- Trimmed README and performance tooling to the Tauri workflow

### Fixed
- Restored the executable bit on `packaging/linux/install-local.sh`

## [0.2.0] — 2026-10-16

GeekCOM now uses the Tauri desktop application as its main implementation. The
Qt implementation remains available on the `qt` branch for comparison and
rollback.

### Added
- Tauri 2 + React 19 + Rust desktop application for Linux, Windows, and macOS builds
- Editable serial-port selector with device refresh and manual path entry
- Text, HEX, and aligned HEX/ASCII receive views with bounded history and export
- xterm-based serial terminal with ANSI/VT100 input and display support
- Text/HEX sending, line endings, payload preview, periodic sending, and file transfer
- Persistent light, dark, and system themes using the `#E85552` brand palette
- Rust unit and PTY integration tests, browser UI tests, native desktop smoke tests, and performance benchmarks

### Changed
- Made the Tauri application the main development path while retaining the Qt source for reference
- Virtualized the receive history and batched terminal writes to keep long sessions responsive
- Limited retained receive history to 4,000 records or 2 MiB and reported discarded data
- Updated application icons, workbench branding, and the Ubuntu local installation flow

### Fixed
- Synchronized connection, disconnect, and pause controls across the workbench
- Prevented delayed polling responses from restoring stale data after disconnect
- Preserved partial-write accounting without automatically retrying unknown serial data
- Added strict HEX validation, explicit line endings, and clearer connection and transfer errors
- Corrected Linux application and Dock icon handling

### Validation
- Passed frontend unit tests, browser UI regression tests, and Rust unit/PTY tests
- Completed 60-minute debug and terminal PTY runs at 9,600 B/s without accumulating P95 latency
- Linux desktop is the validated release platform; native Windows and macOS device testing remains pending

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
