// FILE: Basic.Ast.ICompilationContext.cs

namespace Basic;

public interface ICompilationContext
{
    IEnumerable<ProcedureDefinition> GetAllProcedures();
    Dictionary<int, FunctionDefinition> ResolvedSpecializations { get; }
    Dictionary<string, BasicModule> LoadedModules { get; }
    Dictionary<string, ImmutableArray<object>> DataStoresByModule { get; }
    IEnumerable<VariableDeclaration> GetAllGlobalVariables();
    IEnumerable<UserDefinedType> GetAllUserDefinedTypes();
    IEnumerable<ClassDefinition> GetAllClasses();
    BasicModule? MainModule { get; }
}
