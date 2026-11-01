# native/repl — ReplNative (C): REPL del Native Executor (vmc|c99), mai IL.
# Replica REPL (C#): CLI completa, settings, help, comandi. Esecuzione vmc|c99
# 100% C (gmb_vm_host / cc); la compilazione .bas->chunk/.c usa il ponte C#
# (compilatore P-Code C: gmb_compile -> chunk -> gmb_vm_host)
cmake_minimum_required(VERSION 3.10)
project(gmb_repl C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(../executor executor)
# gmb_runtime (gmb_agent.c) referenzia i simboli di gmb_agent_runtime (gmb_agent_ask_with_tools,
# gmb_brain_*) -> serve il target linkabile (come fa gmb_vm_c).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_agent_runtime gmb_agent_runtime_build)

add_executable(gmb_repl
    ../gmb_vm_c/gmb_chunk.c
    main.c
    arguments.c
    repl.c
    settings.c
    helps.c
    editor.c
    gmb_debug.c
    gmb_lsp.c
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
    bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_bytecode/src/gmb_disasm.c
)
target_link_libraries(gmb_repl PRIVATE gmb_runtime gmb_agent_runtime gmb_executor_native gmb_compiler_c)
target_include_directories(gmb_repl PRIVATE ../gmb_runtime_c/include ../gmb_compiler_c/include ../executor ../gmb_vm_c ../gmb_bytecode/include)
