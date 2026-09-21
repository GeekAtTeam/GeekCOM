#!/usr/bin/env bash
# Install a standalone Tauri build and its icon for the current desktop user.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BINARY="${1:-${ROOT_DIR}/src-tauri/target/debug/geekcom}"
PREFIX="${HOME}/.local"
APP_DIR="${PREFIX}/share/geekcom-tauri"
DESKTOP_FILE="${PREFIX}/share/applications/geekcom.desktop"

if [[ ! -x "${BINARY}" ]]; then
  echo "Build first: npm run tauri build -- --debug --no-bundle" >&2
  exit 1
fi

install -d "${PREFIX}/bin" "${PREFIX}/share/applications" "${APP_DIR}"
if [[ -f "${DESKTOP_FILE}" ]]; then
  cp -p "${DESKTOP_FILE}" "${APP_DIR}/geekcom.desktop.backup.$(date +%s%N)"
fi
# Replace atomically so an already running instance can finish normally.
TEMP_BINARY="${PREFIX}/bin/.geekcom.install.$$"
trap 'rm -f "${TEMP_BINARY}"' EXIT
install -m 755 "${BINARY}" "${TEMP_BINARY}"
mv -f "${TEMP_BINARY}" "${PREFIX}/bin/geekcom"
ICON_HASH="$(sha256sum "${ROOT_DIR}/src-tauri/icons/icon.png" | cut -d ' ' -f 1)"
ICON_PATH="${APP_DIR}/icon-${ICON_HASH}.png"
install -m 644 "${ROOT_DIR}/src-tauri/icons/icon.png" "${ICON_PATH}"

cat > "${DESKTOP_FILE}" <<EOF
[Desktop Entry]
Type=Application
Name=GeekCOM
Comment=串口调试工作台 · Tauri
Exec="${PREFIX}/bin/geekcom"
Icon=${ICON_PATH}
Terminal=false
Categories=Development;Electronics;
StartupNotify=true
StartupWMClass=geekcom
EOF

if command -v desktop-file-validate >/dev/null; then
  desktop-file-validate "${DESKTOP_FILE}"
fi
if command -v update-desktop-database >/dev/null; then
  update-desktop-database "${PREFIX}/share/applications"
fi
echo "Installed: ${PREFIX}/bin/geekcom"
echo "Desktop entry: ${DESKTOP_FILE} (previous entry backed up in ${APP_DIR})"
