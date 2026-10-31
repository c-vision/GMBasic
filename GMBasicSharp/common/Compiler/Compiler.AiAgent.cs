// FILE: Interpreter.Compiler.AiAgent.cs

namespace Compiler;

public sealed partial class Compiler
{
    private void VisitCreateAgentStatement(CreateAgentStmt stmt)
    {
        // CREATE AGENT name → AIAGENT.CREATE(name)
        EmitConstant(stmt.AgentName.Lexeme);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.AIAGENTCREATE);
        EmitByte(1);
        EmitByte((byte)OpCode.OP_POP);
    }

    private void VisitDeleteAgentStatement(DeleteAgentStmt stmt)
    {
        // DELETE AGENT name → AIAGENT.DELETE(name)
        EmitConstant(stmt.AgentName.Lexeme);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.AIAGENTDELETE);
        EmitByte(1);
        EmitByte((byte)OpCode.OP_POP);
    }

    private void VisitSaveAgentStatement(SaveAgentStmt stmt)
    {
        // SAVE AGENT name → AIAGENT.SAVE(name, name + ".agent.json")
        EmitConstant(stmt.AgentName.Lexeme);
        EmitConstant(stmt.AgentName.Lexeme + ".agent.json");
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.AIAGENTSAVE);
        EmitByte(2);
        EmitByte((byte)OpCode.OP_POP);
    }

    private void VisitLoadAgentStatement(LoadAgentStmt stmt)
    {
        // LOAD AGENT name → AIAGENT.LOAD(name, name + ".agent.json")
        EmitConstant(stmt.AgentName.Lexeme);
        EmitConstant(stmt.AgentName.Lexeme + ".agent.json");
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.AIAGENTLOAD);
        EmitByte(2);
        EmitByte((byte)OpCode.OP_POP);
    }
}
