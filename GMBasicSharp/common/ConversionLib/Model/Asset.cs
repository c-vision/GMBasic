// ConversionLib/Model/Asset.cs

namespace Basic.ConversionLib.Model;

public sealed record Asset(int Id, string MediaType, string OriginPart, byte[] Bytes);
