// ConversionLib/Model/InlineStyle.cs

namespace Basic.ConversionLib.Model;

public readonly record struct InlineStyle(bool Bold, bool Italic, bool Strike, bool Code)
{
    public static InlineStyle None => default;
    public bool Any => Bold || Italic || Strike || Code;
}
