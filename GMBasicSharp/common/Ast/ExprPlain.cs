// FILE: Basic.ExprPlain
using System.Globalization;

namespace Basic;

/// <summary>
/// Testo di un'espressione senza i suffissi <c>#SymbolId</c> di <see cref="Expr.ToString"/>.
/// Serve a SET FILTER / JOIN FOR / SET RELATION: il runtime rivaluta il testo sui campi
/// del record, quindi serve il nome campo ("ID"), non "ID#305".
/// </summary>
public static class ExprPlain
{
    public static string From(Expr expression) =>
        expression switch
        {
            VariableExpr v => v.NameCleaned.Lexeme,
            FieldAccessExpr f => f.Field.Lexeme,
            PropertyAccessExpr p => p.PropertyName.Lexeme,
            QualifiedNameExpr q => q.Parts.Length > 0 ? q.Parts[^1].Lexeme : "",
            BinaryExpr b => $"{From(b.Left)} {b.Op.Lexeme} {From(b.Right)}",
            GroupingExpr g => From(g.Expression),
            LambdaExpr l when l.ExpressionBody is not null => From(l.ExpressionBody),
            StringExpr s => $"\"{s.Value}\"",
            NumberExpr n => n.Value.ToString(CultureInfo.InvariantCulture),
            BooleanExpr b => b.Value ? "TRUE" : "FALSE",
            UnaryExpr u => $"{u.Op.Lexeme} {From(u.Right)}",
            MacroExpr m => From(m.Expression),
            _ => expression.ToString() ?? ""
        };

    /// <summary>Testo di una condizione SET FILTER: se l'intera condizione è una STRINGA
    /// (`SET FILTER TO "Name > 'A'"`), quella stringa È GIÀ il testo del filtro da ri-parsare —
    /// <see cref="From"/> la ri-quoterebbe (caso <c>StringExpr</c>, pensato per una stringa
    /// ANNIDATA dentro un'espressione più grande, es. il lato destro di `Name > 'A'`), producendo
    /// un filtro che valuta sempre alla costante stringa (sempre vero) invece di ri-parsare
    /// `Name > 'A'`. Solo al livello TOP la stringa va presa per valore; nei sotto-nodi resta
    /// corretto ri-quotarla, quindi si delega a <see cref="From"/> in ogni altro caso.</summary>
    public static string FilterText(Expr expression) =>
        expression is StringExpr s ? s.Value : From(expression);

    /// <summary>Testo BASIC con qualifiche preservate, per condizioni di JOIN fra due tabelle
    /// ("C.ID" e "O.ID" sono campi diversi — a differenza di <see cref="From"/>, pensato per
    /// filtri a tabella singola dove l'alias viene deliberatamente scartato). Stessa tecnica di
    /// CleanQueryExprText (SPEED): <c>Expr.ToString()</c> + rimozione dei suffissi #SymbolId.
    /// Usato SOLO dai siti di emissione JOIN (IL/PCode); gli altri restano su From.
    /// </summary>
    public static string FromQualified(Expr expression)
    {
        switch (expression)
        {
            case BinaryExpr b:
                return $"{FromQualified(b.Left)} {b.Op.Lexeme} {FromQualified(b.Right)}";
            case FieldAccessExpr f:
                return $"{FromQualified(f.Structure)}.{f.Field.Lexeme}";
            case VariableExpr v:
                return v.NameCleaned.Lexeme;
            case StringExpr s:
                return $"\"{s.Value}\"";
            case NumberExpr n:
                return n.Value.ToString(CultureInfo.InvariantCulture);
            default:
                var text = expression.ToString() ?? "";
                return System.Text.RegularExpressions.Regex.Replace(text, @"#-?\d+", "");
        }
    }

    /// <summary>Nome del campo chiave di SET RELATION, o null se va valutato a runtime (macro/stringa).</summary>
    public static string? RelationKeyName(Expr? expression) =>
        expression switch
        {
            null => null,
            VariableExpr v => v.NameCleaned.Lexeme,
            FieldAccessExpr f => f.Field.Lexeme,
            PropertyAccessExpr p => p.PropertyName.Lexeme,
            QualifiedNameExpr q => q.Parts.Length > 0 ? q.Parts[^1].Lexeme : null,
            StringExpr s => s.Value,
            _ => null
        };
}
