cmake_minimum_required(VERSION 3.16)
# gmb_llm_server — core C del server OpenAI-compatible (due versioni indipendenti C/C#, mai
# bridge in-process). Questa e' la versione C: parla direttamente con l'engine C
# (libgmb_ai_mlx) e il tokenizer nativo (libgmb_tokenizer_bpe), nessuna dipendenza da C#.
project(gmb_llm_server C)

# Motore AI UNICO = LLMEnv (sorgente unica; GMBasic non tiene piu' copie native/gmb_*).
set(LLMENV "$ENV{LLMENV}")
if(NOT LLMENV)
    if(EXISTS "$ENV{HOME}/Documents/Projects/LLMEnv/LLMEngine/mlx")
        set(LLMENV "$ENV{HOME}/Documents/Projects/LLMEnv")
    else()
        set(LLMENV "${CMAKE_CURRENT_SOURCE_DIR}/../../LLMEnv")
    endif()
endif()
set(GMB_MLX_BUILD "${LLMENV}/LLMEngine/mlx/build-gmbasic")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
set(CMAKE_C_STANDARD 11)

if(NOT TARGET llmtoken)
    add_subdirectory(${LLMENV}/LLMToken llmtoken_build)
endif()
if(NOT TARGET llmjinja)
    add_subdirectory(${LLMENV}/LLMJinja llmjinja_build)
endif()

add_executable(gmb_llm_server gmb_llm_server.c)
target_include_directories(gmb_llm_server PRIVATE
    "${LLMENV}/LLMEngine/include"
    "${LLMENV}/LLMEngine/mlx"
    "${LLMENV}/LLMEngine/Options"
    "${LLMENV}/LLMCodec/include")
target_link_directories(gmb_llm_server PRIVATE "${GMB_MLX_BUILD}")
target_link_libraries(gmb_llm_server PRIVATE llm_ai_mlx llmtoken llmjinja)
set_target_properties(gmb_llm_server PROPERTIES
    BUILD_RPATH "${GMB_MLX_BUILD}"
    INSTALL_RPATH "${GMB_MLX_BUILD}")

# Registro opzioni: gmb_opt_env (ponte env->registro) — infra 2026-09-29
target_include_directories(gmb_llm_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include)
# --- GUARDIA ANTI-STALE (2026-10-01): le lib AI PRECOMPILATE (libgmbllm.a / libgmb_runtime.a) sono
# linkate by-path -> nessuna dipendenza CMake. Se piu' VECCHIE dei loro sorgenti, il consumer userebbe il
# runtime VECCHIO in silenzio. Qui si FALLISCE con errore esplicito.

target_link_libraries(gmb_llm_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/build/libgmb_runtime.a)

# Garanzia anti-stale GLOBALE (auto-rebuild + blocco): modulo condiviso.
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbAiNativeLibs.cmake")
gmb_ai_native_depends(gmb_llm_server)
