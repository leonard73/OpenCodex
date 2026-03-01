CC ?= cc
CFLAGS ?= -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -Wpedantic
DBG_CFLAGS ?= -std=c11 -D_POSIX_C_SOURCE=200809L -g -O0 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=

CURL_CFLAGS := $(shell pkg-config --cflags libcurl 2>/dev/null)
CURL_LIBS := $(shell pkg-config --libs libcurl 2>/dev/null || echo -lcurl)
CJSON_CFLAGS := $(shell pkg-config --cflags libcjson 2>/dev/null)
CJSON_LIBS := $(shell pkg-config --libs libcjson 2>/dev/null)

CPPFLAGS += $(CURL_CFLAGS) $(CJSON_CFLAGS)
LDLIBS ?= $(CURL_LIBS) $(CJSON_LIBS) -lpthread

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

SRC_DIR := src/c
BUILD_DIR := build
MODELS_DIR := models

MODEL_FILENAME ?= DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf
MODEL_PATH := $(MODELS_DIR)/$(MODEL_FILENAME)
MODEL_URL ?= https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-1.5B-GGUF/resolve/main/$(MODEL_FILENAME)?download=true
OFFLINE_MODEL_PATH ?= /home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf
OFFLINE_OLLAMA_MODEL ?= opencodex-local-deepseek-r1-1_5b-q4
OFFLINE_OLLAMA_BASE_URL ?= http://127.0.0.1:11434
OFFLINE_PROMPT ?= Say hello from OpenCodex in one sentence.
OFFLINE_BACKEND ?= auto
OFFLINE_MODE ?= chat
OFFLINE_TOKENS ?= 96
OFFLINE_THREADS ?= $(shell nproc 2>/dev/null || echo 4)
OFFLINE_RECREATE_MODEL ?= 1
OFFLINE_SYSTEM_PROMPT ?= You are OpenCodex assistant. Be natural and helpful. Give direct final answers. Do not output internal reasoning or chain-of-thought.
OFFLINE_TEMPERATURE ?= 0.3
OFFLINE_TOP_P ?= 0.9
OFFLINE_TOP_K ?= 40
OFFLINE_SEED ?= 42
OFFLINE_NUM_CTX ?= 4096
OFFLINE_NUM_PREDICT ?= 256

COMMON_SRCS := \
	$(SRC_DIR)/llm_helper.c \
	$(SRC_DIR)/http_client.c \
	$(SRC_DIR)/json_parser.c \
	$(SRC_DIR)/model_loader.c \
	$(SRC_DIR)/file_ops.c \
	$(SRC_DIR)/thread_pool.c

COMMON_OBJS := $(COMMON_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

CHAT_BIN := $(BUILD_DIR)/opencodex-chat
CODEX_BIN := $(BUILD_DIR)/opencodex
MEMORY_BIN := $(BUILD_DIR)/memory-baseline

VERSION ?= $(shell git rev-parse --short HEAD 2>/dev/null || echo local)
PACKAGE_NAME := OpenCodex-src-$(VERSION).tar.gz

.PHONY: all build clean download install uninstall dependencies test_demo_chat test_demo_codex test_demo_offline \
	offline_chat_codegen offline_chat_codes offline_chat_ubuntu_cmd_gen offline_char_ubuntu_cmd_gen \
	offline_chat_debugger offline_chat_explain_c test_demo_codex_local build_debug valgrind_chat valgrind_codex benchmark package memory_baseline dirs check_ollama

all: build

build: dirs $(CHAT_BIN) $(CODEX_BIN) $(MEMORY_BIN)

$(BUILD_DIR)/.dir:
	mkdir -p $(BUILD_DIR)
	touch $@

$(MODELS_DIR)/.dir:
	mkdir -p $(MODELS_DIR)
	touch $@

dirs: $(BUILD_DIR)/.dir $(MODELS_DIR)/.dir

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(CHAT_BIN): $(BUILD_DIR)/chat_main.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(CODEX_BIN): $(BUILD_DIR)/coding_main.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MEMORY_BIN): $(BUILD_DIR)/memory_baseline.o
	$(CC) $(LDFLAGS) $^ -o $@

download: $(MODELS_DIR)/.dir
	@if [ -f "$(MODEL_PATH)" ]; then \
		echo "Model already exists: $(MODEL_PATH)"; \
		exit 0; \
	fi
	@if command -v curl >/dev/null 2>&1; then \
		echo "Downloading model using curl from $(MODEL_URL)"; \
		curl -fL "$(MODEL_URL)" -o "$(MODEL_PATH)"; \
	elif command -v wget >/dev/null 2>&1; then \
		echo "Downloading model using wget from $(MODEL_URL)"; \
		wget -O "$(MODEL_PATH)" "$(MODEL_URL)"; \
	else \
		echo "Error: curl or wget is required for download target" >&2; \
		exit 1; \
	fi

