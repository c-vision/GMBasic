// ConversionLib/Model/Inline.cs

using System.Collections.Immutable;

namespace Basic.ConversionLib.Model;

public abstract record Inline;

public sealed record TextInline(string Text, InlineStyle Style) : Inline
{
    public bool Equals(TextInline? other) =>
        other != null && Text == other.Text && Style == other.Style;
    public override int GetHashCode() => HashCode.Combine(Text, Style);
}

public sealed record Link(ImmutableArray<Inline> Content, string Url) : Inline;
public sealed record Image(string Alt, string? Source) : Inline;
public sealed record NoteRef(string Id) : Inline;
public sealed record LineBreak : Inline;
