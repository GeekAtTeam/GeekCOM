#!/usr/bin/env bash
# 将 GeekCOM 安装到 ~/.local（二进制 + .desktop + 图标），便于 GNOME/Ubuntu Dock 显示正确图标。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "请先编译：cmake -S \"${ROOT_DIR}\" -B build && cmake --build build" >&2
  exit 1
fi

PREFIX="${HOME}/.local"
BIN_PATH="${PREFIX}/bin/GeekCOM"
DESKTOP_FILE="${PREFIX}/share/applications/geekcom.desktop"

echo "Installing to ${PREFIX} ..."
cmake --install "${BUILD_DIR}" --prefix "${PREFIX}"

# GNOME 从菜单启动时未必继承 shell 的 PATH；改为绝对路径，避免找不到二进制、Dock 退回占位图标
if [[ -f "${DESKTOP_FILE}" && -x "${BIN_PATH}" ]]; then
  sed -i "s|^Exec=.*|Exec=${BIN_PATH}|" "${DESKTOP_FILE}"
  sed -i "s|^TryExec=.*|TryExec=${BIN_PATH}|" "${DESKTOP_FILE}"
fi

if command -v desktop-file-validate >/dev/null 2>&1; then
  desktop-file-validate "${DESKTOP_FILE}" || true
fi

update-desktop-database "${PREFIX}/share/applications" 2>/dev/null || true
gtk-update-icon-cache -f -t "${PREFIX}/share/icons/hicolor" 2>/dev/null || true

echo "完成。请从「应用程序」网格打开 GeekCOM（勿长期用 ./build/GeekCOM），再固定到 Dock。"
echo "若仍无图标：注销重新登录一次，或确认 Wayland 会话下已用本脚本安装（xprop 仅对 X11 窗口有效）。"
