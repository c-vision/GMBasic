cmake_minimum_required(VERSION 3.10)
project(gmb_vm_c C)

# VM minimale per embedding (todo VM-Minimale-Embedding): con GMB_VM_MIN la tabella dei
# builtin potra' omettere i domini non-core a compile-time (il linker scarta i loro handler).
# OFF = comportamento attuale, nessun cambio.
option(GMB_VM_MIN "VM minimale: esclude i domini non-core dalla tabella builtin" OFF)
option(GMB_VM_SPECIALIZED "Tabella builtin specializzata per chunk (solo handler usati)" OFF)
if(GMB_VM_MIN)
    add_compile_definitions(GMB_VM_MIN)
endif()
if(GMB_VM_SPECIALIZED)
    add_compile_definitions(GMB_VM_SPECIALIZED)
endif()

# OBJCXX: il sottodominio LLMEngine/moe (incluso via gmb_compiler_c) ha sorgenti .mm.
# Senza un enable_language(OBJCXX) esplicito a livello di progetto, la generazione può fallire
# con "CMAKE_OBJCXX_COMPILE_OBJECT not set" quando il sottodominio non lo abilita in tempo.
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# K6: parità bit-exact (VM/C) — niente contrazione FMA (default Clang/GCC non uniformi).
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-ffp-contract=off -ffunction-sections -fdata-sections)
endif()
# Scarto del codice morto: SEMPRE. Con una sezione per funzione il linker puo' eliminare
# i builtin di dominio non raggiunti (prerequisito della VM minimale).
if(APPLE)
    add_link_options(-Wl,-dead_strip)
elseif(UNIX)
    add_link_options(-Wl,--gc-sections)
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

# PERFORMANCE: di default build RELEASE (-O2) — la VM/C interpreta il bytecode e senza
# ottimizzazione il dispatcher perde ~2.8x (misurato 2026-08-18). Per debug esplicito:
# cmake -DCMAKE_BUILD_TYPE=Debug.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "gmb_vm_c: CMAKE_BUILD_TYPE non specificato → Release (-O2)")
endif()

# Runtime condiviso (transpiler + VM): librerie C comuni.
# Le funzioni di libreria (list/dict/math/stringhe/...) vivono qui una sola volta
# e sono usate sia dal codice transpilato (emitter C#) sia dagli opcode della VM/C.
# NOTA percorso: gmb_runtime_c è un fratello in native/ (infrastruttura condivisa,
# usata anche da native/gmb_db_c per il modulo DB C#). gmb_vm_c è stato ripristinato
# qui dal refactor 2026-08-27 (prima viveva in executor-native/gmb_vm_c, archiviato).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c gmb_runtime_c_build)

# Libreria documenti (gmb_doc_c): conversione CONVERT.* (md/html/text/docx/odt/epub/xlsx/pptx/web).
# Fratello in native/, usata anche dal transpiler C99 (emitter C#) e dal modulo DB C#.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_doc_c gmb_doc_c_build)

# Agent Runtime — Universal Action Runtime (tool_registry, computer_provider, capability, vault, verify)
# Fratello in native/, runtime condiviso VM/C + C99Native + host C# via P/Invoke.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_agent_runtime gmb_agent_runtime_build)

# Compilatore C — per AI VERIFY "COMPILE <path>" (B2): il runtime gmb_verify.c referenzia
# DEBOLMENTE gmb_compile_bas_file; qui lo si collega all'host VM/C. Il compilatore è autonomo
# (nessuna dipendenza dal runtime), quindi il link è pulito. Opzione per decouplare la build
# dell'host quando il compilatore è in uno stato transitorio (working tree condiviso).
option(GMB_VM_LINK_COMPILER "Collega il compilatore C all'host VM/C (AI VERIFY COMPILE)" ON)
if(GMB_VM_LINK_COMPILER)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_compiler_c gmb_compiler_c_build)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Tabella dei domini: inclusa SEMPRE nel build completo (compilatore completo).
# Esclusa solo nel build minimale (GMB_VM_MIN): senza riferimenti, il linker scarta
# handler e dipendenze di dominio.
if(GMB_VM_MIN OR GMB_VM_SPECIALIZED)
    set(GMB_VM_EXT_SRC "")
else()
    set(GMB_VM_EXT_SRC gmb_builtin_ext.c)
endif()
if(GMB_VM_SPECIALIZED)
    set(GMB_VM_DOMAIN_SRCS "")
else()
    set(GMB_VM_DOMAIN_SRCS gmb_syscall_db.c gmb_syscall_fileio.c gmb_syscall_rdd.c gmb_syscall_vectordb.c
    gmb_syscall_sql.c gmb_vm_tui.c gmb_vm_tui_controls.c gmb_vm_console.c)
endif()

