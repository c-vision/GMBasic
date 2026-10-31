// ConversionLib/Model/Block.cs

using System.Collections.Immutable;

namespace Basic.ConversionLib.Model;

public abstract record Block;

public sealed record Heading(int Level, ImmutableArray<Inline> Content, string? Anchor = null) : Block;
public sealed record Paragraph(ImmutableArray<Inline> Content) : Block;
public sealed record CodeBlock(string? Language, string Text) : Block;
public sealed record BlockQuote(ImmutableArray<Block> Blocks) : Block;
public sealed record Rule : Block;
