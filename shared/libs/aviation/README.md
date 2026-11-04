# gmbaviation — dominio aviation (namespace BASIC `OPTIMIZE`)

**Prefisso C:** `gor_` · **Dipendenze:** C99 + libm (self-contained)

Revenue management, fleet/crew/tail/slot assignment, recovery, ULD, hub/Steiner.
Motore `gmbortools` (`gor_aviation`, `gor_aviation2`, `gor_assign`, `gor_cover`,
`gor_pack`, `gor_graph2`), copiato qui. Binding: `shared/include/aviation.inc`.

## Superficie (15)
| Gruppo | Funzioni |
|---|---|
| Revenue | `AVEMSRB`, `AVOVERBOOKING`, `AVBIDPRICE` |
| Assegnazioni | `AVFLEET`, `AVCREWPAIRING`, `AVTAILASSIGNMENT`, `AVSLOT`, `AVRECOVERY` |
| ULD | `AVULD` |
| Avanzato | `AVAIRCRAFTROUTING`, `AVCREWRESCHED`, `AVHUB`, `AVSTEINER`, `AVTAILCONTINUITY`, `AVCOLUMNGEN` |

## Build
```sh
shared/libs/aviation/build.sh
codesign --force --sign - shared/libs/aviation/libgmbaviation.dylib
```
