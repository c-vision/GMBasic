// ConversionLib/Model/Document.cs

using System.Collections.Immutable;

namespace Basic.ConversionLib.Model;

public sealed record Document(ImmutableArray<Block> Blocks, ImmutableArray<Asset> Assets)
{
    public static Document Empty => new([], []);
}
