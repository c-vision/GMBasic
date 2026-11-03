cmake_minimum_required(VERSION 3.10)
project(gmb_doc_c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Runtime condiviso: aggiunto solo se non già presente nel build (gmb_vm_c lo aggiunge
# a sua volta — evitare doppio add_subdirectory della stessa sorgente).
if(NOT TARGET gmb_runtime)
    add_subdirectory(../gmb_runtime_c gmb_runtime_c_build)
endif()

set(GMB_DOC_SOURCES
    src/gmb_doc.c
    src/gmb_doc_pdf.c
    src/gmb_doc_ocr.c
)

add_library(gmb_doc STATIC ${GMB_DOC_SOURCES})
target_include_directories(gmb_doc PUBLIC include)
target_include_directories(gmb_doc PRIVATE ../gmb_runtime_c/include)
target_link_libraries(gmb_doc PUBLIC gmb_runtime ${CMAKE_DL_LIBS})

add_library(gmb_doc_shared SHARED ${GMB_DOC_SOURCES})
target_include_directories(gmb_doc_shared PUBLIC include)
target_include_directories(gmb_doc_shared PRIVATE ../gmb_runtime_c/include)
target_link_libraries(gmb_doc_shared PUBLIC gmb_runtime ${CMAKE_DL_LIBS})
# Dopo lo split core/AI (2026-11-02) il runtime statico referenzia i servizi che vivono in
# `gmb_agent_runtime` (gmb_brain_ask_sync, gmb_agent_ask_with_tools, ...): un dylib deve
# risolverli a load-time, quindi si usa lo stesso idioma degli altri dylib del repo
# (gmb_runtime_ai, gmb_fonts, gmb_grep, sql ext). Su ELF/Windows resta il link normale.
if(APPLE)
    target_link_options(gmb_doc_shared PRIVATE -Wl,-undefined,dynamic_lookup)
endif()

# Selfcheck
add_executable(gmb_doc_selfcheck tests/gmb_doc_selfcheck.c)
target_include_directories(gmb_doc_selfcheck PRIVATE include ../gmb_runtime_c/include)
target_link_libraries(gmb_doc_selfcheck PRIVATE gmb_doc gmb_runtime)

enable_testing()
add_test(NAME gmb_doc_selfcheck COMMAND gmb_doc_selfcheck)
