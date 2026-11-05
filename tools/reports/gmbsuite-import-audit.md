# Audit import gmbsuite → GMBasicMonoRepo

Lacune esatte (sezioni 1–3): **3**

## 1. Copie migrate (storico vs vivo)

| lib/pacchetto | esito | header/def/file | dettaglio |
|---|---|---|---|
| media | OK | h old=129/new=129 · def old=215/new=215 · file old=15/new=16 | header_mancanti=0 def_mancanti=0 file_mancanti=0 alias_mancanti=0 |
| grpc | OK | h old=4/new=4 · def old=11/new=11 · file old=4/new=4 | header_mancanti=0 def_mancanti=0 file_mancanti=0 alias_mancanti=0 |
| amqp | OK | h old=9/new=9 · def old=26/new=26 · file old=4/new=4 | header_mancanti=0 def_mancanti=0 file_mancanti=0 alias_mancanti=0 |
| nats | OK | h old=8/new=8 · def old=12/new=12 · file old=4/new=4 | header_mancanti=0 def_mancanti=0 file_mancanti=0 alias_mancanti=0 |
| kafka | OK | h old=7/new=7 · def old=35/new=35 · file old=4/new=4 | header_mancanti=0 def_mancanti=0 file_mancanti=0 alias_mancanti=0 |
| mqtt | OK | h old=8/new=8 · def old=12/new=12 · file old=4/new=4 | header_mancanti=0 def_mancanti=0 file_mancanti=0 alias_mancanti=0 |
| protobuf | OK | h old=10/new=10 · def old=18/new=18 · file old=4/new=4 | header_mancanti=0 def_mancanti=0 file_mancanti=0 alias_mancanti=0 |
| netlb | OK | h old=31/new=31 · def old=97/new=97 · file old=11/new=11 | header_mancanti=0 def_mancanti=0 file_mancanti=0 alias_mancanti=0 |
| redis | GAP | h old=37/new=37 · def old=57/new=0 · file old=6/new=0 | header_mancanti=0 def_mancanti=57 file_mancanti=6 alias_mancanti=0 |

## 2. Lib FFI del monorepo — coerenza interna + binario

| lib | esito | h/def/alias | note |
|---|---|---|---|
| amqp | OK | h=9 def=26 alias=9 exported=9 | header_senza_def=0 alias_senza_def=0 alias_senza_header=0 alias_non_esportati=0 |
| aviation | OK | h=71 def=111 alias=15 exported=86 | header_senza_def=0 alias_senza_def=0 alias_senza_header=15 alias_non_esportati=0 |
| conic | OK | h=14 def=33 alias=7 exported=21 | header_senza_def=0 alias_senza_def=0 alias_senza_header=7 alias_non_esportati=0 |
| flow | OK | h=36 def=58 alias=10 exported=46 | header_senza_def=0 alias_senza_def=0 alias_senza_header=10 alias_non_esportati=0 |
| gametheory | OK | h=25 def=39 alias=5 exported=30 | header_senza_def=0 alias_senza_def=0 alias_senza_header=5 alias_non_esportati=0 |
| graph | OK | h=50 def=99 alias=33 exported=83 | header_senza_def=0 alias_senza_def=0 alias_senza_header=33 alias_non_esportati=0 |
| grpc | OK | h=4 def=11 alias=4 exported=4 | header_senza_def=0 alias_senza_def=0 alias_senza_header=0 alias_non_esportati=0 |
| kafka | OK | h=7 def=35 alias=7 exported=7 | header_senza_def=0 alias_senza_def=0 alias_senza_header=0 alias_non_esportati=0 |
| lagrangian | OK | h=5 def=3 alias=1 exported=2 | header_senza_def=4 alias_senza_def=0 alias_senza_header=1 alias_non_esportati=0 |
| localsearch | OK | h=25 def=48 alias=5 exported=28 | header_senza_def=2 alias_senza_def=0 alias_senza_header=5 alias_non_esportati=0 |
| mathprog | OK | h=28 def=49 alias=17 exported=41 | header_senza_def=6 alias_senza_def=0 alias_senza_header=17 alias_non_esportati=0 |
| media | OK | h=129 def=215 alias=54 exported=129 | header_senza_def=0 alias_senza_def=0 alias_senza_header=0 alias_non_esportati=0 |
| mqtt | OK | h=8 def=12 alias=8 exported=8 | header_senza_def=0 alias_senza_def=0 alias_senza_header=0 alias_non_esportati=0 |
| nats | OK | h=8 def=12 alias=8 exported=8 | header_senza_def=0 alias_senza_def=0 alias_senza_header=0 alias_non_esportati=0 |
| opt | OK | h=84 def=124 alias=12 exported=82 | header_senza_def=16 alias_senza_def=0 alias_senza_header=0 alias_non_esportati=0 |
| or | OK | h=126 def=282 alias=106 exported=231 | header_senza_def=1 alias_senza_def=0 alias_senza_header=106 alias_non_esportati=0 |
| protobuf | OK | h=10 def=18 alias=10 exported=10 | header_senza_def=0 alias_senza_def=0 alias_senza_header=0 alias_non_esportati=0 |

