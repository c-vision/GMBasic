# Notebook/ — notebook server di GMBASIC, progetto C autonomo
#
# Un processo HTTP su 127.0.0.1 che esegue celle BASIC in una sessione con stato
# (variabili persistenti tra le celle). È il ripristino in **C** del `NotebookServer`
# che viveva nella REPL C# (eliminata, F6b): stesso protocollo JSON.
cmake_minimum_required(VERSION 3.10)
project(gmbnotebook C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# RID + rpath rilocabile (una sola ricetta: native/cmake/GmbRid.cmake).
include(${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/cmake/GmbRid.cmake)

# Runtime C condiviso (valutatore espressioni/statement, dict, JSON): stessa sorgente
# del mondo C, nessuna dipendenza da C#.
add_subdirectory(../GMBasic/native/executor executor)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../GMBasic/native/gmb_agent_runtime gmb_agent_runtime_build)

add_executable(gmbnotebook
    main.c
    notebook_kernel.c
    notebook_server.c
)

target_link_libraries(gmbnotebook PRIVATE gmb_runtime gmb_agent_runtime)
target_include_directories(gmbnotebook PRIVATE ../GMBasic/native/gmb_runtime_c/include)

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
target_link_libraries(gmbnotebook PRIVATE "${GMB_EVAL_LIB}")
# libgmb_eval è linkata per path ASSOLUTO (artefatto della build di gmb_vm_c), ma il suo
# install_name è `@rpath/libgmb_eval.dylib`: l'rpath deve quindi essere RELATIVO, non la
# directory assoluta del checkout (che finiva cucita nel binario e non esisteva altrove).
# `gmb_exe_relocatable` include già `../GMBasic/native/gmb_vm_c/build` e `libs/<rid>/`.
gmb_exe_relocatable(gmbnotebook)
