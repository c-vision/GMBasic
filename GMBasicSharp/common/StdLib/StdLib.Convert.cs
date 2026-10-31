// StdLib/StdLib.Convert.cs
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Convert — namespace 'CONVERT'
 * 31 funzioni | 4 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (31):
 *   Batch
 *   Close
 *   CodeBlocks
 *   Extract
 *   ExtractEngine
 *   Format
 *   Formats
 *   Headings
 *   Html
 *   Images
 *   Links
 *   Markdown
 *   MdImages
 *   Metadata
 *   Open
 *   PageCount
 *   Parse
 *   Query
 *   Render
 *   RenderHtml
 *   RenderText
 *   Search
 *   Strip
 *   Tables
 *   Text
 *   ToHtml
 *   ToMd
 *   ToText
 *   Tree
 *   Validate
 *   Web
 * COMANDI (4):
 *   CONVBAT
 *   EXTRACT
 *   RENDER
 *   WEB2MD
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateConvertNamespace()
    {
        const string ns = "CONVERT";
        var pub = SymbolFlags.IsPublic;
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ConvDoc, "ToMd"), [P("path", false)], new StringType(), TokenKind.ConvDoc, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ConvHtml, "ToHtml"), [P("path", false)], new StringType(), TokenKind.ConvHtml, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ConvText, "ToText"), [P("path", false)], new StringType(), TokenKind.ConvText, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.CvtBatch, "Batch"), [P("dir", false), P("fmt", true), P("rec", true)], new StringType(), TokenKind.CvtBatch, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WebToMd, "Web"), [P("url", false)], new StringType(), TokenKind.WebToMd, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ConvFormats, "Formats"), [], new StringType(), TokenKind.ConvFormats, ExecutionKind: NativeExecutionKind.Pure),
            // Handle-based document API (PDFOxide via NativeDocPdf, stessa lib del C).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocOpen, "Open"), [P("src", false)], new DoubleType(), TokenKind.DocOpen, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocClose, "Close"), [P("h", false)], new DoubleType(), TokenKind.DocClose, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocPageCount, "PageCount"), [P("h", false)], new DoubleType(), TokenKind.DocPageCount, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocText, "Text"), [P("h", false), P("page", true)], new StringType(), TokenKind.DocText, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocMarkdown, "Markdown"), [P("h", false), P("page", true)], new StringType(), TokenKind.DocMarkdown, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocHtml, "Html"), [P("h", false), P("page", true)], new StringType(), TokenKind.DocHtml, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocMetadata, "Metadata"), [P("h", false)], new StringType(), TokenKind.DocMetadata, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocSearch, "Search"), [P("h", false), P("pattern", false)], new StringType(), TokenKind.DocSearch, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocImages, "Images"), [P("h", false), P("page", true), P("dir", false)], new DoubleType(), TokenKind.DocImages, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocRender, "Render"), [P("h", false), P("page", false), P("dpi", true), P("out", false)], new DoubleType(), TokenKind.DocRender, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocExtract, "Extract"), [P("src", false)], new StringType(), TokenKind.DocExtract, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.DocExtractEngine, "ExtractEngine"), [P("src", false)], new StringType(), TokenKind.DocExtractEngine, ExecutionKind: NativeExecutionKind.Pure),
            // Fase 5 — Markdown AST.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdParse, "Parse"), [P("text", false)], new JsonType(), TokenKind.MdParse, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdHeadings, "Headings"), [P("doc", false), P("level", true)], new ListType(new JsonType()), TokenKind.MdHeadings, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdLinks, "Links"), [P("doc", false)], new ListType(new JsonType()), TokenKind.MdLinks, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdImages, "MdImages"), [P("doc", false)], new ListType(new JsonType()), TokenKind.MdImages, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdTables, "Tables"), [P("doc", false)], new ListType(new JsonType()), TokenKind.MdTables, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdCodeBlocks, "CodeBlocks"), [P("doc", false)], new ListType(new JsonType()), TokenKind.MdCodeBlocks, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdTree, "Tree"), [P("doc", false)], new StringType(), TokenKind.MdTree, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdToHtml, "RenderHtml"), [P("doc", false)], new StringType(), TokenKind.MdToHtml, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdToText, "RenderText"), [P("doc", false)], new StringType(), TokenKind.MdToText, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdValidate, "Validate"), [P("text", false)], new StringType(), TokenKind.MdValidate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdQuery, "Query"), [P("doc", false), P("selector", false)], new ListType(new JsonType()), TokenKind.MdQuery, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdStrip, "Strip"), [P("text", false)], new StringType(), TokenKind.MdStrip, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MdFormat, "Format"), [P("doc", false)], new StringType(), TokenKind.MdFormat, ExecutionKind: NativeExecutionKind.Pure),
        };
        RegisterBuiltinNamespace(ns, procedures, new Dictionary<string, DataType>());
        RegisterCommand(TokenKind.ConvDoc, "CONVERT", ns, Parser.ParseConvertStatement);
        RegisterCommand(TokenKind.CvtBatch, "CONVBAT", ns, Parser.ParseConvertBatchStatement);
        RegisterCommand(TokenKind.WebToMd, "WEB2MD", ns, Parser.ParseWebToMdStatement);
    }
}
