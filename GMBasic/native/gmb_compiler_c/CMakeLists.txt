# native/gmb_compiler_c — orchestratore dei DUE compilatori C (speculari a common/{Lexer,Parser,Linker,Compiler}):
#   * frontend/     front-end CONDIVISO (lexer -> parser -> AST -> semantica -> linker)
#   * ../gmb_c99native/   back-end C99Native (.bas -> sorgente C)
#   * ../gmb_bytecode/    back-end BYTECODE (.bas -> chunk A1/GMBC per la VM/C)
# Qui restano: il target AGGREGATO `gmb_compiler_c` (lib statica con TUTTI i backend, per i
# consumatori: executor/repl/gmb_vm_c/gmb_runtime_c) e il MODEL compiler (checkpoint MoE).
# Libreria autonoma, NESSUNA dipendenza dal runtime (gmb_runtime_c).
cmake_minimum_required(VERSION 3.10)
# PAVIMENTO macOS allineato a `tools/sh/zcc` (MACOS_FLOOR): senza, cmake non passa alcun
# deployment target e clang prende il minimo dello SDK del builder (qui 27.2) — i binari
# distribuiti (`gmbnative`, `gmb`, `gmblsp`, `gmbchunk`, …) partivano SOLO su macOS 27.2,
# mentre i programmi prodotti da zcc dichiarano 12.0. Va impostato PRIMA di `project()`,
# che è quando CMake inizializza i flag del compilatore; `CMAKE_HOST_SYSTEM_NAME` è già
# valorizzato a questo punto (`APPLE` non lo è).
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
endif()
project(gmb_compiler_c C)

# RID della macchina di build (unico posto: native/cmake/GmbRid.cmake): l'output delle nostre
# dylib deve finire in build/libs/<rid>. Era "osx-arm64" hardcoded: su Linux il build scriveva
# nella cartella macOS (libgmb_ai_modelio.so finiva in build/libs/${GMB_RID}).
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
# ARCHINC: include del runtime condiviso (llmdesc.h) per i target che usano il converter.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include ${LLMENV}/LLMDesc ${LLMENV}/LLMEngine/plugin)

set(CMAKE_C_STANDARD 99)

# K6: parità bit-exact (compiler/driver C) — niente contrazione FMA.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-ffp-contract=off)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── I due back-end + il front-end condiviso (OBJECT libs) ─────────────────────────────
add_subdirectory(frontend)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_bytecode gmb_bytecode)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_c99native gmb_c99native)

# Target AGGREGATO storico: `gmb_compiler_c` = front-end + entrambi i back-end.
# Mantiene NOME e include dirs per non rompere executor/repl/gmb_vm_c/gmb_runtime_c.
add_library(gmb_compiler_c STATIC
  $<TARGET_OBJECTS:gmb_frontend>
  $<TARGET_OBJECTS:gmb_bytecode>
  $<TARGET_OBJECTS:gmb_c99native>
)
target_include_directories(gmb_compiler_c PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_bytecode/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_c99native/include"
)

# ── Driver dei due back-end (output in build/ come prima, per gmbsuite) ───────────────
# Driver C99Native (.bas -> C): usato da tools/sh/gmb_c99native_run.sh, tools/test_mcp_e2e.sh
# e da gmbsuite gbt_build.c. Senza target CMake il binario restava STALE dopo le modifiche.
add_executable(gmbnative "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_c99native/driver/gmb_c99native_driver.c")
target_link_libraries(gmbnative PRIVATE gmb_compiler_c)
target_compile_options(gmbnative PRIVATE -Wall -Wextra)
target_sources(gmbnative PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src/gmb_opts.c ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src/gmb_paths.c ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src/gmb_abi.c ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src/gmb_first_use_spawn.c)   # ponte registro->env (gmb_opt_env) + primo uso (delega a gmbadmin)
target_include_directories(gmbnative PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include)
# Ricetta unica (native/cmake/GmbRid.cmake): il tool nasce in <root>/bin + rpath relativo.
gmb_exe_relocatable(gmbnative)

