# GmbRid.cmake — RID della macchina di BUILD, unico posto in cui si decide.
#
# PERCHÉ. `osx-arm64` era scritto a mano in 5 punti di gmb_runtime_c (output delle nostre
# dylib, rpath, cartella delle esterne) e in 4 di gmb_plugin_example. Effetto: su Linux o
# Windows il build compilava, ma gli artefatti finivano nella stessa cartella (`libs-runtime/osx-arm64`) e sarebbero
# stati pubblicati con l'etichetta sbagliata (`libs_publish_gh.sh <gruppo> <rid>` prende il RID
# dagli argomenti, quindi un bundle "linux" poteva contenere file presi da una cartella osx).
#
# La mappatura è la STESSA del resolver C# (`LibsResolver.Rid`) e del runtime C
# (`gmb_paths_*`): se cambia là, va cambiata qui (una sola tabella per mondo, ma valori uguali).
#
# Espone:
#   GMB_RID          osx-arm64 | osx-x64 | linux-x64 | linux-arm64 | win-x64 | win-arm64
#   GMB_RPATH_ORIGIN @loader_path (Apple) | $ORIGIN (Linux) | "" (Windows)
#
# Uso: include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbRid.cmake)

# GMB_RID può essere IMPOSTATO dal chiamante (-DGMB_RID=osx-x64) per una build CROSS (su macOS
# si compila anche per Intel). In quel caso si rispetta, ma i valori DERIVATI (rpath) vanno
# comunque calcolati: prima il `return()` anticipato lasciava GMB_RPATH_ORIGIN vuoto e le dylib
# cross-compilate uscivano senza rpath.
if(NOT DEFINED GMB_RID)
    if(WIN32)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|ARM64|aarch64|AARCH64")
            set(GMB_RID win-arm64)
        else()
            set(GMB_RID win-x64)
        endif()
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(GMB_RID osx-arm64)
        else()
            set(GMB_RID osx-x64)
        endif()
    else()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(GMB_RID linux-arm64)
        else()
            set(GMB_RID linux-x64)
        endif()
    endif()
endif()

# Derivati (indipendenti dal fatto che GMB_RID sia arrivato da -D o calcolato qui).
if(WIN32)
    set(GMB_RPATH_ORIGIN "")
elseif(APPLE)
    set(GMB_RPATH_ORIGIN "@loader_path")
else()
    set(GMB_RPATH_ORIGIN "$ORIGIN")
endif()

# ── Dove finiscono i NOSTRI artefatti (dylib/so dei sotto-progetti) ────────────────────────
# Dentro l'ALBERO DI BUILD, mai in una cartella sorgente. Prima erano in `libs-runtime/<rid>/`
# nel checkout: una seconda sorgente di verità accanto alla cache utente, che il repo si
# portava dietro (fino a 928 MB) e che un `git status` non vedeva nemmeno (era ignorata).
#   GMB_LIBS_OUT     build/libs/<rid>/     dylib/so/statiche nostre (gmb_*, llm*, …)
#   GMB_PLUGINS_OUT  build/plugins/<rid>/  plugin di ESEMPIO (codec/prim), caricati con dlopen
set(GMB_LIBS_OUT    "${CMAKE_CURRENT_LIST_DIR}/../../build/libs/${GMB_RID}")
set(GMB_PLUGINS_OUT "${CMAKE_CURRENT_LIST_DIR}/../../build/plugins/${GMB_RID}")

# Cartella di QUESTO file, catturata adesso. Dentro una function() CMAKE_CURRENT_LIST_DIR punta
# al CMakeLists CHIAMANTE (quindi a `gmb_runtime_c/`): il POST_BUILD cercava lì
# GmbLinkCacheLibs.cmake e falliva con "script non trovato" su ogni eseguibile.
set(GMB_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# ── gmb_exe_relocatable(<target>) ─────────────────────────────────────────────────────────
# Rende un ESEGUIBILE rilocabile: l'rpath contiene SOLO percorsi relativi all'eseguibile
# (@loader_path / $ORIGIN). Nessun path del builder, quindi nessun `/Users/<nome>/...` e
# nessun `$HOME/.gmbasic/libs/<rid>` cucito dentro: un binario copiato in un bundle (o su
# un'altra macchina con un altro utente) carica le stesse dylib. Con
# BUILD_WITH_INSTALL_RPATH TRUE CMake NON aggiunge più le directory assolute delle librerie
# linkate (era il meccanismo che infilava la cache e build/libs/<rid> negli eseguibili).
#
# Ordine delle voci (dyld/ld.so SALTANO quelle che non esistono, quindi le varianti delle
# due profondità dell'albero convivono):
#  1. accanto all'eseguibile                        bundle con le dylib in bin/
#  2. libs/<rid>/ accanto all'eseguibile            bundle "pulito": bin/libs/<rid>/
#  3. ../GMBasic/build/libs/<rid>                   checkout: nostre dylib (da bin/)
#  4. ../../GMBasic/build/libs/<rid>                checkout: da <progetto>/build/
#  5. ../../build/libs/<rid>                        checkout: da native/gmb_vm_c/build/
#  6. ../GMBasic/native/gmb_vm_c/build             checkout: libgmb_eval, libllmcodec_*
#  7. ../../GMBasic/native/gmb_vm_c/build           idem, da <progetto>/build/
#  8. gmb_runtime_c_build/llmtoken_build            checkout: libllmtoken (sottoprogetto)
#
# In SVILUPPO le terze parti (SDL2, OpenSSL, …) non stanno nel repo ma nella cache utente:
# `libs/<rid>` accanto all'eseguibile è creata come SYMLINK alla cache a ogni build. In
# RELEASE quella stessa cartella contiene le dylib vere (nessun symlink, nessuna dipendenza
# dalla macchina che ha compilato).
function(gmb_exe_relocatable _target)
    if(WIN32)
        set(_rpath "")
    else()
        set(_o "${GMB_RPATH_ORIGIN}")
        set(_rpath
            "${_o}"
            "${_o}/libs/${GMB_RID}"
            "${_o}/../GMBasic/build/libs/${GMB_RID}"
            "${_o}/../../GMBasic/build/libs/${GMB_RID}"
            "${_o}/../../build/libs/${GMB_RID}"
            "${_o}/../GMBasic/native/gmb_vm_c/build"
            "${_o}/../../GMBasic/native/gmb_vm_c/build"
            "${_o}/gmb_runtime_c_build/llmtoken_build")
    endif()
    set_target_properties(${_target} PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "${_rpath}")
    if(NOT WIN32)
        # Il symlink lo crea uno script idempotente: `cmake -E create_symlink` NON sovrascrive,
        # quindi al secondo eseguibile della stessa cartella (e a ogni build successiva) il
        # POST_BUILD moriva con `File exists` e faceva fallire il target. Lo script non tocca
        # nulla se la destinazione esiste già (in release `bin/libs/<rid>/` è una cartella vera).
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${_target}>/libs"
            COMMAND ${CMAKE_COMMAND}
                    "-DLINK_SRC=$ENV{HOME}/.gmbasic/libs/${GMB_RID}"
                    "-DLINK_DST=$<TARGET_FILE_DIR:${_target}>/libs/${GMB_RID}"
                    -P "${GMB_CMAKE_DIR}/GmbLinkCacheLibs.cmake"
            COMMENT "rpath rilocabile: libs/${GMB_RID} -> cache utente (solo sviluppo)")
    endif()
endfunction()

message(STATUS "GmbRid: RID di build ${GMB_RID} (cross=${CMAKE_OSX_ARCHITECTURES}) → artefatti in ${GMB_LIBS_OUT}")
