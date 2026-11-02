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
