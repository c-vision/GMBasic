// ConversionLib/Model/ListBlock.cs

using System.Collections.Immutable;

namespace Basic.ConversionLib.Model;

public enum MarkerKind { Bullet, Decimal, LowerAlpha, UpperAlpha, LowerRoman, UpperRoman }

public sealed record ListBlock(MarkerKind Marker, int Start, ImmutableArray<ListItem> Items) : Block;

public sealed record ListItem(ImmutableArray<Block> Blocks, string? Checked = null, string? Label = null);
