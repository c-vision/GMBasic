// IlBackend/IlDisassembler.cs — IL disassembler using System.Reflection.Metadata

using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;

namespace Basic.IlBackend;

public static class IlDisassembler
{
    /// <summary>Target di una call nel codice IL: tipo, metodo e namespace dichiarante.</summary>
    public readonly record struct CallTarget(
        string TypeName, string MethodName, string? ParentNamespace, bool IsIlRuntime)
    {
        public override string ToString() => $"{TypeName}::{MethodName} ({ParentNamespace})";
    }

    /// <summary>Raccoglie i target delle istruzioni call/callvirt/newobj in tutti i metodi.</summary>
    public static List<CallTarget> CollectCallTargets(string dllPath)
    {
        var result = new List<CallTarget>();
        using var fs = File.OpenRead(dllPath);
        using var pe = new PEReader(fs);
        var mr = pe.GetMetadataReader();

        foreach (var mh in mr.MethodDefinitions)
        {
            var body = pe.GetMethodBody(mr.GetMethodDefinition(mh).RelativeVirtualAddress);
            if (body == null) continue;
            var reader = body.GetILReader();
            while (reader.RemainingBytes > 0)
            {
                int code = reader.ReadByte();
                if (code == 0xFE) code = 0xFE00 | reader.ReadByte();
                switch (code)
                {
                    case 0x28 or 0x73 or 0x6F: // call, newobj, callvirt
                        var tok = MetadataTokens.EntityHandle(reader.ReadInt32());
                        var target = ResolveCallTarget(mr, tok);
                        if (target.HasValue) result.Add(target.Value);
                        break;
                    default:
                        SkipOperand(code, ref reader);
                        break;
                }
            }
        }
        return result;
    }

    private static void SkipOperand(int code, ref BlobReader reader)
    {
        // InlineI8/R (8 byte)
        if (code is 0x21 or 0x23) { reader.ReadInt64(); return; }
        // Inline (4 byte): branch, token, switch, ldc.i4
        if (code is 0x20 or 0x38 or 0x39 or 0x3A or 0x3B or 0x3C or 0x3D or 0x3E or 0x3F or 0x40
            or 0x41 or 0x42 or 0x43 or 0x44 or 0x45 or 0x74 or 0x75 or 0x79 or 0x7B or 0x7C or 0x7D
            or 0x7E or 0x7F or 0x80 or 0x8C or 0x8D or 0xA5 or 0xD0 or 0x72)
        {
            if (code == 0x45) { int n = reader.ReadInt32(); for (int i = 0; i < n; i++) reader.ReadInt32(); }
            else reader.ReadInt32();
            return;
        }
        // ShortInline (1 byte)
        if (code is 0x0E or 0x0F or 0x10 or 0x11 or 0x12 or 0x13 or 0x1E or 0x1F or 0x2B or 0x2C
            or 0x2D or 0x2E or 0x2F or 0x30 or 0x31 or 0x32 or 0x33 or 0x34 or 0x35 or 0x36 or 0x37)
            reader.ReadByte();
        // FE short (1 byte) e FE branch (4 byte)
        if (code is 0xFE01 or 0xFE02 or 0xFE09 or 0xFE0C or 0xFE0D or 0xFE0E or 0xFE0F) reader.ReadByte();
        if (code is 0xFE03 or 0xFE04 or 0xFE05) reader.ReadInt32();
    }

