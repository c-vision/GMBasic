cmake_minimum_required(VERSION 3.10)
project(gmb_agent_runtime C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Motore AI UNICO = LLMEnv (sorgente unica; GMBasic non tiene piu' copie native/gmb_*).
# Override con LLMENV=<path>; default: <HOME>/Documents/Projects/LLMEnv, poi ../LLMEnv.
set(LLMENV "$ENV{LLMENV}")
if(NOT LLMENV)
    if(EXISTS "$ENV{HOME}/Documents/Projects/LLMEnv/LLMEngine/mlx")
        set(LLMENV "$ENV{HOME}/Documents/Projects/LLMEnv")
    else()
        set(LLMENV "${CMAKE_CURRENT_SOURCE_DIR}/../../LLMEnv")
    endif()
endif()
add_library(gmb_agent_runtime STATIC
    tool_registry.c
    action_resolver.c
    gmb_computer_provider.c
    gmb_computer_provider_mac.c
    gmb_computer_provider_windows.c
    gmb_computer_provider_linux.c
    capability.c
    credential_vault.c
    verify.c
    builtin_seed.c
    gmb_browser_cdp.c
    gmb_agent_action.c
    gmb_agent_loop.c
    gmb_brain_http.c
    gmb_brain_mlx.c
    gmb_cua_runtime.c
    gmb_code_exec.c
    gmb_codegen.c
    gmb_chat_template.c
)
target_include_directories(gmb_agent_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include
    ${LLMENV}/LLMJinja
    # Motore MLX (ABI commune LLMEnv): llmengine.h + llm_gguf.h/llm_ggml.h.
    ${LLMENV}/LLMEngine/include
    ${LLMENV}/LLMEngine/mlx
    # refactor(layering): gli atomi di formato (llmcodec_ggml_quants.h/llmcodec_gguf_read.h) vivono nel
    # CODEC di LLMEnv; llm_ggml.h e llm_gguf.h li includono -> serve il path del codec.
    ${LLMENV}/LLMCodec/include)

# gmb_jinja (motore Jinja C) — ora LLMJinja di LLMEnv (sorgente unica), libreria statica,
# condiviso col resto del mondo C (gmb_llm_server). Nessun bridge verso C#.
add_subdirectory("${LLMENV}/LLMJinja" "${CMAKE_CURRENT_BINARY_DIR}/llmjinja_build")
# SPLIT core/AI (2026-11-02): il runtime non propaga più le lib LLMEnv. Chi usa direttamente
# llmcodec/llmtoken (chat template, tokenizer) le dichiara qui.
target_link_libraries(gmb_agent_runtime PUBLIC llmjinja llmcodec llmtoken)

if(APPLE)
    enable_language(OBJC)
    # Framework macOS per il ComputerProvider reale (CGEvent/Display/ImageIO/AX)
    target_link_libraries(gmb_agent_runtime PUBLIC
        "-framework CoreGraphics"
        "-framework ApplicationServices"
        "-framework ImageIO"
        "-framework CoreServices"
        "-framework AppKit")
    target_compile_options(gmb_agent_runtime PRIVATE -fobjc-arc)
    # AX reale (UI.*) + clipboard via NSPasteboard (ObjC)
    target_sources(gmb_agent_runtime PRIVATE gmb_ax_mac.c gmb_clipboard_mac.m)
    set_source_files_properties(gmb_clipboard_mac.m PROPERTIES LANGUAGE OBJC)
endif()

# gmb_opt_env — infra 2026-09-29
target_include_directories(gmb_agent_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include)
# --- GUARDIA ANTI-STALE (2026-10-01): le lib AI PRECOMPILATE (libgmbllm.a / libgmb_runtime.a) sono
# linkate by-path -> nessuna dipendenza CMake. Se piu' VECCHIE dei loro sorgenti, il consumer userebbe il
# runtime VECCHIO in silenzio. Qui si FALLISCE con errore esplicito.

target_link_libraries(gmb_agent_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/build/libgmb_runtime.a)

# Garanzia anti-stale GLOBALE (auto-rebuild + blocco): modulo condiviso.
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbAiNativeLibs.cmake")
gmb_ai_native_depends(gmb_agent_runtime)
