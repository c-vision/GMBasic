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

# Selfcheck
add_executable(gmb_doc_selfcheck tests/gmb_doc_selfcheck.c)
target_include_directories(gmb_doc_selfcheck PRIVATE include ../gmb_runtime_c/include)
target_link_libraries(gmb_doc_selfcheck PRIVATE gmb_doc gmb_runtime)

enable_testing()
add_test(NAME gmb_doc_selfcheck COMMAND gmb_doc_selfcheck)