# ── CLI di PROGETTO (O7): `gmb check|build|run --project <dir>`. Linka SOLO il front-end
# (la discovery `gmb_inc_resolve`, la stessa del compilatore: niente copie nel CLI). È il
# contratto a processo che l'IDE userà (D4); il driver C99Native lo cerca accanto al binario.
add_executable(gmb ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src/gmb_first_use_spawn.c
               src/cli/gmb_cli.c src/cli/gmb_cli_project.c src/cli/gmb_cli_dump.c src/cli/gmb_cli_engine.c src/cli/gmb_cli_usage.c src/cli/gmb_cli_admin.c src/cli/gmb_cli_file.c src/cli/gmb_cli_deploy.c src/cli/gmb_cli_args.c src/cli/gmb_cli_bsp.c src/cli/gmb_cli_doctor.c
               ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_bytecode/src/gmb_disasm.c
               ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_vm_c/gmb_chunk.c
               $<TARGET_OBJECTS:gmb_frontend>)   # split per la soglia 1200 righe (§0quater)
# RID di build: il CLI cerca librerie/header nella CACHE (`~/.gmbasic/libs/<rid>/`).
# Una sola mappatura (GmbRid.cmake), passata come costante al compilatore.
target_compile_definitions(gmb PRIVATE GMB_RID_STR="${GMB_RID}")
target_include_directories(gmb PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/frontend/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_bytecode/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_vm_c")
target_compile_options(gmb PRIVATE -Wall -Wextra)
gmb_exe_relocatable(gmb)   # in <root>/bin, accanto a gmbnative che il CLI lancia

# ── Language Server (LSP) come TOOL SEPARATO (F2c): serve le IDE e VS Code.
# Prima viveva dentro la REPL (flag --lsp) e la costringeva a linkare il front-end.
add_executable(gmblsp
               "${CMAKE_CURRENT_SOURCE_DIR}/../lsp/main.c"
               "${CMAKE_CURRENT_SOURCE_DIR}/../lsp/gmb_lsp.c")
target_link_libraries(gmblsp PRIVATE gmb_compiler_c)
target_include_directories(gmblsp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/frontend/include")
target_compile_options(gmblsp PRIVATE -Wall -Wextra)
gmb_exe_relocatable(gmblsp)

# ── Driver chunk A1/GMBC (.bas -> bytecode VM/C): usato da tools/test_mcp_e2e.sh e da
# gmbsuite. Stesso problema del gmbnative: senza target CMake il binario
# restava STALE dopo ogni modifica a parser/emitter.
add_executable(gmbchunk "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_bytecode/driver/gmb_chunk_driver.c"
               "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src/gmb_bsp_compile.c")
target_include_directories(gmbchunk PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src")
target_link_libraries(gmbchunk PRIVATE gmb_compiler_c "-lc++" m pthread)
target_compile_options(gmbchunk PRIVATE -Wall -Wextra)
target_sources(gmbchunk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src/gmb_opts.c ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src/gmb_paths.c ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src/gmb_abi.c)   # ponte registro->env (gmb_opt_env)
target_include_directories(gmbchunk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include)
gmb_exe_relocatable(gmbchunk)

# ── Model compiler: conversione checkpoint MoE (safetensors/MLX) -> container GGUF ──
# Modulo DEL COMPILATORE (non tool esterno). Linka il runtime MoE per il writer del container;
# la libreria gmb_compiler_c resta dependency-free per il pipeline .bas.
add_subdirectory(${LLMENV}/LLMEngine/moe llmengine_moe_build)
# `GGUF` DEPRECATO: i tool gmb_model_quantize / gmb_conv_verify / gmb_conv_parity (converter GGUF)
# sono RIMOSSI. La conversione supportata è GGUF/MLX via codec (gmb_mlx_to_gguf / gmb_conv_*_arch).

add_executable(gmb_model_quantize driver/gmb_model_quantize.c src/model/gmb_model_quantize.c src/model/gmb_st.c)
target_include_directories(gmb_model_quantize PRIVATE include)
target_link_libraries(gmb_model_quantize PRIVATE llmengine_moe)
target_compile_options(gmb_model_quantize PRIVATE -Wall -Wextra -Wpedantic)

add_executable(gmb_model_dequantize driver/gmb_model_dequantize.c src/model/gmb_model_dequantize.c src/model/gmb_st.c)
target_include_directories(gmb_model_dequantize PRIVATE include)
target_link_libraries(gmb_model_dequantize PRIVATE llmengine_moe)
target_compile_options(gmb_model_dequantize PRIVATE -Wall -Wextra -Wpedantic)

# §E2 step 2: quantizzatore/dequantizzatore ESPERTI 3D (test in-repo, round-trip bounded).
add_executable(test_model_quantize_3d tests/test_model_quantize_3d.c src/model/gmb_model_quantize.c src/model/gmb_model_dequantize.c src/model/gmb_st.c)
target_include_directories(test_model_quantize_3d PRIVATE include)
target_link_libraries(test_model_quantize_3d PRIVATE llmengine_moe)
target_compile_options(test_model_quantize_3d PRIVATE -Wall -Wextra -Wpedantic)

# §E4a (Q2-b): il consumatore imatrix dell'allocatore mixed usa davvero l'importanza per-colonna.
add_executable(test_quant_auto_imatrix tests/test_quant_auto_imatrix.c src/model/gmb_model_quantize.c src/model/gmb_model_dequantize.c src/model/gmb_st.c)
target_include_directories(test_quant_auto_imatrix PRIVATE include)
target_link_libraries(test_quant_auto_imatrix PRIVATE llmengine_moe)
target_compile_options(test_quant_auto_imatrix PRIVATE -Wall -Wextra -Wpedantic)

# K7 (residuo PUL): verifica PER-SHARD (index weight_map + header di ogni shard).
add_executable(test_gst_verify_shards tests/test_gst_verify_shards.c src/model/gmb_st.c)
target_include_directories(test_gst_verify_shards PRIVATE include)
target_compile_options(test_gst_verify_shards PRIVATE -Wall -Wextra -Wpedantic)

# ── Dylib `gmb_ai_modelio` — P/Invoke dal C# per il MODEL I/O (convert/quantize/inspect) ──
# Il C# chiama queste funzioni invece di spawnare gmb_model_quantize/gmb-discover (AGENTS:
# C#→C via P/Invoke; C→C# mai).
add_library(gmb_ai_modelio SHARED
    src/model/gmb_modelio_shim.c
    ../gmb_runtime_c/src/gmb_abi.c
    ../gmb_runtime_c/src/gmb_paths.c
    ${LLMENV}/LLMDesc/llmdesc.c
    ${LLMENV}/LLMEngine/plugin/llmengine_plugin.c
    src/model/gmb_model_quantize.c
    src/model/gmb_st.c)
target_include_directories(gmb_ai_modelio PRIVATE include src/model
    ${LLMENV}/LLMEngine/moe/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src)
# Cartella ABI di LLMEnv anche per il compilatore: paths.abiDir in system.settings.json (stessa funzionalita' del runtime, scope SYSTEM).
target_compile_definitions(gmb_ai_modelio PRIVATE GMB_ABI_SCOPE_SYSTEM)
target_link_libraries(gmb_ai_modelio PRIVATE gmb_compiler_c llmengine_moe)
set_target_properties(gmb_ai_modelio PROPERTIES
    PREFIX "lib" OUTPUT_NAME "gmb_ai_modelio"
    LIBRARY_OUTPUT_DIRECTORY "${GMB_LIBS_OUT}")