    private static CallTarget? ResolveCallTarget(MetadataReader mr, EntityHandle handle)
    {
        try
        {
            switch (handle.Kind)
            {
                case HandleKind.MethodDefinition:
                    var md = mr.GetMethodDefinition((MethodDefinitionHandle)handle);
                    var dt = mr.GetTypeDefinition(md.GetDeclaringType());
                    var dtName = mr.GetString(dt.Name);
                    return new CallTarget(dtName, mr.GetString(md.Name), mr.GetString(dt.Namespace),
                        dtName == "IlRuntime");
                case HandleKind.MemberReference:
                    var mb = mr.GetMemberReference((MemberReferenceHandle)handle);
                    var parent = mb.Parent;
                    string pName = "?";
                    string? pNs = null;
                    if (parent.Kind == HandleKind.TypeReference)
                    {
                        var tr = mr.GetTypeReference((TypeReferenceHandle)parent);
                        pName = mr.GetString(tr.Name);
                        pNs = mr.GetString(tr.Namespace);
                    }
                    else if (parent.Kind == HandleKind.TypeDefinition)
                    {
                        var td2 = mr.GetTypeDefinition((TypeDefinitionHandle)parent);
                        pName = mr.GetString(td2.Name);
                        pNs = mr.GetString(td2.Namespace);
                    }
                    return new CallTarget(pName, mr.GetString(mb.Name), pNs, pName == "IlRuntime");
            }
        }
        catch (BadImageFormatException) { }
        return null;
    }

    public static void Dump(string dllPath, string outputPath)
    {
        using var fs = File.OpenRead(dllPath);
        using var pe = new PEReader(fs);
        var mr = pe.GetMetadataReader();

        using var writer = new StreamWriter(outputPath);

        foreach (var mh in mr.MethodDefinitions)
        {
            var m = mr.GetMethodDefinition(mh);
            var name = mr.GetString(m.Name);
            var declType = mr.GetTypeDefinition(m.GetDeclaringType());
            var typeName = mr.GetString(declType.Name);
            var body = pe.GetMethodBody(m.RelativeVirtualAddress);
            if (body == null) continue;

            var sig = m.DecodeSignature(new DisassemblingSigProvider(mr), default);
            writer.WriteLine($".method {sig.ReturnType} {typeName}::{name}({string.Join(", ", sig.ParameterTypes)})");
            writer.WriteLine($"  .maxstack {body.MaxStack}");
            writer.WriteLine($"  .locals init ({string.Join(", ", DecodeLocals(mr, body.LocalSignature))})");

            var reader = body.GetILReader();
            while (reader.RemainingBytes > 0)
            {
                int offset = reader.Offset;
                int b = reader.ReadByte();
                int code = b == 0xFE ? 0xFE00 | reader.ReadByte() : b;

                var (mnemonic, operand) = DecodeInstruction(mr, code, ref reader);
                writer.Write($"  IL_{offset:X4}: {mnemonic}");
                if (operand != null) writer.Write($" {operand}");
                writer.WriteLine();
            }
            writer.WriteLine();
        }

        Console.Error.WriteLine($"[IL dump] wrote {outputPath}");
    }

