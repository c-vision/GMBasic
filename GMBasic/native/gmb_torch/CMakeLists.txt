# ============================================================================
# gmb_torch — host ML (PyTorch/libtorch) a superficie C stabile (gmb_ml.h)
# ============================================================================
# Produce libgmb_torch.{dylib,so,dll} — il wrapper C++ su libtorch caricato a
# runtime (dlopen da gmb_torch.c / P/Invoke da MlNativeEngine), MAI linkato.
#
# Uso:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release -DTORCH_PATH=<dir con lib+include>
#   cmake --build build -j
#
# TORCH_PATH punta alla radice della distribuzione libtorch (include/ + lib/):
#   - una distro C++ ufficiale (pytorch.org), oppure
#   - un venv Python torch (<venv>/lib/pythonX.Y/site-packages/torch).
# ============================================================================

cmake_minimum_required(VERSION 3.18)
project(gmb_torch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED TORCH_PATH)
    set(TORCH_PATH "$ENV{TORCH_PATH}")
endif()
if(NOT TORCH_PATH)
    # Auto-rilevamento (nessun -DTORCH_PATH richiesto in locale): il venv/conda ATTIVO,
    # poi il venv canonico del progetto (~/ai/env312), poi qualunque venv ~/ai/env*,
    # infine una libtorch C++ vendored in native/gmb_torch/libtorch.
    file(GLOB _torch_vv   "$ENV{VIRTUAL_ENV}/lib/python3.*/site-packages/torch")
    file(GLOB _torch_cond "$ENV{CONDA_PREFIX}/lib/python3.*/site-packages/torch")
    file(GLOB _torch_ai   "$ENV{HOME}/ai/env*/lib/python3.*/site-packages/torch")
    foreach(_cand "$ENV{HOME}/ai/env312/lib/python3.12/site-packages/torch"
                  ${_torch_vv} ${_torch_cond} ${_torch_ai}
                  "${CMAKE_CURRENT_SOURCE_DIR}/libtorch")
        if(NOT TORCH_PATH AND EXISTS "${_cand}/include/torch/csrc/api/include/torch/torch.h")
            set(TORCH_PATH "${_cand}")
        endif()
    endforeach()
endif()
if(NOT TORCH_PATH)
    message(FATAL_ERROR "TORCH_PATH non impostato e nessuna libtorch trovata. "
        "Passa -DTORCH_PATH=<radice con include/ + lib/> "
        "(venv: <venv>/lib/pythonX.Y/site-packages/torch).")
endif()
message(STATUS "gmb_torch: TORCH_PATH=${TORCH_PATH}")

set(TORCH_INCLUDE "${TORCH_PATH}/include")
set(TORCH_INCLUDE_API "${TORCH_PATH}/include/torch/csrc/api/include")
set(TORCH_LIB "${TORCH_PATH}/lib")

find_library(TORCH_LIBRARY torch PATHS "${TORCH_LIB}" NO_DEFAULT_PATH REQUIRED)
find_library(TORCH_CPU_LIBRARY torch_cpu PATHS "${TORCH_LIB}" NO_DEFAULT_PATH REQUIRED)
find_library(C10_LIBRARY c10 PATHS "${TORCH_LIB}" NO_DEFAULT_PATH REQUIRED)

add_library(gmb_torch SHARED src/gmb_host_ml.cpp)
target_include_directories(gmb_torch PUBLIC include PRIVATE "${TORCH_INCLUDE}" "${TORCH_INCLUDE_API}")
target_compile_definitions(gmb_torch PRIVATE GMB_ML_BUILD_SHARED)

if(APPLE)
    # libomp del venv/libtorch: reference @rpath/libomp.dylib (l'install_name
    # assoluto /opt/llvm-openmp non esiste) — risolto dal rpath a TORCH_LIB.
    target_link_libraries(gmb_torch PRIVATE "${TORCH_LIBRARY}" "${TORCH_CPU_LIBRARY}" "${C10_LIBRARY}")
    target_link_options(gmb_torch PRIVATE
        "-Wl,-rpath,${TORCH_LIB}"
        "-Wl,-install_name,@rpath/libgmb_torch.dylib")
else()
    target_link_libraries(gmb_torch PRIVATE "${TORCH_LIBRARY}" "${TORCH_CPU_LIBRARY}" "${C10_LIBRARY}")
endif()

if(APPLE)
    add_custom_command(TARGET gmb_torch POST_BUILD
        COMMAND install_name_tool -change "/opt/llvm-openmp/lib/libomp.dylib" "@rpath/libomp.dylib" "$<TARGET_FILE:gmb_torch>")
endif()

# Registro opzioni: gmb_opt_env (ponte env->registro) — infra 2026-09-29
target_include_directories(gmb_torch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/include)
# --- GUARDIA ANTI-STALE (2026-10-01): le lib AI PRECOMPILATE (libgmbllm.a / libgmb_runtime.a) sono
# linkate by-path -> nessuna dipendenza CMake. Se piu' VECCHIE dei loro sorgenti, il consumer userebbe il
# runtime VECCHIO in silenzio. Qui si FALLISCE con errore esplicito.

target_link_libraries(gmb_torch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c/build/libgmb_runtime.a)

# Garanzia anti-stale GLOBALE (auto-rebuild + blocco): modulo condiviso.
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbAiNativeLibs.cmake")
gmb_ai_native_depends(gmb_torch)
