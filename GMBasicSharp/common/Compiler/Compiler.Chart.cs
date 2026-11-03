// FILE: Compiler/Compiler.Chart.cs
// Compilazione dei comandi di configurazione CHART (SET/GET/GETS) verso il runtime C grafico.
namespace Compiler;

public sealed partial class Compiler
{
    private static string ChartPropStr(Expr e) => e is StringExpr s ? (s.Value ?? "") : "";

    private void VisitChartConfigStatement(ChartConfigStmt cm)
    {
        switch (cm.SubKind)
        {
            case TokenKind.Set: VisitChartConfigSet(cm); break;
            case TokenKind.Get: VisitChartConfigGet(cm, gets: false); break;
            case TokenKind.ChartGets: VisitChartConfigGet(cm, gets: true); break;
            default:
                // Costruzioni legacy (SubKind Identifier): fallback sulla grafia risolta.
                switch (cm.Sub.ToUpperInvariant())
                {
                    case "SET": VisitChartConfigSet(cm); break;
                    case "GET": VisitChartConfigGet(cm, gets: false); break;
                    case "GETS": VisitChartConfigGet(cm, gets: true); break;
                }
                break;
        }
    }

    private void VisitChartConfigSet(ChartConfigStmt cm)
    {
        VisitExpression(cm.Handle);
        EmitConstant(ChartPropStr(cm.Prop));
        if (cm.Num != null) VisitExpression(cm.Num); else EmitConstant(0.0);
        EmitConstant(cm.Str is StringExpr ss ? (ss.Value ?? "") : "");
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_SET); EmitByte((byte)4);
    }

    private void VisitChartConfigGet(ChartConfigStmt cm, bool gets)
    {
        VisitExpression(cm.Handle);
        EmitConstant(ChartPropStr(cm.Prop));
        EmitByte((byte)OpCode.OP_SYSCALL);
        EmitSysCall(gets ? SysCall.CHART_GETS : SysCall.CHART_GET);
        EmitByte((byte)2);
        if (cm.Into != null) AssignVariable(cm.Into, isLet: false);
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