    private static (string mnemonic, string? operand) DecodeInstruction(MetadataReader mr, int code, ref BlobReader reader)
    {
        string mnemonic = code switch {
            0x00 => "nop", 0x01 => "break", 0x02 => "ldarg.0", 0x03 => "ldarg.1",
            0x04 => "ldarg.2", 0x05 => "ldarg.3", 0x06 => "ldloc.0", 0x07 => "ldloc.1",
            0x08 => "ldloc.2", 0x09 => "ldloc.3", 0x0A => "stloc.0", 0x0B => "stloc.1",
            0x0C => "stloc.2", 0x0D => "stloc.3", 0x0E => "ldarg.s", 0x0F => "ldarga.s",
            0x10 => "starg.s", 0x11 => "ldloc.s", 0x12 => "ldloca.s", 0x13 => "stloc.s",
            0x14 => "ldnull", 0x15 => "ldc.i4.m1", 0x16 => "ldc.i4.0", 0x17 => "ldc.i4.1",
            0x18 => "ldc.i4.2", 0x19 => "ldc.i4.3", 0x1A => "ldc.i4.4", 0x1B => "ldc.i4.5",
            0x1C => "ldc.i4.6", 0x1D => "ldc.i4.7", 0x1E => "ldc.i4.8", 0x1F => "ldc.i4.s",
            0x20 => "ldc.i4", 0x21 => "ldc.i8", 0x22 => "ldc.r4", 0x23 => "ldc.r8",
            0x25 => "dup", 0x26 => "pop", 0x27 => "jmp", 0x28 => "call",
            0x29 => "calli", 0x2A => "ret", 0x2B => "br.s", 0x2C => "brfalse.s",
            0x2D => "brtrue.s", 0x2E => "beq.s", 0x2F => "bge.s", 0x30 => "bgt.s",
            0x31 => "ble.s", 0x32 => "blt.s", 0x33 => "bne.un.s", 0x34 => "bge.un.s",
            0x35 => "bgt.un.s", 0x36 => "ble.un.s", 0x37 => "blt.un.s", 0x38 => "br",
            0x39 => "brfalse", 0x3A => "brtrue", 0x3B => "beq", 0x3C => "bge",
            0x3D => "bgt", 0x3E => "ble", 0x3F => "blt", 0x40 => "bne.un",
            0x41 => "bge.un", 0x42 => "bgt.un", 0x43 => "ble.un", 0x44 => "blt.un",
            0x45 => "switch", 0x46 => "ldind.i1", 0x58 => "add", 0x59 => "sub",
            0x5A => "mul", 0x5B => "div", 0x5C => "div.un", 0x5D => "rem",
            0x5E => "rem.un", 0x5F => "and", 0x60 => "or", 0x61 => "xor",
            0x62 => "shl", 0x63 => "shr", 0x64 => "shr.un", 0x65 => "neg",
            0x66 => "not", 0x67 => "conv.i1", 0x68 => "conv.i2", 0x69 => "conv.i4",
            0x6A => "conv.i8", 0x6B => "conv.r4",             0x6C => "conv.r8", 0x6D => "conv.u4",
            0x6E => "conv.u8", 0x6F => "callvirt",
            0x72 => "ldstr", 0x73 => "newobj", 0x74 => "castclass", 0x75 => "isinst",
            0x79 => "unbox", 0x7A => "throw", 0x7B => "ldfld", 0x7C => "ldflda",
            0x7D => "stfld", 0x7E => "ldsfld", 0x7F => "ldsflda", 0x80 => "stsfld",
            0x81 => "stobj", 0x82 => "conv.ovf.i1.un", 0x83 => "conv.ovf.i2.un",
            0x84 => "conv.ovf.i4.un", 0x85 => "conv.ovf.i8.un", 0x86 => "conv.ovf.u1.un",
            0x87 => "conv.ovf.u2.un", 0x88 => "conv.ovf.u4.un", 0x89 => "conv.ovf.u8.un",
            0x8A => "conv.ovf.i.un", 0x8B => "conv.ovf.u.un", 0x8C => "box",
            0x8D => "newarr", 0x8E => "ldlen", 0x8F => "ldelema", 0x90 => "ldelem.i1",
            0x91 => "ldelem.u1", 0x92 => "ldelem.i2", 0x93 => "ldelem.u2",
            0x94 => "ldelem.i4", 0x95 => "ldelem.u4", 0x96 => "ldelem.i8",
            0x97 => "ldelem.i", 0x98 => "ldelem.r4", 0x99 => "ldelem.r8",
            0x9A => "ldelem.ref", 0x9B => "stelem.i", 0x9C => "stelem.i1",
            0x9D => "stelem.i2", 0x9E => "stelem.i4", 0x9F => "stelem.i8",
            0xA0 => "stelem.r4", 0xA1 => "stelem.r8", 0xA2 => "stelem.ref",
            0xA3 => "ldelem", 0xA4 => "stelem", 0xA5 => "unbox.any",
            0xB3 => "conv.r.un", 0xB4 => "ckfinite", 0xC6 => "mkrefany",
            0xD0 => "ldtoken", 0xD1 => "conv.u2", 0xD2 => "conv.u1",
            0xD3 => "conv.i", 0xD4 => "conv.ovf.i", 0xD5 => "conv.ovf.u",
            0xD6 => "add.ovf", 0xD7 => "add.ovf.un", 0xD8 => "mul.ovf",
            0xD9 => "mul.ovf.un", 0xDA => "sub.ovf", 0xDB => "sub.ovf.un",
            0xDC => "endfinally", 0xDD => "leave", 0xDE => "leave.s",
            0xDF => "stind.i", 0xE0 => "conv.u",             0xFE01 => "ceq", 0xFE02 => "cgt", 0xFE03 => "cgt.un",
            0xFE04 => "clt", 0xFE05 => "clt.un",
            0xFE09 => "ldarg",
            0xFE0A => "ldarga", 0xFE0B => "starg", 0xFE0C => "ldloc",
            0xFE0D => "ldloca", 0xFE0E => "stloc", 0xFE0F => "localloc",
            0xFE11 => "endfilter", 0xFE12 => "unaligned.", 0xFE13 => "volatile.",
            0xFE14 => "tail.", 0xFE15 => "initobj", 0xFE16 => "constrained.",
            0xFE17 => "cpblk", 0xFE18 => "initblk", 0xFE1A => "rethrow",
            0xFE1C => "sizeof", 0xFE1D => "refanytype", 0xFE1E => "readonly.",
            _ => $"unknown({code:X4})"
        };

        string? operand = null;
        switch (code)
        {
            // Short inline operand (1 byte)
            case 0x0E or 0x10 or 0x11 or 0x12 or 0x13:
                operand = reader.ReadByte().ToString(); break;
            case 0x1F:
                operand = reader.ReadSByte().ToString(); break;
            case 0x2B or 0x2C or 0x2D or 0x2E or 0x2F or 0x30 or 0x31 or 0x32 or 0x33 or 0x34 or 0x35 or 0x36 or 0x37:
                operand = $"IL_{reader.Offset + 1 + reader.ReadSByte():X4}"; break;
            case 0xDE:
                operand = $"IL_{reader.Offset + 1 + reader.ReadSByte():X4}"; break;

            // Inline operand (4 bytes)
            case 0x20:
                operand = reader.ReadInt32().ToString(); break;
            case 0x22:
                operand = reader.ReadSingle().ToString(System.Globalization.CultureInfo.InvariantCulture); break;
            case 0x38 or 0x39 or 0x3A or 0x3B or 0x3C or 0x3D or 0x3E or 0x3F:
            case 0x40 or 0x41 or 0x42 or 0x43 or 0x44:
                operand = $"IL_{reader.Offset + 4 + reader.ReadInt32():X4}"; break;
            case 0xDD:
                operand = $"IL_{reader.Offset + 4 + reader.ReadInt32():X4}"; break;

            case 0x72: // ldstr
                operand = $"\"{mr.GetUserString(MetadataTokens.UserStringHandle(reader.ReadInt32()))}\""; break;
            case 0x28 or 0x73 or 0x6F:
                {
                    var tok = MetadataTokens.EntityHandle(reader.ReadInt32());
                    operand = ResolveToken(mr, tok);
                }
                break;
            case 0x7B or 0x7C or 0x7D or 0x7E or 0x7F or 0x80:
                {
                    var tok = MetadataTokens.EntityHandle(reader.ReadInt32());
                    operand = ResolveToken(mr, tok);
                }
                break;
            case 0x8D:
                {
                    var tok = MetadataTokens.EntityHandle(reader.ReadInt32());
                    operand = ResolveToken(mr, tok);
                }
                break;
            // Token di tipo (4 byte) — box, castclass, isinst, unbox, unbox.any, ldtoken.
            case 0x74 or 0x75 or 0x79 or 0x8C or 0xA5 or 0xD0:
                {
                    var tok = MetadataTokens.EntityHandle(reader.ReadInt32());
                    operand = ResolveToken(mr, tok);
                }
                break;

            // Inline I8 / R8 (8 bytes)
            case 0x21:
                operand = reader.ReadInt64().ToString(); break;
            case 0x23:
                operand = reader.ReadDouble().ToString(System.Globalization.CultureInfo.InvariantCulture); break;

            // Switch
            case 0x45:
                {
                    int count = reader.ReadInt32();
                    int switchEnd = reader.Offset + count * 4;   // base dei target = fine dell'istruzione switch
                    var targets = new System.Text.StringBuilder();
                    for (int i = 0; i < count; i++)
                    {
                        if (i > 0) targets.Append(", ");
                        targets.Append($"IL_{switchEnd + reader.ReadInt32():X4}");
                    }
                    operand = targets.ToString();
                }
                break;
        }

        return (mnemonic, operand);
    }