# libffi: motore unico delle chiamate FFI (vedi gmb_call.h).
# Preferisce la copia VENDORIZZATA in shared/libs/ffi (build.sh), altrimenti la lib di sistema.
set(GMB_FFI_VENDORED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../shared/libs/ffi")
if(EXISTS "${GMB_FFI_VENDORED_DIR}/out/lib/libffi.a")
    set(GMB_FFI_LIBRARY "${GMB_FFI_VENDORED_DIR}/out/lib/libffi.a")
    set(GMB_FFI_INCLUDE_DIR "${GMB_FFI_VENDORED_DIR}/out/include")
    add_compile_definitions(GMB_FFI_VENDORED)
    message(STATUS "libffi: vendorizzata (${GMB_FFI_LIBRARY})")
else()
    find_library(GMB_FFI_LIBRARY NAMES ffi ffi-8)
    if(NOT GMB_FFI_LIBRARY)
        message(FATAL_ERROR "libffi non trovata: eseguire shared/libs/ffi/build.sh o installare libffi")
    endif()
    message(STATUS "libffi: di sistema (${GMB_FFI_LIBRARY})")
endif()

add_library(gmb_vm STATIC
    ${GMB_VM_EXT_SRC}
    gmb_call.c
    gmb_vm_value.c
    gmb_chunk.c
    gmb_vm_core.c
    ${GMB_VM_DOMAIN_SRCS}
    gmb_builtin.c
    gmb_builtin_bits.c
    gmb_builtin_tray.c
    gmb_mcp_exec.c
    gmb_uncertainty_exec.c
    gmb_vm_eval.c
    gmb_vm_events.c
    gmb_gpm.c
    gmb_adapter.c
    gmb_vm_signal.c
    gmb_vm_bsp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_torch/src/gmb_torch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_torch/src/gmb_torch_str.c
)
# SPLIT 2026-11-02: il runtime non propaga più gmbllm/LLMEnv. La VM dispatcha i comandi
# LLM (LLM TENSOR/ARCH/KERNEL…) e usa direttamente llmmodelio_* → linka gmbllm_core (che a sua
# volta porta llmcodec/llmtoken/llmdesc in PUBLIC). ASR/DISTILL/modelio passano dal gateway.
target_include_directories(gmb_vm PUBLIC ${GMB_FFI_INCLUDE_DIR})
target_link_libraries(gmb_vm PUBLIC pthread gmb_runtime gmb_doc gmb_agent_runtime gmbllm_core ${GMB_FFI_LIBRARY} ${CMAKE_DL_LIBS})
target_include_directories(gmb_vm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_bytecode/include ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_compiler_c/include ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_compiler_c/frontend/include ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/src ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_doc_c/include ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_torch/include ${LLMENV}/LLMEngine/include ${LLMENV}/LLMEngine/mlx ${LLMENV}/LLMEngine/Options ${LLMENV}/LLMEngine/plugin ${LLMENV}/LLMEngine/moe/include ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/include ${CMAKE_CURRENT_SOURCE_DIR}/../../gmbllm/include/quant ${LLMENV}/LLMToken/include ${LLMENV}/LLMCodec/include ${LLMENV}/LLMDesc)

# Database su file (DBF/ISAM/SPEED/Ledger/Clipper RDD) via gmb_db: SEMPRE presente.
# Prima era un option(GMB_HAS_COMMERCIAL_DB ... OFF) legato all'"edizione": con la rimozione
# della matrice Edition il supporto e' parte del build unico (nessun flag da attivare).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_db_c gmb_db_c_build)
target_compile_definitions(gmb_vm PRIVATE GMB_NATIVE_DB)
target_include_directories(gmb_vm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_db_c/include)
target_link_libraries(gmb_vm PRIVATE gmb_db)

# ── Valutatore a runtime come libreria CONDIVISA (libgmb_eval) ──────────────
# Compilatore C + VM minima, caricata ON DEMAND (dlopen) dai contesti senza VM
# (filtri DBF/SPEED, programmi C99Native). Non è linkata nei binari normali.
add_library(gmb_eval SHARED gmb_eval_lib.c)
target_include_directories(gmb_eval PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include)
target_link_libraries(gmb_eval PRIVATE gmb_vm gmb_runtime gmb_compiler_c pthread ${CMAKE_DL_LIBS})
set_target_properties(gmb_eval PROPERTIES
    OUTPUT_NAME gmb_eval
    C_VISIBILITY_PRESET default)
if(APPLE)
    target_link_options(gmb_eval PRIVATE "LINKER:-u,_gmb_builtin_ext_anchor")
endif()

# ── BSP engine come libreria CONDIVISA per P/Invoke (C#) ─────────────────────
# Espone il renderer VM/C (gmb_vm_bsp.c) senza passare dal processo gmb_vm_host:
# un host C# può servire le pagine .bsp eseguendole nella VM C in-process.
# Direzione unica C# → C: il C non conosce il managed.
# Il renderer consuma il sidecar A1 "<pagina>.bsp.chunk" (precompilazione managed).
add_library(gmb_bsp SHARED
    gmb_bsp_pinvoke.c
)
target_include_directories(gmb_bsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include)
target_link_libraries(gmb_bsp PRIVATE gmb_vm gmb_runtime pthread ${CMAKE_DL_LIBS})
# Il runtime (gmb_verify.c/gmb_observe_test.c) referenzia il compilatore C
# (gmb_compile_bas_file): come per gmb_vm_host, va linkato anche qui.
if(GMB_VM_LINK_COMPILER)
    target_link_libraries(gmb_bsp PRIVATE gmb_compiler_c)
endif()
set_target_properties(gmb_bsp PROPERTIES
    OUTPUT_NAME gmb_bsp
    C_VISIBILITY_PRESET default)
# Il registro dei builtin di dominio si registra via constructor in una static lib:
# con -dead_strip serve un riferimento esplicito (stesso ancoraggio di gmb_vm_host).
if(APPLE)
    target_link_options(gmb_bsp PRIVATE "LINKER:-u,_gmb_builtin_ext_anchor")
else()
    target_link_options(gmb_bsp PRIVATE "LINKER:--undefined=gmb_builtin_ext_anchor")
endif()

# Host CLI: legge un chunk A1 e lo esegue con la VM.
add_executable(gmb_vm_host
    gmb_vm_host.c
)
target_link_libraries(gmb_vm_host PRIVATE gmb_vm gmb_runtime pthread)
if(GMB_VM_LINK_COMPILER)
    target_link_libraries(gmb_vm_host PRIVATE gmb_compiler_c)
endif()
# Il registro dei builtin di dominio (gmb_builtin_ext.c) si registra via constructor:
# essendo in una static library con -dead_strip, senza un riferimento esplicito il linker
# scarta l'intero object e la VM risponde "builtin N non implementato". Ancoriamo l'object
# con -u (un solo simbolo forza il pull dell'intero .o, constructor compreso).
if(APPLE)
    target_link_options(gmb_vm_host PRIVATE "LINKER:-u,_gmb_builtin_ext_anchor")
elseif(UNIX)
    target_link_options(gmb_vm_host PRIVATE "LINKER:-u,gmb_bf_498")
endif()
# rpath RILOCABILE: l'host è nel PATH del client e viene lanciato dalla REPL (`gmb_native_exec`),
# quindi il binario deve caricare libllmtoken/libgmb_tray/SDL2 senza path del builder. Prima
# l'rpath portava `/Users/<nome>/.gmbasic/libs/<rid>` e `…/gmb_runtime_c_build/llmtoken_build`:
# copiato in un bundle non partiva. Vedi gmb_exe_relocatable (native/cmake/GmbRid.cmake).
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbRid.cmake)
gmb_exe_relocatable(gmb_vm_host)

