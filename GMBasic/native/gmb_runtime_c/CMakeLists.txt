cmake_minimum_required(VERSION 3.10)
project(gmb_runtime_c C)

# RID della macchina di build (GMB_RID, GMB_RPATH_ORIGIN) — unico posto in cui si decide.
# Prima "osx-arm64" era hardcoded qui sotto e nell'output delle dylib: su Linux/Windows gli
# artefatti finivano nella cartella sbagliata.
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbRid.cmake)

# PIC ovunque: su Linux/arm64 una STATIC lib non-PIC NON può essere linkata dentro una SHARED
# ("relocation R_AARCH64_ADR_PREL_PG_HI21 ... recompile with -fPIC"). Su macOS è il default,
# quindi il problema emerge solo qui. Le nostre statiche (gmb_runtime, gmb_chart, …) finiscono
# dentro gmb_gfx/gmb_pkg/…: tutte devono essere -fPIC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Portabilità (nessuna dipendenza dall'ambiente del builder): l'output usa INSTALL_RPATH (@loader_path), così
# le dep linkate dal bundle Resources/libs/<rid> (install_name @rpath/<nome>) si risolvono
# nella stessa cartella. Gli eseguibili (gmb_vm_host) risolvono via DYLD_LIBRARY_PATH (runner).
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
# ld64 tiene nella dylib SOLO le librerie di cui usa davvero i simboli. Senza, ogni `-l`
# elencato (crypto, ssl, zstd, brotli, lz4, ...) finiva in LC_LOAD_DYLIB anche se inutilizzato:
# `libgmb_gfx` si portava dietro OpenSSL e 4 compressori per niente.
if(APPLE)
    # ld64 tiene nella dylib SOLO le librerie di cui usa davvero i simboli.
    add_link_options(-Wl,-dead_strip_dylibs)
elseif(UNIX)
    # GNU ld / lld: stesso intento (non registrare le lib linkate e inutilizzate).
    add_link_options(-Wl,--as-needed)
endif()
# Le librerie native NON stanno più in Resources/libs (che nel checkout era un SYMLINK alla
# cache: il build ci scriveva dentro e rimetteva le copie vecchie). Rpath:
#   1. accanto alla dylib/eseguibile                      (@loader_path)
#   2. output del build                                   (build/libs/<rid>)
#   3. CACHE ON-DEMAND dell'utente                        (~/.gmbasic/libs/<rid>)
# La cache è l'ultima: in sviluppo vince il build appena fatto, nel client esiste solo lei.
if(WIN32)
    set(CMAKE_INSTALL_RPATH "")   # Windows non ha rpath: risolve accanto all'eseguibile
else()
    set(CMAKE_INSTALL_RPATH "${GMB_RPATH_ORIGIN};${GMB_LIBS_OUT};$ENV{HOME}/.gmbasic/libs/${GMB_RID}")
endif()

# Motore AI UNICO = LLMEnv (sorgente unica; GMBasic non tiene piu' copie native/gmb_*).
set(LLMENV "$ENV{LLMENV}")
if(NOT LLMENV)
    if(EXISTS "$ENV{HOME}/Documents/Projects/LLMEnv/LLMEngine/mlx")
        set(LLMENV "$ENV{HOME}/Documents/Projects/LLMEnv")
    else()
        set(LLMENV "${CMAKE_CURRENT_SOURCE_DIR}/../../LLMEnv")
    endif()
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# K6: parità bit-exact tra backend/golden — niente contrazione FMA in C (i default dei
# compilatori non sono uniformi: Clang `on`, GCC `fast`). Vedi
# Knowledge/FP-Contraction-Parita-Bit-Exact.md.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-ffp-contract=off)
    # Ottimizzazioni e codegen a sezioni SEMPRE: ogni funzione/dato in una sezione propria,
    # cosi' il linker finale puo' scartare il codice morto (anche una singola funzione dentro
    # un .o altrimenti tutto-o-niente dell'archivio). Lo scarto e' attivato al link.
    add_compile_options(-O2 -ffunction-sections -fdata-sections)
endif()
if(APPLE)
    add_link_options(-Wl,-dead_strip)
elseif(UNIX)
    add_link_options(-Wl,--gc-sections)
endif()

# macOS/Mach-O: `__attribute__((weak))` NON rende debole una REFERENZA esterna (a differenza di
# ELF) → i target che linkano gmb_runtime senza gmb_agent_runtime (che definisce gmb_brain_ask_sync,
# referenza debole opzionale in gmb_agent.c) fallivano con "Undefined symbols: _gmb_brain_ask_sync".
# `-U` consente il simbolo non definito (→ indirizzo 0 a runtime, come su Linux).
if(APPLE)
    add_link_options("-Wl,-U,_gmb_brain_ask_sync")
    # Stessa ragione per il system prompt opzionale del brain MLX (gmb_brain_mlx_set_system,
    # referenza debole in gmb_agent.c): i target che non linkano gmb_agent_runtime lo lasciano a 0.
    add_link_options("-Wl,-U,_gmb_brain_mlx_set_system")
    # Idem per il limite di token nuovi (gmb_brain_mlx_set_max_new).
    add_link_options("-Wl,-U,_gmb_brain_mlx_set_max_new")
    # Idem per il tool-loop di AI AGENT ASK (gmb_agent_ask_with_tools, in gmb_agent_runtime).
    add_link_options("-Wl,-U,_gmb_agent_ask_with_tools")
endif()

include(CTest)

include_directories(include)
include_directories(src)
include_directories(${LLMENV}/LLMEngine/moe/include)
include_directories(${LLMENV}/LLMEngine/asr/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_compiler_c/include)

