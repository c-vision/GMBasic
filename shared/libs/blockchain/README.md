# shared/libs/blockchain

Copia fi­sica della lib **blockchain** (dominio omonimo, dipendenza esterna OpenSSL 3).

- **Canonico**: `gmbsuite/gmbblockchain` (sorgenti + `include/` + `packaging/`).
- **Pacchetto**: `packages/blockchain/<versione>/` (artifact autonomo: `src/`, `include/`,
  `blockchain.inc`, `lib/`).
- **Questa cartella**: copia di lavoro usata dal monorepo, con la stessa struttura
  (`src/`, `include/`, `blockchain.inc`, `packaging/`).

Le tre copie devono restare **identiche** sui sorgenti: `build.sh` qui delega al pacchetto,
così la build ha una sola autorità. Se modifichi `src/`, aggiorna anche
`packages/blockchain/<versione>/src` e `gmbsuite/gmbblockchain/src`.
