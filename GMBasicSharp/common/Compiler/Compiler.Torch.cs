// FILE: Interpreter.Compiler.Compiler.Torch.cs
// Codegen P-Code per i comandi legacy TORCH (sintassi a spazio, nessun valore di ritorno):
//   TORCH SET DEVICE "cpu" / TORCH LOAD LIB / TORCH UNLOAD LIB /
//   TORCH SET LIB PATH "dir" / TORCH INFO

namespace Compiler;

public sealed partial class Compiler
{
    private void VisitTorchSetDeviceStatement(TorchSetDeviceStmt stmt)
    {
        VisitExpression(stmt.Device);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TORCH_SET_DEVICE); EmitByte((byte)1);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }

    private void VisitTorchLoadLibStatement(TorchLoadLibStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TORCH_LOAD_LIB); EmitByte((byte)0);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }

    private void VisitTorchUnloadLibStatement(TorchUnloadLibStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TORCH_UNLOAD_LIB); EmitByte((byte)0);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }

    private void VisitTorchSetLibPathStatement(TorchSetLibPathStmt stmt)
    {
        VisitExpression(stmt.LibPath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TORCH_SET_LIB_PATH); EmitByte((byte)1);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }

    private void VisitTorchInfoStatement(TorchInfoStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TORCH_INFO); EmitByte((byte)0);
        EmitByte((byte)OpCode.OP_POP); // discard return value
    }
}
