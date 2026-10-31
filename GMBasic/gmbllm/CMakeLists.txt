# =============================================================================
# gmbllm — parte LLM NON-Apache di GMBasic (licenza PUL).
#
# Contiene il codice che PRODUCE/ALTERA pesi (training, quant-ENCODE, surgery/
# planning). Vive DENTRO GMBasic (PUL) e **linka** le librerie Apache-2.0 di
# LLMEnv (llmcodec/llmtoken/llmdesc/llmengine_plugin), mai l'inverso.
# Vedi: Decisions/ADR-Confine-Licenza-Engine-Apache-Training-Quant-PUL.md
#       Piani/(open) - Piano-Separazione-Licenza-LLMEnv-Apache-gmbllm-PUL.md
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(gmbllm C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# I target Apache (llmcodec, llmtoken, llmdesc, llmengine_plugin) sono gia' definiti
# dal parent (GMBasic, native/gmb_runtime_c/CMakeLists.txt, add_subdirectory di LLMEnv).
# LLMENV e' settata dal parent.
if(NOT LLMENV)
    message(FATAL_ERROR "gmbllm: LLMENV non definita dal parent (atteso da native/gmb_runtime_c/CMakeLists.txt)")
endif()

# --- F2: model compiler / surgery / planning (ex-LLMModelIO) -----------------
# --- F3: quant-ENCODE (da LLMCodec; il DEQUANT resta Apache) ------------------
# --- F4: training (dal LLMEngine; il forward differenziabile resta Apache) ----
set(GMBLLM_SOURCES
    src/llmcodec_gguf_mlx.c      # converter MLX<->GGUF (PUL): orchestrazione via registry (read/write Apache, encode PUL)
    src/llmmodelio_mlx_merge.c   # MERGE LoRA su modello MLX (dir) — dequant/fuse/requant
    src/llmmodelio_model.c       # model compiler: quantize/convert/ablate/add/adapter/module/merge/prune + report LLM MODEL/TENSOR
    src/llmmodelio_recover.c     # RECOVER (§9.B): gate->retrain->re-eval->ACCEPT/ROLLBACK (dominio separato)
    src/llmmodelio_imatrix.c     # imatrix: read/write (calibrazione)
    src/llmmodelio_infer.c       # forward di calibrazione (conteggio routing)
    src/llmmodelio_tokenizer.c   # B2: tokenizer/vocab come sub-verbi LLM TENSOR (wiring su llmtoken)
    src/llmmodelio_registry.c    # B3: model registry/versioning (LLM REGISTRY)
    src/llmmodelio_quanterror.c  # B4: errore di quantizzazione (LLM TENSOR QUANTERROR)
    src/llmmodelio_compare.c     # B6: confronto per-tensore tra due GGUF (LLM TENSOR COMPARE)
    src/llmmodelio_derive.c      # B5: pipeline di derivazione riproducibile (LLM DERIVE)
    src/llmmodelio_patch.c       # B5: delta binario applicabile (LLM MAKEPATCH)
    ${LLMENV}/LLMJinja/llmjinja.c            # B2: rendering chat template (Apache, consumato da PUL)
    ${LLMENV}/LLMJinja/llmjinja_builtins.c   # B2: builtins Jinja
    ${LLMENV}/LLMEngine/moe/src/llmengine_moe_quant_alloc.c   # allocatore AUTO formato-per-tensore
    src/quant/llmcodec_iq2_xxs.c    # F3a
    src/quant/llmcodec_iq4_xs.c     # F3a-bis: encode IQ4_XS/IQ4_NL (PUL)
    src/quant/llmcodec_hf_quant.c   # F3d: encode HF (NF4/AWQ/GPTQ/FP4) — contropartite dei decoder Apache
    src/quant/llmcodec_moe_quant.c   # F3c (strangler): encode affine MoE (il dequant resta Apache) (strangler): encode ggml k-quants (il dequant resta Apache): quant-ENCODE iq (iq2/iq1/iq3) — il DEQUANT resta Apache in LLMCodec
    src/train/llm_train.c            # F4: orchestrazione training/DPO (PUL) — consuma l'ABI Apache
    src/train/llm_train_ce.c         # F4: la LOSS (masked CE/DPO) sopra i logits del forward Apache
)

add_library(gmbllm STATIC ${GMBLLM_SOURCES})
target_include_directories(gmbllm PUBLIC include include/quant
    ${LLMENV}/LLMCodec/include       # header Apache (fp16, iq_grids) — gmbllm li CONSUMA, mai l'inverso
    ${LLMENV}/LLMEngine/moe/include
    ${LLMENV}/LLMJinja              # llmjinja.h (rendering chat template) — Apache, consumato da PUL
    ${LLMENV}/LLMEngine/mlx          # ABI Apache (llmengine_mlx.h, llmengine_mlx_diff_forward.h) — forward differenziabile
    ${LLMENV}/LLMEngine/Options      # llm_engine_opts.h (transitivo)
    ${LLMENV}/libs/mlx-deps/mlx-c-src   # <mlx/c/*.h> (closure/value_and_grad) — TRAIN
    ${CMAKE_CURRENT_SOURCE_DIR}/../native/gmb_runtime_c/include)  # gmb_c99gpm.h: GMB_MUTEX_*/GMB_THREAD_* (abstraction thread GPM-safe)
# Dipendenze SOLO in direzione gmbllm -> Apache (mai il contrario).
target_link_libraries(gmbllm PUBLIC llmcodec llmtoken llmengine_plugin llmdesc ${CMAKE_DL_LIBS} m)

# --- Self-check dei componenti PUL (converter MLX<->GGUF + quant-ENCODE) ---------------------
# Opzionale, default OFF: non tocca la build normale (runtime/dylib gmb_ai_llm). I plugin Apache
# (llmcodec_*) sono gia' target del parent; questo converte i self-check ex-LLMCodec del converter
# (spostati qui col converter stesso: appartengono al confine PUL).
option(GMBLLM_TESTS "Build gmbllm self-check (converter + quant PUL)" OFF)
if(GMBLLM_TESTS)
    enable_testing()
    set(_gmbllm_test_plugins llmcodec_ggml llmcodec_mlx_affine llmcodec_fp8 llmcodec_plain llmcodec_mxfp
        llmcodec_gptq llmcodec_awq llmcodec_bitnet llmcodec_turboquant llmcodec_fp4 llmcodec_nvfp
        llmcodec_bnb llmcodec_hqq llmcodec_aqlm llmcodec_vptq llmcodec_spqr llmcodec_higgs)
    set(_gmbllm_test_includes include include/quant ${LLMENV}/LLMCodec/include)
    foreach(_t test_ggml_quants test_conv_gguf_mlx test_conv_sharded test_rolemap test_recipe
              test_expert3d test_dsv41_source test_gguf_map test_engram test_plugin test_a1_fp8 test_quantcfg)
        add_executable(gmbllm_${_t}_selfcheck tests/${_t}.c)
        target_link_libraries(gmbllm_${_t}_selfcheck PRIVATE gmbllm llmcodec ${_gmbllm_test_plugins})
        target_include_directories(gmbllm_${_t}_selfcheck PRIVATE ${_gmbllm_test_includes})
        add_test(NAME gmbllm_${_t}_selfcheck COMMAND gmbllm_${_t}_selfcheck)
    endforeach()
    target_compile_definitions(gmbllm_test_rolemap_selfcheck PRIVATE LLMCODEC_ABI_DIR="${LLMENV}/ABI")
    # modulo codec di prova usato da test_plugin (nome logico "test_id" -> libllmcodec_test_id)
    add_library(llmcodec_test_id MODULE tests/plugin_test_id.c)
    target_include_directories(llmcodec_test_id PRIVATE ${_gmbllm_test_includes})
    set_target_properties(llmcodec_test_id PROPERTIES PREFIX "lib" OUTPUT_NAME "llmcodec_test_id")
    if(TARGET gmbllm_test_plugin_selfcheck)
        add_dependencies(gmbllm_test_plugin_selfcheck llmcodec_test_id)
    endif()
    # CLI di conversione (richiede il tokenizer per i metadata tokenizer.ggml.*)
    if(TARGET llmtoken)
        add_executable(gmbllm_gguf_convert tests/gguf_convert.c)
        target_link_libraries(gmbllm_gguf_convert PRIVATE gmbllm llmcodec ${_gmbllm_test_plugins} llmtoken)
        target_include_directories(gmbllm_gguf_convert PRIVATE ${_gmbllm_test_includes})
    endif()
endif()
