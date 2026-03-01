#!/usr/bin/env bash
set -euo pipefail

if ! command -v apt-get >/dev/null 2>&1; then
  echo "Error: apt-get is required by install_deps.sh" >&2
  exit 1
fi

SUDO=""
if [[ "${EUID}" -ne 0 ]]; then
  if ! command -v sudo >/dev/null 2>&1; then
    echo "Error: run as root or install sudo" >&2
    exit 1
  fi
  SUDO="sudo"
fi

run_step() {
  local desc="$1"
  shift
  echo "[deps] ${desc}"
  if ! "$@"; then
    echo "Error: failed step '${desc}'" >&2
    exit 1
  fi
}

run_step "apt-get update" ${SUDO} apt-get update
run_step "install build dependencies" ${SUDO} apt-get install -y \
  libcurl4-openssl-dev \
  build-essential \
  jq \
  cmake \
  git \
  libcjson-dev \
  valgrind \
  linux-perf \
  wget \
  curl

echo "Dependency installation complete."
