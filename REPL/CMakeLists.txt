# REPL/ — la REPL unica del monorepo (C): pilota il compilatore C via CLI (`gmb`),
# settings, help (REPL/helps), comandi, IDE, package/fixtures. Esecuzione vmc|c99
# 100% C (gmb_vm_host / cc); engine C# opzionale via `gmbcs`.
cmake_minimum_required(VERSION 3.10)
project(gmbrepl C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# RID + rpath rilocabile (una sola ricetta: native/cmake/GmbRid.cmake).
include(${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/cmake/GmbRid.cmake)

add_subdirectory(../GMBasic/native/executor executor)
# gmb_runtime (gmb_agent.c) referenzia i simboli di gmb_agent_runtime (gmb_agent_ask_with_tools,
# gmb_brain_*) -> serve il target linkabile (come fa gmb_vm_c).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/gmb_agent_runtime gmb_agent_runtime_build)

add_executable(gmbrepl
    ../GMBasic/native/gmb_vm_c/gmb_chunk.c
    main.c
    arguments.c
    repl.c
    settings.c
    helps.c
    editor.c
    gmb_debug.c
    debug_server.c
    menu.c
    ml.c
    help_form.c
    packages.c
    ide.c
    chunk_utils.c
    progress.c
    orchestrator.c
    vmc_runner.c
    c99_runner.c
    cs_runner.c
    bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/gmb_bytecode/src/gmb_disasm.c
    # PRIMO USO: marker + delega a `gmbadmin first-use` (nessun link nuovo; la logica di
    # download resta UNA, in gmb_first_use.c dietro `gmbadmin`).
    ${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/gmb_runtime_c/src/gmb_first_use_spawn.c
)
# F2c (2026-11-01): la REPL NON linka più il front-end. Il LSP è il tool separato
# `gmblsp` (native/lsp), avviato come processo dai client (IDE GMBasic, VS Code).
# Il package manager CLI è `gmbadmin pkg` (tool C esterno): la REPL non linka gmb_pkg.
target_link_libraries(gmbrepl PRIVATE gmb_runtime gmb_agent_runtime)
target_include_directories(gmbrepl PRIVATE ../GMBasic/native/gmb_runtime_c/include ../GMBasic/native/executor ../GMBasic/native/gmb_vm_c ../GMBasic/native/gmb_bytecode/include)

# Nessun path del builder nell'eseguibile: rpath con soli @loader_path relativi (dylib
# accanto al binario o in libs/<rid>/). Prima l'rpath conteneva la cache dell'utente
# (`/Users/<nome>/.gmbasic/libs/<rid>`) e `build/libs/<rid>` del checkout: il binario
# copiato in un bundle non caricava su un'altra macchina.
gmb_exe_relocatable(gmbrepl)
