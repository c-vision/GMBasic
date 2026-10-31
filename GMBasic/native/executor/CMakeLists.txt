# native/executor — Native Executor V2 (C): --backend vmc|c99, MAI IL.
# E' il secondo executor V2 (accanto a Executor C#). Esecuzione 100% C:
#   vmc -> gmb_vm_host (chunk A1/GMBC)
#   c99 -> cc + libgmb_runtime.a (generazione sorgente C ancora C#: divergenza)
# Compilazione vmc 100% C: gmb_compile -> chunk A1/GMBC -> gmb_vm_host.
cmake_minimum_required(VERSION 3.10)
project(gmb_executor_native C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(../gmb_runtime_c gmb_runtime_c)
add_subdirectory(../gmb_compiler_c gmb_compiler_c)

# Libreria del Native Executor: espone gmb_native_exec_vmc / gmb_native_exec_c99
# (usata da ReplNative C). Eseguibile driver incluso.
add_library(gmb_executor_native STATIC executor.c)
target_include_directories(gmb_executor_native PUBLIC .)
target_link_libraries(gmb_executor_native PUBLIC gmb_runtime gmb_compiler_c)

add_executable(gmb_executor_native_host executor_main.c)
target_link_libraries(gmb_executor_native_host PRIVATE gmb_executor_native gmb_runtime gmb_compiler_c)

# Garanzia anti-stale GLOBALE (auto-rebuild + blocco): modulo condiviso.
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbAiNativeLibs.cmake")
if(TARGET gmb_executor_native)
  gmb_ai_native_depends(gmb_executor_native)
endif()
if(TARGET gmb_executor_native_host)
  gmb_ai_native_depends(gmb_executor_native_host)
endif()
