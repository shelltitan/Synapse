#!/usr/bin/env bash
set -euo pipefail

# Folder containing the dependency CMake project and CMakePresets.json.
DEPS_DIR="${DEPS_DIR:-Dependencies}"
CLEAN=0
REQUESTED_CONFIG=""

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
  cmake -S "$SRC_DIR" --list-presets |
    awk '
      BEGIN { in_cfg=0 }
      /Available configure presets:/ { in_cfg=1; next }
      /^Available / { in_cfg=0 }
      in_cfg && match($0, /"[^"]+"/) {
        print substr($0, RSTART + 1, RLENGTH - 2)
      }
    '
}

usage() {
  cat <<EOF
Usage:
  $(basename "$0")                         # build all configure presets
  $(basename "$0") <preset> [...]          # build selected presets
  $(basename "$0") --clean <preset>        # clean, configure and build
  $(basename "$0") --config Release <preset>
  $(basename "$0") --list

Env:
  DEPS_DIR=Dependencies (default, relative to the application root)
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

POSITIONAL=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean)
      CLEAN=1
      shift
      ;;
    --config)
      if [[ $# -lt 2 || ( "$2" != "Debug" && "$2" != "Release" ) ]]; then
        echo "--config requires Debug or Release" >&2
        exit 2
      fi
      REQUESTED_CONFIG="$2"
      shift 2
      ;;
    *)
      POSITIONAL+=("$1")
      shift
      ;;
  esac
done
set -- "${POSITIONAL[@]}"

if [[ $# -gt 0 ]]; then
  PRESETS=("$@")
else
  mapfile -t PRESETS < <(list_configure_presets)
fi

if [[ ${#PRESETS[@]} -eq 0 ]]; then
  echo "No configure presets found in ${SRC_DIR}." >&2
  exit 1
fi

for preset in "${PRESETS[@]}"; do
  preset_lower="${preset,,}"
  preset_configuration=""
  if [[ "$preset_lower" == *-debug ]]; then
    preset_configuration="Debug"
  elif [[ "$preset_lower" == *-release ]]; then
    preset_configuration="Release"
  fi

  if [[ -n "$REQUESTED_CONFIG" && -n "$preset_configuration" &&
        "$REQUESTED_CONFIG" != "$preset_configuration" ]]; then
    echo "Preset '$preset' is a $preset_configuration single-config preset; requested configuration: $REQUESTED_CONFIG." >&2
    exit 2
  fi

  echo "==> [$preset] configure"

  if [[ "$CLEAN" -eq 1 ]]; then
    clean_preset_dirs "$preset"
  fi

  install_dir="${SRC_DIR}/install/${preset}"
  mkdir -p "$install_dir"
  cmake --preset "$preset" -S "$SRC_DIR"

  if [[ -n "$REQUESTED_CONFIG" ]]; then
    configurations=("$REQUESTED_CONFIG")
  elif [[ -n "$preset_configuration" ]]; then
    configurations=("$preset_configuration")
  else
    # A multi-config dependency prefix must contain both runtime variants.
    configurations=(Debug Release)
  fi

  for configuration in "${configurations[@]}"; do
    echo "==> [$preset] build $configuration"
    cmake --build "${SRC_DIR}/build/${preset}" --config "$configuration"
  done
done