install: build
	install -Dm755 $(CHAT_BIN) $(DESTDIR)$(BINDIR)/opencodex-chat
	install -Dm755 $(CODEX_BIN) $(DESTDIR)$(BINDIR)/opencodex

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/opencodex-chat $(DESTDIR)$(BINDIR)/opencodex

dependencies:
	./install_deps.sh

memory_baseline: $(MEMORY_BIN)
	./$(MEMORY_BIN)

check_ollama:
	@if ! command -v curl >/dev/null 2>&1; then \
		echo "Error: curl is required to check local Ollama API." >&2; \
		exit 1; \
	fi; \
	if ! curl -fsS "http://127.0.0.1:11434/api/tags" >/dev/null 2>&1; then \
		echo "Error: local Ollama API is not reachable at http://127.0.0.1:11434/api/tags" >&2; \
		echo "Start it with: OLLAMA_HOST=127.0.0.1:11434 ollama serve" >&2; \
		exit 1; \
	fi

test_demo_chat: build check_ollama
	./$(CHAT_BIN) --local-model "$(OFFLINE_MODEL_PATH)" --local-model-name "$(OFFLINE_OLLAMA_MODEL)" --local-recreate --timeout 1200 "Say hello from OpenCodex in one sentence."

test_demo_codex: build check_ollama
	./$(CODEX_BIN) --local-model "$(OFFLINE_MODEL_PATH)" --local-model-name "$(OFFLINE_OLLAMA_MODEL)" --local-recreate --timeout 1200 --prompt "Write a tiny bash script that echoes OpenCodex ready." --output "$(BUILD_DIR)/demo_output.sh" --yes

test_demo_codex_local: build
	./$(CODEX_BIN) --local-model "$(OFFLINE_MODEL_PATH)" \
		--local-model-name "opencodex-local-coding" \
		--local-recreate \
		--timeout 1200 \
		--prompt "Write a C function compute_pi_nilakantha(unsigned long long iterations) without external libraries and include a short main demo." \
		--output "$(BUILD_DIR)/demo_local_codegen.c" --yes

test_demo_offline: build
	OFFLINE_MODEL_PATH="$(OFFLINE_MODEL_PATH)" \
	OFFLINE_OLLAMA_MODEL="$(OFFLINE_OLLAMA_MODEL)" \
	OFFLINE_OLLAMA_BASE_URL="$(OFFLINE_OLLAMA_BASE_URL)" \
	OFFLINE_PROMPT="$(OFFLINE_PROMPT)" \
	OFFLINE_BACKEND="$(OFFLINE_BACKEND)" \
	OFFLINE_MODE="$(OFFLINE_MODE)" \
	OFFLINE_TOKENS="$(OFFLINE_TOKENS)" \
	OFFLINE_THREADS="$(OFFLINE_THREADS)" \
	OFFLINE_RECREATE_MODEL="$(OFFLINE_RECREATE_MODEL)" \
	OFFLINE_SYSTEM_PROMPT="$(OFFLINE_SYSTEM_PROMPT)" \
	OFFLINE_TEMPERATURE="$(OFFLINE_TEMPERATURE)" \
	OFFLINE_TOP_P="$(OFFLINE_TOP_P)" \
	OFFLINE_TOP_K="$(OFFLINE_TOP_K)" \
	OFFLINE_SEED="$(OFFLINE_SEED)" \
	OFFLINE_NUM_CTX="$(OFFLINE_NUM_CTX)" \
	OFFLINE_NUM_PREDICT="$(OFFLINE_NUM_PREDICT)" \
	CHAT_BIN="./$(CHAT_BIN)" \
	./scripts/test_demo_offline.sh

offline_chat_codegen:
	$(MAKE) test_demo_offline \
		OFFLINE_BACKEND=ollama \
		OFFLINE_MODE=chat \
		OFFLINE_RECREATE_MODEL=1 \
		OFFLINE_OLLAMA_MODEL=opencodex-chat-codegen \
		OFFLINE_TEMPERATURE=0.05 \
		OFFLINE_TOP_P=0.85 \
		OFFLINE_TOP_K=40 \
		OFFLINE_SEED=7 \
		OFFLINE_NUM_CTX=8192 \
		OFFLINE_NUM_PREDICT=1200 \
		OFFLINE_SYSTEM_PROMPT="You are a senior C/C++/Bash engineer. Provide accurate, runnable code and short practical notes. No internal reasoning."

offline_chat_codes:
	$(MAKE) test_demo_offline \
		OFFLINE_BACKEND=ollama \
		OFFLINE_MODE=chat \
		OFFLINE_RECREATE_MODEL=1 \
		OFFLINE_OLLAMA_MODEL=opencodex-chat-codes \
		OFFLINE_TEMPERATURE=0.1 \
		OFFLINE_TOP_P=0.9 \
		OFFLINE_TOP_K=60 \
		OFFLINE_SEED=11 \
		OFFLINE_NUM_CTX=8192 \
		OFFLINE_NUM_PREDICT=1400 \
		OFFLINE_SYSTEM_PROMPT="You are a software engineer for multi-file code generation. Return complete code blocks, file paths, and compile commands. No internal reasoning."

