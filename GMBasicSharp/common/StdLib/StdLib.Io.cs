// FILE: Interpreter.StdLib.Io.cs
// ReSharper disable InconsistentNaming

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Io — namespace 'IO'
 * 35 funzioni | 22 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (35):
 *   AppPath
 *   BinPath
 *   CountLines
 *   Cwd
 *   DIR
 *   DirExists
 *   EOF
 *   FClose
 *   FCreate
 *   FError
 *   FINPUT
 *   FOpen
 *   FRead
 *   FSeek
 *   FWrite
 *   FileExists
 *   FolderSize
 *   FreeFile
 *   GETTEMPDIR
 *   GETTEMPFILE
 *   GetFolderPath
 *   LOF
 *   MEMORYMAP
 *   MMAPPEEK
 *   MMAPREADBYTES
 *   NormalizePath
 *   ParentDir
 *   ProjectFolder
 *   ReadBytes
 *   ReadFile
 *   ReadLines
 *   ResolveFilePath
 *   SEEK
 *   TempDir
 *   TempFile
 * COMANDI (22):
 *   CHDIR
 *   CLOSE
 *   COPY
 *   ERASE
 *   FREAD
 *   KILL
 *   LOCK
 *   MEMORYMAP
 *   MKDIR
 *   MMAPPOKE
 *   MMAPWRITEBYTES
 *   MOVE
 *   OPEN
 *   PUT
 *   RENAME
 *   RMDIR
 *   SEEK
 *   UNLOCK
 *   UNWATCH
 *   WATCH
 *   WRITE
 *   XCOPY
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- IO NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'IO'.
    /// VERSIONE CORRETTA E DEFINITIVA: Include sia le CommandRegistration che le NativeProcedureDefinition.
    /// </summary>
    private void CreateIONamespace()
    {
        const string namespaceName = "IO";
        const SymbolFlags publicFuncFlag =
            SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags byRefFlag = SymbolFlags.IsByRef;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        // --- Registrazione dei Comandi (per sintassi legacy e dispatching) ---
        RegisterCommand(TokenKind.Open, "OPEN", namespaceName, Parser.ParseIoOpenStatement);
        RegisterCommand(TokenKind.Close, "CLOSE", namespaceName, Parser.ParseIoCloseStatement);
        RegisterCommand(TokenKind.Put, "PUT", namespaceName, Parser.ParseIoPutStatement);
        RegisterCommand(TokenKind.Copy, "COPY", namespaceName, Parser.ParseIoCopyStatement);
        RegisterCommand(TokenKind.Write, "WRITE", namespaceName, Parser.ParseWriteFileStatement);
        RegisterCommand(TokenKind.Kill, "KILL", namespaceName, Parser.ParseKillStatement);
        RegisterCommand(TokenKind.Erase, "ERASE", namespaceName,
            Parser.ParseKillStatement); // ERASE è un alias di KILL
        RegisterCommand(TokenKind.Rename, "RENAME", namespaceName, Parser.ParseRenameStatement);
        RegisterCommand(TokenKind.Move, "MOVE", namespaceName, Parser.ParseMoveStatement);
        RegisterCommand(TokenKind.XCopy, "XCOPY", namespaceName, Parser.ParseXCopyStatement);
        RegisterCommand(TokenKind.MkDir, "MKDIR", namespaceName, Parser.ParseMkDirStatement);
        RegisterCommand(TokenKind.RmDir, "RMDIR", namespaceName, Parser.ParseRmDirStatement);
        RegisterCommand(TokenKind.ChDir, "CHDIR", namespaceName, Parser.ParseChDirStatement);
        RegisterCommand(TokenKind.Seek, "SEEK", namespaceName, Parser.ParseIoSeekStatement);
        // Comando (B): FREAD handle [, count] INTO buffer — legge e SCRIVE nel buffer.
        RegisterCommand(TokenKind.FRead, "FREAD", namespaceName, Parser.ParseIoFReadIntoStatement);
        RegisterCommand(TokenKind.Lock, "LOCK", namespaceName, Parser.ParseLockStatement);
        RegisterCommand(TokenKind.Unlock, "UNLOCK", namespaceName, Parser.ParseUnlockStatement);
        RegisterCommand(TokenKind.Watch, "WATCH", namespaceName, Parser.ParseIoWatchStatement);
        RegisterCommand(TokenKind.Unwatch, "UNWATCH", namespaceName, Parser.ParseIoUnwatchStatement);

        // --- Aggiunta delle registrazioni qualificate per il dispatcher ---
        
        // CLOSE è gestito dal dispatcher polimorfico
        // GET e INPUT sono gestiti dal dispatcher polimorfico

        // --- Funzioni Native (per sintassi qualificata e funzioni che ritornano valori) ---
        var procedures = new ProcedureDefinition[]
        {
            // --- Funzioni che restituiscono un valore ---
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.EofFunc, "EOF"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false)],
                new BitType(), TokenKind.EofFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Lof, "LOF"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.Lof),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FreeFile, "FreeFile"), [], new LongType(),
                TokenKind.FreeFile),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FileInput, "FINPUT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "charCount"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false)
            ], new StringType(), TokenKind.FileInput),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.CwdFunc, "Cwd"), [], new StringType(),
                TokenKind.CwdFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FileExistsFunc, "FileExists"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new BitType(), TokenKind.FileExistsFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DirExistsFunc, "DirExists"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new BitType(), TokenKind.DirExistsFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.GetFolderPathFunc, "GetFolderPath"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "folder"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "maxDepth"), new LongType(), SymbolFlags.IsOptional, null, false)
                ],
                new StringType(), TokenKind.GetFolderPathFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.NormalizePathFunc, "NormalizePath"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.NormalizePathFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FolderSizeFunc, "FolderSize"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new QuadType(), TokenKind.FolderSizeFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.AppPathFunc, "AppPath"), [], new StringType(),
                TokenKind.AppPathFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.BinPathFunc, "BinPath"), [], new StringType(),
                TokenKind.BinPathFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ProjectFolderFunc, "ProjectFolder"), [], new StringType(),
                TokenKind.ProjectFolderFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ParentDirFunc, "ParentDir"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.ParentDirFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.GetTempFileFunc, "TempFile"), [], new StringType(),
                TokenKind.GetTempFileFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.GetTempDirFunc, "TempDir"), [], new StringType(),
                TokenKind.GetTempDirFunc),
            // Alias Clipper GETTEMPFILE/GETTEMPDIR: STESSA procedura di TempFile/TempDir, un solo
            // nome in piu' — nessun TokenKind nuovo. Dichiarazione per esteso (niente `T(...)`),
            // con i DUE TokenKind distinti ben visibili:
            //   - Name.Kind   = TokenKind CANONICO (le tabelle native generate da
            //                   tools/registry-export scrivono la riga della grafia con QUESTO
            //                   token: `{ "GETTEMPFILE", GMB_TOK_GetTempFileFunc }`);
            //   - BuiltinKind = CHIAVE DI DISPATCH (ultimo argomento), uguale su tutte le gambe.
            new NativeProcedureDefinition(
                SymbolId: -1,
                Flags: publicFuncFlag,
                Name: new Token(TokenKind.GetTempFileFunc, "GETTEMPFILE", null, SourcePosition.Unknown),
                Parameters: [],
                ReturnType: new StringType(),
                BuiltinKind: TokenKind.GetTempFileFunc),
            new NativeProcedureDefinition(
                SymbolId: -1,
                Flags: publicFuncFlag,
                Name: new Token(TokenKind.GetTempDirFunc, "GETTEMPDIR", null, SourcePosition.Unknown),
                Parameters: [],
                ReturnType: new StringType(),
                BuiltinKind: TokenKind.GetTempDirFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ResolveFilePathFunc, "ResolveFilePath"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.ResolveFilePathFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ReadBytesFunc, "ReadBytes"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new ArrayType(new ByteType()), TokenKind.ReadBytesFunc),
            // WRITEBYTES(path, bytes) — scrive un ARRAY DI BYTE su file in modo byte-esatto
            // (binario: 0x00 e 0x80..0xFF inclusi). Complementare a READBYTES; FWrite non basta
            // perché passa da fputs/UTF-8. Ritorna i byte scritti (-1 su errore).
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.WriteBytesFunc, "WriteBytes"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "bytes"), new ArrayType(new ByteType()), noFlags, null, false)
                ], new LongType(), TokenKind.WriteBytesFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ReadLinesFunc, "ReadLines"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new ArrayType(new StringType()), TokenKind.ReadLinesFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ReadFileFunc, "ReadFile"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.ReadFileFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.CountLinesFunc, "CountLines"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)],
                new LongType(), TokenKind.CountLinesFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DirFunc, "DIR"),
                [new Parameter(-1, T(TokenKind.Identifier, "pathMask"), new StringType(), noFlags, null, false)],
                new ArrayType(new StringType()), TokenKind.DirFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FOpen, "FOpen"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileName"), new StringType(), noFlags, null, false)],
                new LongType(), TokenKind.FOpen),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FCreate, "FCreate"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileName"), new StringType(), noFlags, null, false)],
                new LongType(), TokenKind.FCreate),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FClose, "FClose"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.FClose),
            // FREAD(handle [, count]) — FUNZIONE PURA (§1/§3: le funzioni non scrivono mai):
            // ritorna i dati letti (una riga se `count` è omesso, altrimenti `count` byte).
            // La scrittura in un buffer è il COMANDO `FREAD handle [, count] INTO buffer`
            // (registrato sotto), che desugara in un'assegnazione.
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FRead, "FRead"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "count"), new LongType(), optionalFlag, null, false)
            ], new StringType(), TokenKind.FRead),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FWrite, "FWrite"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)
            ], new LongType(), TokenKind.FWrite),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Seek, "SEEK"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.Seek),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FSeek, "FSeek"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "position"), new LongType(), noFlags, null, false)
            ], new LongType(), TokenKind.FSeek),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FError, "FError"), [], new LongType(),
                TokenKind.FError),

            // --- Comandi come Funzioni (restituiscono VoidType) ---
        };

        // Le costanti FILE_CREATED/FILE_DELETED/FILE_CHANGED/FILE_RENAMED si sono spostate come
        // %FILE_CREATED ecc. in CreateConstantsNamespace() (StdLib.Constants.cs) — sempre
        // disponibili senza IMPORT IO, che ora serve solo per le funzioni file I/O.
        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
    
    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'MEMORYMAP'.
    /// Include i comandi per mappare i file, scrivere singoli byte (POKE),
    /// e scrivere/leggere blocchi di byte (WRITEBYTES, READBYTES), oltre alla funzione PEEK.
    /// </summary>
    private void CreateMemoryMapNamespace()
    {
        const string namespaceName = "MEMORYMAP"; // Nuovo namespace dedicato
        const SymbolFlags publicFuncFlag =
            SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        // MEMORYMAP (es. MEMORYMAP "file.dat" AS handle)
        RegisterCommand(TokenKind.MemoryMap, "MEMORYMAP", namespaceName, Parser.ParseMemoryMapStatement);

        // MMAPPOKE (es. MMAPPOKE handle, offset, value)
        RegisterCommand(TokenKind.MemoryMapPoke, "MMAPPOKE", namespaceName, Parser.ParseMemoryMapPokeStatement);

        // MMAPWRITEBYTES (es. MMAPWRITEBYTES handle, offset, sourceArray, [count])
        RegisterCommand(TokenKind.MemoryMapWriteBytes, "MMAPWRITEBYTES", namespaceName, Parser.ParseMemoryMapWriteBytesStatement);
        
        // --- Definizione delle Funzioni Native (Expression) ---
        var procedures = new ProcedureDefinition[]
        {
            // MEMORYMAP (come funzione che restituisce un handle)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MemoryMap, "MEMORYMAP"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "filePath"), new StringType(), noFlags, null, false)
            ], new LongType(), TokenKind.MemoryMap),

            // PEEK (funzione che restituisce un singolo byte)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MemoryMapPeek, "MMAPPEEK"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "offset"), new LongType(), noFlags, null, false)
            ], new ByteType(), TokenKind.MemoryMapPeek),

            // READBYTES (funzione che restituisce un nuovo array di byte)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MemoryMapReadBytes, "MMAPREADBYTES"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "offset"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "count"), new LongType(), noFlags, null, false)
            ], new ArrayType(new ByteType()), TokenKind.MemoryMapReadBytes)
        };

        // Registra il namespace con le sue procedure e tipi.
        // Il namespace MEMORYMAP non espone tipi definiti al momento.
        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
    
}