## 3. Pacchetti del monorepo

| pacchetto | esito | h/def/alias | note |
|---|---|---|---|
| avif | OK | h=8 def=8 alias=5 | header_senza_def=0 alias_senza_def=0 alias_non_esportati=0 |
| dom | OK | h=8 def=10 alias=8 | header_senza_def=0 alias_senza_def=0 alias_non_esportati=0 |
| duckdb | NO-C-SOURCE | h=15 def=0 alias=11 | header_senza_def=0 alias_senza_def=0 alias_non_esportati=0 |
| heif | OK | h=8 def=9 alias=5 | header_senza_def=0 alias_senza_def=0 alias_non_esportati=0 |
| mongo | OK | h=4 def=5 alias=4 | header_senza_def=0 alias_senza_def=0 alias_non_esportati=0 |
| netlb | OK | h=31 def=97 alias=30 | header_senza_def=0 alias_senza_def=0 alias_non_esportati=0 |
| redis | NO-C-SOURCE | h=37 def=0 alias=36 | header_senza_def=0 alias_senza_def=0 alias_non_esportati=0 |
| video | OK | h=4 def=4 alias=4 | header_senza_def=0 alias_senza_def=0 alias_non_esportati=0 |
| webp | OK | h=8 def=8 alias=5 | header_senza_def=0 alias_senza_def=0 alias_non_esportati=0 |

## 4. Inventario progetti gmbsuite (match per nome: euristico)

| progetto | header | definiti | match nome | non match |
|---|---:|---:|---:|---:|
| gmbcompute | 207 | 254 | 11 | 196 |
| gmbgeo | 29 | 45 | 0 | 29 |
| gmbgraphics | 165 | 173 | 1 | 164 |
| gmbhdf5 | 17 | 21 | 0 | 17 |
| gmbmath | 319 | 552 | 7 | 312 |
| gmbmath-core | 38 | 38 | 4 | 34 |
| gmbmath-linalg | 105 | 119 | 13 | 91 |
| gmbmath-numerical | 87 | 98 | 5 | 82 |
| gmbmath-special | 30 | 42 | 19 | 11 |
| gmbmath-vector | 189 | 191 | 0 | 189 |
| gmbneural | 247 | 306 | 0 | 247 |
| gmbortools | 395 | 589 | 297 | 98 |
| gmbscheduling | 3 | 4 | 2 | 1 |
| gmbstats | 16 | 0 | 0 | 16 |
| gmbstats-base | 48 | 57 | 0 | 48 |
| gmbstats-finance | 40 | 76 | 0 | 40 |
| gmbstats-fitting | 27 | 29 | 0 | 27 |
| gmbstats-multivariate | 28 | 56 | 1 | 27 |
| gmbstats-regression | 18 | 18 | 0 | 18 |
| gmbstats-simulation | 116 | 99 | 2 | 114 |
| gmbstats-stats | 184 | 238 | 82 | 102 |
| gmbstats-timeseries | 81 | 163 | 6 | 75 |
| gmbtranspilers | 239 | 2124 | 5 | 234 |
| betting/gmbbetting | 0 | 0 | 0 | 0 |

> Il match per nome usa registry stdlib + nomi/alias `.inc` + header lib + funzioni C del runtime. Serve al triage: la prova di completezza è nelle sezioni 1–3.
