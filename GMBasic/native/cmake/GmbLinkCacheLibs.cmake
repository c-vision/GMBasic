# GmbLinkCacheLibs.cmake — crea <dir>/libs/<rid> -> cache utente (SOLO sviluppo).
#
# PERCHÉ un file a parte. `cmake -E create_symlink` NON sovrascrive: il POST_BUILD di
# gmb_exe_relocatable gira per OGNI eseguibile che finisce nella stessa cartella, quindi il
# secondo target (e ogni build successiva) moriva con
#   CMake Error: failed to create symbolic link '.../libs/osx-arm64': File exists
# e il target veniva dichiarato fallito (link già avvenuto). Un `rm` prima sarebbe peggio:
# in RELEASE quella stessa cartella è `bin/libs/<rid>/` con le dylib VERE, non uno symlink.
#
# Regola: se la destinazione esiste già — symlink (anche pendente) o cartella reale — non si
# tocca nulla. Idempotente, portabile (CMake puro, nessuna shell).
#
# Uso: cmake -DLINK_SRC=<cache>/<rid> -DLINK_DST=<dir>/libs/<rid> -P GmbLinkCacheLibs.cmake

if(NOT DEFINED LINK_SRC OR NOT DEFINED LINK_DST)
    message(FATAL_ERROR "GmbLinkCacheLibs: servono -DLINK_SRC=... e -DLINK_DST=...")
endif()

# EXISTS segue i symlink: uno symlink pendente non esiste ma va comunque rispettato.
if(EXISTS "${LINK_DST}" OR IS_SYMLINK "${LINK_DST}")
    return()
endif()

if(NOT EXISTS "${LINK_SRC}")
    # Cache utente assente (macchina senza terze parti scaricate): niente symlink, non è un errore.
    return()
endif()

file(CREATE_LINK "${LINK_SRC}" "${LINK_DST}" SYMBOLIC RESULT _err)
if(_err)
    message(WARNING "GmbLinkCacheLibs: symlink non creato (${LINK_DST}): ${_err}")
endif()
