// FILE: Compiler/Compiler.Chart.cs
// Compilazione dei comandi di configurazione CHART (SET/GET/GETS) verso il runtime C grafico.
namespace Compiler;

public sealed partial class Compiler
{
    private static string ChartPropStr(Expr e) => e is StringExpr s ? (s.Value ?? "") : "";

    private void VisitChartConfigStatement(ChartConfigStmt cm)
    {
        switch (cm.Sub.ToUpperInvariant())
        {
            case "SET":
                VisitExpression(cm.Handle);
                EmitConstant(ChartPropStr(cm.Prop));
                if (cm.Num != null) VisitExpression(cm.Num); else EmitConstant(0.0);
                EmitConstant(cm.Str is StringExpr ss ? (ss.Value ?? "") : "");
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_SET); EmitByte((byte)4);
                break;
            case "GET":
                VisitExpression(cm.Handle);
                EmitConstant(ChartPropStr(cm.Prop));
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_GET); EmitByte((byte)2);
                if (cm.Into != null) AssignVariable(cm.Into, isLet: false);
                break;
            case "GETS":
                VisitExpression(cm.Handle);
                EmitConstant(ChartPropStr(cm.Prop));
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_GETS); EmitByte((byte)2);
                if (cm.Into != null) AssignVariable(cm.Into, isLet: false);
                break;
        }
    }

    private static string ChartOpStr(Expr e) => e is StringExpr s ? (s.Value ?? "") : "";

    private void VisitChartDrawStatement(ChartDrawStmt cm)
    {
        VisitExpression(cm.Handle);
        EmitConstant(ChartOpStr(cm.Op));
        VisitListLiteralExpression(new ListLiteralExpr(cm.Args, cm.Token));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_DRAW); EmitByte((byte)3);
        if (cm.Into != null) AssignVariable(cm.Into, isLet: false);
    }
}
