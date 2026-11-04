# Notebook/ — notebook server di GMBASIC, progetto C autonomo
#
# Un processo HTTP su 127.0.0.1 che esegue celle BASIC in una sessione con stato
# (variabili persistenti tra le celle). È il ripristino in **C** del `NotebookServer`
# che viveva nella REPL C# (eliminata, F6b): stesso protocollo JSON.
cmake_minimum_required(VERSION 3.10)
project(gmb_notebook C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Runtime C condiviso (valutatore espressioni/statement, dict, JSON): stessa sorgente
# del mondo C, nessuna dipendenza da C#.
add_subdirectory(../GMBasic/native/executor executor)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/gmb_agent_runtime gmb_agent_runtime_build)

add_executable(gmb_notebook
    main.c
    notebook_kernel.c
    notebook_server.c
)

target_link_libraries(gmb_notebook PRIVATE gmb_runtime gmb_agent_runtime)
target_include_directories(gmb_notebook PRIVATE ../GMBasic/native/gmb_runtime_c/include)

# Esecuzione delle celle = P-CODE: libgmb_eval porta il COMPILATORE C (gmb_compile_snippet)
# e la VM/C; la cella è compilata in chunk A1/GMBC ed eseguita dalla VM (nessun valutatore
# di espressioni separato, nessun gmb_expr). Artefatto della build di native/gmb_vm_c: se
# manca, il notebook NON si configura (errore esplicito, niente stub).
set(GMB_EVAL_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/gmb_vm_c/build/libgmb_eval.dylib")
if(NOT EXISTS "${GMB_EVAL_LIB}")
    message(FATAL_ERROR
        "libgmb_eval assente (${GMB_EVAL_LIB}): costruisci prima native/gmb_vm_c — "
        "cmake --build GMBasic/native/gmb_vm_c/build --target gmb_eval")
endif()
target_link_libraries(gmb_notebook PRIVATE "${GMB_EVAL_LIB}")
set_target_properties(gmb_notebook PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/gmb_vm_c/build"
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/gmb_vm_c/build")
