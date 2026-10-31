// FILE: Interpreter.StdLib.BlockChain.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: BlockChain — namespace 'BLOCKCHAIN'
 * 24 funzioni | 0 comandi | 1 tipi | 0 costanti
 *
 * FUNZIONI (24):
 *   ABIDECODE
 *   ABIENCODE
 *   BASE32DECODE
 *   BASE32ENCODE
 *   BASE58DECODE
 *   BASE58ENCODE
 *   BLAKE2B256
 *   BLOCKDIFFICULTY
 *   BTCADDRFROMPUB
 *   CONTRACT
 *   DERIVEKEY
 *   ETHADDRCHECKSUM
 *   ETHADDRFROMPUB
 *   GENERATEMNEMONIC
 *   GETPUBLICKEY
 *   KECCAK256
 *   MNEMONICTOSEED
 *   RIPEMD160
 *   RLPENCODE
 *   SECP256K1SIGN
 *   STRKECCAK256
 *   STRRIPEMD160
 *   VARINTDECODE
 *   VARINTENCODE
 * TIPI (1):
 *   IERC20
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- BLOCKCHAIN NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'BLOCKCHAIN'.
    /// Contiene funzioni per hashing, encoding e utility specifiche per blockchain come Bitcoin ed Ethereum.
    /// </summary>
    private void CreateBlockchainNamespace()
    {
        const string namespaceName = "BLOCKCHAIN";
        var byteArrayType = new ArrayType(new ByteType());
        var noFlags = SymbolFlags.None;
        var optionalFlag = SymbolFlags.IsOptional;
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;
        const SymbolFlags paramArrayFlag = SymbolFlags.IsParamArray;

        // Flag per i metodi di interfaccia: Pubblici, Astratti (senza corpo), Virtuali
        const SymbolFlags interfaceMethodFlags =
            SymbolFlags.IsMethod |
            SymbolFlags.IsPublic |
            SymbolFlags.IsAbstract |
            SymbolFlags.IsVirtual;

        var procedures = new ProcedureDefinition[]
        {
            // --- Funzioni Hashing di Base ---
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Keccak256, "KECCAK256"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                byteArrayType, TokenKind.Keccak256),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.StrKeccak256, "STRKECCAK256"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.StrKeccak256),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.RipeMd160, "RIPEMD160"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                byteArrayType, TokenKind.RipeMd160),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.StrRipeMd160, "STRRIPEMD160"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.StrRipeMd160),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Blake2b256, "BLAKE2B256"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                byteArrayType, TokenKind.Blake2b256),

            // --- Encoding/Decoding (Base58, Base32, RLP, VarInt) ---
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.Base58Encode, "BASE58ENCODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new ByteType(), arrayFlag, null, false)],
                new StringType(), TokenKind.Base58Encode),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.Base58Decode, "BASE58DECODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "base58_string"), new StringType(), noFlags, null, false)],
                byteArrayType, TokenKind.Base58Decode),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.Base32Encode, "BASE32ENCODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new ByteType(), arrayFlag, null, false)],
                new StringType(), TokenKind.Base32Encode),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.Base32Decode, "BASE32DECODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "base32_string"), new StringType(), noFlags, null, false)],
                byteArrayType, TokenKind.Base32Decode),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.RlpEncode, "RLPENCODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                byteArrayType, TokenKind.RlpEncode),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.VarIntEncode, "VARINTENCODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new LongType(), noFlags, null, false)],
                byteArrayType, TokenKind.VarIntEncode),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.VarIntDecode, "VARINTDECODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new ByteType(), arrayFlag, null, false)],
                new LongType(), TokenKind.VarIntDecode),

            // --- Utility Ethereum ---
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.EthAddressFromPub, "ETHADDRFROMPUB"),
                [new Parameter(-1, T(TokenKind.Identifier, "public_key"), new ByteType(), arrayFlag, null, false)],
                new StringType(), TokenKind.EthAddressFromPub),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.EthAddressChecksum, "ETHADDRCHECKSUM"),
                [new Parameter(-1, T(TokenKind.Identifier, "address"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.EthAddressChecksum),

            // --- Utility Bitcoin ---
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.BtcAddressFromPub, "BTCADDRFROMPUB"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "public_key"), new ByteType(), arrayFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "is_testnet"), new BitType(), optionalFlag, null, false)
                ], new StringType(), TokenKind.BtcAddressFromPub),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.BlockDifficulty, "BLOCKDIFFICULTY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "header"), new ByteType(), arrayFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "bits"), new LongType(), noFlags, null, false)
                ], new DoubleType(), TokenKind.BlockDifficulty),

            // --- Crypto Primitives (Firme e Chiavi) ---
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.EcdsaSecp256k1Sign, "SECP256K1SIGN"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "message_hash"), new ByteType(), arrayFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "private_key"), new ByteType(), arrayFlag, null, false)
                ], byteArrayType, TokenKind.EcdsaSecp256k1Sign),

            // --- Funzioni HD Wallet (BIP32/BIP39/BIP44) ---
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.GenerateMnemonic, "GENERATEMNEMONIC"),
                [new Parameter(-1, T(TokenKind.Identifier, "wordCount"), new IntegerType(), optionalFlag, null, false)],
                new StringType(), TokenKind.GenerateMnemonic),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.MnemonicToSeed, "MNEMONICTOSEED"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "mnemonic"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "passphrase"), new StringType(), optionalFlag, null,
                        false)
                ], byteArrayType, TokenKind.MnemonicToSeed),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.DeriveKey, "DERIVEKEY"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "seed"), new ByteType(), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)
            ], new DictType(new StringType(), byteArrayType), TokenKind.DeriveKey),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.GetPublicKey, "GETPUBLICKEY"),
                [new Parameter(-1, T(TokenKind.Identifier, "privateKey"), new ByteType(), arrayFlag, null, false)],
                byteArrayType, TokenKind.GetPublicKey),

            // ============================================================================================
            // --- SMART CONTRACT INTERACTION (TODO #53) ---
            // ============================================================================================

            // <summary>
            // Crea un PROXY DINAMICO per interagire con uno Smart Contract su una blockchain EVM.
            // </summary>
            // <param name="address">L'indirizzo del contratto (es. "0x...")</param>
            // <param name="interfaceName">Il nome dell'INTERFACCIA definita nel codice BASIC (es. "IERC20")</param>
            // <param name="rpcUrl">L'URL del nodo RPC (opzionale, usa default se omesso)</param>
            // <param name="privateKey">La chiave privata per firmare le transazioni di scrittura (opzionale)</param>
            // <returns>Un oggetto dinamico (AnyType) che implementa l'interfaccia specificata.</returns>
            // <remarks>
            // LOGICA PER L'EXECUTOR:
            // 1. Cercare 'interfaceName' nella SymbolTable per ottenere la definizione dell'InterfaceDefinition.
            // 2. Creare un oggetto runtime speciale 'SmartContractProxy' che intercetta tutte le chiamate ai metodi.
            // 3. Quando l'utente chiama un metodo del proxy (es. token.BalanceOf(addr)):
            //    a. Calcolare il selettore della funzione (primi 4 byte di Keccak256 della firma, es. "balanceOf(address)").
            //    b. Codificare gli argomenti secondo lo standard ABI EVM.
            //    c. Se il metodo è una VIEW/PURE (lettura): Eseguire 'eth_call' via RPC.
            //    d. Se il metodo modifica lo stato (scrittura): Costruire la transazione, firmarla con 'privateKey' e inviarla via 'eth_sendRawTransaction'.
            //    e. Decodificare il risultato esadecimale restituito dall'RPC nel tipo di ritorno definito nell'interfaccia.
            // 4. Mappatura Tipi:
            //    - GM-BASIC STRING -> EVM address, string, uint256 (per evitare overflow)
            //    - GM-BASIC INTEGER -> EVM uint8, int8, uint16, int16
            //    - GM-BASIC LONG -> EVM uint32, int32
            //    - GM-BASIC UQUAD -> EVM uint64
            //    - GM-BASIC BIT -> EVM bool
            //    - GM-BASIC ARRAY OF BYTE -> EVM bytes
            // </remarks>
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Contract, "CONTRACT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "address"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "interfaceName"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "rpcUrl"), new StringType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "privateKey"), new StringType(), optionalFlag, null, false)
            ], new InferredType(), TokenKind.Contract),

            // <summary>
            // Codifica argomenti in formato ABI (Application Binary Interface) per chiamate raw.
            // </summary>
            // <param name="signature">La firma del metodo (es. "transfer(address,uint256)")</param>
            // <param name="args">Lista variadica di argomenti da codificare.</param>
            // <returns>Una stringa esadecimale (es. "0xa9059cbb...") che rappresenta il payload dati.</returns>
            // <remarks>
            // LOGICA PER L'EXECUTOR:
            // 1. Calcolare l'hash della firma (Method ID).
            // 2. Iterare su 'args' e codificare ciascuno secondo il tipo specificato nella firma.
            // 3. Concatenare Method ID e parametri codificati (padding a 32 byte).
            // </remarks>
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.AbiEncode, "ABIENCODE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "signature"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "args"), new ArrayType(new InferredType()),
                    arrayFlag | paramArrayFlag, null, false)
            ], new StringType(), TokenKind.AbiEncode),

            // <summary>
            // Decodifica una stringa esadecimale ABI in un array di valori BASIC.
            // </summary>
            // <param name="data">La stringa esadecimale dei dati (es. output di una eth_call).</param>
            // <param name="types">Array di stringhe che descrivono i tipi attesi (es. ["uint256", "address"]).</param>
            // <returns>Un array di valori decodificati.</returns>
            // <remarks>
            // LOGICA PER L'EXECUTOR:
            // 1. Rimuovere prefisso '0x' se presente.
            // 2. Decodificare lo stream di byte secondo i tipi specificati in 'types'.
            // 3. Convertire i valori EVM nei corrispondenti tipi BASIC (BigInt -> String, Address -> String, Bool -> Bit).
            // </remarks>
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.AbiDecode, "ABIDECODE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "types"), new ArrayType(new StringType()), arrayFlag, null,
                    false)
            ], new ArrayType(new InferredType()), TokenKind.AbiDecode)
        };

        // ============================================================================================
        // --- DEFINIZIONE INTERFACCE STANDARD (ERC20) ---
        // ============================================================================================

        var erc20Members = ImmutableArray.CreateBuilder<ClassMember>();

        // 1. TotalSupply() AS STRING (uint256)
        erc20Members.Add(new MethodMember(
            new FunctionDefinition(-1, T(TokenKind.Identifier, "TotalSupply"), [], new StringType(), [],
                interfaceMethodFlags),
            AccessModifier.Public
        ));

        // 2. BalanceOf(account AS STRING) AS STRING (uint256)
        erc20Members.Add(new MethodMember(
            new FunctionDefinition(-1, T(TokenKind.Identifier, "BalanceOf"),
                [new Parameter(-1, T(TokenKind.Identifier, "account"), new StringType(), noFlags, null, false)],
                new StringType(), [], interfaceMethodFlags),
            AccessModifier.Public
        ));

        // 3. Transfer(to AS STRING, amount AS STRING) AS BIT (bool)
        erc20Members.Add(new MethodMember(
            new FunctionDefinition(-1, T(TokenKind.Identifier, "Transfer"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "to"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "amount"), new StringType(), noFlags, null, false)
                ],
                new BitType(), [], interfaceMethodFlags),
            AccessModifier.Public
        ));

        // 4. Allowance(owner AS STRING, spender AS STRING) AS STRING (uint256)
        erc20Members.Add(new MethodMember(
            new FunctionDefinition(-1, T(TokenKind.Identifier, "Allowance"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "owner"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "spender"), new StringType(), noFlags, null, false)
                ],
                new StringType(), [], interfaceMethodFlags),
            AccessModifier.Public
        ));

        // 5. Approve(spender AS STRING, amount AS STRING) AS BIT (bool)
        erc20Members.Add(new MethodMember(
            new FunctionDefinition(-1, T(TokenKind.Identifier, "Approve"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "spender"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "amount"), new StringType(), noFlags, null, false)
                ],
                new BitType(), [], interfaceMethodFlags),
            AccessModifier.Public
        ));

        // 6. TransferFrom(from AS STRING, to AS STRING, amount AS STRING) AS BIT (bool)
        erc20Members.Add(new MethodMember(
            new FunctionDefinition(-1, T(TokenKind.Identifier, "TransferFrom"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "from"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "to"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "amount"), new StringType(), noFlags, null, false)
                ],
                new BitType(), [], interfaceMethodFlags),
            AccessModifier.Public
        ));

        // 7. Name() AS STRING
        erc20Members.Add(new MethodMember(
            new FunctionDefinition(-1, T(TokenKind.Identifier, "Name"), [], new StringType(), [], interfaceMethodFlags),
            AccessModifier.Public
        ));

        // 8. Symbol() AS STRING
        erc20Members.Add(new MethodMember(
            new FunctionDefinition(-1, T(TokenKind.Identifier, "Symbol"), [], new StringType(), [],
                interfaceMethodFlags),
            AccessModifier.Public
        ));

        // 9. Decimals() AS INTEGER (uint8)
        erc20Members.Add(new MethodMember(
            new FunctionDefinition(-1, T(TokenKind.Identifier, "Decimals"), [], new IntegerType(), [],
                interfaceMethodFlags),
            AccessModifier.Public
        ));

        var ierc20Def = new InterfaceDefinition(-1, T(TokenKind.Identifier, "IERC20"),
            SymbolFlags.IsPublic | SymbolFlags.IsInterface,
            erc20Members.ToImmutable());

        // --- Registrazione dei Tipi del Namespace ---
        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "IERC20", new InterfaceType(ierc20Def) }
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}