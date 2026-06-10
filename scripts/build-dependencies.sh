#!/usr/bin/env bash
set -euo pipefail

# Folder that contains the dependencies CMake project plus its CMakePresets.json
DEPS_DIR="${DEPS_DIR:-Dependencies}"
CLEAN=0

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

if [[ "$DEPS_DIR" = /* ]]; then
  SRC_DIR="$DEPS_DIR"
else
  SRC_DIR="${APP_DIR}/${DEPS_DIR}"
fi

if [[ ! -d "$SRC_DIR" ]]; then
  echo "Dependencies directory not found: $SRC_DIR" >&2
  exit 1
fi

SRC_DIR="$(cd -- "$SRC_DIR" && pwd)"

list_configure_presets() {
  # Extract only the configure preset names (the quoted strings) from:
  #   cmake -S <src> --list-presets
  cmake -S "$SRC_DIR" --list-presets |
    awk '
      BEGIN { in_cfg=0 }
      /Available configure presets:/ { in_cfg=1; next }
      /^Available / { in_cfg=0 }
      in_cfg {
        if (match($0, /"([^"]+)"/, m)) print m[1]
      }
    '
}

usage() {
  cat <<EOF
Usage:
  $(basename "$0")                 # configure+build ALL configure presets
  $(basename "$0") <preset> [...]  # configure+build only the given presets
  $(basename "$0") --clean <preset> # delete build/install for the preset, then configure+build
  $(basename "$0") --list          # list available configure presets

Env:
  DEPS_DIR=Dependencies (default, relative to the application root)  # set if your folder name differs
EOF
}

clean_preset_dirs() {
  local preset="$1"
  local dir

  for dir in \
    "${SRC_DIR}/build/${preset}" \
    "${SRC_DIR}/install/${preset}"; do
    if [[ -e "$dir" ]]; then
      local resolved
      resolved="$(realpath "$dir")"

      case "$resolved" in
        "${SRC_DIR}"/*) ;;
        *)
          echo "Refusing to remove path outside dependencies root: $resolved" >&2
          exit 1
          ;;
      esac

      echo "==> [$preset] clean $resolved"
      rm -rf -- "$resolved"
    fi
  done
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

if [[ "${1:-}" == "--list" ]]; then
  list_configure_presets
  exit 0
fi

if [[ "${1:-}" == "--clean" ]]; then
  CLEAN=1
  shift
fi

if [[ $# -gt 0 ]]; then
  PRESETS=("$@")
else
  mapfile -t PRESETS < <(list_configure_presets)
fi

if [[ ${#PRESETS[@]} -eq 0 ]]; then
  echo "No configure presets found (looked in: ${SRC_DIR}/CMakePresets.json and/or CMakeUserPresets.json)."
  exit 1
fi

for preset in "${PRESETS[@]}"; do
  echo "==> [$preset] configure"

  if [[ "$CLEAN" -eq 1 ]]; then
    clean_preset_dirs "$preset"
  fi

  install_dir="${SRC_DIR}/install/${preset}"
  mkdir -p "$install_dir"

  cmake --preset "$preset" \
    -S "$SRC_DIR"

  echo "==> [$preset] build"
  cmake --build "${SRC_DIR}/build/${preset}"
done
