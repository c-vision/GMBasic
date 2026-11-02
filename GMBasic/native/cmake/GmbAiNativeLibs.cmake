# GmbAiNativeLibs.cmake — GARANZIA anti-stale GLOBALE per le lib/dylib AI native.
#
# PROBLEMA (ricorrente): le lib AI native (libgmb_runtime.a, libgmbllm_core.a, libgmbllm_train.a) e le dylib
# (libgmb_ai_llm, libgmb_ai_modelio) sono PRODOTTE da ALBERI CMake separati
# (native/gmb_runtime_c, native/gmb_compiler_c) e linkate BY-PATH dai consumer: CMake NON le
# conosce come dipendenze -> un cambio di sorgente AI le lascia STALE, e il consumer usa il
# runtime VECCHIO in silenzio (bug reali, es. LLM TENSOR VIEW/KVPLAN con output vecchio).
#
# SOLUZIONE: rendere AUTOMATICO il rebuild. `gmb_ai_native_depends(<target>)` crea UN custom
# target (per build dir) che esegue `tools/sh/gmb_ensure_ai_libs.sh` (ricostruisce le lib/dylib in
# modo incrementale) e lo aggancia con add_dependencies: gira SEMPRE PRIMA della build del
# consumer. Se il rebuild FALLISCE (o il build dir delle lib manca) -> la build del consumer si
# BLOCCA con errore (niente lib stale in silenzio).
#
# USO, in OGNI CMakeLists di un consumer nativo che linka le lib AI:
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbAiNativeLibs.cmake)
#   gmb_ai_native_depends(<target>)          # una riga per ogni target consumer
#
# (Sostituisce i vecchi blocchi `gmb_ai_stale_check`, che giravano solo al CONFIG e quindi non
#  intercettavano i cambi di sorgente avvenuti dopo la configurazione.)

function(gmb_ai_native_depends _target)
  if(NOT TARGET ${_target})
    message(FATAL_ERROR "gmb_ai_native_depends: target '${_target}' inesistente")
  endif()
  if(NOT TARGET gmb_ai_native_libs)
    set(_repo "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_custom_target(gmb_ai_native_libs ALL        COMMAND /bin/zsh "${_repo}/../tools/sh/gmb_ensure_ai_libs.sh"
      COMMENT "gmb_ai_native_libs: ricostruisco le lib/dylib AI native (garanzia anti-stale)"
      VERBATIM)
  endif()
  add_dependencies(${_target} gmb_ai_native_libs)
endfunction()
