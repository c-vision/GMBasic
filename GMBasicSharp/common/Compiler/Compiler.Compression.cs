// FILE: Compiler.Compression.cs — statement legacy ZIP/UNZIP.

namespace Compiler;

public sealed partial class Compiler
{
    private void VisitZipStatement(ZipStmt stmt)
    {
        VisitExpression(stmt.Source);
        VisitExpression(stmt.Destination);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.COMPRESSIONZIP);
        EmitByte((byte)2);
        EmitByte((byte)OpCode.OP_POP);
    }

    private void VisitUnzipStatement(UnzipStmt stmt)
    {
        VisitExpression(stmt.Archive);
        VisitExpression(stmt.Destination);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.COMPRESSIONUNZIP);
        EmitByte((byte)2);
        EmitByte((byte)OpCode.OP_POP);
    }
}