# AMOS: sorgenti Bessel (f2c) in src/amos/ — vedi gmb_bessely/gmb_besselk in gmb_math.c.
file(GLOB AMOS_SRCS CONFIGURE_DEPENDS src/amos/*.c)

# Progetto C autonomo: codec GGML (+ future conversioni GGUF<->MLX). Nessuna dipendenza dal runtime.
add_subdirectory(${LLMENV}/LLMCodec llmcodec_build)

# Progetto C autonomo: tokenizer (UNICA sorgente di tokenizer.json/tiktoken/wordpiece).
# Guard: se il target esiste gia' nel tree, riusalo (evita target duplicato).
if(NOT TARGET llmtoken)
    add_subdirectory(${LLMENV}/LLMToken llmtoken_build)
endif()

# Progetti C autonomi (scomposizione del motore di inferenza): ABI plugin + descrizione .gmbarch.
if(NOT TARGET llmengine_plugin)
    add_subdirectory(${LLMENV}/LLMEngine/plugin llmengine_plugin_build)
endif()
if(NOT TARGET llmdesc)
    add_subdirectory(${LLMENV}/LLMDesc llmdesc_build)
endif()
# Engine ASR Qwen3-ASR (inference-only, Apache): vive in LLMEnv e viene CONSUMATO (no copie).
if(NOT TARGET llm_asr)
    add_subdirectory(${LLMENV}/LLMEngine/asr llmasr_build)
endif()
# GMBLLM (PUL): model compiler/surgery/planning + quant-encode + training. Dentro GMBasic, licenza PUL.
# Linka le librerie Apache di LLMEnv (mai l'inverso). Vedi ADR-Confine-Licenza-Engine-Apache-Training-Quant-PUL.
if(NOT TARGET gmbllm)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm gmbllm_build)
endif()

# ── DYLIB AI del runtime (2026-11-02): le SOLE parti che dipendono da LLMEnv ──────────────────
# `gmb_asr_cmd.c`, `gmb_llm_tok.c`, `gmb_model_io.c` usano ASR/llmmodelio/llmdesc di LLMEnv.
# Vivono in una dylib separata, caricata su richiesta dal core (`gmb_ai_gateway.c`): così il
# runtime — e quindi REPL/gmbadmin/vm_host — si compila e linka SENZA LLMEnv.
# Non linka il runtime: risolve i simboli del core a load-time (evita una seconda copia degli
# stati globali del core). Su macOS `-undefined dynamic_lookup`; su ELF servono i simboli
# esportati dal consumer (`-Wl,--export-dynamic`, sotto).
# Windows NON ha il "dynamic_lookup": là lo split non c'è, le sorgenti AI restano nel CORE
# (vedi il blocco condizionale su GMB_CORE_SRCS) e questa dylib non si costruisce.
if(NOT WIN32)
    set(GMB_RUNTIME_AI_SRCS
        src/gmb_ai_register.c
        src/gmb_asr_cmd.c
        src/gmb_llm_tok.c
        src/gmb_model_io.c
        src/gmb_distill.c
        src/gmb_ai_decide_local.c
        ../gmb_compiler_c/src/model/gmb_model_quantize.c
        ${LLMENV}/LLMEngine/moe/src/llmengine_moe_quant_alloc.c
        ${LLMENV}/LLMEngine/moe/src/llmengine_moe_gguf.c
        ${LLMENV}/LLMEngine/moe/src/llmengine_moe_gguf_expert.c
        ${LLMENV}/LLMEngine/moe/src/llmengine_moe_ggml.c
        ${LLMENV}/LLMEngine/moe/src/llmengine_moe_discover.c)
    add_library(gmb_runtime_ai SHARED ${GMB_RUNTIME_AI_SRCS})
    target_include_directories(gmb_runtime_ai PRIVATE include src
        ${LLMENV}/LLMEngine/include
        ${LLMENV}/LLMCodec/include
        ${LLMENV}/LLMToken/include
        ${LLMENV}/LLMEngine/moe/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/include/quant)
    target_link_libraries(gmb_runtime_ai PRIVATE
        llm_asr llmcodec llmtoken llmdesc llmengine_plugin gmbllm_core ${CMAKE_DL_LIBS} m)
    if(APPLE)
        target_link_options(gmb_runtime_ai PRIVATE "-Wl,-undefined,dynamic_lookup")
    endif()
    set_target_properties(gmb_runtime_ai PROPERTIES
        PREFIX "lib" OUTPUT_NAME "gmb_runtime_ai"
        LIBRARY_OUTPUT_DIRECTORY "${GMB_LIBS_OUT}")
endif()

# Motore chart autonomo e riusabile (native/gmb_chart): pura libc/math + function table
# di disegno fornita dall'host. Linkato dal runtime via l'adapter gmb_graphics_chart.c;
# linkabile anche da app C esterne. Nessuna dipendenza da GMBasic/SDL/g_gfx.
if(NOT TARGET gmb_chart)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_chart gmb_chart_build)
endif()

set(GMB_ALL_SRCS
    src/gmb_libs_fetch.c   # download on-demand di UNA libreria (asset per-file)
    src/gmb_pkg_manifest.c # parser manifest (gmb-package.json / brains-manifest.json): runtime E gmb_pkg
    src/gmb_fetch.c        # motore download: streaming + resume + lock + sha (brains/pacchetti/libs)
    src/gmb_fetch_curl.c   # trasporto HTTP del motore (libcurl); senza curl = NULL, non stub
    src/gmb_brains.c       # cervelli come dati: risoluzione (vendor/exe/cache) + install da manifest
    src/gmb_ai_gateway.c   # entry point AI (ASR/LLM/modelio) → dylib gmb_runtime_ai, deboli nel core
    src/gmb_brain_registry.c   # Second Brain: registry brains.json + manifest brain.manifest.json
    src/gmb_brain_tools.c      # Second Brain: stats/verify/create/register/buildvault/optimize/rename/export/sign
    src/gmb_brain_tools2.c     # Second Brain: dedupe/extract/merge/convert/verify-golden/diff/doctor
    src/gmb_brain_graph.c      # Second Brain: modello grafo + export (json/stats/mermaid/graphml/html/obsidian)
    src/gmb_fixtures.c     # fixture di mercato on demand (manifest pubblico + gmb_fetch)
    src/gmb_fsutil.c       # mkdir -p / rmtree / isdir / spazio: una sola implementazione
    src/gmb_ui_stub.c   # debole, PRIMO membro: risolve i riferimenti UI del core in headless
    src/gmb_px_renderer.c   # registry renderer pixel (core); i renderer si registrano dal target UI
    src/gmb_ui_gfx.c   # interfaccia verso la pagina grafica (core; tabella installata dal target UI)
    src/gmb_gfx_input.c   # code FIFO input (core: NIENTE SDL; gmb_graphics_chart la referenzia)
    src/gmb_tui_controls_core.c   # factory testuali dei controlli (core)
    src/gmb_tui_surface_model.c   # primitivo rounded-rect a celle (core)
    src/gmb_graphics_controls_px.c   # renderer PIXEL dei controlli (target UI)
    src/gmb_read.c
    src/gmb_events.c
    src/gmb_ide.c
    src/gmb_form.c
    src/gmb_form_read.c
    src/gmb_msg.c
    # Bus dei segnali (namespace SIGNAL): filtro strutturato + consegna agli handler.
    src/gmb_signal.c
    src/gmb_core.c
    src/gmb_module.c
    src/gmb_eval_scope.c
    # Scope dinamico BASIC (DYNAMIC): catena di frame per-task, condivisa VM/C/C99Native
    src/gmb_dscope.c
    src/gmb_bsp.c
    src/gmb_bsp_compile.c
    src/gmb_bsp_gateway.c
    src/gmb_state_hook.c   # hook opzionale di stato (agenti/team/workflow/memoria) per la VM/C ospitata
    src/gmb_list.c
    src/gmb_dict.c
    src/gmb_console.c
    src/gmb_observation.c
    src/gmb_opts.c
    src/gmb_dbg_time.c
    src/gmb_bits.c
    src/gmb_file.c
    src/gmb_file_extra.c
    src/gmb_fonts.c        # namespace FONTS: metriche/render TrueType (stb_truetype, una sola sorgente)
    src/gmb_finetune.c
    # MODEL COMPILER LLM (save/quantize/convert) + imatrix/forward: progetto gmb_model_io (linkato sotto)
    # registry plugin + .gmbarch: progetti autonomi (LLMEngine/plugin / LLMDesc), linkati sotto
    # MODEL I/O in-process (LLM MODEL CONVERT/QUANTIZE/INSPECT): la logica di discover/convert/
    # quantize sta nel runtime condiviso così i 3 backend C la chiamano DIRETTAMENTE (niente
    # subprocess da gmb_moestream.c). Le stesse sorgenti sono usate dalla dylib gmb_ai_modelio.
    # LLM MODEL LOAD/UNLOAD/CACHE/PREFETCH/DEVICE/PROMOTE/DEMOTE + LLM.STREAMSTATS: provider
    # MoE out-of-core (dlopen libgmb_ai_moestream). Ripristinato 2026-09-25.
    src/gmb_moestream.c
    ${LLMENV}/LLMEngine/moe/src/llmengine_moe_arch.c
    ${LLMENV}/LLMEngine/moe/src/llmengine_moe_json.c
    ../gmb_compiler_c/src/model/gmb_st.c
    src/gmb_ipc_client.c
    src/gmb_job_registry.c
    src/gmb_shell.c
    src/gmb_event.c
    src/gmb_timeseries.c
    src/gmb_array.c
    # CommonLib AudioUtils (5 func math, no SDL dep)
    src/gmb_audio.c
    # ASR: entry-point P/Invoke `gmb_asr_cmd` (engine in LLMEnv/LLMEngine/asr, target llm_asr).
    src/gmb_workflow.c
    src/gmb_strings.c
    src/gmb_stringbuilder.c
    # Accumulatore growable per il pattern s$=s$+expr riconosciuto dal codegen C99 (§11)
    src/gmb_strbuf.c
    src/gmb_datetime.c
    src/gmb_format.c
    # SemVer 2.0 + range matching — riferimento C del Package Manager (§"backend di riferimento è C")
    src/gmb_semver.c
    src/gmb_math.c
    src/gmb_iowatch.c
    src/gmb_opt_pso.c
    src/gmb_regex_extra.c
    src/gmb_grep.c
    src/gmb_matcher.c
    src/gmb_timer_extra.c
    src/gmb_collections_extra.c
    src/gmb_bitserializers.c
    src/gmb_memory.c
    src/gmb_bsp_session.c
    src/gmb_joy.c
    src/gmb_screens_extra.c
    src/gmb_screen_config.c
    src/gmb_sdl_console.c
    src/gmb_iconsole.c
    src/gmb_sdl_manager.c
    src/gmb_sdl_renderer.c
    src/gmb_sdl_context.c
    src/gmb_console_extra.c
    src/gmb_keyboard_extra.c
    src/gmb_mouse_input.c
    src/gmb_kdtree.c
    src/gmb_filters.c
    src/gmb_stats.c
    src/gmb_ode.c
    src/gmb_interp.c
    src/gmb_spatial.c
    src/gmb_network.c
    src/gmb_hpack.c
    src/gmb_http2.c
    src/gmb_validation.c
    src/gmb_matrix.c
    src/gmb_finance.c
    src/gmb_finance_qlnet.c
    src/gmb_agent.c
    src/gmb_context.c
    src/gmb_worker.c
    src/gmb_agent_queue.c
    src/gmb_blackboard.c
    src/gmb_category.c
    src/gmb_taskgraph.c
    src/gmb_plan.c
    src/gmb_hook.c
    src/gmb_verify.c
    src/gmb_observe_test.c
    src/gmb_observe_sources.c
    src/gmb_verify_require.c
    src/gmb_eval_hook.c
    src/gmb_artifact.c
    src/gmb_resource.c
    src/gmb_handoff.c
    src/gmb_rules.c
    src/gmb_goal.c
    src/gmb_replay.c
    src/gmb_metrics.c
    src/gmb_hashline.c
    src/gmb_checkpoint.c
    src/gmb_orchestration.c
    src/gmb_agent_http.c
    src/gmb_message_bus.c
    src/gmb_agent_fabric.c
    src/gmb_kg.c
    src/gmb_team.c
    src/gmb_ai.c
    src/gmb_ai_decide.c
    src/gmb_decision_head.c
    src/gmb_mcp_server.c
    src/gmb_ai_server.c
    src/gmb_brain.c
    src/gmb_secondbrain.c
    src/gmb_eval.c
    src/gmb_connector.c
    # Risoluzione percorsi dati GMBASIC nel MONDO C (config/, rag/, graphs/, build/libs/)
    src/gmb_paths.c
    src/gmb_abi.c
    src/gmb_rdd.c
    src/gmb_rdd_rowbuf.c
    src/gmb_sql.c
    src/gmb_vectordb.c
    # RAG paritario col C# (SqlVectorSchema+hybrid RRF+FTS5, condiviso VM/C e C99)
    src/gmb_rag.c
    src/gmb_rag_search.c
    # Quantizzazione SQ8 + scoring int8 (unica implementazione §0bis, parità C#/VM/C/C99)
    src/gmb_vecquant.c
    # Virtual table `csv` (scan diretto CSV; sorgente unico §0bis, registrazione statica qui e
    # come estensione caricabile per gli host C#)
    src/gmb_csv_vtab.c
    src/gmb_parquet_vtab.c
    # RAG eval: quartetto retrieval (P@K/R@K/MRR/NDCG) + giudizi generazione tier-1 (todo Rag-Eval-Gap-Analysis Fase A)
    src/gmb_rag_eval.c
    # Stream D: self-distillation lm_head-only — dataset/report condiviso (AI DISTILL, fase D1)
    # RAG eval avanzato R4-R8: diagnosi, Cohen kappa, Wilson CI, calibrazione, gate, PSI, z-test, Welch t, compare
    src/gmb_rageval_advanced.c
    # Self-Consistency / Semantic Entropy (Spanda R_sc) — todo AI-SelfConsistency-Uncertainty-Spanda-Gap-Analysis
    src/gmb_uncertainty.c
    src/gmb_graph.c
    src/gmb_graph_alg.c
    src/gmb_dag.c
    src/gmb_collections.c
    src/gmb_hll.c          # HyperLogLog (stima cardinalità) — runtime condiviso (§0bis)
    # FFI moduli nativi (gmb_ffi.h/.c — marshalling copy-in/copy-out per pacchetti)
    src/gmb_ffi.c
    # FFI dl condiviso (dlopen/dlsym + preload pacchetti): usato da C99, VM/C e host IL
    src/gmb_ffi_dl.c
    # SYSTEM.SYS (OS/ARCH/CPU/MACHINE/USER/CWD) + memoria di sistema (API native)
    src/gmb_sysinfo.c
    # CommonGraphics (porting software rendering: pagine/palette/primitive/testo/PNG) — split per evitare monolite
    src/gmb_graphics.c
    src/gmb_graphics_text.c
    src/stb_truetype_impl.c
    src/gmb_gfx_cellfont.c
    src/gmb_graphics_texture.c
    src/gmb_graphics_sprite.c
    src/gmb_graphics_tilemap.c
    src/gmb_graphics_chart.c
    src/gmb_graphics_primitive.c
    src/gmb_graphics_image.c
    src/gmb_graphics_screenshot.c
    src/gmb_graphics_palette.c
    # F3 multimodale (2026-09-11): dominio immagini (decode/resize/crop/encode PNG condivisi).
    src/gmb_image.c
    # F2 multimodale (2026-09-11): estrazione strutturata documenti (euristiche + LLM).
    src/gmb_docfields.c
    # F4 multimodale (2026-09-11): TTS via motore di sistema.
    src/gmb_tts.c
    # F7 multimodale (2026-09-11): VIDEO (Y4M nativo + ffmpeg).
    src/gmb_video.c
    # F6 multimodale (2026-09-11): agent ask multimodale (separato: dipende da gmb_doc_c).
    src/gmb_agent_file.c
    # Crypto/blockchain/encoding/compress (ex gmb_commonlib_c — unificato nel runtime)
    src/gmb_crypto.c
    src/gmb_encoding.c
    src/gmb_difficulty.c
    src/gmb_screen_buffer.c
    src/gmb_blockchain.c
    src/gmb_compress.c
    src/gmb_bzip2.c
    src/gmb_aes.c
    src/gmb_keys.c
    # TUI (porting ClipperLib: CellColor, ScreenBuffer, Layout, Mask, controlli,
    # stringhe, numeri, Quantize) — vedi include/gmb_tui.h
    src/gmb_settings.c
    src/gmb_tui.c
    src/gmb_c99gpm.c
    # AMOS (Bessel special functions): traduzione f2c 1:1 della libreria AMOS di
    # D. E. Amos (scipy/netlib) — bit-esatta con MathNet 5.0.0 (canone C# SPECIAL).
    ${AMOS_SRCS}
)

# ── Eseguibili-C-Minimali: split core/ui su OBJECT libs (una sola compilazione) ──
# UI = sorgenti che dipendono da SDL/gfx/tui; il core resta linkabile da solo.
set(GMB_UI_SRCS "")
set(GMB_CORE_SRCS "")
foreach(_s IN LISTS GMB_ALL_SRCS)
  get_filename_component(_bn "${_s}" NAME)
  if(NOT (_bn STREQUAL "gmb_tui.c" OR _bn STREQUAL "gmb_console_extra.c" OR _bn STREQUAL "gmb_tui_alertbox.c") AND (_bn MATCHES "^gmb_(gfx|tui|sdl|iconsole|graphics)" OR _bn STREQUAL "gmb_screen_config.c"
     OR _bn STREQUAL "gmb_screens_extra.c" OR _bn STREQUAL "gmb_keyboard_extra.c"
     OR _bn STREQUAL "gmb_joy.c"))
    list(APPEND GMB_UI_SRCS "${_s}")
  else()
    list(APPEND GMB_CORE_SRCS "${_s}")
  endif()
endforeach()
list(REMOVE_ITEM GMB_CORE_SRCS src/gmb_sql.c src/gmb_csv_vtab.c src/gmb_parquet_vtab.c)
# SPLIT core/AI — Windows: niente risoluzione dinamica dei simboli del core, quindi le
# sorgenti AI restano nel core (link statico, come prima dello split) e il gateway (che
# parla solo dlopen/LoadLibrary) non si compila: i simboli forti sono già qui.
if(WIN32)
  list(REMOVE_ITEM GMB_CORE_SRCS src/gmb_ai_gateway.c)
  list(APPEND GMB_CORE_SRCS
      src/gmb_asr_cmd.c
      src/gmb_llm_tok.c
      src/gmb_model_io.c
      src/gmb_distill.c
      src/gmb_ai_decide_local.c
      ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_compiler_c/src/model/gmb_model_quantize.c
      ${LLMENV}/LLMEngine/moe/src/llmengine_moe_quant_alloc.c
      ${LLMENV}/LLMEngine/moe/src/llmengine_moe_gguf.c
      ${LLMENV}/LLMEngine/moe/src/llmengine_moe_gguf_expert.c
      ${LLMENV}/LLMEngine/moe/src/llmengine_moe_ggml.c
      ${LLMENV}/LLMEngine/moe/src/llmengine_moe_discover.c)
endif()
set(GMB_SQL_SRCS src/gmb_sql.c src/gmb_csv_vtab.c src/gmb_parquet_vtab.c src/gmb_sql_iface.c)
add_library(gmb_runtime_deps INTERFACE)
if(WIN32)
  # Windows (split assente): il core contiene le sorgenti AI → serve LLMEnv al link.
  target_link_libraries(gmb_runtime_deps INTERFACE
      llmcodec llmcodec_ggml llmcodec_fp8 llmcodec_mlx_affine llmtoken llmengine_plugin
      llmdesc llm_asr gmbllm_core)
endif()
add_library(gmb_runtime_sql_obj OBJECT ${GMB_SQL_SRCS})
target_link_libraries(gmb_runtime_sql_obj PRIVATE gmb_runtime_deps)
add_library(gmb_runtime_sql STATIC $<TARGET_OBJECTS:gmb_runtime_sql_obj>)
target_link_libraries(gmb_runtime_sql PUBLIC gmb_runtime_deps)
add_library(gmb_runtime_core_obj OBJECT ${GMB_CORE_SRCS})
add_library(gmb_runtime_ui_obj OBJECT ${GMB_UI_SRCS})
target_link_libraries(gmb_runtime_core_obj PRIVATE gmb_runtime_deps)
# Il runtime COMPLETO include l'UI (FORM/READ con listbox/combobox/ombra/input).
target_compile_definitions(gmb_runtime_core_obj PRIVATE GMB_WITH_UI=1)
# Path di BUILD della dylib AI: il gateway la trova anche senza install (dev/REPL build dir).
target_compile_definitions(gmb_runtime_core_obj PRIVATE
    GMB_RUNTIME_AI_PATH="${GMB_LIBS_OUT}/${CMAKE_SHARED_LIBRARY_PREFIX}gmb_runtime_ai${CMAKE_SHARED_LIBRARY_SUFFIX}")
target_link_libraries(gmb_runtime_ui_obj PRIVATE gmb_runtime_deps)
# La copia UI (TUI + present SDL/Vulkan) dichiara GMB_WITH_UI: è lei che fornisce
# listbox/combobox/input/ombra a FORM e READ. Il CORE resta senza riferimenti all'UI,
# così un eseguibile testuale linka solo core (niente SDL/MoltenVK).
target_compile_definitions(gmb_runtime_ui_obj PRIVATE GMB_WITH_UI=1)
add_library(gmb_runtime STATIC $<TARGET_OBJECTS:gmb_runtime_core_obj> $<TARGET_OBJECTS:gmb_runtime_ui_obj> $<TARGET_OBJECTS:gmb_runtime_sql_obj>)
add_library(gmb_runtime_core STATIC $<TARGET_OBJECTS:gmb_runtime_core_obj>)
# ── Variante MINIMA (senza UI) ──────────────────────────────────────────────────────────────
# Stesse fonti del core ma SENZA GMB_WITH_UI: FORM/READ non referenziano listbox/combobox/
# input/ombra → un eseguibile testuale linka questa e porta con sé ZERO librerie esterne
# (niente SDL2/SDL2_ttf/MoltenVK). Il runtime completo (gmb_runtime) resta quello con l'UI.
add_library(gmb_runtime_min_obj OBJECT ${GMB_CORE_SRCS})
target_link_libraries(gmb_runtime_min_obj PRIVATE gmb_runtime_deps)
# IMBUTO INPUT: il lettore del terminale (termios, ZERO SDL) e la coda eventi condivisa
# sono core, non UI. Senza queste due la variante minima ripiegava sugli stub di
# gmb_ui_null.c: INKEY leggeva sempre "" e i tasti non entravano mai nel gestore eventi
# (nessun TUI/GUI/ON KEY li vedeva). Cfr. gmb_input_feeder.c.
target_sources(gmb_runtime_min_obj PRIVATE src/gmb_tui_io.c src/gmb_gfx_input.c src/gmb_input_feeder.c)
# GMB_ALL_SRCS è una lista esplicita: il feeder input va aggiunto anche al core completo.
target_sources(gmb_runtime_core_obj PRIVATE src/gmb_input_feeder.c)
# Stesso path di build della dylib AI del core completo: anche i programmi generati da
# `gmb run` (linkati contro libgmb_runtime_min.a) devono trovare gmb_runtime_ai.
target_compile_definitions(gmb_runtime_min_obj PRIVATE
    GMB_RUNTIME_AI_PATH="${GMB_LIBS_OUT}/${CMAKE_SHARED_LIBRARY_PREFIX}gmb_runtime_ai${CMAKE_SHARED_LIBRARY_SUFFIX}")
# Stub NO-OP dell'API UI: permettono alla variante minima di linkare senza SDL/Vulkan.
add_library(gmb_ui_null_obj OBJECT src/gmb_ui_null.c)
target_link_libraries(gmb_ui_null_obj PRIVATE gmb_runtime_deps)
add_library(gmb_runtime_min STATIC $<TARGET_OBJECTS:gmb_runtime_min_obj> $<TARGET_OBJECTS:gmb_ui_null_obj>)

# SPLIT core/AI (2026-11-02) — ELF: la dylib AI è caricata con dlopen e risolve a load-time i
# simboli del core. Su Linux l'eseguibile deve esportarli (`-rdynamic`), altrimenti il
# caricamento fallisce con "undefined symbol". macOS non ne ha bisogno (dynamic_lookup).
if(UNIX AND NOT APPLE)
    target_link_options(gmb_runtime INTERFACE "-Wl,--export-dynamic")
    target_link_options(gmb_runtime_min INTERFACE "-Wl,--export-dynamic")
    target_link_options(gmb_runtime_core INTERFACE "-Wl,--export-dynamic")
endif()

# ── libgmb_screen: SOLO il buffer di schermo a celle (zero terze parti) ────────────────────────
# La console testuale (C# ScreenBuffer/CClipperBuffer) usa solo queste funzioni: tenerle in
# gmb_gfx costringeva ogni processo a caricare SDL/Vulkan (~8 MB) per un buffer di testo.
add_library(gmb_screen SHARED src/gmb_screen_buffer.c)
target_include_directories(gmb_screen PRIVATE include src)
target_link_libraries(gmb_screen PRIVATE gmb_runtime_deps)
set_target_properties(gmb_screen PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${GMB_LIBS_OUT}")
target_link_libraries(gmb_runtime_min PUBLIC gmb_runtime_deps)
add_library(gmb_runtime_ui STATIC $<TARGET_OBJECTS:gmb_runtime_ui_obj>)
target_link_libraries(gmb_runtime PUBLIC gmb_runtime_deps)
target_link_libraries(gmb_runtime_core PUBLIC gmb_runtime_deps)

# GPM C99 (piano C99Native FASE 2.6a): porting 1:1 del prologo goroutine di
# C99CodeEmitter.cs. Il file è stato generato UNA TANTUM con lo script
# `tools/gmb_c99gpm_gen.py`, RIMOSSO il 2026-10-28: il suo input (C99CodeEmitter.cs) è
# ritirato (sorgenti in GMBasic-Deprecated) e gmb_c99gpm.c è ora sorgente stabile e
# tracciato — non c'è più nulla da rigenerare (regola Python-free, AGENTS).
# Definizioni WEAK per i simboli condivisi con gmb_gpm.c del VM (il VM host linka
# entrambi i GPM: il suo per il bytecode + questo per il park del runtime form/TUI).
# __thread (GNU): C_EXTENSIONS ON.
set_source_files_properties(src/gmb_c99gpm.c PROPERTIES C_EXTENSIONS ON)

# Moduli TUI per-file (controlli/console/input/eventi/widget) — un file per dominio.
# CONFIGURE_DEPENDS: un nuovo gmb_tui_*.c entra nel build senza riconfigure manuali.
file(GLOB GMB_TUI_MODULES CONFIGURE_DEPENDS src/gmb_tui_*.c)
list(REMOVE_ITEM GMB_TUI_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui.c)
# Porting Clipper TUI 1:1 con common/ClipperLib — inclusi ESPLICITAMENTE (motore
# layout FlexLayout.cs + controllo interattivo Controls/Repl.cs): il porting C# è 1:1,
# restano nel progetto anche se il VM non li dispatcia ancora a runtime.
list(REMOVE_ITEM GMB_TUI_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_flex.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_repl.c)
list(REMOVE_ITEM GMB_TUI_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_alertbox.c)
list(REMOVE_ITEM GMB_TUI_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_tbrowse.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_spinner.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_windowmanager.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_theme.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_io.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_control.c)
list(REMOVE_ITEM GMB_TUI_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_clipper.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_style.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_mouse.c)
list(REMOVE_ITEM GMB_TUI_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_numericupdown.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_scrollbar.c ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_starrating.c)
list(REMOVE_ITEM GMB_TUI_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_tui_datatypes.c)
target_sources(gmb_runtime_core_obj PRIVATE src/gmb_tui_datatypes.c)
target_sources(gmb_runtime_core_obj PRIVATE src/gmb_tui_controls_core.c)
target_sources(gmb_runtime_core_obj PRIVATE src/gmb_tui_surface_model.c)
target_sources(gmb_runtime_core_obj PRIVATE src/gmb_tui_control_registry.c)
target_sources(gmb_runtime_core_obj PRIVATE src/gmb_tui_numericupdown.c src/gmb_tui_scrollbar.c src/gmb_tui_starrating.c)
target_sources(gmb_runtime_core_obj PRIVATE src/gmb_tui_clipper.c src/gmb_tui_style.c src/gmb_tui_mouse.c)
target_sources(gmb_runtime_core_obj PRIVATE src/gmb_tui_tbrowse.c src/gmb_tui_spinner.c src/gmb_tui_windowmanager.c src/gmb_tui_theme.c src/gmb_tui_io.c src/gmb_tui_control.c)
target_sources(gmb_runtime_core_obj PRIVATE src/gmb_tui_alertbox.c)   # renderer celle (core); px nel target UI
target_sources(gmb_runtime_ui_obj PRIVATE ${GMB_TUI_MODULES} src/gmb_tui_flex.c src/gmb_tui_repl.c)

target_include_directories(gmb_runtime_deps INTERFACE include ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_chart/include)
target_link_libraries(gmb_runtime_deps INTERFACE gmb_chart)

# ── Package Manager — riferimento C, come SHARED lib per il P/Invoke del C# ──
# ("il PM di riferimento vive in C": il C# CONSUMA queste funzioni, non le ricopia;
#  direzione unica C#→C, mai C→C#.)
add_library(gmb_pkg SHARED
    src/gmb_semver.c
    src/gmb_pkg_lock.c
    src/gmb_pkg_trust.c
    src/gmb_pkg_glob.c
    src/gmb_pkg_contentaddr.c
    src/gmb_pkg_resolver.c
    src/gmb_pkg_registry.c
    src/gmb_pkg_artifact.c
    src/gmb_pkg_install.c
    src/gmb_pkg_pack.c
    src/gmb_pkg_advisories.c
    src/gmb_pkg_ed25519.c
    src/gmb_pkg_ecdsa.c
    src/gmb_pkg_util.c
    src/gmb_pkg_graph.c
    src/gmb_pkg_brains.c   # ponte P/Invoke dei cervelli (logica in gmb_brains.c, runtime)
)
target_include_directories(gmb_pkg PUBLIC include)
target_link_libraries(gmb_pkg PRIVATE gmb_runtime)
set_target_properties(gmb_pkg PROPERTIES
    OUTPUT_NAME gmb_pkg
    C_VISIBILITY_PRESET default)

# FONTS (TrueType/OpenType) come libreria CONDIVISA per P/Invoke (IL/PCodeVM): stesse
# sorgenti della static (gmb_fonts.c + l'unico TU di stb_truetype), nessuna duplicazione
# dell'algoritmo nei backend. Espone gmb_fonts_tt_* (gmb_fonts.h).
add_library(gmb_fonts SHARED
    src/gmb_fonts.c
    src/stb_truetype_impl.c
)
target_include_directories(gmb_fonts PUBLIC include)
target_link_libraries(gmb_fonts PRIVATE gmb_runtime)
set_target_properties(gmb_fonts PROPERTIES
    OUTPUT_NAME gmb_fonts
    C_VISIBILITY_PRESET default)
# Stessa ricetta di gmb_grep: la static gmb_runtime trascina oggetti UI/TUI con simboli
# forniti dall'host (form/widget/window manager). Le funzioni FONTS non li usano: vengono
# risolti a runtime, non imposti a link-time.
if(APPLE)
    target_link_options(gmb_fonts PRIVATE -Wl,-undefined,dynamic_lookup)
else()
    target_link_options(gmb_fonts PRIVATE -Wl,--allow-shlib-undefined)
endif()

# GREP engine come libreria CONDIVISA per P/Invoke (REPL C#). Stessa sorgente
# (gmb_grep.c + gmb_matcher.c) della static; linka gmb_runtime per le dipendenze.
add_library(gmb_grep SHARED
    src/gmb_grep.c
    src/gmb_matcher.c
)
target_include_directories(gmb_grep PUBLIC include)
target_link_libraries(gmb_grep PRIVATE gmb_runtime)
set_target_properties(gmb_grep PROPERTIES
    OUTPUT_NAME gmb_grep
    C_VISIBILITY_PRESET default)
# Il grep usa i simboli gmb_doc_pdf_* (progetto gmb_doc_c, caricato dai consumer come VM/C e
# REPL): sono dichiarati weak_import → risolti a runtime, non imposti a link-time.
if(APPLE)
    target_link_options(gmb_grep PRIVATE -Wl,-undefined,dynamic_lookup)
else()
    target_link_options(gmb_grep PRIVATE -Wl,--allow-shlib-undefined)
endif()

# COMPRESSION come libreria CONDIVISA per P/Invoke (REPL/backends C#): esporta
# gmb_compress_b64/decompress_b64/free (zstd/lz4/xz + zlib/bz2/brotli).
add_library(gmb_compress SHARED
    src/gmb_compress.c
)
target_include_directories(gmb_compress PUBLIC include)
target_link_libraries(gmb_compress PRIVATE gmb_runtime)
set_target_properties(gmb_compress PROPERTIES
    OUTPUT_NAME gmb_compress
    C_VISIBILITY_PRESET default)
# Tray (icona barra SO): libreria CONDIVISA unica. Prima era compilata DENTRO gmb_runtime
# (statica) → la classe objc GmbTrayTarget finiva in OGNI lib che linkava gmb_runtime
# (gmb_gfx, gmb_pkg) + nel dylib caricato dal C# → "Class implemented in both" + copie
# ridondanti. Ora è UNA sola lib: la carica il C# (TrayUtils) e la linkano i backend C.
add_library(gmb_tray SHARED src/gmb_tray.c src/gmb_tray_icon.c)
target_include_directories(gmb_tray PUBLIC include)
if(APPLE)
    enable_language(OBJC)
    target_sources(gmb_tray PRIVATE src/gmb_tray_macos.m)
    target_compile_definitions(gmb_tray PRIVATE GMB_TRAY_COCOA)
    target_link_libraries(gmb_tray PUBLIC "-framework Cocoa")
endif()
if(WIN32)
    target_sources(gmb_tray PRIVATE src/gmb_tray_win.c)
    target_link_libraries(gmb_tray PUBLIC shell32)
endif()
set_target_properties(gmb_tray PROPERTIES
    OUTPUT_NAME gmb_tray
    LIBRARY_OUTPUT_DIRECTORY "${GMB_LIBS_OUT}")
if(APPLE)
    set_target_properties(gmb_tray PROPERTIES INSTALL_NAME_DIR "@rpath")
endif()
# gmb_runtime (statica) ha ancora riferimenti a gmb_tray_* (es. gmb_c99gpm.c): propagali.
target_link_libraries(gmb_runtime_deps INTERFACE gmb_tray)

target_link_libraries(gmb_runtime_deps INTERFACE)   # SPLIT 2026-11-02: nessuna dipendenza LLMEnv nel core (AI = dylib gmb_runtime_ai)   # codec GGML = plugin; tokenizer/plugin/model_desc/model_io/ASR = progetti autonomi
# F2 multimodale (2026-09-11): gmb_docfields.c usa gmb_doc_extract (header gmb_doc_c).
target_include_directories(gmb_runtime_deps INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_doc_c/include)
target_link_libraries(gmb_runtime_deps INTERFACE ${CMAKE_DL_LIBS})
# TUI: funzioni math (ClipperNumbers: ceil/pow/sqrt/exp/log/...).
target_link_libraries(gmb_runtime_deps INTERFACE m)
# gmb_moe_compress.c (via gmb_model_io -> convert): Apple libcompression (LZ4/LZFSE).
if(APPLE)
    target_link_libraries(gmb_runtime_deps INTERFACE compression)
endif()
# libcurl per HTTP client (se disponibile nel sistema)
find_library(GMB_CURL_LIB curl)
if(GMB_CURL_LIB)
    target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_CURL_LIB})
endif()
# Crypto/compress (ex gmb_commonlib_c): CommonCrypto (macOS), OpenSSL, zlib, bz2, brotli.
if(APPLE)
    find_library(GMB_COMMONCRYPTO_LIB CommonCrypto)
    if(GMB_COMMONCRYPTO_LIB)
        target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_COMMONCRYPTO_LIB})
    endif()
endif()
# Directory delle librerie ESTERNE per RID. Le librerie non stanno più in Resources/libs
# (era un symlink alla cache: il build ci scriveva dentro rimettendo copie vecchie). Ordine:
#   1. CACHE ON-DEMAND ~/.gmbasic/libs/<rid>  (fonte canonica: bundle pubblicati, sha verificati)
#   2. build/libs/<rid>                   (artefatti nostri appena compilati / senza cache)
set(_rid "${GMB_RID}")   # vedi native/cmake/GmbRid.cmake (una sola derivazione)
if(EXISTS "$ENV{HOME}/.gmbasic/libs/${_rid}")
    set(GMB_RID_RES "$ENV{HOME}/.gmbasic/libs/${_rid}")
elseif(EXISTS "${GMB_LIBS_OUT}")
    set(GMB_RID_RES "${GMB_LIBS_OUT}")
else()
    set(GMB_RID_RES "${GMB_LIBS_OUT}")   # né cache né build: si compila, il link lo dirà
endif()
message(STATUS "gmb_runtime_c: librerie esterne da ${GMB_RID_RES}")
# ── Terze parti: SOLO dalla cache (o dagli artefatti di build) ────────────────────────────────
# NIENTE fallback su directory del builder: un `find_library` che ripiega fuori dalla cache
# produce binari con un path ASSOLUTO cucito dentro, che su un'altra macchina non carica — e
# maschera una libreria mancante dal bundle. Se manca qualcosa ci si ferma e si dice come
# rimediare (`gmb install`).
set(GMB_DEPS_HINT "esegui `gmb install --runtime-only` (librerie) e `gmb install --dev` (header)")

function(gmb_need_lib _var)
    find_library(${_var} NAMES ${ARGN} PATHS "${GMB_RID_RES}" NO_DEFAULT_PATH)
    if(NOT ${_var})
        message(FATAL_ERROR "${_var} non trovata in ${GMB_RID_RES}: ${GMB_DEPS_HINT}")
    endif()
    set(${_var} "${${_var}}" PARENT_SCOPE)
endfunction()

# Header delle terze parti: il gruppo `dev` li deposita in `<cache>/<rid>/include/`.
if(EXISTS "${GMB_RID_RES}/include")
    target_include_directories(gmb_runtime_deps INTERFACE "${GMB_RID_RES}/include")
endif()

gmb_need_lib(GMB_CRYPTO_LIB crypto libcrypto.3.dylib libcrypto)
target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_CRYPTO_LIB})
# NIENTE `-rpath,<directory della libcrypto>` qui. Quella directory è `~/.gmbasic/libs/<rid>`
# (o `build/libs/<rid>`): un path ASSOLUTO per-utente che questo INTERFACE propagava a OGNI
# consumer (gmb_vm_host, gmbrepl, gmbnotebook, driver C99) — ed era l'ultima voce assoluta nei
# tre eseguibili della release, che così non caricavano su un'altra macchina. La libcrypto ha
# install_name `@rpath/libcrypto.3.dylib` e i consumer la risolvono con l'rpath RILOCABILE di
# `gmb_exe_relocatable()` (`@loader_path/libs/<rid>`: symlink alla cache in sviluppo, dylib
# vere nel bundle). Gli eseguibili di solo sviluppo continuano a trovarla perché CMake aggiunge
# da sé la directory della libreria linkata all'rpath del build tree.
target_compile_definitions(gmb_runtime_deps INTERFACE HAVE_OPENSSL=1)

gmb_need_lib(GMB_SSL_LIB ssl libssl.3.dylib libssl)
target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_SSL_LIB})

# zlib e bz2 sono librerie di SISTEMA su macOS/Linux: si legano dal sistema.
find_library(GMB_ZLIB_LIB z)
if(GMB_ZLIB_LIB)
    target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_ZLIB_LIB})
endif()
find_library(GMB_BZ2_LIB bz2)
if(GMB_BZ2_LIB)
    target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_BZ2_LIB})
endif()

# zstd (Parquet ZSTD)
gmb_need_lib(GMB_ZSTD_LIB zstd)
target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_ZSTD_LIB})
target_compile_definitions(gmb_runtime_deps INTERFACE GMB_PARQUET_HAVE_ZSTD GMB_HAVE_ZSTD=1)

# brotli, lzma, lz4: algoritmi di compressione
gmb_need_lib(GMB_BROTLIENC_LIB brotlienc libbrotlienc.1.dylib)
gmb_need_lib(GMB_BROTLIDEC_LIB brotlidec libbrotlidec.1.dylib)
target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_BROTLIENC_LIB} ${GMB_BROTLIDEC_LIB})
target_compile_definitions(gmb_runtime_deps INTERFACE GMB_HAVE_BROTLI=1)

gmb_need_lib(GMB_LZMA_LIB lzma)
target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_LZMA_LIB})
target_compile_definitions(gmb_runtime_deps INTERFACE GMB_HAVE_LZMA=1)

gmb_need_lib(GMB_LZ4_LIB lz4)
target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_LZ4_LIB})
target_compile_definitions(gmb_runtime_deps INTERFACE GMB_HAVE_LZ4=1)

if(GMB_BZ2_LIB)
    target_compile_definitions(gmb_runtime_deps INTERFACE GMB_HAVE_BZ2=1)
endif()

# PCRE2 (regex) — richiesta anche da C# (RegexWrapper.cs)
gmb_need_lib(GMB_PCRE2_LIB pcre2-8 pcre2-8.0)
target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_PCRE2_LIB})
message(STATUS "gmb_runtime_c: PCRE2 da ${GMB_RID_RES}")

# Audio SDL_mixer — CARICATA A RUNTIME (dlopen), NON LINKATA: linkarla faceva ereditare a ogni
# dylib la catena del mixer (FLAC/ogg/opus/vorbis/sndfile/fluidsynth… ~18 lib e ~15 MB).
target_link_libraries(gmb_runtime_deps INTERFACE ${CMAKE_DL_LIBS})
target_compile_definitions(gmb_runtime_deps INTERFACE GMB_GFX_SDL_MIXER=1)

# SDL2_ttf (opzionale): se manca, TTF_Init diventa no-op.
find_library(GMB_SDL2_TTF_LIB SDL2_ttf PATHS "${GMB_RID_RES}" NO_DEFAULT_PATH)
if(GMB_SDL2_TTF_LIB)
    target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_SDL2_TTF_LIB})
    message(STATUS "gmb_runtime_c: SDL2_ttf trovata -> FontAtlas/TTF console attivo")
else()
    message(STATUS "gmb_runtime_c: SDL2_ttf assente -> TTF_Init no-op (gmb install --runtime-only)")
endif()

# SQLite: libreria di SISTEMA (macOS la include; Linux la fornisce il pacchetto libsqlite3).
# L'header viene dal SDK/dal sistema.
find_package(SQLite3)
if(SQLite3_FOUND)
    target_link_libraries(gmb_runtime_deps INTERFACE SQLite::SQLite3)
else()
    find_library(GMB_SQLITE3_LIB sqlite3)
    if(GMB_SQLITE3_LIB)
        target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_SQLITE3_LIB})
    endif()
endif()

# Presentazione SDL opzionale (gmb_gfx_present_sdl.c): gmb_graphics.c disegna in
# un framebuffer software ma non lo mostrava mai a una vera finestra — questo e'
# il pezzo mancante. Compilato SOLO se SDL2 e' disponibile: build headless (CI,
# server) restano prive della dipendenza, come documentato in gmb_graphics.h/.c.
find_package(SDL2 QUIET PATHS ${GMB_RID_RES} NO_DEFAULT_PATH)
if(NOT SDL2_FOUND)
    find_library(GMB_SDL2_LIB SDL2 PATHS ${GMB_RID_RES} NO_DEFAULT_PATH)
    # Header SOLO dalla cache (`<cache>/<rid>/include/SDL2/SDL.h`, gruppo `dev`).
    find_path(GMB_SDL2_INCLUDE_DIR SDL.h PATHS "${GMB_RID_RES}/include" PATH_SUFFIXES SDL2 NO_DEFAULT_PATH)
    # L'header si include come <SDL2/SDL.h>: se `find_path` ha restituito la cartella SDL2
    # stessa (`<cache>/<rid>/include/SDL2`) serve anche il GENITORE, altrimenti il compilatore
    # cerca .../SDL2/SDL2/SDL.h e fallisce ("'SDL2/SDL.h' file not found" — era il caso di
    # gmb_gfx_present_input_selfcheck, che non compilava da tempo).
    if(GMB_SDL2_INCLUDE_DIR)
        get_filename_component(_gmb_sdl2_parent "${GMB_SDL2_INCLUDE_DIR}" DIRECTORY)
        list(APPEND GMB_SDL2_INCLUDE_DIR "${_gmb_sdl2_parent}")
    endif()
    # Nessun fallback: se SDL2 non è in cache si compila headless (e lo dice).
    if(NOT GMB_SDL2_LIB)
        message(STATUS "gmb_runtime_c: SDL2 assente in cache -> build headless (gmb install --runtime-only)")
    endif()
endif()
if(SDL2_FOUND OR (GMB_SDL2_LIB AND GMB_SDL2_INCLUDE_DIR))
    message(STATUS "gmb_runtime_c: SDL2 trovata -> presentazione finestra grafica abilitata (GMB_GFX_SDL_PRESENT)")
    target_sources(gmb_runtime_ui_obj PRIVATE src/gmb_gfx_present_sdl.c)
target_sources(gmb_runtime_ui_obj PRIVATE src/gmb_md.c)
    target_compile_definitions(gmb_runtime_deps INTERFACE GMB_GFX_SDL_PRESENT=1)
    if(SDL2_FOUND)
        if(TARGET SDL2::SDL2)
            target_link_libraries(gmb_runtime_deps INTERFACE SDL2::SDL2)
        else()
            target_include_directories(gmb_runtime_deps INTERFACE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(gmb_runtime_deps INTERFACE ${SDL2_LIBRARIES})
        endif()
    else()
        target_include_directories(gmb_runtime_deps INTERFACE ${GMB_SDL2_INCLUDE_DIR})
        target_link_libraries(gmb_runtime_deps INTERFACE ${GMB_SDL2_LIB})
    endif()
else()
    message(STATUS "gmb_runtime_c: SDL2 non trovata -> VM/C resta headless (nessuna finestra grafica reale)")
endif()

# ── Vulkan opzionale (present accelerato): MoltenVK su macOS, loader altrove ──
if(APPLE)
    # MoltenVK e' una libreria ESTERNA: vive SOLO nella cache/BUILD del RID (`GMB_RID_RES`), mai un
    # fallback fuori dal bundle: nasconderebbe una libreria mancante e farebbe linkare copie diverse da
    # quelle del progetto. Se non c'e', la configurazione fallisce.
    # SOLO se Vulkan e' RICHIESTO: era un FATAL_ERROR incondizionato, quindi un RID il cui bundle
    # non contiene MoltenVK (es. osx-x64) non si configurava NEMMENO con Vulkan spento (default).
    if(GMB_PRESENT_VULKAN)
        find_library(GMB_MOLTENVK_LIB NAMES MoltenVK PATHS ${GMB_RID_RES} NO_DEFAULT_PATH)
        if(NOT GMB_MOLTENVK_LIB)
            message(FATAL_ERROR "gmb_runtime_c: libMoltenVK.dylib non trovata in ${GMB_RID_RES} — le librerie esterne vanno nella cache (nessun fallback esterno)")
        endif()
    endif()
    # header Vulkan: dal gruppo `dev` della cache
    find_path(GMB_VULKAN_INCLUDE_DIR vulkan/vulkan.h PATHS "${GMB_RID_RES}/include" NO_DEFAULT_PATH)
    if(GMB_MOLTENVK_LIB AND GMB_VULKAN_INCLUDE_DIR)
        set(GMB_GFX_VULKAN_FOUND TRUE)
    endif()
else()
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        set(GMB_GFX_VULKAN_FOUND TRUE)
    endif()
endif()

if(GMB_GFX_VULKAN_FOUND)
    set(GMB_SPIRV_HEADER "${CMAKE_CURRENT_BINARY_DIR}/gmb_gfx_spirv.h")
    add_custom_command(
        OUTPUT "${GMB_SPIRV_HEADER}"
        COMMAND "${CMAKE_COMMAND}" -DOUT="${GMB_SPIRV_HEADER}"
                -DIN1="${CMAKE_CURRENT_SOURCE_DIR}/shaders/prebuilt/sprite.vert.spv" -DNAME1=sprite_vert_spv
                -DIN2="${CMAKE_CURRENT_SOURCE_DIR}/shaders/prebuilt/sprite.frag.spv" -DNAME2=sprite_frag_spv
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/prebuilt/sprite.vert.spv"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/prebuilt/sprite.frag.spv"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        COMMENT "Embed SPIR-V shaders (gmb_gfx_spirv.h)")
endif()

# ── Dylib grafica (motore UNICO C) — P/Invoke dal mondo gestito (C#→C; mai C→C#) ──
# Compila DIRETTAMENTE le sorgenti gfx/SDL nella dylib (come gmb_ai_llm): così ogni
# simbolo gmb_gfx_*/gmb_sdl_* è incluso ed esportato per il bridge C#.
add_library(gmb_gfx SHARED
    src/gmb_graphics.c
    src/gmb_graphics_primitive.c
    src/gmb_graphics_text.c
    src/gmb_gfx_cellfont.c
    src/gmb_graphics_image.c
    src/gmb_graphics_sprite.c
    src/gmb_graphics_texture.c
    src/gmb_graphics_tilemap.c
    src/gmb_graphics_chart.c
    src/gmb_graphics_screenshot.c
    src/gmb_graphics_palette.c
    src/gmb_gfx_present_sdl.c
    src/gmb_sdl_manager.c
    src/gmb_sdl_context.c
    src/gmb_sdl_renderer.c
    src/gmb_sdl_console.c
    src/gmb_screen_config.c
    src/gmb_screens_extra.c
    src/gmb_audio.c
    src/gmb_joy.c)
# Moduli TUI aggiunti al dylib gmb_gfx: il managed (CGfx) li chiama (pixel E tui, stessa pipeline).
target_sources(gmb_gfx PRIVATE ${GMB_TUI_MODULES} src/gmb_tui.c src/gmb_tui_flex.c src/gmb_tui_repl.c src/gmb_ide.c)
# READ unificato esportato nel dylib per il P/Invoke dei backend gestiti (C#→C):
# senza questo il linker statico non estrae gmb_read_* e il P/Invoke fallisce silenziosamente.
target_sources(gmb_gfx PRIVATE src/gmb_read.c)

target_include_directories(gmb_gfx PRIVATE include src ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_chart/include)
target_link_libraries(gmb_gfx PRIVATE gmb_runtime gmb_chart)
# La dylib compila gmb_audio.c direttamente: il define del mixer (PRIVATE su
# gmb_runtime) non si propaga, va ripetuto qui o il playback C è un no-op muto.
# Con il dlopen il define è SEMPRE attivo (non dipende dalla lib presente al configure).
target_compile_definitions(gmb_gfx PRIVATE GMB_GFX_SDL_MIXER=1 GMB_WITH_UI=1)
# Backend Vulkan (present): la dylib gmb_gfx E il runtime STATICO (C99Native/VM
# standalone) usano lo stesso present accelerato. Compila il backend + shader embedded.
# Present VULKAN (MoltenVK, 5,1 MB) è OPT-IN: il backend di default è SDL (gmb_gfx_present_sdl).
# Tenerlo spento toglie una dipendenza pesante da `libgmb_gfx` e da ogni processo che carica il
# motore grafico; per riattivarlo: cmake -DGMB_PRESENT_VULKAN=ON.
option(GMB_PRESENT_VULKAN "Backend present Vulkan/MoltenVK (opt-in: costa 5,1 MB di dipendenza)" OFF)
if(GMB_GFX_VULKAN_FOUND AND NOT GMB_PRESENT_VULKAN)
    message(STATUS "gmb_runtime_c: present Vulkan DISATTIVATO (default) -> si usa SDL (nessuna dipendenza MoltenVK)")
    set(GMB_GFX_VULKAN_FOUND FALSE)
endif()
if(GMB_GFX_VULKAN_FOUND)
    target_sources(gmb_runtime_ui_obj PRIVATE src/gmb_gfx_vulkan.c "${GMB_SPIRV_HEADER}")
    target_compile_definitions(gmb_runtime_deps INTERFACE GMB_GFX_VULKAN=1)
    target_include_directories(gmb_runtime_deps INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
    if(APPLE)
        target_include_directories(gmb_runtime_deps INTERFACE "${GMB_VULKAN_INCLUDE_DIR}")
        target_link_libraries(gmb_runtime_deps INTERFACE "${GMB_MOLTENVK_LIB}")
    else()
        target_link_libraries(gmb_runtime_deps INTERFACE Vulkan::Vulkan)
    endif()
    target_sources(gmb_gfx PRIVATE src/gmb_gfx_vulkan.c "${GMB_SPIRV_HEADER}")
    target_compile_definitions(gmb_gfx PRIVATE GMB_GFX_VULKAN=1)
    if(APPLE)
        target_include_directories(gmb_gfx PRIVATE "${GMB_VULKAN_INCLUDE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries(gmb_gfx PRIVATE "${GMB_MOLTENVK_LIB}")
    else()
        target_include_directories(gmb_gfx PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries(gmb_gfx PRIVATE Vulkan::Vulkan)
    endif()
    message(STATUS "gmb_runtime_c: Vulkan trovato -> backend present Vulkan (gmb_gfx)")
    if(SDL2_FOUND OR (GMB_SDL2_LIB AND GMB_SDL2_INCLUDE_DIR))
        add_executable(gmb_gfx_vulkan_selfcheck tests/gmb_gfx_vulkan_selfcheck.c)
        target_link_libraries(gmb_gfx_vulkan_selfcheck PRIVATE gmb_gfx)
        target_include_directories(gmb_gfx_vulkan_selfcheck PRIVATE include)
        if(SDL2_FOUND)
            target_link_libraries(gmb_gfx_vulkan_selfcheck PRIVATE SDL2::SDL2)
        else()
            target_link_libraries(gmb_gfx_vulkan_selfcheck PRIVATE ${GMB_SDL2_LIB})
            target_include_directories(gmb_gfx_vulkan_selfcheck PRIVATE ${GMB_SDL2_INCLUDE_DIR})
        endif()
        target_compile_options(gmb_gfx_vulkan_selfcheck PRIVATE -O2)
        add_test(NAME gmb_gfx_vulkan_selfcheck COMMAND gmb_gfx_vulkan_selfcheck)
    endif()
else()
    message(STATUS "gmb_runtime_c: Vulkan non trovato -> solo backend SDL")
endif()
set_target_properties(gmb_gfx PROPERTIES
    PREFIX "lib" OUTPUT_NAME "gmb_gfx"
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY "${GMB_LIBS_OUT}")

# ── libgmb_boot: PUNTO UNICO di ricerca/download/caricamento delle librerie ──────────────────
# Deve poter essere caricata SEMPRE (anche dal C# prima di ogni altra lib): dipendenze minime
# (libSystem + libcurl). Non include gfx/AI/SDL.
# gmb_paths.c porta il parser delle impostazioni (self-contained) e richiede l'anchor dell'ABI
# (gmb_abi.c, 347 righe): entrambi nel target per restare sullo stesso comportamento del runtime.
add_library(gmb_boot SHARED src/gmb_boot.c src/gmb_libs_fetch.c src/gmb_paths.c src/gmb_boot_abi_stub.c)
target_include_directories(gmb_boot PRIVATE include src)
target_link_libraries(gmb_boot PRIVATE ${CMAKE_DL_LIBS})
if(GMB_CURL_LIB)
    target_link_libraries(gmb_boot PRIVATE ${GMB_CURL_LIB})
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(gmb_boot PRIVATE m)
endif()
set_target_properties(gmb_boot PROPERTIES
    PREFIX "lib" OUTPUT_NAME "gmb_boot"
    LIBRARY_OUTPUT_DIRECTORY "${GMB_LIBS_OUT}")

# CLI del downloader on-demand (`gmb_libs_fetch <libs.txt> <file>...`): la usa il runner C99
# prima del link (il compilatore C scarica le librerie mancanti). Non va nella libreria: `main`.
add_executable(gmb_libs_fetch tools/gmb_libs_fetch_cli.c)
target_link_libraries(gmb_libs_fetch PRIVATE gmb_runtime)
target_include_directories(gmb_libs_fetch PRIVATE include)
gmb_exe_relocatable(gmb_libs_fetch)   # nessun path del builder nell'rpath (release: 0 voci assolute)

# Motore di download da riga di comando (`gmb_fetch <url> <file>`): utile per provare la
# ripresa a mano su un asset grosso (Ctrl-C + rilancio) e per gli script di distribuzione.
add_executable(gmb_fetch_cli tools/gmb_fetch_cli.c)
target_link_libraries(gmb_fetch_cli PRIVATE gmb_runtime)
target_include_directories(gmb_fetch_cli PRIVATE include)
set_target_properties(gmb_fetch_cli PROPERTIES OUTPUT_NAME gmb_fetch)
gmb_exe_relocatable(gmb_fetch_cli)

# Cervelli di un programma DISTRIBUITO: legge `<exe>.brains.txt` (+ lo snapshot di
# brains-manifest.json accanto all'eseguibile) e li installa in `<exe>/brains`, che la
# risoluzione consulta prima della cache. Così l'app parte senza rete.
add_executable(gmb_brain_fetch_cli tools/gmb_brain_fetch_cli.c)
target_link_libraries(gmb_brain_fetch_cli PRIVATE gmb_runtime)
target_include_directories(gmb_brain_fetch_cli PRIVATE include)
set_target_properties(gmb_brain_fetch_cli PROPERTIES OUTPUT_NAME gmb_brain_fetch)
gmb_exe_relocatable(gmb_brain_fetch_cli)

# Fixture di mercato on demand (`gmb_fixtures list|pull`): prima erano ~182 MB nel repo.
add_executable(gmb_fixtures_cli tools/gmb_fixtures_cli.c)
target_link_libraries(gmb_fixtures_cli PRIVATE gmb_runtime)
target_include_directories(gmb_fixtures_cli PRIVATE include)
set_target_properties(gmb_fixtures_cli PROPERTIES OUTPUT_NAME gmb_fixtures)
gmb_exe_relocatable(gmb_fixtures_cli)

add_executable(gmb_rdd_db_selfcheck tests/rdd_db_selfcheck.c)
target_link_libraries(gmb_rdd_db_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_rdd_db_selfcheck PRIVATE include)

add_executable(gmb_grep_selfcheck tests/gmb_grep_selfcheck.c)
target_link_libraries(gmb_grep_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_grep_selfcheck PRIVATE include)
if(APPLE)
    target_link_options(gmb_grep_selfcheck PRIVATE -Wl,-undefined,dynamic_lookup)
endif()
add_test(NAME gmb_grep_selfcheck COMMAND gmb_grep_selfcheck)

# Decision layer (JEV/SystemOne): contratto + Mock/Rules + 24 slot canonici (model-free).
add_executable(gmb_decision_selfcheck tests/decision_selfcheck.c)
target_link_libraries(gmb_decision_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_decision_selfcheck PRIVATE include)
add_test(NAME gmb_decision_selfcheck COMMAND gmb_decision_selfcheck)

# Testa di scoring condivisa (T3): formula z=(lp+bias)/(head_T*T_kind), confidence, artifact JSON.
add_executable(gmb_decision_head_selfcheck tests/decision_head_selfcheck.c)
target_link_libraries(gmb_decision_head_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_decision_head_selfcheck PRIVATE include)
add_test(NAME gmb_decision_head_selfcheck COMMAND gmb_decision_head_selfcheck)

# ── Second Brain (registry/manifest/toolchain) e pack firmato: selfcheck dedicati ──────────
# Isolati con gmb_paths_test_override (nessuna scrittura nella config dell'utente).
add_executable(gmb_brain_registry_selfcheck tests/brain_registry_selfcheck.c)
target_link_libraries(gmb_brain_registry_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_brain_registry_selfcheck PRIVATE include)
add_test(NAME gmb_brain_registry_selfcheck COMMAND gmb_brain_registry_selfcheck)

add_executable(gmb_brain_tools_selfcheck tests/brain_tools_selfcheck.c)
target_link_libraries(gmb_brain_tools_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_brain_tools_selfcheck PRIVATE include)
add_test(NAME gmb_brain_tools_selfcheck COMMAND gmb_brain_tools_selfcheck)

# gmb_pkg_pack/ed25519 vivono nella lib condivisa gmb_pkg: qui si compilano nel test
# (gmb_compress/gmb_zip_* sono già in gmb_runtime, non vanno duplicati).
add_executable(gmb_pkg_pack_signed_selfcheck tests/gmb_pkg_pack_signed_selfcheck.c
    src/gmb_pkg_pack.c src/gmb_pkg_glob.c src/gmb_pkg_ed25519.c)
target_link_libraries(gmb_pkg_pack_signed_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_pack_signed_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_pack_signed_selfcheck COMMAND gmb_pkg_pack_signed_selfcheck)

add_executable(gmb_compress_selfcheck tests/gmb_compress_selfcheck.c)
target_link_libraries(gmb_compress_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_compress_selfcheck PRIVATE include)
add_test(NAME gmb_compress_selfcheck COMMAND gmb_compress_selfcheck)

# Motore di download (streaming/resume/lock/sha): provato OFFLINE con un trasporto finto.
# Nessuna rete: quello che con la rete sarebbe casuale (ripresa, Range ignorato, lock, sha)
# qui è deterministico.
add_executable(gmb_fetch_selfcheck tests/gmb_fetch_selfcheck.c)
target_link_libraries(gmb_fetch_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_fetch_selfcheck PRIVATE include)
add_test(NAME gmb_fetch_selfcheck COMMAND gmb_fetch_selfcheck)

# END-TO-END del trasporto libcurl VERO, contro un server HTTP LOCALE (Range, Range ignorato,
# Bearer, 404). Nessuna rete esterna: si prova il pezzo che il trasporto finto non copre.
add_executable(gmb_fetch_http_selfcheck tests/gmb_fetch_http_selfcheck.c)
target_link_libraries(gmb_fetch_http_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_fetch_http_selfcheck PRIVATE include)
add_test(NAME gmb_fetch_http_selfcheck COMMAND gmb_fetch_http_selfcheck)

# Cervelli come dati: risoluzione (vendor/exe/cache), manifest, installazione e fail-closed.
# Offline: HOME sandboxata + trasporto finto (nessuna rete, nessun cervello vero toccato).
add_executable(gmb_brains_selfcheck tests/gmb_brains_selfcheck.c)
target_link_libraries(gmb_brains_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_brains_selfcheck PRIVATE include)
add_test(NAME gmb_brains_selfcheck COMMAND gmb_brains_selfcheck)

# Fixture on demand: risoluzione, download+estrazione, idempotenza, sha sbagliato (offline).
add_executable(gmb_fixtures_selfcheck tests/gmb_fixtures_selfcheck.c)
target_link_libraries(gmb_fixtures_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_fixtures_selfcheck PRIVATE include)
add_test(NAME gmb_fixtures_selfcheck COMMAND gmb_fixtures_selfcheck)


add_executable(gmb_dscope_selfcheck tests/dscope_selfcheck.c)
target_link_libraries(gmb_dscope_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_dscope_selfcheck PRIVATE include)

add_executable(gmb_bsp_selfcheck tests/bsp_selfcheck.c)
target_link_libraries(gmb_bsp_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_bsp_selfcheck PRIVATE include)

add_executable(gmb_semver_selfcheck tests/gmb_semver_selfcheck.c)
target_link_libraries(gmb_semver_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_semver_selfcheck PRIVATE include)
add_test(NAME gmb_semver_selfcheck COMMAND gmb_semver_selfcheck)

add_executable(gmb_json_selfcheck tests/gmb_json_selfcheck.c)
target_link_libraries(gmb_json_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_json_selfcheck PRIVATE include)
add_test(NAME gmb_json_selfcheck COMMAND gmb_json_selfcheck)
# SPLIT 2026-11-02: il modulo ASR host non vive piu' nel runtime (sta in LLMEnv). Se l'header non
# c'e', il selfcheck non ha nulla da compilare: target CONDIZIONALE, cosi' la build non si blocca
# su un file rimasto senza il suo modulo (nessun file cancellato: torna attivo appena l'header c'e').
# Il gate segue il rename 2026-10-31 di LLMEnv (`gmb_asr.h` -> `llm_asr.h`): prima cercava il nome
# vecchio, che non esiste piu', e i due harness restavano spenti.
if(EXISTS "${LLMENV}/LLMEngine/asr/include/llm_asr.h" OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/llm_asr.h")
    add_executable(gmb_asr_selfcheck tests/gmb_asr_selfcheck.c)
    target_link_libraries(gmb_asr_selfcheck PRIVATE gmb_runtime llm_asr)
    target_include_directories(gmb_asr_selfcheck PRIVATE include)
    add_test(NAME gmb_asr_selfcheck COMMAND gmb_asr_selfcheck)
    add_executable(gmb_asr_run tests/gmb_asr_run.c)
    target_link_libraries(gmb_asr_run PRIVATE gmb_runtime llm_asr)
    target_include_directories(gmb_asr_run PRIVATE include)
endif()
add_executable(gmb_pkg_manifest_selfcheck tests/gmb_pkg_manifest_selfcheck.c)
target_link_libraries(gmb_pkg_manifest_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_manifest_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_manifest_selfcheck COMMAND gmb_pkg_manifest_selfcheck)

add_executable(gmb_pkg_lock_selfcheck tests/gmb_pkg_lock_selfcheck.c src/gmb_pkg_lock.c)
target_link_libraries(gmb_pkg_lock_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_lock_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_lock_selfcheck COMMAND gmb_pkg_lock_selfcheck)

add_executable(gmb_pkg_trust_selfcheck tests/gmb_pkg_trust_selfcheck.c src/gmb_pkg_trust.c)
target_link_libraries(gmb_pkg_trust_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_trust_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_trust_selfcheck COMMAND gmb_pkg_trust_selfcheck)

add_executable(gmb_pkg_glob_selfcheck tests/gmb_pkg_glob_selfcheck.c src/gmb_pkg_glob.c)
target_include_directories(gmb_pkg_glob_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_glob_selfcheck COMMAND gmb_pkg_glob_selfcheck)

add_executable(gmb_pkg_contentaddr_selfcheck tests/gmb_pkg_contentaddr_selfcheck.c src/gmb_pkg_contentaddr.c src/gmb_pkg_lock.c)
target_link_libraries(gmb_pkg_contentaddr_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_contentaddr_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_contentaddr_selfcheck COMMAND gmb_pkg_contentaddr_selfcheck)

add_executable(gmb_pkg_resolver_selfcheck tests/gmb_pkg_resolver_selfcheck.c
    src/gmb_pkg_resolver.c src/gmb_pkg_lock.c src/gmb_semver.c src/gmb_pkg_registry.c src/gmb_pkg_util.c)
target_link_libraries(gmb_pkg_resolver_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_resolver_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_resolver_selfcheck COMMAND gmb_pkg_resolver_selfcheck)

add_executable(gmb_pkg_registry_selfcheck tests/gmb_pkg_registry_selfcheck.c src/gmb_pkg_registry.c)
target_link_libraries(gmb_pkg_registry_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_registry_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_registry_selfcheck COMMAND gmb_pkg_registry_selfcheck)

add_executable(gmb_net_http_selfcheck tests/gmb_net_http_selfcheck.c)
target_link_libraries(gmb_net_http_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_net_http_selfcheck PRIVATE include)
add_test(NAME gmb_net_http_selfcheck COMMAND gmb_net_http_selfcheck)

add_executable(gmb_pkg_artifact_selfcheck tests/gmb_pkg_artifact_selfcheck.c src/gmb_pkg_artifact.c)
target_link_libraries(gmb_pkg_artifact_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_artifact_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_artifact_selfcheck COMMAND gmb_pkg_artifact_selfcheck)

add_executable(gmb_pkg_install_selfcheck tests/gmb_pkg_install_selfcheck.c
    src/gmb_pkg_install.c src/gmb_pkg_lock.c)
target_link_libraries(gmb_pkg_install_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_install_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_install_selfcheck COMMAND gmb_pkg_install_selfcheck)

add_executable(gmb_pkg_pack_selfcheck tests/gmb_pkg_pack_selfcheck.c
    src/gmb_pkg_pack.c src/gmb_pkg_glob.c src/gmb_pkg_ed25519.c)
target_link_libraries(gmb_pkg_pack_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_pack_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_pack_selfcheck COMMAND gmb_pkg_pack_selfcheck)

add_executable(gmb_pkg_advisories_selfcheck tests/gmb_pkg_advisories_selfcheck.c
    src/gmb_pkg_advisories.c src/gmb_semver.c src/gmb_pkg_lock.c)
target_link_libraries(gmb_pkg_advisories_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_advisories_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_advisories_selfcheck COMMAND gmb_pkg_advisories_selfcheck)

add_executable(gmb_pkg_ed25519_selfcheck tests/gmb_pkg_ed25519_selfcheck.c src/gmb_pkg_ed25519.c)
target_link_libraries(gmb_pkg_ed25519_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_ed25519_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_ed25519_selfcheck COMMAND gmb_pkg_ed25519_selfcheck)

add_executable(gmb_pkg_ecdsa_selfcheck tests/gmb_pkg_ecdsa_selfcheck.c src/gmb_pkg_ecdsa.c src/gmb_pkg_ed25519.c)
target_link_libraries(gmb_pkg_ecdsa_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_ecdsa_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_ecdsa_selfcheck COMMAND gmb_pkg_ecdsa_selfcheck)

add_executable(gmb_pkg_util_selfcheck tests/gmb_pkg_util_selfcheck.c src/gmb_pkg_util.c src/gmb_semver.c)
target_link_libraries(gmb_pkg_util_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_util_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_util_selfcheck COMMAND gmb_pkg_util_selfcheck)

add_executable(gmb_pkg_graph_selfcheck tests/gmb_pkg_graph_selfcheck.c src/gmb_pkg_graph.c)
target_link_libraries(gmb_pkg_graph_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_pkg_graph_selfcheck PRIVATE include)
add_test(NAME gmb_pkg_graph_selfcheck COMMAND gmb_pkg_graph_selfcheck)

add_executable(gmb_graph_selfcheck tests/gmb_graph_selfcheck.c)
target_link_libraries(gmb_graph_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_graph_selfcheck PRIVATE include)
add_test(NAME gmb_graph_selfcheck COMMAND gmb_graph_selfcheck)

add_executable(gmb_gfx_input_selfcheck tests/gmb_gfx_input_selfcheck.c src/gmb_gfx_input.c)
target_include_directories(gmb_gfx_input_selfcheck PRIVATE include)
add_test(NAME gmb_gfx_input_selfcheck COMMAND gmb_gfx_input_selfcheck)

add_executable(gmb_form_selfcheck tests/gmb_form_selfcheck.c)
target_link_libraries(gmb_form_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_form_selfcheck PRIVATE include)
add_test(NAME gmb_form_selfcheck COMMAND gmb_form_selfcheck)

add_executable(gmb_form_events_selfcheck tests/gmb_form_events_selfcheck.c)
target_link_libraries(gmb_form_events_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_form_events_selfcheck PRIVATE include)
add_test(NAME gmb_form_events_selfcheck COMMAND gmb_form_events_selfcheck)
add_executable(gmb_win_events_selfcheck tests/gmb_win_events_selfcheck.c)
target_link_libraries(gmb_win_events_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_win_events_selfcheck PRIVATE include)
add_test(NAME gmb_win_events_selfcheck COMMAND gmb_win_events_selfcheck)

# Tastiera → finestra focalizzata (tui_key_focus_sync): il tasto non deve andare al controllo
# di una finestra di sfondo quando la finestra attiva è un'altra.
add_executable(gmb_tui_focus_selftest tests/gmb_tui_focus_selftest.c)
target_link_libraries(gmb_tui_focus_selftest PRIVATE gmb_runtime)
target_include_directories(gmb_tui_focus_selftest PRIVATE include)
add_test(NAME gmb_tui_focus_selftest COMMAND gmb_tui_focus_selftest)

# Finestre PIXEL: capacità portate dal C# GfxWindowManager al runtime C (colori per-finestra,
# client rect in pixel, hit-test titlebar/bordi, chip taskbar).
add_executable(gmb_pixel_wm_selftest tests/gmb_pixel_wm_selftest.c)
target_link_libraries(gmb_pixel_wm_selftest PRIVATE gmb_runtime)
target_include_directories(gmb_pixel_wm_selftest PRIVATE include)
add_test(NAME gmb_pixel_wm_selftest COMMAND gmb_pixel_wm_selftest)
add_executable(gmb_tui_events_selfcheck tests/gmb_tui_events_selfcheck.c)
target_link_libraries(gmb_tui_events_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_tui_events_selfcheck PRIVATE include)
add_test(NAME gmb_tui_events_selfcheck COMMAND gmb_tui_events_selfcheck)

add_executable(gmb_form_win_selfcheck tests/gmb_form_win_selfcheck.c)
target_link_libraries(gmb_form_win_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_form_win_selfcheck PRIVATE include)
add_test(NAME gmb_form_win_selfcheck COMMAND gmb_form_win_selfcheck)

add_executable(gmb_form_dialog_selfcheck tests/gmb_form_dialog_selfcheck.c)
target_link_libraries(gmb_form_dialog_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_form_dialog_selfcheck PRIVATE include)

add_executable(gmb_vm_input_drain_selfcheck tests/gmb_vm_input_drain_selfcheck.c src/gmb_gfx_input.c)
target_link_libraries(gmb_vm_input_drain_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_vm_input_drain_selfcheck PRIVATE include)
add_test(NAME gmb_vm_input_drain_selfcheck COMMAND gmb_vm_input_drain_selfcheck)

add_executable(gmb_imgdiff tools/gmb_imgdiff.c)
target_link_libraries(gmb_imgdiff PRIVATE gmb_runtime)
target_include_directories(gmb_imgdiff PRIVATE include src)
gmb_exe_relocatable(gmb_imgdiff)

add_executable(gmb_audio_selfcheck tests/gmb_audio_selfcheck.c)
target_link_libraries(gmb_audio_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_audio_selfcheck PRIVATE include)
add_test(NAME gmb_audio_selfcheck COMMAND gmb_audio_selfcheck)

# Parita' TUI C vs C#: Control/ApplyCase, Button, TextInput, ComboBox, FocusManager.
add_executable(tui_widgets_selfcheck tests/tui_widgets_selfcheck.c)
target_link_libraries(tui_widgets_selfcheck PRIVATE gmb_runtime)
target_include_directories(tui_widgets_selfcheck PRIVATE include)
add_test(NAME tui_widgets_selfcheck COMMAND tui_widgets_selfcheck)

# Integrazione SDL→input: usa la dylib gmb_gfx col driver dummy (headless).
if(SDL2_FOUND)
    add_executable(gmb_gfx_present_input_selfcheck tests/gmb_gfx_present_input_selfcheck.c)
    target_link_libraries(gmb_gfx_present_input_selfcheck PRIVATE gmb_gfx SDL2::SDL2)
    target_include_directories(gmb_gfx_present_input_selfcheck PRIVATE include)
    add_test(NAME gmb_gfx_present_input_selfcheck COMMAND gmb_gfx_present_input_selfcheck)
elseif(GMB_SDL2_LIB AND GMB_SDL2_INCLUDE_DIR)
    add_executable(gmb_gfx_present_input_selfcheck tests/gmb_gfx_present_input_selfcheck.c)
    target_link_libraries(gmb_gfx_present_input_selfcheck PRIVATE gmb_gfx ${GMB_SDL2_LIB})
    target_include_directories(gmb_gfx_present_input_selfcheck PRIVATE include ${GMB_SDL2_INCLUDE_DIR})
    add_test(NAME gmb_gfx_present_input_selfcheck COMMAND gmb_gfx_present_input_selfcheck)
endif()

# SdlConsole a CELLE headless (driver dummy): creazione senza finestra + ponte input -> controlli.
if(SDL2_FOUND)
    add_executable(gmb_sdl_console_selfcheck tests/gmb_sdl_console_selfcheck.c)
    target_link_libraries(gmb_sdl_console_selfcheck PRIVATE gmb_runtime SDL2::SDL2)
    target_include_directories(gmb_sdl_console_selfcheck PRIVATE include)
    add_test(NAME gmb_sdl_console_selfcheck COMMAND gmb_sdl_console_selfcheck)
elseif(GMB_SDL2_LIB AND GMB_SDL2_INCLUDE_DIR)
    add_executable(gmb_sdl_console_selfcheck tests/gmb_sdl_console_selfcheck.c)
    target_link_libraries(gmb_sdl_console_selfcheck PRIVATE gmb_runtime ${GMB_SDL2_LIB})
    target_include_directories(gmb_sdl_console_selfcheck PRIVATE include ${GMB_SDL2_INCLUDE_DIR})
    add_test(NAME gmb_sdl_console_selfcheck COMMAND gmb_sdl_console_selfcheck)
endif()

add_executable(gmb_ffi_selfcheck tests/gmb_ffi_selfcheck.c)
target_link_libraries(gmb_ffi_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_ffi_selfcheck PRIVATE include)
add_test(NAME gmb_ffi_selfcheck COMMAND gmb_ffi_selfcheck)

add_executable(gmb_gaps_selfcheck tests/gmb_gaps_selfcheck.c)
target_link_libraries(gmb_gaps_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_gaps_selfcheck PRIVATE include)
add_test(NAME gmb_gaps_selfcheck COMMAND gmb_gaps_selfcheck)

add_executable(gmb_sb_selfcheck tests/gmb_sb_selfcheck.c)
target_link_libraries(gmb_sb_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_sb_selfcheck PRIVATE include)
add_test(NAME gmb_sb_selfcheck COMMAND gmb_sb_selfcheck)

add_executable(gmb_ingest_selfcheck tests/gmb_ingest_selfcheck.c)
target_link_libraries(gmb_ingest_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_ingest_selfcheck PRIVATE include)
add_test(NAME gmb_ingest_selfcheck COMMAND gmb_ingest_selfcheck)

add_executable(bench_vault_27k tests/bench_vault_27k.c)
target_link_libraries(bench_vault_27k PRIVATE gmb_runtime)
target_include_directories(bench_vault_27k PRIVATE include)

add_executable(gmb_rag_selfcheck tests/gmb_rag_selfcheck.c)
target_link_libraries(gmb_rag_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_rag_selfcheck PRIVATE include)
add_test(NAME gmb_rag_selfcheck COMMAND gmb_rag_selfcheck)

# Self-Consistency / Semantic Entropy (Spanda R_sc) — P1 gate
add_executable(gmb_uncertainty_selfcheck tests/gmb_uncertainty_selfcheck.c)
target_link_libraries(gmb_uncertainty_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_uncertainty_selfcheck PRIVATE include)
add_test(NAME gmb_uncertainty_selfcheck COMMAND gmb_uncertainty_selfcheck)

add_executable(mcp_server_selfcheck tests/mcp_server_selfcheck.c)
target_link_libraries(mcp_server_selfcheck PRIVATE gmb_runtime)
target_include_directories(mcp_server_selfcheck PRIVATE include)
add_test(NAME mcp_server_selfcheck COMMAND mcp_server_selfcheck)

add_executable(gmb_agents_selfcheck tests/gmb_agents_selfcheck.c)
target_link_libraries(gmb_agents_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_agents_selfcheck PRIVATE include)
add_test(NAME gmb_agents_selfcheck COMMAND gmb_agents_selfcheck)

find_package(Threads REQUIRED)
add_executable(gmb_dag_selfcheck tests/gmb_dag_selfcheck.c)
target_link_libraries(gmb_dag_selfcheck PRIVATE gmb_runtime Threads::Threads)
target_include_directories(gmb_dag_selfcheck PRIVATE include)
add_test(NAME gmb_dag_selfcheck COMMAND gmb_dag_selfcheck)

add_executable(gmb_vecquant_selfcheck tests/gmb_vecquant_selfcheck.c)
target_link_libraries(gmb_vecquant_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_vecquant_selfcheck PRIVATE include)
add_test(NAME gmb_vecquant_selfcheck COMMAND gmb_vecquant_selfcheck)

add_executable(gmb_sql_selfcheck tests/gmb_sql_selfcheck.c)
target_link_libraries(gmb_sql_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_sql_selfcheck PRIVATE include)
add_test(NAME gmb_sql_selfcheck COMMAND gmb_sql_selfcheck)

add_executable(gmb_sb_graph_selfcheck tests/gmb_sb_graph_selfcheck.c)
target_link_libraries(gmb_sb_graph_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_sb_graph_selfcheck PRIVATE include)
add_test(NAME gmb_sb_graph_selfcheck COMMAND gmb_sb_graph_selfcheck)

add_executable(gmb_finetune_selfcheck tests/finetune_selfcheck.c)
target_link_libraries(gmb_finetune_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_finetune_selfcheck PRIVATE include)
add_test(NAME gmb_finetune_selfcheck COMMAND gmb_finetune_selfcheck)

# R2: IPC fake host + selfcheck
add_executable(ipc_fake_host tests/ipc_fake_host.c)
target_link_libraries(ipc_fake_host PRIVATE gmb_runtime)
target_include_directories(ipc_fake_host PRIVATE include)

add_executable(gmb_ipc_selfcheck tests/ipc_selfcheck.c)
target_link_libraries(gmb_ipc_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_ipc_selfcheck PRIVATE include)
add_test(NAME gmb_ipc_selfcheck COMMAND gmb_ipc_selfcheck)
set_tests_properties(gmb_ipc_selfcheck PROPERTIES
    ENVIRONMENT "PATH=${CMAKE_CURRENT_BINARY_DIR}:${CMAKE_CURRENT_SOURCE_DIR}/tests:$ENV{PATH}")

# R2.6: certificazione IPC
add_executable(gmb_ipc_cert_selfcheck tests/ipc_cert_selfcheck.c)
target_link_libraries(gmb_ipc_cert_selfcheck PRIVATE gmb_runtime)
target_include_directories(gmb_ipc_cert_selfcheck PRIVATE include)
add_test(NAME gmb_ipc_cert_selfcheck COMMAND gmb_ipc_cert_selfcheck)
set_tests_properties(gmb_ipc_cert_selfcheck PROPERTIES
    ENVIRONMENT "PATH=${CMAKE_CURRENT_BINARY_DIR}:${CMAKE_CURRENT_SOURCE_DIR}/tests:$ENV{PATH}")

# ── Dylib nativa del MODEL COMPILER LLM (`gmb_ai_llm`) — per P/Invoke dal mondo gestito ──
# Espone gmb_llm_model_*/llmdesc_*/gmb_llm_{arch,policy}_cmd ai backend C# (IL, PCodeVM).
# La logica resta UNA (queste sorgenti C): il C# la CHIAMA, non la ricopia (AGENTS: C#→C ammesso;
# C→C# mai). Nome `gmb_ai_llm` → risolta da Basic.Providers.AiNativeResolver.
# NB: i sorgenti del MODEL COMPILER (gmb_llm_model.c/llmdesc.c/…) vivono nel progetto
# `gmb_model_io` (STATIC). Se la dylib si limitasse a LINKARE la static lib, il linker macOS
# scarterebbe `gmb_llm_model.o` (nessun riferimento dagli altri oggetti) e i simboli `gmb_llm_*`
# NON verrebbero esportati → i backend gestiti (IL/PCodeVM) fallivano la P/Invoke
# ("Unable to find an entry point named 'gmb_llm_*'"). I sorgenti vanno quindi COMPILATI
# direttamente nella dylib (come il target originale `fbeee5a58`): così ogni simbolo globale è
# incluso ed esportato. Le dipendenze (gmb_arch/ggml_codec/…) arrivano dai target linkati.
add_library(gmb_ai_llm SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/src/llmmodelio_model.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/src/llmmodelio_imatrix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/src/llmmodelio_infer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/src/quant/llmcodec_iq2_xxs.c   # F3a: quant-ENCODE iq (PUL)
    ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/src/quant/llmcodec_moe_quant.c   # F3c: encode affine MoE (PUL)
    ${LLMENV}/LLMEngine/moe/src/llmengine_moe_quant_alloc.c
    src/gmb_distill.c
    src/gmb_asr_cmd.c)   # ASR: entry-point P/Invoke `gmb_asr_cmd` (superficie ASR.TRANSCRIBE; engine in gmb_runtime)
target_include_directories(gmb_ai_llm PRIVATE include src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/include/quant
    ${LLMENV}/LLMDesc
    ${LLMENV}/LLMEngine/moe/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_doc_c/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_torch/include)
target_link_libraries(gmb_ai_llm PRIVATE gmb_runtime llmcodec llmcodec_ggml llmcodec_mlx_affine llmcodec_fp8 llmengine_plugin llmdesc llm_asr llmtoken gmbllm_core)   # IQ2_XXS + affine = plugin
# Forza l inclusione/esportazione di gmb_opt_env (usato da GmbEnv C#) dalla runtime static.
if(APPLE)
    target_link_options(gmb_ai_llm PRIVATE "-Wl,-u,_gmb_opt_env")
endif()

set_target_properties(gmb_ai_llm PROPERTIES
    PREFIX "lib" OUTPUT_NAME "gmb_ai_llm"
    LIBRARY_OUTPUT_DIRECTORY "${GMB_LIBS_OUT}")
set_target_properties(gmb_runtime PROPERTIES INSTALL_RPATH "@loader_path")

# ── Theme snapshot CLI (read-only) per l'IDE: palette + stile pixel del tema.
add_executable(gmb_theme tools/gmb_theme_cli.c src/gmb_tui_theme.c)
target_include_directories(gmb_theme PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(gmb_theme PRIVATE -Wall -Wextra)
gmb_exe_relocatable(gmb_theme)

# Cartella ABI di LLMEnv: settings JSONC (paths.abiDir) + risoluzione alla prima esecuzione. Include il .c: prova anche le statiche.
add_executable(test_abi_settings tests/test_abi_settings.c ${LLMENV}/LLMDesc/llmdesc.c src/gmb_paths.c)
target_include_directories(test_abi_settings PRIVATE include ${LLMENV}/LLMDesc)
target_compile_options(test_abi_settings PRIVATE -w)
add_test(NAME test_abi_settings COMMAND test_abi_settings)