    private static string ResolveToken(MetadataReader mr, EntityHandle handle)
    {
        try
        {
            return handle.Kind switch
            {
                HandleKind.MethodDefinition => ResolveMethodDef(mr, (MethodDefinitionHandle)handle),
                HandleKind.MemberReference => ResolveMemberRef(mr, (MemberReferenceHandle)handle),
                HandleKind.FieldDefinition => ResolveFieldDef(mr, (FieldDefinitionHandle)handle),
                HandleKind.TypeReference => ResolveTypeRef(mr, (TypeReferenceHandle)handle),
                HandleKind.TypeDefinition => ResolveTypeDef(mr, (TypeDefinitionHandle)handle),
                HandleKind.TypeSpecification => "TypeSpec",
                _ => $"token(0x{MetadataTokens.GetToken(handle):X8})"
            };
        }
        catch { return $"token(0x{MetadataTokens.GetToken(handle):X8})"; }
    }

    private static string ResolveMethodDef(MetadataReader mr, MethodDefinitionHandle h)
    {
        var m = mr.GetMethodDefinition(h);
        var type = mr.GetTypeDefinition(m.GetDeclaringType());
        return $"{mr.GetString(type.Name)}::{mr.GetString(m.Name)}";
    }

    private static string ResolveMemberRef(MetadataReader mr, MemberReferenceHandle h)
    {
        var m = mr.GetMemberReference(h);
        var parent = GetMemberReferenceParentName(mr, m);
        return $"{parent}::{mr.GetString(m.Name)}";
    }

