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

message(STATUS "GmbRid: RID di build ${GMB_RID} (cross=${CMAKE_OSX_ARCHITECTURES}) → artefatti in ${GMB_LIBS_OUT}")
