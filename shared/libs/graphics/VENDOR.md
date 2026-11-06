# Sorgenti venduti (ricompilabile stand-alone)

La libreria è **self-contained**: `build.sh` compila solo i file in `src/` con gli header in
`include/`, senza dipendenze esterne (solo `libm`). Nessun path verso `gmbsuite` a build time.

| File | Origine |
|---|---|
| `src/gc_core.c`, `include/gc_core.h` | `gmbsuite/gmbcompute` |
| `src/gm_core.c`, `include/gm_core.h` | `gmbsuite/gmbmath-core` |
| `src/gm_vec.c`, `include/gm_vec.h` | `gmbsuite/gmbmath-vector` |
| `src/gm_color.c`, `include/gm_color.h` | `gmbsuite/gmbgfx-color` |
| `src/gm_gfx_shape.c`, `include/gm_gfx_shape.h` | `gmbsuite/gmbgfx-2d` |
| `src/gm_gfx_image.c`, `include/gm_gfx_image.h` | `gmbsuite/gmbgfx-2d` |
| `src/gm_mat.c`, `include/gm_mat.h` | `gmbsuite/gmbmath-vector` |
| `src/gm_quat.c`, `include/gm_quat.h` | `gmbsuite/gmbmath-vector` |
| `src/gm_gfx_geom.c`, `include/gm_gfx_geom.h` | `gmbsuite/gmbgfx-3d` |
| `src/gm_gfx_intersect.c`, `include/gm_gfx_intersect.h` | `gmbsuite/gmbgfx-3d` |
| `src/gm_gfx_ray.c`, `include/gm_gfx_ray.h` | `gmbsuite/gmbgfx-3d` |
| `src/gm_gfx_mesh.c`, `include/gm_gfx_mesh.h` | `gmbsuite/gmbgfx-3d` |
| `src/gm_gfx_raster.c`, `include/gm_gfx_raster.h` | `gmbsuite/gmbgfx-3d` |
| `src/gm_gfx_bitmap.c`, `include/gm_gfx_bitmap.h` | `gmbsuite/gmbgfx-2d` |
| `src/gm_gfx_surface.c`, `include/gm_gfx_surface.h` | `gmbsuite/gmbgfx-surface` |
| `src/gm_bc1.c`, `include/gm_bc1.h` | `gmbsuite/gmbgfx-codec` |
| `src/ofx_gfx.c`, `src/ofx_gfx_3d.c` | shim FFI di questo pacchetto |


Aggiornare i file qui sopra quando si ri-vendono i sorgenti dai domini `gmbsuite/gmbgfx-*`, poi ricompilare con
`bash build.sh` (output `libgmbgraphics.dylib`) e verificare con
`bash ../../../tools/sh/gmb_lib.sh check graphics`.

## Provenienza terze parti (dai commenti degli header originali)

`gm_color` da nixers-projects/urnn; `gm_gfx_shape` da RLMath e bitmap-editor;
`gm_gfx_image` da bitmap-editor, cnnvisualizer, MEMC-Net/flownet2; `gm_gfx_geom`/`gm_gfx_ray`/
`gm_gfx_mesh`/`gm_gfx_raster`/`gm_gfx_intersect` da c3dlas, SoftwareRenderer, ZINC, cub3d42;
`gm_vec`/`gm_mat`/`gm_quat`/`gm_core` dal progetto GMMath. Testi di licenza: `gmbsuite/gmbmath/THIRD_PARTY_NOTICES.md` (voce da
integrare quando si pubblica la libreria).