    private static string ResolveFieldDef(MetadataReader mr, FieldDefinitionHandle h)
    {
        var f = mr.GetFieldDefinition(h);
        var type = mr.GetTypeDefinition(f.GetDeclaringType());
        return $"{mr.GetString(type.Name)}::{mr.GetString(f.Name)}";
    }

    private static string ResolveTypeRef(MetadataReader mr, TypeReferenceHandle h)
    {
        var tr = mr.GetTypeReference(h);
        return $"{mr.GetString(tr.Namespace)}.{mr.GetString(tr.Name)}";
    }

    private static string ResolveTypeDef(MetadataReader mr, TypeDefinitionHandle h)
    {
        var td = mr.GetTypeDefinition(h);
        return $"{mr.GetString(td.Namespace)}.{mr.GetString(td.Name)}";
    }

    private static string GetMemberReferenceParentName(MetadataReader mr, MemberReference m)
    {
        if (m.Parent.Kind == HandleKind.TypeReference)
        {
            var tr = mr.GetTypeReference((TypeReferenceHandle)m.Parent);
            return mr.GetString(tr.Name);
        }
        if (m.Parent.Kind == HandleKind.TypeDefinition)
        {
            var td = mr.GetTypeDefinition((TypeDefinitionHandle)m.Parent);
            return mr.GetString(td.Name);
        }
        if (m.Parent.Kind == HandleKind.TypeSpecification) return "TypeSpec";
        if (m.Parent.Kind == HandleKind.ModuleReference) return "ModuleRef";
        return "?";
    }

