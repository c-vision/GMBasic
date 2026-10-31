// FILE: Interpreter.Linker.Optimization.ConstantFolding.cs

using System.Globalization;

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// Pass di ottimizzazione AST: Constant Folding e Semplificazione delle Espressioni.
    /// Valuta a compile-time le espressioni con operandi costanti letterali (es. 1 + 2 -> 3, "A" + "B" -> "AB", TRUE AND TRUE -> TRUE).
    /// </summary>
    public static class ConstantFoldingOptimizer
    {
        public static Stmt FoldStatement(Stmt statement)
        {
            return statement switch
            {
                IfStmt ifStmt => FoldIfStatement(ifStmt),
                ExpressionStmt exprStmt => exprStmt with { Expression = FoldExpression(exprStmt.Expression) },
                LetStmt letStmt => letStmt with
                {
                    Assignments = [.. letStmt.Assignments.Select(a => a with { Value = FoldExpression(a.Value) })]
                },
                PrintStmt printStmt => printStmt with
                {
                    Arguments = [.. printStmt.Arguments.Select(arg => arg.Expression != null ? arg with { Expression = FoldExpression(arg.Expression) } : arg)]
                },
                ReturnStmt returnStmt => returnStmt.Value != null ? returnStmt with { Value = FoldExpression(returnStmt.Value) } : returnStmt,
                ForStmt forStmt => forStmt with
                {
                    Start = FoldExpression(forStmt.Start),
                    End = FoldExpression(forStmt.End),
                    Step = forStmt.Step != null ? FoldExpression(forStmt.Step) : null
                },
                _ => statement
            };
        }

        public static Expr FoldExpression(Expr expression)
        {
            switch (expression)
            {
                case UnaryExpr unary:
                    var foldedOperand = FoldExpression(unary.Right);
                    return EvaluateUnary(unary.Op, foldedOperand, unary.Position);

                case BinaryExpr binary:
                    var leftFolded = FoldExpression(binary.Left);
                    var rightFolded = FoldExpression(binary.Right);
                    return EvaluateBinary(leftFolded, binary.Op, rightFolded, binary.Position);

                case GroupingExpr group:
                    return FoldExpression(group.Expression);

                default:
                    return expression;
            }
        }

        private static Stmt FoldIfStatement(IfStmt ifStmt)
        {
            var foldedClauses = new List<IfClause>();
            foreach (var clause in ifStmt.IfClauses)
            {
                var foldedCond = FoldExpression(clause.Condition);
                var foldedStmts = clause.Statements.Select(FoldStatement).ToList();
                foldedClauses.Add(clause with { Condition = foldedCond, Statements = [.. foldedStmts] });
            }

            ElseClause? foldedElse = null;
            if (ifStmt.ElseClause != null)
            {
                foldedElse = ifStmt.ElseClause with { Statements = [.. ifStmt.ElseClause.Statements.Select(FoldStatement)] };
            }

            return ifStmt with { IfClauses = [.. foldedClauses], ElseClause = foldedElse };
        }

        private static Expr EvaluateUnary(Token op, Expr operand, SourcePosition pos)
        {
            if (operand is NumberExpr numLit)
            {
                if (op.Kind == TokenKind.Minus)
                {
                    var val = -numLit.Value;
                    var tok = new Token(TokenKind.Number, val.ToString(CultureInfo.InvariantCulture), val, pos);
                    return new NumberExpr(val, tok);
                }
                if (op.Kind == TokenKind.Plus)
                    return numLit;
            }
            else if (operand is BooleanExpr boolLit)
            {
                if (op.Kind == TokenKind.Not)
                {
                    var val = !boolLit.Value;
                    var tok = new Token(val ? TokenKind.True : TokenKind.False, val ? "TRUE" : "FALSE", val, pos);
                    return new BooleanExpr(val, tok);
                }
            }

            return new UnaryExpr(op, operand);
        }

        private static Expr EvaluateBinary(Expr left, Token op, Expr right, SourcePosition pos)
        {
            // Numbers (integers or doubles)
            if (left is NumberExpr lNum && right is NumberExpr rNum)
            {
                switch (op.Kind)
                {
                    case TokenKind.Plus:
                        return MakeNumber(lNum.Value + rNum.Value, pos);
                    case TokenKind.Minus:
                        return MakeNumber(lNum.Value - rNum.Value, pos);
                    case TokenKind.Multiply:
                        return MakeNumber(lNum.Value * rNum.Value, pos);
                    case TokenKind.Divide when rNum.Value != 0:
                        return MakeNumber(lNum.Value / rNum.Value, pos);
                    case TokenKind.Equal:
                        return MakeBoolean(Math.Abs(lNum.Value - rNum.Value) < 1e-12, pos);
                    case TokenKind.NotEqual:
                        return MakeBoolean(Math.Abs(lNum.Value - rNum.Value) >= 1e-12, pos);
                    case TokenKind.LessThan:
                        return MakeBoolean(lNum.Value < rNum.Value, pos);
                    case TokenKind.LessEqual:
                        return MakeBoolean(lNum.Value <= rNum.Value, pos);
                    case TokenKind.GreaterThan:
                        return MakeBoolean(lNum.Value > rNum.Value, pos);
                    case TokenKind.GreaterEqual:
                        return MakeBoolean(lNum.Value >= rNum.Value, pos);
                }
            }

            // Strings concatenation
            if (left is StringExpr lStr && right is StringExpr rStr && op.Kind == TokenKind.Plus)
            {
                var val = lStr.Value + rStr.Value;
                var tok = new Token(TokenKind.StringLiteral, $"\"{val}\"", val, pos);
                return new StringExpr(val, tok);
            }

            // Booleans (AND / OR)
            if (left is BooleanExpr lBool && right is BooleanExpr rBool)
            {
                switch (op.Kind)
                {
                    case TokenKind.And:
                        return MakeBoolean(lBool.Value && rBool.Value, pos);
                    case TokenKind.Or:
                        return MakeBoolean(lBool.Value || rBool.Value, pos);
                    case TokenKind.Equal:
                        return MakeBoolean(lBool.Value == rBool.Value, pos);
                    case TokenKind.NotEqual:
                        return MakeBoolean(lBool.Value != rBool.Value, pos);
                }
            }

            return new BinaryExpr(left, op, right);
        }

        private static NumberExpr MakeNumber(double value, SourcePosition pos)
        {
            var tok = new Token(TokenKind.Number, value.ToString(CultureInfo.InvariantCulture), value, pos);
            return new NumberExpr(value, tok);
        }

        private static BooleanExpr MakeBoolean(bool value, SourcePosition pos)
        {
            var tok = new Token(value ? TokenKind.True : TokenKind.False, value ? "TRUE" : "FALSE", value, pos);
            return new BooleanExpr(value, tok);
        }
    }
}
