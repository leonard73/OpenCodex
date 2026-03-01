# OpenCodex

OpenCodex is a native C + bash local coding assistant.

It is now **local-model only**:
- GGUF model file on disk
- local Ollama service on `127.0.0.1:11434`
- no remote URL selection in the CLI menus

## Features

- `opencodex`: interactive terminal chooser with modes:
  - `codegen`
  - `chat`
  - `fileformat`
- `opencodex-chat`: interactive local chat with selectable profile:
  - `general`
  - `codegen`
  - `fileformat`
- local GGUF import to Ollama model (`ollama create`) from CLI
- mmap-based file I/O utility functions

## Prerequisites

```bash
make dependencies
```

This runs `install_deps.sh` and installs build + runtime dependencies.

## Build and install

```bash
make build
make install
```

Installed binaries:
- `/usr/local/bin/opencodex`
- `/usr/local/bin/opencodex-chat`

## Local model files

Default local model path used by prompts:
- `/home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf`

Optional project-local download target:

```bash
make download
```

## Run (no parameters)

This is the recommended flow.

```bash
opencodex
opencodex-chat
```

Both commands now open interactive terminal setup and ask for needed values.

## Direct command mode

Generate code file using local model:

```bash
opencodex \
  --local-model /home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q5_K_M.gguf \
  --local-model-name opencodex-local-coding \
  --local-recreate \
  --timeout 1200 \
  --prompt "Write a C function to compute PI without external libraries." \
  --output pi.c --yes
```

Single chat question:

```bash
opencodex-chat \
  --local-model /home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q5_K_M.gguf \
  --local-model-name opencodex-local-chat \
  --timeout 1200 \
  --profile codegen \
  "Write a C11 function for fast integer power."
```

## Demo targets

```bash
make test_demo_chat
make test_demo_codex
make test_demo_codex_local
make test_demo_offline
```

## Preset chat profiles

```bash
make offline_chat_codegen
make offline_chat_codes
make offline_chat_ubuntu_cmd_gen
make offline_char_ubuntu_cmd_gen
make offline_chat_debugger
make offline_chat_explain_c
```

## Quality targets

```bash
make build_debug
make valgrind_chat
make valgrind_codex
make benchmark
make package
```

## End-to-end script

```bash
./scripts/global_codex_job_compiling_and_testing_self_cmd.sh
./global_codex_job_compiling_and_testing_self_cmd
```
