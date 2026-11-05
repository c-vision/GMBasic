# Sorgenti venduti (ricompilabile stand-alone)

La libreria è **self-contained**: `build.sh` compila solo i file in `src/` con gli header in
`include/`, senza dipendenze esterne (solo `libm`). Nessun path verso `gmbsuite` a build time.

| File | Origine |
|---|---|
| `src/gc_core.c`, `include/gc_core.h` | `gmbsuite/gmbcompute` |
| `src/gm_core.c`, `include/gm_core.h` | `gmbsuite/gmbmath-core` |
| `src/gm_vec.c`, `include/gm_vec.h` | `gmbsuite/gmbmath-vector` |
| `src/gm_color.c`, `include/gm_color.h` | `gmbsuite/gmbgraphics` |
| `src/gm_gfx_shape.c`, `include/gm_gfx_shape.h` | `gmbsuite/gmbgraphics` |
| `src/gm_gfx_image.c`, `include/gm_gfx_image.h` | `gmbsuite/gmbgraphics` |
| `src/ofx_gfx.c` | shim FFI di questo pacchetto (Alessandro/GMBasic) |

Aggiornare i file qui sopra quando si ri-vendono i sorgenti da gmbsuite, poi ricompilare con
`bash build.sh` (output `libgmbgraphics.dylib`) e verificare con
`bash ../../../tools/sh/gmb_lib.sh check graphics`.

## Provenienza terze parti (dai commenti degli header originali)

`gm_color` da nixers-projects/urnn; `gm_gfx_shape` da RLMath e bitmap-editor;
`gm_gfx_image` da bitmap-editor, cnnvisualizer, MEMC-Net/flownet2; `gm_vec`/`gm_core` dal
progetto GMMath. Testi di licenza: `gmbsuite/gmbmath/THIRD_PARTY_NOTICES.md` (voce da
integrare quando si pubblica la libreria).
