# OpenCodex

OpenCodex is a native C and bash project that builds local command-line tools similar to Codex-style workflows.

## Features

- `opencodex-chat`: interactive REPL or one-shot prompt chat tool.
- `opencodex`: coding tool that generates code and writes to files.
- Ollama HTTP integration (`/api/generate`) with libcurl.
- cJSON response parsing.
- mmap-based file read/write utilities.
- basic thread pool utility (`thread_pool.c`) for concurrent task scaffolding.

## Project layout

- `src/c`: C source files.
- `models`: local model files (GGUF download target).
- `build`: build outputs.
- `scripts/global_codex_job_compiling_and_testing_self_cmd.sh`: end-to-end integration script.
- `global_codex_job_compiling_and_testing_self_cmd`: wrapper command for the integration script.

## Prerequisites (Ubuntu/Debian)

Run:

```bash
make dependencies
```

This executes `install_deps.sh`, which installs required packages with `apt-get`.

## Runtime requirement

The chat and coding tools call an Ollama-compatible HTTP endpoint:

- default URL: `http://localhost:11434/api/generate`
- default model name: `deepseek-r1:1.5b`

You can override either value using CLI flags (`--url`, `--model`) or `make` variables (`OLLAMA_URL`, `OLLAMA_MODEL`).

## Build and install

```bash
make build
make install
```

Installed binaries:

- `/usr/local/bin/opencodex-chat`
- `/usr/local/bin/opencodex`

Use `DESTDIR` or `PREFIX` for custom install locations:

```bash
make install PREFIX=$HOME/.local
```

## Model download (optional)

If you do not already have the GGUF model in `models/`:

```bash
make download
```

Override model URL or filename if needed:

```bash
make download MODEL_URL="https://.../model.gguf" MODEL_FILENAME="model.gguf"
```

## Demo usage

```bash
make test_demo_chat
make test_demo_codex
make test_demo_offline
```

`test_demo_chat` and `test_demo_codex` require a running Ollama API on `http://localhost:11434`.

`test_demo_offline` uses your local GGUF file and automates:

1. Start/check Ollama service.
2. Create a local Ollama model from `OFFLINE_MODEL_PATH`.
3. Start an interactive chat session in your terminal.

Override offline defaults if needed:

```bash
make test_demo_offline \
  OFFLINE_MODEL_PATH=/home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q5_K_M.gguf \
  OFFLINE_OLLAMA_MODEL=opencodex-local-q5 \
  OFFLINE_PROMPT="Write one short bash tip."
```

Offline mode selection:

- `OFFLINE_MODE=chat` (default): interactive terminal chat.
- `OFFLINE_MODE=single`: one-shot response then exit.

Offline backend selection:

- `OFFLINE_BACKEND=auto` (default): use `llama-cli` if present, otherwise Ollama.
- `OFFLINE_BACKEND=llama-cli`: force direct GGUF inference (no API server).
- `OFFLINE_BACKEND=ollama`: force Ollama flow using local GGUF import.

Natural chat tuning (Ollama backend):

- `OFFLINE_RECREATE_MODEL=1` (default): rebuild local model config each run.
- `OFFLINE_SYSTEM_PROMPT`: system behavior style.
- `OFFLINE_TEMPERATURE`, `OFFLINE_TOP_P`, `OFFLINE_TOP_K`, `OFFLINE_SEED`: sampling/reproducibility tuning.
- `OFFLINE_NUM_CTX`, `OFFLINE_NUM_PREDICT`: context/output limits.

Recommended natural interactive chat:

```bash
make test_demo_offline \
  OFFLINE_BACKEND=ollama \
  OFFLINE_MODE=chat \
  OFFLINE_MODEL_PATH=/home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf \
  OFFLINE_OLLAMA_MODEL=opencodex-local-chat \
  OFFLINE_RECREATE_MODEL=1 \
  OFFLINE_SYSTEM_PROMPT="You are OpenCodex assistant. Be natural and concise. Do not output internal reasoning."
```

Preset offline chat profiles:

```bash
make offline_chat_codegen
make offline_chat_codes
make offline_chat_ubuntu_cmd_gen
make offline_char_ubuntu_cmd_gen
make offline_chat_debugger
make offline_chat_explain_c
```

Notes:

- `offline_chat_codegen`: stricter low-temperature coding assistant.
- `offline_chat_codes`: multi-file code generation style.
- `offline_chat_ubuntu_cmd_gen`: safe Ubuntu command generator.
- `offline_char_ubuntu_cmd_gen`: alias for the target above.
- `offline_chat_debugger`: bug/root-cause focused.
- `offline_chat_explain_c`: C explanation/teaching profile.

Direct calls:

```bash
opencodex-chat "Explain mmap in one paragraph"
opencodex --prompt "Write hello world in C" --output hello.c
```

## Quality targets

```bash
make build_debug
make valgrind_chat
make valgrind_codex
make benchmark
make package
```

## End-to-end pipeline

```bash
./scripts/global_codex_job_compiling_and_testing_self_cmd.sh
./global_codex_job_compiling_and_testing_self_cmd
```