offline_chat_ubuntu_cmd_gen:
	$(MAKE) test_demo_offline \
		OFFLINE_BACKEND=ollama \
		OFFLINE_MODE=chat \
		OFFLINE_RECREATE_MODEL=1 \
		OFFLINE_OLLAMA_MODEL=opencodex-chat-ubuntu-cmd \
		OFFLINE_TEMPERATURE=0.05 \
		OFFLINE_TOP_P=0.85 \
		OFFLINE_TOP_K=30 \
		OFFLINE_SEED=17 \
		OFFLINE_NUM_CTX=4096 \
		OFFLINE_NUM_PREDICT=700 \
		OFFLINE_SYSTEM_PROMPT="You are an Ubuntu CLI expert. Return safe, minimal shell commands with one-line explanations. Ask confirmation before destructive commands. No internal reasoning."

offline_char_ubuntu_cmd_gen: offline_chat_ubuntu_cmd_gen

offline_chat_debugger:
	$(MAKE) test_demo_offline \
		OFFLINE_BACKEND=ollama \
		OFFLINE_MODE=chat \
		OFFLINE_RECREATE_MODEL=1 \
		OFFLINE_OLLAMA_MODEL=opencodex-chat-debugger \
		OFFLINE_TEMPERATURE=0.1 \
		OFFLINE_TOP_P=0.9 \
		OFFLINE_TOP_K=50 \
		OFFLINE_SEED=23 \
		OFFLINE_NUM_CTX=8192 \
		OFFLINE_NUM_PREDICT=1200 \
		OFFLINE_SYSTEM_PROMPT="You are a debugging specialist. Identify root causes, give minimal fixes, and provide reproducible test commands. No internal reasoning."

offline_chat_explain_c:
	$(MAKE) test_demo_offline \
		OFFLINE_BACKEND=ollama \
		OFFLINE_MODE=chat \
		OFFLINE_RECREATE_MODEL=1 \
		OFFLINE_OLLAMA_MODEL=opencodex-chat-explain-c \
		OFFLINE_TEMPERATURE=0.2 \
		OFFLINE_TOP_P=0.92 \
		OFFLINE_TOP_K=60 \
		OFFLINE_SEED=29 \
		OFFLINE_NUM_CTX=8192 \
		OFFLINE_NUM_PREDICT=1200 \
		OFFLINE_SYSTEM_PROMPT="You explain C systems programming clearly with concise examples and practical tradeoffs. No internal reasoning."

build_debug:
	$(MAKE) CFLAGS="$(DBG_CFLAGS)" clean build

valgrind_chat: build_debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(CHAT_BIN) --local-model "$(OFFLINE_MODEL_PATH)" --local-model-name "$(OFFLINE_OLLAMA_MODEL)" --timeout 1200 "ping"

valgrind_codex: build_debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(CODEX_BIN) --local-model "$(OFFLINE_MODEL_PATH)" --local-model-name "$(OFFLINE_OLLAMA_MODEL)" --timeout 1200 --prompt "write a hello world c file" --output "$(BUILD_DIR)/valgrind_output.c" --yes

benchmark: build
	/usr/bin/time -f "chat latency: %e sec" ./$(CHAT_BIN) --local-model "$(OFFLINE_MODEL_PATH)" --local-model-name "$(OFFLINE_OLLAMA_MODEL)" --timeout 1200 "ping"
	/usr/bin/time -f "codex latency: %e sec" ./$(CODEX_BIN) --local-model "$(OFFLINE_MODEL_PATH)" --local-model-name "$(OFFLINE_OLLAMA_MODEL)" --timeout 1200 --prompt "write a hello world c file" --output "$(BUILD_DIR)/bench_output.c" --yes
	@if command -v perf >/dev/null 2>&1; then \
		echo "Running perf stat for coding tool"; \
		perf stat ./$(CODEX_BIN) --local-model "$(OFFLINE_MODEL_PATH)" --local-model-name "$(OFFLINE_OLLAMA_MODEL)" --timeout 1200 --prompt "write one function in c" --output "$(BUILD_DIR)/perf_output.c" --yes >/dev/null; \
	else \
		echo "perf not available; skipping perf profiling"; \
	fi

package:
	mkdir -p $(BUILD_DIR)
	tar --exclude='./.git' --exclude='./build' --exclude='./models/.dir' --exclude='./models/*.gguf' -czf "$(BUILD_DIR)/$(PACKAGE_NAME)" .
	@echo "Created package: $(BUILD_DIR)/$(PACKAGE_NAME)"

clean:
	rm -rf $(BUILD_DIR)
