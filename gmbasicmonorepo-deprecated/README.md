# gmbasicmonorepo-deprecated

Materiale **dismesso** dal monorepo, conservato per tracciabilità (nessun file è stato
cancellato). Non è compilato, non è eseguito dai runner e non fa parte della superficie del
linguaggio.

## Indice

| Percorso deprecato | Sostituto | Data |
|---|---|---|
| `GMBasicSharp/common/CommonLib/Trading/General.cs` | `GMBasicSharp/common/CommonLib/Trading/General.cs` (facciata P/Invoke su `libgmbquant`) + sorgente C in `gmbsuite/gmbquant` / `shared/libs/quant` | 2026-11-07 |
| `GMBasicSharp/common/CommonLib/Trading/Metrics.cs` | `GMBasicSharp/common/CommonLib/Trading/Metrics.cs` (facciata P/Invoke su `libgmbquant`) + sorgente C in `gmbsuite/gmbquant` / `shared/libs/quant` | 2026-11-07 |
| `GMBasicSharp/common/CommonLib/Trading/Bonds.cs` | `GMBasicSharp/common/CommonLib/Trading/Bonds.cs` (facciata P/Invoke su `libgmbquant`) + sorgente C in `gmbsuite/gmbquant` / `shared/libs/quant` | 2026-11-07 |
| `GMBasicSharp/common/CommonLib/Trading/Options.cs` | `GMBasicSharp/common/CommonLib/Trading/Options.cs` (facciata P/Invoke su `libgmbquant`) + sorgente C in `gmbsuite/gmbquant` / `shared/libs/quant` | 2026-11-07 |
| `GMBasicSharp/common/StdLib/StdLib.BlockChain.cs` | pacchetto `blockchain` + `packages/blockchain/0.1.0/blockchain.inc` (dominio `BLOCKCHAIN` via FFI) | 2026-11-06 |
| `shared/minitests/blockchain/*.bas` | stessi nomi in `shared/minitests/blockchain/` riscritti sull'include FFI (array di BYTE) | 2026-11-06 |
| `shared/samples/Crypto/CryptoBlockchain.bas` | `shared/samples/Crypto/CryptoBlockchain.bas` (riscritto) | 2026-11-06 |
| `shared/samples/Crypto/CryptoWalletHD.bas` | `shared/samples/Crypto/CryptoWalletHD.bas` (riscritto) | 2026-11-06 |
| `shared/samples/Crypto/CryptoERC20.bas` | `shared/samples/Crypto/CryptoERC20.bas` (riscritto: `CONTRACT`/`IERC20` non esistono più, ABI di basso livello) | 2026-11-06 |
| `shared/samples/Crypto/CryptoFullCoverage.bas` | `shared/samples/Crypto/CryptoFullCoverage.bas` (riscritto) | 2026-11-06 |

## Trading (2026-11-07)

`CommonLib/Trading/General.cs`, `Metrics.cs`, `Bonds.cs` e `Options.cs` contenevano le
formule C# della superficie `TRADING.*` (ondate 1-2, 69 funzioni). Sono archiviate qui e
sostituite da **facade P/Invoke** (`CommonLib.Trading.General` / `Metrics` / `Bonds` /
`Options` → `QuantNative` → `libgmbquant`): la sorgente unica delle formule è ora C, in
`gmbsuite/gmbquant` con la copia compilata in `shared/libs/quant` (è una **lib**, non un
package). IL/PCodeVM usano il P/Invoke; VM/C e C99Native usano direttamente la stessa lib
C. Le funzioni non ancora migrate (`Portfolio`, `Backtest`, `QLNet`, `MadSolver`,
`PsoOptimizer`) restano C# fino all'ondata 3 e saranno archiviate qui a mano a mano.

## Perché

Il namespace `BLOCKCHAIN` era un builtin C# (24 funzioni + il tipo d'interfaccia `IERC20`).
Il dominio è stato spostato in una **libreria C** con sorgenti canonici in
`gmbsuite/gmbblockchain` e distribuito come **pacchetto** del package manager
(`packages/blockchain/0.1.0/`), perché richiede OpenSSL (`libcrypto`): un pacchetto, non una
`shared/libs`. La superficie BASIC resta identica per nome (`BLOCKCHAIN.KECCAK256`,
`BLOCKCHAIN.ETHADDRFROMPUB`, …) ma l'ABI è cambiata: **array di BYTE** al posto delle stringhe
hex, con i buffer di uscita dimensionati dal chiamante.

`CONTRACT` (proxy di smart contract) e il tipo `IERC20` **non** hanno un equivalente C: erano
tipi/parsing del linguaggio, non ABI nativa, e non sono stati portati.
| `GMBasic/native/gmb_runtime_c/src/gmb_blockchain.c` | funzioni equivalenti nel pacchetto (`packages/blockchain/0.1.0/src/`); BIP39/BIP32/secp256k1/indirizzi/ABI ora vivono lì | 2026-11-06 |
| `REPL/helps/blockchain.json` | aiuto del package manager / `blockchain.inc` (`packages/blockchain/0.1.0/README.md`) | 2026-11-06 |
| `tools/reports/manifests/C99.json` | manifest del leg **C# C99 ritirato** (non più emesso da `gmb_audit --all`, che produce solo IL/PCodeVM/VMC/C99Native): elencava `BASE32ENCODE`/`BASE58ENCODE` fra i nomi non coperti di quella superficie dismessa | 2026-11-06 |

## Superficie rimossa dai backend (2026-11-06)

Con il passaggio a pacchetto, le **implementazioni builtin** del dominio sono state rimosse da
tutte e quattro le gambe (restano solo i `TokenKind` nel lexer, che ora non hanno più un
dispatch: una chiamata al vecchio nome senza l'include FFI fallisce invece di eseguire il
builtin dismesso):

| Gamba | Rimosso |
|---|---|
| C99Native | tabella `emit_bf_blockchain` + helper `bc_*` in `gmb_c99_emit.c`/`gmb_c99_emit_builtin.c`, casi `DIM`/typing (`ABIDECODE`, `DERIVEKEY`/`CONTRACT` dict, byte-string) |
| VM/C | funzioni `gmb_bf_340/341/375/380-392` + righe della tabella di dispatch in `gmb_builtin.c` |
| Runtime C | `src/gmb_blockchain.c` (file intero) + dichiarazioni in `gmb_commonlib.h` |
| IL/PCode | wrapper `IlRuntime.{Keccak256,StrKeccak256,RipeMd160,StrRipeMd160,Blake2b256,RlpEncode,VarInt*,EthAddress*,BtcAddressFromPub,BlockDifficulty,EcdsaSecp256k1Sign,GetPublicKey,MnemonicToSeed,GenerateMnemonic,DeriveKey,Contract,AbiEncode,AbiDecode}`, entry `PCodeProcessor.Builtins`, casi dell'emitter e mapping `TokenKind → BuiltInFunction` |
