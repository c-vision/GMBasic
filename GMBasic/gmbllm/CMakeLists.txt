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
# ── SPLIT 2026-11-02 — core (consumato dal runtime) vs train (MLX/LoRA) ──────────────
# `gmbllm_core`: model compiler/surgery + quant-ENCODE + jinja. NON include header del motore
#   MLX: è ciò che il runtime C (= REPL, gmbadmin, vm_host) linka davvero.
# `gmbllm_train`: orchestrazione training/DPO e loss sopra il forward differenziabile MLX
#   (`llmengine_mlx*.h`, `<mlx/c/*>`). Lo linkano SOLO i tool di training (gmb_ai_finetune).
# `gmbllm` resta il nome storico ma è un AGGREGATO (INTERFACE) dei due, così i consumer che lo
#   linkano per nome continuano a funzionare (self-check, build esistenti).
set(GMBLLM_CORE_SOURCES
    src/llmcodec_gguf_mlx.c      # converter MLX<->GGUF (PUL): orchestrazione via registry (read/write Apache, encode PUL)
    src/llm_decide_calib.c       # T5 PUL: fit temperatura 2 stadi (Brier) -> scoring_head.json
    src/llm_decide_loss.c        # T4 PUL: loss decisionali (CE/Brier/RPS) + gradiente
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
    src/llmwsp_store.c           # WSP W1: Weight Store (safetensors slice-read, sha256, copertura) — sola lettura
    src/llmwsp_doc.c             # WSP W2: WSP Document (sezionamento completo, partizioni, copertura, indirizzi)
    src/llmwsp_mat.c             # WSP W3: Materializer (clone shard intatti, riscrive solo quelli modificati, provenance)
    src/llmwsp_linalg.c          # WSP W4: algebra lineare (SVD Jacobi, autovalori non simmetrici, angoli principali)
    src/llmwsp_analyze.c         # WSP W4: analizzatori senza forward (norme, circuiti OV/QK, copy score, composizione)
    src/llmwsp_fixture.c         # WSP W4: fixture compilate a verità nota (copy, induction) per validare gli analizzatori
    src/llmwsp_overlay.c         # WSP W6: overlay copy-on-write, edit, journal, recipe/replay, patch sparsa
    src/llmwsp_derive.c          # WSP W7: chirurgia di architettura (layer, gruppi kv, neuroni MLP) con config/gmbarch coerenti
    src/llmwsp_verify.c          # WSP W8: verifica, gate con soglie dichiarate, hash di modello e catena di provenienza
    src/llmwsp_agent.c           # WSP W9: sessione agente a verbi (budget, trascrizione JSONL, replay deterministico)
    ${LLMENV}/LLMJinja/llmjinja.c            # B2: rendering chat template (Apache, consumato da PUL)
    ${LLMENV}/LLMJinja/llmjinja_builtins.c   # B2: builtins Jinja
    ${LLMENV}/LLMEngine/moe/src/llmengine_moe_quant_alloc.c   # allocatore AUTO formato-per-tensore
    src/quant/llmcodec_iq2_xxs.c    # F3a
    src/quant/llmcodec_iq4_xs.c     # F3a-bis: encode IQ4_XS/IQ4_NL (PUL)
    src/quant/llmcodec_hf_quant.c   # F3d: encode HF (NF4/AWQ/GPTQ/FP4) — contropartite dei decoder Apache
    src/quant/llmcodec_moe_quant.c  # F3c: encode affine MoE (il dequant resta Apache)
)

# Solo questi due richiedono il MOTORE MLX (llmengine_mlx*.h + <mlx/c/*>).
set(GMBLLM_TRAIN_SOURCES
    src/train/llm_train.c            # F4: orchestrazione training/DPO (PUL) — consuma l'ABI Apache
    src/train/llm_train_ce.c         # F4: la LOSS (masked CE/DPO) sopra i logits del forward Apache
)

add_library(gmbllm_core STATIC ${GMBLLM_CORE_SOURCES})
target_include_directories(gmbllm_core PUBLIC include include/quant
    ${LLMENV}/LLMCodec/include       # header Apache (fp16, iq_grids) — gmbllm li CONSUMA, mai l'inverso
    ${LLMENV}/LLMEngine/moe/include
    ${LLMENV}/LLMJinja              # llmjinja.h (rendering chat template) — Apache, consumato da PUL
    ${LLMENV}/LLMEngine/include      # llmengine.h (ABI comune) — serve a llmengine_moe_quant_alloc
    ${CMAKE_CURRENT_SOURCE_DIR}/../native/gmb_runtime_c/include)  # gmb_c99gpm.h: GMB_MUTEX_*/GMB_THREAD_*
# Dipendenze SOLO in direzione gmbllm -> Apache (mai il contrario).
target_link_libraries(gmbllm_core PUBLIC llmcodec llmtoken llmengine_plugin llmdesc ${CMAKE_DL_LIBS} m)

