cmake_minimum_required(VERSION 3.10)
project(gmb_db_c C)

# Commercial-only DB engines (D8/D11). Do NOT add this directory to gmb_runtime_c.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ABI bridge (gmb_db_*) kept until 1:1 engines are wired.
set(GMB_DB_ABI_SOURCES
    src/common/gmb_db_common.c
    src/dbf/gmb_dbf.c
    src/dbf/gmb_dbf_index.c
    src/isam/gmb_isam.c
    src/speed/gmb_speed.c
    src/speed/gmb_speed_query.c
    src/ledger/gmb_ledger.c
    src/wal/gmb_dbf_wal.c
    src/wal/gmb_speed_wal.c
    src/lock/gmb_db_lock.c
    src/repl/gmb_db_repl.c
)

# 1:1 C# → C (DbfFile.cs → DbfFile.c). Exclude ABI gmb_*.c.
file(GLOB_RECURSE GMB_DB_PORT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
list(FILTER GMB_DB_PORT_SOURCES EXCLUDE REGEX "/gmb_[^/]+\\.c$")
list(FILTER GMB_DB_PORT_SOURCES EXCLUDE REGEX "/common/")

set(GMB_DB_SOURCES ${GMB_DB_ABI_SOURCES} ${GMB_DB_PORT_SOURCES})

find_package(Threads REQUIRED)

add_library(gmb_db STATIC ${GMB_DB_SOURCES})
target_include_directories(gmb_db PUBLIC include)
target_include_directories(gmb_db PRIVATE src/common)
target_include_directories(gmb_db PRIVATE ../gmb_runtime_c/include)
target_link_libraries(gmb_db PUBLIC Threads::Threads)

add_library(gmb_db_shared SHARED ${GMB_DB_SOURCES})
target_include_directories(gmb_db_shared PUBLIC include)
target_include_directories(gmb_db_shared PRIVATE src/common)
target_include_directories(gmb_db_shared PRIVATE ../gmb_runtime_c/include)
target_compile_definitions(gmb_db_shared PRIVATE GMB_DB_BUILD_SHARED)
set_target_properties(gmb_db_shared PROPERTIES OUTPUT_NAME gmb_db)
target_link_libraries(gmb_db_shared PUBLIC Threads::Threads)
# Dopo lo split core/AI (2026-11-02) il runtime statico referenzia servizi che vivono in
# `gmb_agent_runtime` (gmb_brain_ask_sync, gmb_agent_ask_with_tools, ...). Un dylib deve
# risolverli a load-time: stesso idioma degli altri dylib del repo (gmb_runtime_ai, gmb_fonts).
if(APPLE)
    target_link_options(gmb_db_shared PRIVATE -Wl,-undefined,dynamic_lookup)
endif()

# Dummy SPEED / SET FILTER usano gmb_expr. Non aggiungere gmb_db dentro gmb_runtime_c.
if(NOT TARGET gmb_runtime)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../gmb_runtime_c" gmb_runtime_c_from_db)
endif()
set_property(TARGET gmb_runtime PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(gmb_db PUBLIC gmb_runtime)
target_link_libraries(gmb_db_shared PUBLIC gmb_runtime)
if(NOT WIN32)
    target_compile_options(gmb_db PRIVATE -Wall -Wextra)
    target_compile_options(gmb_db_shared PRIVATE -Wall -Wextra)
endif()

add_executable(gmb_db_wal_selfcheck tests/wal_selfcheck.c)
target_link_libraries(gmb_db_wal_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_wal_selfcheck PRIVATE include)

add_executable(gmb_db_ntx_selfcheck tests/ntx_selfcheck.c)
target_link_libraries(gmb_db_ntx_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_ntx_selfcheck PRIVATE include)

add_executable(gmb_db_isam_hdr_selfcheck tests/isam_hdr_selfcheck.c)
target_link_libraries(gmb_db_isam_hdr_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_isam_hdr_selfcheck PRIVATE include)

add_executable(gmb_db_sst_selfcheck tests/sst_selfcheck.c)
target_link_libraries(gmb_db_sst_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_sst_selfcheck PRIVATE include)

add_executable(gmb_db_speed_selfcheck tests/speed_selfcheck.c)
target_link_libraries(gmb_db_speed_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_speed_selfcheck PRIVATE include)

add_executable(gmb_db_ledger_selfcheck tests/ledger_selfcheck.c)
target_link_libraries(gmb_db_ledger_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_ledger_selfcheck PRIVATE include)

add_executable(gmb_db_memo_selfcheck tests/memo_selfcheck.c)
target_link_libraries(gmb_db_memo_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_memo_selfcheck PRIVATE include)

add_executable(gmb_db_ndx_selfcheck tests/ndx_selfcheck.c)
target_link_libraries(gmb_db_ndx_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_ndx_selfcheck PRIVATE include)

add_executable(gmb_db_mdx_selfcheck tests/mdx_selfcheck.c)
target_link_libraries(gmb_db_mdx_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_mdx_selfcheck PRIVATE include)

add_executable(gmb_db_cdx_selfcheck tests/cdx_selfcheck.c)
target_link_libraries(gmb_db_cdx_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_cdx_selfcheck PRIVATE include)

add_executable(gmb_db_idxfactory_selfcheck tests/idxfactory_selfcheck.c)
target_link_libraries(gmb_db_idxfactory_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_idxfactory_selfcheck PRIVATE include)

add_executable(gmb_db_clipper_selfcheck tests/clipper_selfcheck.c)
target_link_libraries(gmb_db_clipper_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_clipper_selfcheck PRIVATE include)

add_executable(gmb_db_engine_selfcheck tests/engine_selfcheck.c)
target_link_libraries(gmb_db_engine_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_engine_selfcheck PRIVATE include)

add_executable(gmb_db_cplm_selfcheck tests/cplm_selfcheck.c)
target_link_libraries(gmb_db_cplm_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_cplm_selfcheck PRIVATE include)

add_executable(gmb_db_repl_tcp_selfcheck tests/repl_tcp_selfcheck.c)
target_link_libraries(gmb_db_repl_tcp_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_repl_tcp_selfcheck PRIVATE include)

add_executable(gmb_db_abi_selfcheck tests/abi_selfcheck.c)
target_link_libraries(gmb_db_abi_selfcheck PRIVATE gmb_db)
target_include_directories(gmb_db_abi_selfcheck PRIVATE include)
