# GmbToolLink.cmake — `<build dir>/<tool>` -> `<root>/bin/<tool>` (symlink, SOLO build dir).
#
# PERCHÉ. I tool si generano DIRETTAMENTE in `<root>/bin/` (una sola copia, quella appena
# linkata: nessun `cp` a mano che possa lasciare un binario vecchio). Ma una decina di script
# e i test del CLI invocano ancora il path storico `…/<progetto>/build/gmb` (gmb, gmbnative,
# gmbchunk, gmb_vm_host, gmbrepl, gmbnotebook). Il symlink tiene VALIDI quei path senza
# duplicare il binario: non può diventare stale, punta sempre al file in bin/.
#
# Idempotente: se nella build dir c'è ancora il file REALE della vecchia configurazione
# (l'output locale di prima), viene rimosso e sostituito dal symlink. Un symlink già
# corretto viene ricreato (costo nullo); se la creazione fallisce è solo un WARNING — il
# tool è comunque in bin/, quindi la build non deve fallire.
#
# Uso: cmake -DTOOL_FILE=<root>/bin/<tool> -DLEGACY_PATH=<build dir>/<tool> -P GmbToolLink.cmake

if(NOT DEFINED TOOL_FILE OR NOT DEFINED LEGACY_PATH)
    message(FATAL_ERROR "GmbToolLink: servono -DTOOL_FILE=... e -DLEGACY_PATH=...")
endif()

if(NOT EXISTS "${TOOL_FILE}")
    # Nessun tool linkato (target saltato): non è un errore, non creare un symlink pendente.
    return()
endif()

if(EXISTS "${LEGACY_PATH}" OR IS_SYMLINK "${LEGACY_PATH}")
    file(REMOVE "${LEGACY_PATH}")
endif()

file(CREATE_LINK "${TOOL_FILE}" "${LEGACY_PATH}" SYMBOLIC RESULT _err)
if(_err)
    message(WARNING "GmbToolLink: symlink non creato (${LEGACY_PATH} -> ${TOOL_FILE}): ${_err}")
endif()
