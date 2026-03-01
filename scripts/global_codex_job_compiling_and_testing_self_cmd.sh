#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

echo "[pipeline] clean + build"
make clean
make build

echo "[pipeline] memory baseline"
make memory_baseline

if curl -fsS http://localhost:11434/api/tags >/dev/null 2>&1; then
  echo "[pipeline] running demo chat"
  make test_demo_chat
  echo "[pipeline] running demo coding"
  make test_demo_codex
else
  echo "[pipeline] Ollama not reachable at http://localhost:11434, skipping demo chat/coding tests"
fi

echo "[pipeline] packaging"
make package

echo "[pipeline] completed"
