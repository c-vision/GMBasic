# Front-end CONDIVISO del compilatore C: lexer -> macro -> preprocess -> parser -> AST ->
# semantica (semgraph/semfold/semquery) -> resolver/rewrite -> linker.
# Usato da ENTRAMBI i back-end (C99Native e bytecode). Nessuna dipendenza dal runtime.
#
# CONFIGURE_DEPENDS (CMake >= 3.12): senza, il GLOB e' valutato SOLO al configure-time e un
# file nuovo (es. gmb_semver.c, 2026-10-06) entrava nel build solo DOPO un `cmake .` manuale:
# il build riusciva apparentemente ("Built target gmb_frontend") e il fallimento emergeva
# solo al LINK, con simboli mancanti tipo _gmb_semver_satisfies. CONFIGURE_DEPENDS fa
# rieseguire il configure automaticamente quando cambiano i sorgenti della cartella.
file(GLOB GMB_FRONTEND_ROOT_SRC   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB GMB_FRONTEND_PARSER_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/parser/*.c")

add_library(gmb_frontend OBJECT ${GMB_FRONTEND_ROOT_SRC} ${GMB_FRONTEND_PARSER_SRC})
# RID della macchina di build (GmbRid.cmake, unico posto): lo usa la ricetta condivisa
# `gmb_cc_defaults.c` per la cache native `~/.gmbasic/libs/<rid>/`. Gli oggetti sono inclusi
# anche dal CLI `gmb`, che ha la stessa definizione sui suoi sorgenti.
target_compile_definitions(gmb_frontend PUBLIC GMB_RID_STR="${GMB_RID}")
# BEFORE: il frontend/include dell'AUTONOMO front-end vince su ../gmb_runtime_c/include (che il
# CMakeLists padre aggiunge globalmente). I due gmb_semver.h condividono la guardia GMB_SEMVER_H:
# senza BEFORE, quello del runtime (senza GMB_SEMVER_PRE_MAX) veniva incluso per primo e quello
# del frontend saltato -> build pulita fallita.
target_include_directories(gmb_frontend BEFORE PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