# Tool-loop di AI AGENT ASK (parità AgentToolLoop.AskWithTools C#): test deterministico
# con brain finto + registry tool. Linka gmb_runtime (gmb_agent_ask) e gmb_agent_runtime
# (loop/registry, transitiva da gmb_vm).
add_executable(gmb_agent_tool_loop_selfcheck tests/agent_tool_loop_selfcheck.c)
target_link_libraries(gmb_agent_tool_loop_selfcheck PRIVATE gmb_vm gmb_runtime pthread)
target_include_directories(gmb_agent_tool_loop_selfcheck PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include)
add_test(NAME gmb_agent_tool_loop_selfcheck COMMAND gmb_agent_tool_loop_selfcheck)

# gmb_opt_env (ponte env->registro) — infra 2026-09-29
target_include_directories(gmb_vm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include)
# --- GUARDIA ANTI-STALE (2026-10-01): le lib AI PRECOMPILATE (libgmbllm.a / libgmb_runtime.a) sono
# linkate by-path -> nessuna dipendenza CMake. Se piu' VECCHIE dei loro sorgenti, il consumer userebbe il
# runtime VECCHIO in silenzio. Qui si FALLISCE con errore esplicito.
# Garanzia anti-stale GLOBALE (auto-rebuild + blocco): modulo condiviso. Sostituisce il vecchio
# check solo-al-config, che non intercettava i cambi di sorgente avvenuti dopo la configurazione.
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbAiNativeLibs.cmake")

target_link_libraries(gmb_vm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/build/libgmb_runtime.a)

# Garantisce lib/dylib AI aggiornate PRIMA di ogni build di questi target.
gmb_ai_native_depends(gmb_vm)
if(TARGET gmb_vm_host)
  gmb_ai_native_depends(gmb_vm_host)
endif()
