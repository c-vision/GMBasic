#!/usr/bin/env bash
# libs_groups.sh — REGOLE DI APPARTENENZA ai gruppi di librerie native (UNICA definizione).
#
# Sorgente condivisa da:
#   tools/sh/libs_publish_gh.sh     (pubblica un gruppo da una cartella di sorgenti)
#   tools/sh/libs_build_release.sh  (compila + raccoglie + verifica + pubblica + indicizza)
#   tools/sh/libs_manifest_gh.sh    (indice: stessa classificazione dei file)
#
# PERCHÉ UN FILE SOLO. Prima la tabella gruppo→file stava dentro `libs_publish_gh.sh`: un
# secondo consumatore (il build/CI) avrebbe dovuto ricopiarla, con la certezza di divergere
# alla prima aggiunta. Qui c'è una sola `case`, e chi ne ha bisogno la chiama.
#
# GRUPPI (per DIPENDENZA, non per proprietario — vedi Knowledge/GMBasicModules-Repo-Fidati.md):
#   support   terze parti (SDL, OpenSSL, curl, freetype, zstd, …): NON nostre, sta nel bundle curato
#   engine    plumbing AI nostro: libllmcodec_*, libllmtoken, libgmb_codec_* — OGNI dylib nostra
#             li linka ⇒ sempre necessario (gruppo BASE)
#   llmmotor  motori AI: MLX (+metallib) e llama.cpp — solo per l'inferenza locale
#   core      librerie PROPRIE di GMBasic (runtime C, gfx, screen, db, doc, pkg, compress, ...)
#   ffi       shim FFI delle nostre lib di gmbsuite (amqp, grpc, kafka, mqtt, nats, protobuf,
#             media): caricate con `DECLARE … LIB "<nome>"`, mai linkate dall'eseguibile.
#             Release dedicata (`ffi-v1`), un asset per file come gli altri gruppi.
set -u

# ATTENZIONE: le regole che dipendono dalla PIATTAFORMA (MLX solo Apple, llama.cpp shared su
# Linux) devono guardare il RID di DESTINAZIONE, non la macchina che esegue lo script. Caso
# reale 2026-10-29: pubblicando un bundle linux da macOS, `uname -s` diceva Darwin e il bundle
# è uscito con 1 file. Il chiamante IMPOSTA `LIBS_RID` (publish/build) prima di chiamare.
libs_rid_is_apple() {
  case "${LIBS_RID:-}" in osx-*|maccatalyst-*) return 0;; *) return 1;; esac
}

