// ConversionLib/Model/TableBlock.cs

using System.Collections.Immutable;

namespace Basic.ConversionLib.Model;

public sealed record TableBlock(ImmutableArray<ImmutableArray<Cell>> Rows, int HeaderRows = 0) : Block;

public sealed record Cell(ImmutableArray<Block> Blocks, int ColSpan = 1, int RowSpan = 1);
