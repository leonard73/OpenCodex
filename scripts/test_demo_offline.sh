#!/usr/bin/env bash
set -euo pipefail

MODEL_PATH="${OFFLINE_MODEL_PATH:-/home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf}"
MODEL_NAME="${OFFLINE_OLLAMA_MODEL:-opencodex-local-deepseek-r1-1_5b-q4}"
BASE_URL="${OFFLINE_OLLAMA_BASE_URL:-http://127.0.0.1:11434}"
PROMPT="${OFFLINE_PROMPT:-Say hello from OpenCodex in one sentence.}"
BACKEND="${OFFLINE_BACKEND:-auto}"
MODE="${OFFLINE_MODE:-chat}"
TOKENS="${OFFLINE_TOKENS:-96}"
THREADS="${OFFLINE_THREADS:-$(nproc 2>/dev/null || echo 4)}"
RECREATE_MODEL="${OFFLINE_RECREATE_MODEL:-1}"
SYSTEM_PROMPT="${OFFLINE_SYSTEM_PROMPT:-You are OpenCodex assistant. Be natural and helpful. Give direct final answers. Do not output internal reasoning or chain-of-thought.}"
TEMPERATURE="${OFFLINE_TEMPERATURE:-0.3}"
TOP_P="${OFFLINE_TOP_P:-0.9}"
TOP_K="${OFFLINE_TOP_K:-40}"
SEED="${OFFLINE_SEED:-42}"
NUM_CTX="${OFFLINE_NUM_CTX:-4096}"
NUM_PREDICT="${OFFLINE_NUM_PREDICT:-256}"
CHAT_BIN="${CHAT_BIN:-./build/opencodex-chat}"
OLLAMA_LOG="${OFFLINE_OLLAMA_LOG:-build/ollama-serve.log}"
OFFLINE_HOME="${OFFLINE_HOME:-$(pwd)/build/offline_home}"

require_cmd() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Error: required command '$cmd' is not installed" >&2
    exit 1
  fi
}

wait_for_ollama() {
  local i
  for i in $(seq 1 30); do
    if curl -fsS "${BASE_URL}/api/tags" >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  return 1
}

mkdir -p "${OFFLINE_HOME}"
export HOME="${OFFLINE_HOME}"

if [[ ! -r "${MODEL_PATH}" ]]; then
  echo "Error: model file is not readable: ${MODEL_PATH}" >&2
  exit 1
fi

if [[ "${BACKEND}" == "auto" ]]; then
  if command -v llama-cli >/dev/null 2>&1; then
    BACKEND="llama-cli"
  elif command -v ollama >/dev/null 2>&1; then
    BACKEND="ollama"
  else
    echo "Error: neither 'llama-cli' nor 'ollama' is installed" >&2
    exit 1
  fi
fi

if [[ "${MODE}" != "chat" && "${MODE}" != "single" ]]; then
  echo "Error: unsupported OFFLINE_MODE='${MODE}' (use chat|single)" >&2
  exit 1
fi

if [[ "${BACKEND}" == "llama-cli" ]]; then
  require_cmd llama-cli
  if [[ "${MODE}" == "chat" ]]; then
    echo "[offline] Interactive chat via llama-cli (type Ctrl+C to exit)"
    exec llama-cli -m "${MODEL_PATH}" -i -cnv -n "${TOKENS}" -t "${THREADS}" \
      --temp "${TEMPERATURE}" --top-p "${TOP_P}" --top-k "${TOP_K}" --seed "${SEED}"
  fi
  echo "[offline] Single response via llama-cli"
  exec llama-cli -m "${MODEL_PATH}" -p "${PROMPT}" -n "${TOKENS}" -t "${THREADS}" \
    --temp "${TEMPERATURE}" --top-p "${TOP_P}" --top-k "${TOP_K}" --seed "${SEED}"
  exit 0
fi

if [[ "${BACKEND}" != "ollama" ]]; then
  echo "Error: unsupported OFFLINE_BACKEND='${BACKEND}' (use auto|llama-cli|ollama)" >&2
  exit 1
fi

if [[ "${RECREATE_MODEL}" != "0" && "${RECREATE_MODEL}" != "1" ]]; then
  echo "Error: OFFLINE_RECREATE_MODEL must be 0 or 1" >&2
  exit 1
fi

require_cmd ollama
require_cmd curl

if ! curl -fsS "${BASE_URL}/api/tags" >/dev/null 2>&1; then
  echo "[offline] Ollama API not reachable at ${BASE_URL}; starting 'ollama serve'"
  mkdir -p "$(dirname "${OLLAMA_LOG}")"
  nohup ollama serve >"${OLLAMA_LOG}" 2>&1 &
  if ! wait_for_ollama; then
    echo "Error: failed to start Ollama service. Check ${OLLAMA_LOG}" >&2
    echo "Hint: start it manually: OLLAMA_HOST=127.0.0.1:11434 ollama serve" >&2
    exit 1
  fi
fi

if [[ "${RECREATE_MODEL}" == "1" ]] || ! ollama list | awk 'NR>1 {print $1}' | grep -qx "${MODEL_NAME}"; then
  echo "[offline] Creating chat-optimized Ollama model '${MODEL_NAME}' from ${MODEL_PATH}"
  modelfile="$(mktemp)"
  {
    printf 'FROM %s\n' "${MODEL_PATH}"
    printf 'SYSTEM %s\n' "${SYSTEM_PROMPT}"
    printf 'PARAMETER temperature %s\n' "${TEMPERATURE}"
    printf 'PARAMETER top_p %s\n' "${TOP_P}"
    printf 'PARAMETER top_k %s\n' "${TOP_K}"
    printf 'PARAMETER seed %s\n' "${SEED}"
    printf 'PARAMETER num_ctx %s\n' "${NUM_CTX}"
    printf 'PARAMETER num_predict %s\n' "${NUM_PREDICT}"
    printf 'PARAMETER repeat_penalty 1.1\n'
  } >"${modelfile}"
  ollama create "${MODEL_NAME}" -f "${modelfile}"
  rm -f "${modelfile}"
else
  echo "[offline] Reusing existing Ollama model '${MODEL_NAME}'"
fi

echo "[offline] Running OpenCodex chat demo with local model"
if [[ "${MODE}" == "chat" ]]; then
  echo "[offline] Interactive chat via Ollama model '${MODEL_NAME}' (type /bye to exit)"
  exec ollama run "${MODEL_NAME}"
fi

echo "[offline] Single response via OpenCodex chat binary"
exec "${CHAT_BIN}" --model "${MODEL_NAME}" --url "${BASE_URL}/api/generate" "${PROMPT}"
