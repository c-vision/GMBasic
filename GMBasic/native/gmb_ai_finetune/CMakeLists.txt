# native/gmb_ai_finetune — fine-tune LoRA + apply IN-PROCESS (dylib gmb_ai_finetune).
#
# La logica vive QUI (non più in tools/ come POC né come eseguibili in build/libs/): il runtime C
# (dlopen) e il C# (P/Invoke) la chiamano come funzione. Linka la dylib del motore `gmb_ai_mlx`
# (gmb_host_ai_mlx/build), già costruita — non si ricostruisce il motore MLX.
cmake_minimum_required(VERSION 3.20)
project(gmb_ai_finetune C)

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
set(MLX_DIR "${LLMENV}/LLMEngine/mlx")
set(MLX_LIB "${MLX_DIR}/build-gmbasic/libllm_ai_mlx.dylib")

add_library(gmb_ai_finetune SHARED gmb_ai_finetune.c)
if(NOT TARGET llmtoken)
    add_subdirectory(${LLMENV}/LLMToken llmtoken_build)
endif()
target_include_directories(gmb_ai_finetune PRIVATE include "${MLX_DIR}" "${LLMENV}/LLMEngine/include" "${LLMENV}/LLMEngine/Options" "${LLMENV}/LLMToken/include" "${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/include")
# gmbllm (PUL): orchestrazione training/DPO (llm_ai_mlx_train_dpo_step). Statico, PRIMA del motore
# (chi usa i simboli precede chi li fornisce). I simboli mlx-c sono esportati dal motore.
set(GMBLLM_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/build/gmbllm_build/libgmbllm.a")
# Tokenizer condiviso (LLMEnv LLMToken: gmb_distill SAMPLE/EVAL; l'engine MLX non espone CAP_TOKENIZE).
if(EXISTS "${GMBLLM_LIB}")
    target_link_libraries(gmb_ai_finetune PRIVATE "${GMBLLM_LIB}" "${MLX_LIB}" llmtoken "-framework Accelerate")
else()
    message(WARNING "libgmbllm.a non trovata (${GMBLLM_LIB}): DPO esatto disabilitato, resta il self-contained")
    target_link_libraries(gmb_ai_finetune PRIVATE "${MLX_LIB}" llmtoken "-framework Accelerate")
endif()

set_target_properties(gmb_ai_finetune PROPERTIES
    PREFIX "lib" OUTPUT_NAME "gmb_ai_finetune"
    LIBRARY_OUTPUT_DIRECTORY "${GMB_LIBS_OUT}"
    BUILD_RPATH "${MLX_DIR}/build-gmbasic"
    INSTALL_RPATH "${MLX_DIR}/build-gmbasic")
