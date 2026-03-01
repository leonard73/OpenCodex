#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

int model_file_is_readable(const char *model_path);
int ensure_local_ollama_model(const char *model_path,
                              const char *model_name,
                              const char *system_prompt,
                              int recreate,
                              double temperature,
                              double top_p,
                              int top_k,
                              int seed,
                              int num_ctx,
                              int num_predict);

#endif