libs_group_pick() {
  local g="$1"; shift
  local f
  for f in "$@"; do
    case "$g" in
      engine) case "$f" in
          libllmcodec_*|libllmtoken.*|libllmengine_moe_stream.*|libgmb_codec_*) echo "$f";;
          # libllm_vision_apple.* solo su Apple: presente anche altrove è innocuo (nessuno lo carica)
          libllm_vision_apple.*) libs_rid_is_apple && echo "$f";;
        esac;;
      llmmotor) case "$f" in
          # libllm_ai_llamacpp è AUTOSUFFICIENTE (llama.cpp statico dentro). libllama/libggml/
          # libmtmd servivano solo a `libgmb_ai_llamacpp`, che nessuno carica → esclusi.
          # MLX esiste SOLO su Apple (Metal): altrove il motore è llama.cpp.
          libllm_ai_mlx.*|mlx.metallib|libgmb_ai_mlx.*) libs_rid_is_apple && echo "$f";;
          libllm_ai_llamacpp.*|libgmb_ai_finetune.*) echo "$f";;
          # Linux/Windows: il build di LLMEngine/llama produce llama.cpp SHARED (su macOS è
          # statico dentro libllm_ai_llamacpp) → il bundle deve portare anche le shared di
          # llama/ggml/mtmd, altrimenti l'adapter non si carica. Su Darwin restano ORFANE.
          libllama*|libggml*|libmtmd*) libs_rid_is_apple || echo "$f";;
          libgmb_ai_moestream.*) ;;   # ORFANO: nessun dlopen/P_Invoke lo carica (verificato 2026-10-28)
        esac;;
      runtime)
        # GRUPPO "ESEGUIRE" — UNO SOLO per RID: boot + core + engine + support.
        # Direttiva 2026-10-29 ("ogni zip incorpora tutto il necessario"): i nostri runtime
        # linkano anche terze parti (SDL2, zstd, brotli, lzma, lz4, crypto/ssl), quindi con
        # gruppi separati NESSUN bundle era autoconsistente (gate libs_bundle_selfcheck).
        # `llmmotor` resta fuori (motori AI, 63 MB, solo per inferenza locale) come `dev`.
        case "$f" in
          libgmb_boot.*|libgmb_ai_llm.*|libgmb_ai_modelio.*|libgmb_tray.*|libgmb_pkg.*|libgmb_gfx.*|libgmb_screen.*|libgmb_compress.*|libgmb_grep.*|gmb_sql_*|libllmcodec_*|libllmtoken.*|libllmengine_moe_stream.*|libgmb_codec_*) echo "$f";;
          libgmb_torch*) ;;                          # wrapper torch: vive col componente `torch` (libtorch è external)
          libgmb_*|gmb_sql_*) ;;                     # nostro ma non in runtime
          libgmbamqp.*|libgmbgrpc.*|libgmbkafka.*|libgmbmqtt.*|libgmbnats.*|libgmbprotobuf.*|libgmbmedia.*) ;;  # gruppo `ffi`
          libllm*|libllama*|libggml*|libmtmd*|mlx.metallib) ;;   # engine/llmmotor gestiti sopra
          *) echo "$f";;                             # il resto = terze parti (support)
        esac;;
      runtime-core)
        # GRUPPO UNICO per "eseguire": boot + core + engine. Direttiva 2026-10-29: un bundle deve
        # chiudere le sue dipendenze, e `core` richiede `engine` (`libgmb_ai_llm` → `libllmtoken`)
        # → tenerli separati rendeva i bundle NON autoconsistenti (gate libs_bundle_selfcheck).
        # `boot` sta qui perché è la prima lib che serve (il C# la carica per risolvere le altre).
        case "$f" in
          libgmb_boot.*|libgmb_ai_llm.*|libgmb_ai_modelio.*|libgmb_torch*|libgmb_tray.*|libgmb_pkg.*|libgmb_gfx.*|libgmb_screen.*|libgmb_compress.*|libgmb_grep.*|gmb_sql_*|libllmcodec_*|libllmtoken.*|libllmengine_moe_stream.*|libgmb_codec_*) echo "$f";;
        esac;;
      ffi)
        # Shim FFI delle nostre lib (gmbsuite): caricate a runtime con `DECLARE … LIB "<nome>"`
        # → NON sono dipendenze di link, quindi NON stanno nel bundle `runtime`/`support`
        # (escluse esplicitamente lì). Un asset per file, release immutabile `ffi-v1`.
        # La `.a` di gmbmedia resta un artefatto di link locale: non si distribuisce.
        case "$f" in
          libgmbamqp.*|libgmbgrpc.*|libgmbkafka.*|libgmbmqtt.*|libgmbnats.*|libgmbprotobuf.*|libgmbmedia.dylib|libgmbmedia.so) echo "$f";;
        esac;;
      dev)
        # PAYLOAD per COMPILARE: header di terze parti (`include/…`) e le nostre statiche
        # (`lib/*.a`). Il nome è il PATH RELATIVO nella cache; l'asset è piatto con `/` → `__`.
        # Direttiva 2026-10-29: si compila senza Homebrew, scaricando questo gruppo.
        case "$f" in
          include/*|lib/*.a|lib/*.dylib) echo "$f";;
        esac;;
      boot) case "$f" in
          # libgmb_boot: il PUNTO UNICO di ricerca/download/caricamento. Piccolissima (55 KB,
          # solo libcurl+libSystem) e deve stare in OGNI installazione: è l'unica lib che il C#
          # carica direttamente (tutte le altre le carica il C su sua indicazione).
          libgmb_boot.*) echo "$f";;
        esac;;
      core) case "$f" in
          libgmb_ai_llm.*|libgmb_ai_modelio.*|libgmb_torch*|libgmb_tray.*|libgmb_pkg.*|libgmb_gfx.*|libgmb_screen.*|libgmb_compress.*|libgmb_grep.*|gmb_sql_*) echo "$f";;
        esac;;
      support) case "$f" in
          libgmb_*|gmb_sql_*|libllm*|libllama*|libggml*|libmtmd*|mlx.metallib|libtorch*|libc10*|libomp*) ;;
          libgmbamqp.*|libgmbgrpc.*|libgmbkafka.*|libgmbmqtt.*|libgmbnats.*|libgmbprotobuf.*|libgmbmedia.*) ;;  # gruppo `ffi`
          *) echo "$f";;
        esac;;
    esac
  done
}
