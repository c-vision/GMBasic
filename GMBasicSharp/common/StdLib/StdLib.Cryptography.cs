// FILE: Interpreter.StdLib.Cryptography.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Cryptography — namespace 'CRYPTOGRAPHY'
 * 40 funzioni | 2 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (40):
 *   AESDECRYPT
 *   AESENCRYPT
 *   CHACHA20DECRYPT
 *   CHACHA20ENCRYPT
 *   CHACHA20POLY1305DECRYPT
 *   CHACHA20POLY1305ENCRYPT
 *   DECRYPTASYM
 *   ENCRYPTASYM
 *   FROMBASE64
 *   FROMBASE64URL
 *   FROMHEX
 *   HASHFILE
 *   HMACSHA256
 *   HMACSHA384
 *   HMACSHA512
 *   KEYEXPORT
 *   KEYGEN
 *   KEYIMPORT
 *   MD5
 *   PASSWORDHASH
 *   PASSWORDVERIFY
 *   RANDOMBYTES
 *   RANDOMCRYPTOBYTES
 *   RANDOMDOUBLE
 *   RANDOMINT
 *   RANDOMNORMAL
 *   SHA1
 *   SHA256
 *   SHA512
 *   SIGN
 *   STRHASHFILE
 *   STRMD5
 *   STRSHA1
 *   STRSHA256
 *   STRSHA512
 *   TOBASE64
 *   TOBASE64URL
 *   TOHEX
 *   VERIFY
 *   VERIFYRAW
 * COMANDI (2):
 *   KEYCLOSE
 *   KEYGEN
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- CRYPTOGRAPHY NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'CRYPTOGRAPHY'.
    /// </summary>
    private void CreateCryptographyNamespace()
    {
        const string namespaceName = "CRYPTOGRAPHY";
        var byteArrayType = new ArrayType(new ByteType());
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;
        const SymbolFlags
            publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;

        RegisterCommand(TokenKind.KeyGen, "KEYGEN", namespaceName, Parser.ParseKeyGenStatement);

        RegisterCommand(TokenKind.KeyClose, "KEYCLOSE", namespaceName, Parser.ParseKeyCloseStatement);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Md5, "MD5"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                byteArrayType,
                TokenKind.Md5, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sha1, "SHA1"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                byteArrayType,
                TokenKind.Sha1, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sha256, "SHA256"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                byteArrayType,
                TokenKind.Sha256, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sha512, "SHA512"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                byteArrayType,
                TokenKind.Sha512, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HashFile, "HASHFILE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "filePath"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "algorithm"), new StringType(), optionalFlag, null, false)
            ], byteArrayType, TokenKind.HashFile),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrMd5, "STRMD5"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.StrMd5, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrSha1, "STRSHA1"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.StrSha1, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrSha256, "STRSHA256"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.StrSha256, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrSha512, "STRSHA512"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.StrSha512, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrHashFile, "STRHASHFILE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "filePath"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "algorithm"), new StringType(), optionalFlag, null, false)
            ], new StringType(), TokenKind.StrHashFile),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AesEncrypt, "AESENCRYPT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "key"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "iv"), new InferredType(), optionalFlag, null, false)
            ], byteArrayType, TokenKind.AesEncrypt),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AesDecrypt, "AESDECRYPT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "encryptedData"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "key"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "iv"), new InferredType(), optionalFlag, null, false)
            ], byteArrayType, TokenKind.AesDecrypt, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ChaCha20Encrypt, "CHACHA20ENCRYPT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "key"), byteArrayType, noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nonce"), byteArrayType, noFlags, null, false)
            ], byteArrayType, TokenKind.ChaCha20Encrypt, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ChaCha20Decrypt, "CHACHA20DECRYPT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "key"), byteArrayType, noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nonce"), byteArrayType, noFlags, null, false)
            ], byteArrayType, TokenKind.ChaCha20Decrypt, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag,
                T(TokenKind.ChaCha20Poly1305Encrypt, "CHACHA20POLY1305ENCRYPT"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "plaintext"), new InferredType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "key"), byteArrayType, noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "nonce"), byteArrayType, noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "aad"), byteArrayType, optionalFlag, null, false)
                ], new DictType(new StringType(), byteArrayType), TokenKind.ChaCha20Poly1305Encrypt, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag,
                T(TokenKind.ChaCha20Poly1305Decrypt, "CHACHA20POLY1305DECRYPT"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "ciphertext"), byteArrayType, noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "tag"), byteArrayType, noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "key"), byteArrayType, noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "nonce"), byteArrayType, noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "aad"), byteArrayType, optionalFlag, null, false)
                ], byteArrayType, TokenKind.ChaCha20Poly1305Decrypt, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.KeyExport, "KEYEXPORT"),
                [new Parameter(-1, T(TokenKind.Identifier, "keyHandle"), new LongType(), noFlags, null, false)],
                new StringType(), TokenKind.KeyExport),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.KeyImport, "KEYIMPORT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "pemData"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "keyHandle"), new LongType(), noFlags, null, false)
            ], new LongType(), TokenKind.KeyImport),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sign, "SIGN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "privateKeyHandle"), new LongType(), noFlags, null, false)
            ], byteArrayType, TokenKind.Sign),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Verify, "VERIFY"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "signature"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "originalData"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "publicKeyHandle"), new LongType(), noFlags, null, false)
            ], new BitType(), TokenKind.Verify),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VerifyRaw, "VERIFYRAW"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "signature"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "originalData"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "publicKeyB64"), new StringType(), noFlags, null, false)
            ], new BitType(), TokenKind.VerifyRaw),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.EncryptAsym, "ENCRYPTASYM"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "publicKeyHandle"), new LongType(), noFlags, null, false)
            ], byteArrayType, TokenKind.EncryptAsym),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DecryptAsym, "DECRYPTASYM"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "encryptedData"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "privateKeyHandle"), new LongType(), noFlags, null, false)
            ], byteArrayType, TokenKind.DecryptAsym),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RandomBytes, "RANDOMBYTES"),
                [new Parameter(-1, T(TokenKind.Identifier, "count"), new LongType(), noFlags, null, false)],
                byteArrayType,
                TokenKind.RandomBytes),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ToBase64, "TOBASE64"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.ToBase64, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FromBase64, "FROMBASE64"),
                [new Parameter(-1, T(TokenKind.Identifier, "base64String"), new StringType(), noFlags, null, false)],
                byteArrayType, TokenKind.FromBase64, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ToHex, "TOHEX"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.ToHex, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FromHex, "FROMHEX"),
                [new Parameter(-1, T(TokenKind.Identifier, "hexString"), new StringType(), noFlags, null, false)],
                byteArrayType, TokenKind.FromHex, ExecutionKind: NativeExecutionKind.Pure),

            // --- Security (2026-09-02): HMAC (hex string) + base64url (JWT). Input bytes
            // (data, key) come InferredType (string/list/array→bytes, convenzione CRYPTOGRAPHY);
            // HMACSHA* → hex STRING, TOBASE64URL → STRING, FROMBASE64URL → byte[]. ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HmacSha256, "HMACSHA256"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "key"), new InferredType(), noFlags, null, false)
                ], new StringType(), TokenKind.HmacSha256, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HmacSha384, "HMACSHA384"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "key"), new InferredType(), noFlags, null, false)
                ], new StringType(), TokenKind.HmacSha384, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HmacSha512, "HMACSHA512"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "key"), new InferredType(), noFlags, null, false)
                ], new StringType(), TokenKind.HmacSha512, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ToBase64Url, "TOBASE64URL"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.ToBase64Url, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FromBase64Url, "FROMBASE64URL"),
                [new Parameter(-1, T(TokenKind.Identifier, "base64urlString"), new StringType(), noFlags, null, false)],
                byteArrayType, TokenKind.FromBase64Url, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RandomInt, "RANDOMINT"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "min"), new IntegerType(), optionalFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "max"), new IntegerType(), noFlags, null, false)
                ],
                new IntegerType(),
                TokenKind.RandomInt),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RandomDouble, "RANDOMDOUBLE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "min"), new DoubleType(), optionalFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "max"), new DoubleType(), optionalFlag, null, false)
                ],
                new DoubleType(),
                TokenKind.RandomDouble),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RandomNormal, "RANDOMNORMAL"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "mean"), new DoubleType(), optionalFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "stddev"), new DoubleType(), optionalFlag, null, false)
                ],
                new DoubleType(),
                TokenKind.RandomNormal),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RandomCryptoBytes, "RANDOMCRYPTOBYTES"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "count"), new IntegerType(), noFlags, null, false)
                ],
                byteArrayType,
                TokenKind.RandomCryptoBytes),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.KeyGen, "KEYGEN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "algorithm"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "keySize"), new IntegerType(), optionalFlag, null, false)
            ], new ArrayType(new LongType()), TokenKind.KeyGen),

            // Hashing di PASSWORD (ex namespace PASSWORD, fuso in CRYPTOGRAPHY 2026-08-28):
            // PASSWORDHASH → hash PBKDF2 (stringa); PASSWORDVERIFY → verifica (BIT).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PasswordHash, "PASSWORDHASH"),
                [new Parameter(-1, T(TokenKind.Identifier, "password"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.PasswordHash),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PasswordVerify, "PASSWORDVERIFY"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "password"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "hash"), new StringType(), noFlags, null, false)
            ], new BitType(), TokenKind.PasswordVerify, ExecutionKind: NativeExecutionKind.Pure),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }

    // ========================================================================
    // --- (ex PASSWORD NAMESPACE fuso in CRYPTOGRAPHY 2026-08-28) ---
    // ========================================================================
}