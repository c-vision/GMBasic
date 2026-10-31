// FILE: Interpreter.Compiler.Compiler.VectorDb.cs
// Codegen P-Code per i comandi legacy VECTORDB (namespace database vettoriale autonomo).

namespace Compiler;

public sealed partial class Compiler
{
    private void VisitVectorDbEnsureCollectionStatement(VectorDbEnsureCollectionStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbPath);
        VisitExpression(stmt.CollectionName);
        EmitConstant(0.0); // dims default (motore usa ActiveDimensions)
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VECTORDB_ENSURE_COLLECTION); EmitByte((byte)3);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }

    private void VisitVectorDbIndexTextStatement(VectorDbIndexTextStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbPath);
        VisitExpression(stmt.CollectionName);
        VisitExpression(stmt.DocId);
        VisitExpression(stmt.Text);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VECTORDB_INDEX_TEXT); EmitByte((byte)4);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }

    private void VisitVectorDbIndexMetaStatement(VectorDbIndexMetaStmt stmt)
    {
        // Prima non c'era un argomento Text separato: MetaJson veniva spacciato per il testo
        // indicizzato e i metadata reali erano sempre "" — VECTORDB PURGE DEPRECATED (che
        // filtra per lo status nei metadata) restava un no-op silenzioso su ogni documento.
        EmitAliasOrExpression(stmt.DbPath);
        VisitExpression(stmt.CollectionName);
        VisitExpression(stmt.DocId);
        VisitExpression(stmt.Text);
        VisitExpression(stmt.MetaJson);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VECTORDB_INDEX_META); EmitByte((byte)5);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }

    private void VisitVectorDbSearchStatement(VectorDbSearchStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbPath);
        VisitExpression(stmt.CollectionName);
        VisitExpression(stmt.Query);
        VisitExpression(stmt.TopK);
        EmitConstant(1.0); // hybrid
        EmitConstant("");  // filter json (vuoto = nessun filtro)
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VECTORDB_SEARCH); EmitByte((byte)6);
        AssignVariable(stmt.Result, isLet: false);
    }

    private void VisitVectorDbCountStatement(VectorDbCountStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbPath);
        VisitExpression(stmt.CollectionName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VECTORDB_COUNT); EmitByte((byte)2);
        AssignVariable(stmt.Result, isLet: false);
    }

    private void VisitVectorDbListCollectionsStatement(VectorDbListCollectionsStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbPath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VECTORDB_LIST_COLLECTIONS); EmitByte((byte)1);
        AssignVariable(stmt.Result, isLet: false);
    }

    private void VisitVectorDbRemoveSourceStatement(VectorDbRemoveSourceStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbPath);
        VisitExpression(stmt.CollectionName);
        VisitExpression(stmt.SourceId);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VECTORDB_REMOVE_SOURCE); EmitByte((byte)3);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }

    private void VisitVectorDbLoadDocumentsStatement(VectorDbLoadDocumentsStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbPath);
        VisitExpression(stmt.CollectionName);
        VisitExpression(stmt.DirectoryPath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VECTORDB_LOAD_DOCUMENTS); EmitByte((byte)3);
        AssignVariable(stmt.Result, isLet: false);
    }

    private void VisitVectorDbPurgeDeprecatedStatement(VectorDbPurgeDeprecatedStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbPath);
        VisitExpression(stmt.CollectionName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VECTORDB_PURGE_DEPRECATED); EmitByte((byte)2);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }
}
