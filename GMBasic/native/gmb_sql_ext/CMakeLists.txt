# gmb_sql_ext — estensioni SQLite caricabili: virtual table `csv` e `parquet`.
# Sorgente unico per ciascuna; usate dai backend C# via SqliteConnection.LoadExtension.
# I backend C registrano gli stessi sorgenti staticamente dal runtime condiviso.
cmake_minimum_required(VERSION 3.20)
project(gmb_sql_ext C)

# Header sqlite3 VENDORIZZATI 3.46.1 (stessa versione del SQLite bundled di Microsoft.Data.Sqlite):
# compilare con header PIÙ NUOVI del runtime host causava un SIGSEGV (offset oltre api_routines).
set(SQLITE3_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sqlite346")

# zstd: SOLO dalla cache on-demand (bundle GMBasicModules, install-name @rpath). Linkare da
# directory del builder registra un path ASSOLUTO nel binario e il bundle non è più portabile —
# era il caso di gmb_sql_parquet_ext (trovato dal gate libs_bundle_selfcheck). NESSUN fallback:
# se manca, ci si ferma e si dice come rimediare.
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GmbRid.cmake)
set(_GMB_RES_CACHE "$ENV{HOME}/.gmbasic/libs/${GMB_RID}")   # radice unica
find_library(ZSTD_LIB zstd PATHS "${_GMB_RES_CACHE}" NO_DEFAULT_PATH)
if(NOT ZSTD_LIB)
    message(FATAL_ERROR "zstd non trovata in ${_GMB_RES_CACHE}: esegui `gmb install --runtime-only` (o `--dev` per gli header)")
endif()
find_library(Z_LIB z)                       # libreria di SISTEMA
find_path(ZSTD_INCLUDE_DIR zstd.h PATHS "${_GMB_RES_CACHE}/include" NO_DEFAULT_PATH)
if(NOT ZSTD_INCLUDE_DIR)
    message(FATAL_ERROR "zstd.h non trovato in ${_GMB_RES_CACHE}/include: esegui `gmb install --dev`")
endif()

# ---- modulo csv ----
add_library(gmb_sql_csv_ext MODULE ../gmb_runtime_c/src/gmb_csv_vtab.c)
target_compile_definitions(gmb_sql_csv_ext PRIVATE GMB_CSV_AS_EXTENSION)
target_include_directories(gmb_sql_csv_ext PRIVATE "${SQLITE3_INCLUDE_DIR}")
set_target_properties(gmb_sql_csv_ext PROPERTIES PREFIX "" OUTPUT_NAME "gmb_sql_csv_ext")

# ---- modulo parquet ----
add_library(gmb_sql_parquet_ext MODULE ../gmb_runtime_c/src/gmb_parquet_vtab.c)
target_compile_definitions(gmb_sql_parquet_ext PRIVATE GMB_PARQUET_AS_EXTENSION)
target_include_directories(gmb_sql_parquet_ext PRIVATE "${SQLITE3_INCLUDE_DIR}")
set_target_properties(gmb_sql_parquet_ext PROPERTIES PREFIX "" OUTPUT_NAME "gmb_sql_parquet_ext")
if(ZSTD_LIB)
  target_compile_definitions(gmb_sql_parquet_ext PRIVATE GMB_PARQUET_HAVE_ZSTD)
  target_link_libraries(gmb_sql_parquet_ext PRIVATE ${ZSTD_LIB})
endif()
if(ZSTD_INCLUDE_DIR)
  target_include_directories(gmb_sql_parquet_ext PRIVATE "${ZSTD_INCLUDE_DIR}")
endif()
if(Z_LIB)
  target_link_libraries(gmb_sql_parquet_ext PRIVATE ${Z_LIB})
endif()

if(APPLE)
  set_target_properties(gmb_sql_csv_ext gmb_sql_parquet_ext PROPERTIES SUFFIX ".dylib")
  # I simboli sqlite3_* non sono linkati (arrivano dall'host via api pointer).
  target_link_options(gmb_sql_csv_ext PRIVATE "-undefined" "dynamic_lookup")
  target_link_options(gmb_sql_parquet_ext PRIVATE "-undefined" "dynamic_lookup")
endif()