add_library(gmbllm_train STATIC ${GMBLLM_TRAIN_SOURCES})
target_include_directories(gmbllm_train PUBLIC
    ${LLMENV}/LLMEngine/mlx          # ABI Apache (llmengine_mlx.h, llmengine_mlx_diff_forward.h)
    ${LLMENV}/LLMEngine/Options      # llm_engine_opts.h (transitivo)
    ${LLMENV}/libs/mlx-deps/mlx-c-src)  # <mlx/c/*.h> (closure/value_and_grad) — TRAIN
target_link_libraries(gmbllm_train PUBLIC gmbllm_core)

# Nome storico: aggregato dei due (link order: train prima di core).
add_library(gmbllm INTERFACE)
target_link_libraries(gmbllm INTERFACE gmbllm_train gmbllm_core)

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
              test_expert3d test_dsv41_source test_gguf_map test_engram test_plugin test_a1_fp8 test_quantcfg test_wsp_store test_wsp_doc test_wsp_mat test_wsp_linalg test_wsp_analyze test_wsp_fixture test_wsp_overlay test_wsp_derive test_wsp_verify test_wsp_agent)
        add_executable(gmbllm_${_t}_selfcheck tests/${_t}.c)
        target_link_libraries(gmbllm_${_t}_selfcheck PRIVATE gmbllm llmcodec ${_gmbllm_test_plugins})
        target_include_directories(gmbllm_${_t}_selfcheck PRIVATE ${_gmbllm_test_includes})
        add_test(NAME gmbllm_${_t}_selfcheck COMMAND gmbllm_${_t}_selfcheck)
    endforeach()
    target_compile_definitions(gmbllm_test_rolemap_selfcheck PRIVATE LLMCODEC_ABI_DIR="${LLMENV}/ABI")
    # T5: calibrazione (model-free) — linka SOLO il core, senza la closure MLX del trainer.
    add_executable(gmbllm_decide_calib_selfcheck tests/test_decide_calib.c)
    target_link_libraries(gmbllm_decide_calib_selfcheck PRIVATE gmbllm_core)
    target_include_directories(gmbllm_decide_calib_selfcheck PRIVATE include)
    add_test(NAME gmbllm_decide_calib_selfcheck COMMAND gmbllm_decide_calib_selfcheck)
    add_executable(gmbllm_decide_loss_selfcheck tests/test_decide_loss.c)
    target_link_libraries(gmbllm_decide_loss_selfcheck PRIVATE gmbllm_core)
    target_include_directories(gmbllm_decide_loss_selfcheck PRIVATE include)
    add_test(NAME gmbllm_decide_loss_selfcheck COMMAND gmbllm_decide_loss_selfcheck)
    # Tool utente: fit da file di campioni -> scoring_head.json (nessuna dipendenza MLX).
    add_executable(gmbllm_decide_calibrate tools/decide_calibrate.c)
    target_link_libraries(gmbllm_decide_calibrate PRIVATE gmbllm_core)
    target_include_directories(gmbllm_decide_calibrate PRIVATE include)
    add_test(NAME gmbllm_decide_calibrate
             COMMAND gmbllm_decide_calibrate
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/decide_calib_samples.txt
                     ${CMAKE_CURRENT_BINARY_DIR}/decide_calib_out.json)
    add_executable(gmbllm_decide_eval tools/decide_eval.c)
    target_link_libraries(gmbllm_decide_eval PRIVATE gmbllm_core)
    target_include_directories(gmbllm_decide_eval PRIVATE include)
    add_test(NAME gmbllm_decide_eval
             COMMAND gmbllm_decide_eval
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/decide_calib_samples.txt)
    # T4 — gate NUMERICO del grafo MLX della decision loss (loss+gradienti vs llm_decide_loss).
    # Richiede i simboli mlx-c esportati dalla dylib del motore: se non c'è, il gate è saltato
    # (la build normale non dipende dal motore MLX).
    set(_gmbllm_mlx_lib "${LLMENV}/LLMEngine/mlx/build-gmbasic/libllm_ai_mlx.dylib")
    if(EXISTS "${_gmbllm_mlx_lib}")
        add_executable(gmbllm_decide_mlx_selfcheck tests/test_decide_mlx_loss.c)
        target_link_libraries(gmbllm_decide_mlx_selfcheck PRIVATE gmbllm_train "${_gmbllm_mlx_lib}" "-framework Accelerate")
        target_include_directories(gmbllm_decide_mlx_selfcheck PRIVATE include
            ${LLMENV}/LLMEngine/mlx ${LLMENV}/libs/mlx-deps/mlx-c-src)
        add_test(NAME gmbllm_decide_mlx_selfcheck COMMAND gmbllm_decide_mlx_selfcheck)
        # CLI T4: training LoRA con la loss decisionale su un JSONL (context/options/label).
        add_executable(gmbllm_decide_train tools/decide_train.c)
        target_link_libraries(gmbllm_decide_train PRIVATE gmbllm "${_gmbllm_mlx_lib}" "-framework Accelerate")
        target_include_directories(gmbllm_decide_train PRIVATE include
            ${LLMENV}/LLMEngine/mlx ${LLMENV}/libs/mlx-deps/mlx-c-src ${LLMENV}/LLMEngine/include)
    else()
        message(STATUS "gmbllm: libllm_ai_mlx non trovata -> gate MLX decide-loss saltato")
    endif()
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