    /// <summary>
    /// Decodifica i tipi dei local di un metodo per il dump `.locals init (...)`.
    /// BUG STORICO (trovato 2026-09-07, analizzando un AccessViolationException su IL con
    /// due FOR EACH su RANGE(...) nella stessa SUB): questa funzione leggeva UN SOLO byte
    /// per local (`blob.ReadByte()`) e mappava solo gli ELEMENT_TYPE primitivi a singolo
    /// byte — qualunque tipo complesso (SZARRAY 0x1D, VALUETYPE 0x11, CLASS 0x12, ecc., che
    /// nella firma occupano PIÙ byte: element-type + token compresso) veniva "decodificato"
    /// come UN byte solo, senza mai avanzare il reader oltre i byte restanti di quel tipo.
    /// Risultato: dal primo local non-primitivo in poi, OGNI local successivo mostrava il
    /// tipo SBAGLIATO (un byte a caso preso dal MEZZO della codifica estesa del local
    /// precedente, mai risincronizzato) — es. un array veniva letto correttamente come
    /// "ET_1D" (il byte SZARRAY grezzo, MAI risolto), ma il local SUCCESSIVO nella lista
    /// mostrava "uint32" pur essendo in realtà un altro array o un local completamente
    /// diverso. Il dump `.locals init` prodotto da `--dump-il` era quindi INAFFIDABILE per
    /// qualunque metodo con almeno un local non-primitivo (array, class, valuetype) — esattamente
    /// il caso di questo bug, portando a un'analisi iniziale sbagliata (sospetto di un local
    /// mistipato come causa diretta) prima di scoprire che era il DUMPER a mentire, non il
    /// codegen. Fix: riusa `DisassemblingSigProvider` (già corretto e già usato per le firme
    /// di metodo alla riga sopra) via `StandaloneSignature.DecodeLocalSignature`, che decodifica
    /// ricorsivamente l'intera firma ECMA-335 (array, generici, valuetype/class per token)
    /// invece di un case singolo per byte.
    /// </summary>
    private static string[] DecodeLocals(MetadataReader mr, StandaloneSignatureHandle sigHandle)
    {
        if (sigHandle.IsNil) return Array.Empty<string>();

        var sig = mr.GetStandaloneSignature(sigHandle);
        return sig.DecodeLocalSignature(new DisassemblingSigProvider(mr), null).ToArray();
    }

    private sealed class DisassemblingSigProvider : ISignatureTypeProvider<string, object?>
    {
        private readonly MetadataReader _mr;
        public DisassemblingSigProvider(MetadataReader mr) => _mr = mr;
        public string GetPrimitiveType(PrimitiveTypeCode tc) => tc switch {
            PrimitiveTypeCode.Void => "void", PrimitiveTypeCode.Boolean => "bool",
            PrimitiveTypeCode.Char => "char", PrimitiveTypeCode.SByte => "int8",
            PrimitiveTypeCode.Byte => "uint8", PrimitiveTypeCode.Int16 => "int16",
            PrimitiveTypeCode.UInt16 => "uint16", PrimitiveTypeCode.Int32 => "int32",
            PrimitiveTypeCode.UInt32 => "uint32", PrimitiveTypeCode.Int64 => "int64",
            PrimitiveTypeCode.UInt64 => "uint64", PrimitiveTypeCode.Single => "float32",
            PrimitiveTypeCode.Double => "float64", PrimitiveTypeCode.String => "string",
            PrimitiveTypeCode.Object => "object", PrimitiveTypeCode.IntPtr => "IntPtr",
            PrimitiveTypeCode.UIntPtr => "UIntPtr", PrimitiveTypeCode.TypedReference => "TypedRef",
            _ => tc.ToString()
        };
        public string GetTypeFromDefinition(MetadataReader r, TypeDefinitionHandle h, byte rawTypeKind)
            => r.GetString(r.GetTypeDefinition(h).Name);
        public string GetTypeFromReference(MetadataReader r, TypeReferenceHandle h, byte rawTypeKind)
            => r.GetString(r.GetTypeReference(h).Name);
        public string GetSZArrayType(string elem) => $"{elem}[]";
        public string GetGenericInstantiation(string g, System.Collections.Immutable.ImmutableArray<string> a)
            => $"{g}<{string.Join(",", a)}>";
        public string GetPinnedType(string e) => $"pinned {e}";
        public string GetByReferenceType(string e) => $"ref {e}";
        public string GetPointerType(string e) => $"{e}*";
        public string GetFunctionPointerType(MethodSignature<string> s) => "fnptr";
        public string GetGenericMethodParameter(object? c, int i) => $"!!{i}";
        public string GetGenericTypeParameter(object? c, int i) => $"!{i}";
        public string GetModifiedType(string m, string u, bool r) => u;
        public string GetArrayType(string e, ArrayShape s) => $"{e}[*]";
        public string GetTypeFromSpecification(MetadataReader r, object? c, TypeSpecificationHandle h, byte rawTypeKind) => "TypeSpec";
    }
}
