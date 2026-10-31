# native/llmengine_plugin_example — plugin CODEC d'esempio (F16). Dimostra l'ABI llmengine_plugin.h:
# un formato = una dylib in build/plugins/<rid>/ (artefatto di build, non del repo), caricata a runtime (dlopen). Nessuna
# ricompilazione del compiler/core.
cmake_minimum_required(VERSION 3.20)
project(llmengine_plugin_example C)

# RID della macchina di build (unico posto: native/cmake/GmbRid.cmake).
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbRid.cmake)

# Motore AI UNICO = LLMEnv (sorgente unica; GMBasic non tiene piu' copie native/gmb_*).
set(LLMENV "$ENV{LLMENV}")
if(NOT LLMENV)
    if(EXISTS "$ENV{HOME}/Documents/Projects/LLMEnv/LLMEngine/mlx")
        set(LLMENV "$ENV{HOME}/Documents/Projects/LLMEnv")
    else()
        set(LLMENV "${CMAKE_CURRENT_SOURCE_DIR}/../../LLMEnv")
    endif()
endif()

add_library(gmb_codec_f16 SHARED gmb_codec_f16.c)
target_include_directories(gmb_codec_f16 PRIVATE ${LLMENV}/LLMEngine/plugin ${LLMENV}/LLMCodec/include)
set_target_properties(gmb_codec_f16 PROPERTIES
    PREFIX "lib" OUTPUT_NAME "gmb_codec_f16"
    LIBRARY_OUTPUT_DIRECTORY "${GMB_PLUGINS_OUT}")

add_library(gmb_prim_rmsnorm SHARED gmb_prim_rmsnorm.c)
target_include_directories(gmb_prim_rmsnorm PRIVATE ${LLMENV}/LLMEngine/plugin)
set_target_properties(gmb_prim_rmsnorm PROPERTIES
    PREFIX "lib" OUTPUT_NAME "llmengine_plugin_prim_rmsnorm"
    LIBRARY_OUTPUT_DIRECTORY "${GMB_PLUGINS_OUT}")

add_library(gmb_codec_e5m2 SHARED gmb_codec_e5m2.c)
target_include_directories(gmb_codec_e5m2 PRIVATE ${LLMENV}/LLMEngine/plugin)
set_target_properties(gmb_codec_e5m2 PROPERTIES PREFIX "lib" OUTPUT_NAME "gmb_codec_e5m2"
    LIBRARY_OUTPUT_DIRECTORY "${GMB_PLUGINS_OUT}")

# Plugin PRIMITIVA MLX-aware: include mlx-c, simboli MLX risolti a RUNTIME dall'host (l'engine li ha).
add_library(gmb_primmlx_rmsnorm SHARED gmb_primmlx_rmsnorm.c)
target_include_directories(gmb_primmlx_rmsnorm PRIVATE
    ${LLMENV}/LLMEngine/plugin
    # MLX-C: appartiene a LLMEnv (ADR-Confine-GMBasic-LLMEnv-MLX-Metal). Nessuna copia locale.
    ${LLMENV}/libs/mlx-deps/mlx-c-src)
target_link_options(gmb_primmlx_rmsnorm PRIVATE "-undefined" "dynamic_lookup")
set_target_properties(gmb_primmlx_rmsnorm PROPERTIES PREFIX "lib" OUTPUT_NAME "llmengine_plugin_primmlx_rmsnorm"
    LIBRARY_OUTPUT_DIRECTORY "${GMB_PLUGINS_OUT}")
