// FILE: Basic.Runtime.DebugTypes.cs
// Tipi DTO del debug condivisi: usati dall'interfaccia IDebugDriver e dai driver di ogni
// backend (P-Code/ILBackend, VM/C, C99). Vivono in BRT perché sia l'IDE (Executor) sia i
// backend (IlBackend) li referenziano senza dipendenza circolare.

namespace Basic.Runtime;

/// <summary>Frame dello stack di chiamata catturato in pausa.</summary>
public class StackFrame
{
    public int Id { get; set; }
    public string Name { get; set; } = "";
    public int Ip { get; set; }
    public int Line { get; set; }
    public List<DebugVariable>? Locals { get; set; }
}

/// <summary>Variabile locale mostrata nel pannello Watch.</summary>
public class DebugVariable
{
    public string Name { get; set; } = "";
    public string Value { get; set; } = "";
    public string Type { get; set; } = "";
    public int VariablesReference { get; set; }
}
