# DBF / Clipper Test Fixtures

Golden binary fixtures for GM-Basic DBF regression tests. Dual-copy layout: **readonly** (immutable backup) and **working** (mutable copies for tests).

## Provenance & License

| Batch | Source | License |
|-------|--------|---------|
| 1 — dBase / FoxPro / VFP / codepage | [yellowfeather/DbfDataReader](https://github.com/yellowfeather/DbfDataReader/tree/main/test/fixtures) | MIT |
| 2 — shapefile DBF sidecars | [mbostock/shapefile](https://github.com/mbostock/shapefile/tree/master/test) | ISC |
| 3 — Natural Earth 110m | [nvkelso/natural-earth-vector](https://github.com/nvkelso/natural-earth-vector) | Public domain |

## Directory Layout

```
Fixtures/Dbf/
├── README.md
├── readonly/          # golden backup — never modify in tests
│   ├── dbase3/
│   ├── dbase3-memo/
│   ├── dbase4-memo/
│   ├── foxpro-memo/
│   ├── vfp/
│   ├── codepage/
│   ├── shapefile/
│   └── natural-earth/
└── working/           # mutable copies for GM-Basic tests
    └── (same subdirs)
```

## Restore from Backup

If a test corrupts a working copy, restore from readonly:

```sh
# single file
cp readonly/dbase3/dbase_03.dbf working/dbase3/dbase_03.dbf

# entire subdir
cp -R readonly/vfp/. working/vfp/

# full reset (all working copies)
for d in dbase3 dbase3-memo dbase4-memo foxpro-memo vfp codepage shapefile natural-earth; do
  cp -R "readonly/$d/." "working/$d/"
done
```

Readonly tree is protected with `chmod -R a-w readonly/`. To refresh fixtures from upstream, re-download into `readonly/`, then re-copy to `working/`.

## Fixture Catalog

| File | Version byte | Memo | Subdir | Source URL |
|------|-------------|------|--------|------------|
| dbase_03.dbf | 0x03 | — | dbase3 | [link](https://raw.githubusercontent.com/yellowfeather/DbfDataReader/main/test/fixtures/dbase_03.dbf) |
| dbase_03_summary.txt | — | — | dbase3 | golden expected output |
| dbase_83.dbf | 0x83 | DBT | dbase3-memo | [link](https://raw.githubusercontent.com/yellowfeather/DbfDataReader/main/test/fixtures/dbase_83.dbf) |
| dbase_83.dbt | — | DBT | dbase3-memo | memo companion |
| dbase_83_summary.txt | — | — | dbase3-memo | golden expected output |
| dbase_83_missing_memo.dbf | 0x83 | DBT (missing) | dbase3-memo | [link](https://raw.githubusercontent.com/yellowfeather/DbfDataReader/main/test/fixtures/dbase_83_missing_memo.dbf) |
| dbase_8b.dbf | 0x8B | DBT | dbase4-memo | [link](https://raw.githubusercontent.com/yellowfeather/DbfDataReader/main/test/fixtures/dbase_8b.dbf) |
| dbase_8b.dbt | — | DBT | dbase4-memo | memo companion |
| dbase_8b_summary.txt | — | — | dbase4-memo | golden expected output |
| dbase_f5.dbf | 0xF5 | FPT | foxpro-memo | [link](https://raw.githubusercontent.com/yellowfeather/DbfDataReader/main/test/fixtures/dbase_f5.dbf) |
| dbase_f5.fpt | — | FPT | foxpro-memo | memo companion |
| dbase_f5_summary.txt | — | — | foxpro-memo | golden expected output |
| dbase_30.dbf | 0x30 | FPT | vfp | [link](https://raw.githubusercontent.com/yellowfeather/DbfDataReader/main/test/fixtures/dbase_30.dbf) |
| dbase_30.fpt | — | FPT | vfp | memo companion |
| dbase_30_summary.txt | — | — | vfp | golden expected output |
| dbase_31.dbf | 0x31 | FPT | vfp | [link](https://raw.githubusercontent.com/yellowfeather/DbfDataReader/main/test/fixtures/dbase_31.dbf) |
| dbase_31_summary.txt | — | — | vfp | golden expected output |
| cp1251.dbf | 0x30 | — | codepage | [link](https://raw.githubusercontent.com/yellowfeather/DbfDataReader/main/test/fixtures/cp1251.dbf) |
| cp1251_summary.txt | — | — | codepage | golden expected output |
| date-property.dbf | 0x03 | — | shapefile | [link](https://raw.githubusercontent.com/mbostock/shapefile/master/test/date-property.dbf) |
| boolean-property.dbf | 0x03 | — | shapefile | [link](https://raw.githubusercontent.com/mbostock/shapefile/master/test/boolean-property.dbf) |
| mixed-properties.dbf | 0x03 | — | shapefile | [link](https://raw.githubusercontent.com/mbostock/shapefile/master/test/mixed-properties.dbf) |
| empty.dbf | 0x03 | — | shapefile | [link](https://raw.githubusercontent.com/mbostock/shapefile/master/test/empty.dbf) |
| ne_110m_admin_0_countries.dbf | 0x03 | — | natural-earth | [link](https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/110m_cultural/ne_110m_admin_0_countries.dbf) |

**Note:** `dbase_83_missing_memo_summary.txt` does not exist upstream; use `dbase_83_missing_memo.dbf` for missing-memo error-path tests only.

## Test Matrix (planned)

| Fixture subdir | Test focus | Sample |
|----------------|------------|--------|
| dbase3 | dBase III header, no memo | `ClipperDbfFixtureRead.bas` |
| dbase3-memo | DBT memo, missing memo file | TBD |
| dbase4-memo | dBase IV memo (0x8B) | TBD |
| foxpro-memo | FoxPro FPT (0xF5) | TBD |
| vfp | Visual FoxPro 0x30/0x31 + FPT | TBD |
| codepage | Windows-1251 OEM codepage | TBD |
| shapefile | ESRI sidecar DBF (minimal schema) | TBD |
| natural-earth | Real-world large record count | TBD |

Golden `*_summary.txt` files from DbfDataReader are the expected row-dump reference for read/compare tests.
