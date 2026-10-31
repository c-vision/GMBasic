// FILE: Basic.IlBackend.NativeDocPdf.cs
// Namespace DOCUMENT/PDF (Fase 4): P/Invoke nella C ABI di PDFOxide (libpdf_oxide).
// Stessa implementazione del backend C (gmb_doc_c/gmb_doc_pdf.c) → parità di comportamento.
// Registry a handle intero (come il C); le stringhe native vengono copiate e liberate
// con free_string().
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;

namespace Basic.IlBackend;

internal static class NativeDocPdf
{
    static NativeDocPdf()
    {
        NativeLibrary.SetDllImportResolver(typeof(NativeDocPdf).Assembly, Resolve);
    }

    private static IntPtr Resolve(string name, System.Reflection.Assembly assembly, DllImportSearchPath? search)
        => CommonLib.GmbBoot.ResolveDllImport(name);

    private static bool? _available;

    /// <summary>True se la lib pdf_oxide è caricabile (Resources/libs/&lt;os&gt; o
    /// GMBASIC_PDFOXIDE_LIB). Usato per preferire PDFOxide su CONVERT.ToMd/ToHtml/ToText
    /// (Fase 3) con fallback PdfPig. L'handle di probe viene rilasciato subito (il
    /// P/Invoke successivo risolve di nuovo via resolver).</summary>
    public static bool IsAvailable()
    {
        if (_available is bool a) return a;
        // Invoca direttamente il resolver (controlla prima GMBASIC_PDFOXIDE_LIB, poi
        // Resources/libs/<os> risalendo dalla cwd). NativeLibrary.TryLoad(assembly)
        // NON garantisce di passare dal DllImportResolver → falso negativo su CWD esterne.
        var probe = Resolve("pdf_oxide", typeof(NativeDocPdf).Assembly, null);
        bool ok = probe != IntPtr.Zero;
        if (ok) NativeLibrary.Free(probe);
        _available = ok;
        return ok;
    }

    [DllImport("pdf_oxide", EntryPoint = "pdf_document_open", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_document_open([MarshalAs(UnmanagedType.LPUTF8Str)] string path, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_document_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void pdf_document_free(IntPtr h);
    [DllImport("pdf_oxide", EntryPoint = "pdf_document_get_page_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int pdf_document_get_page_count(IntPtr h, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_document_to_markdown_all", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_document_to_markdown_all(IntPtr h, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_document_to_markdown", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_document_to_markdown(IntPtr h, int page, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_document_to_plain_text", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_document_to_plain_text(IntPtr h, int page, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_document_to_html", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_document_to_html(IntPtr h, int page, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_document_get_version", CallingConvention = CallingConvention.Cdecl)]
    private static extern void pdf_document_get_version(IntPtr h, out byte major, out byte minor);
    [DllImport("pdf_oxide", EntryPoint = "pdf_document_search_all", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_document_search_all(IntPtr h, [MarshalAs(UnmanagedType.LPUTF8Str)] string term, int caseSensitive, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_oxide_search_result_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int pdf_oxide_search_result_count(IntPtr r);
    [DllImport("pdf_oxide", EntryPoint = "pdf_oxide_search_result_get_text", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_oxide_search_result_get_text(IntPtr r, int index, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_oxide_search_result_get_page", CallingConvention = CallingConvention.Cdecl)]
    private static extern int pdf_oxide_search_result_get_page(IntPtr r, int index, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_oxide_search_result_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void pdf_oxide_search_result_free(IntPtr r);
    [DllImport("pdf_oxide", EntryPoint = "pdf_render_page_raw", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_render_page_raw(IntPtr h, int page, int dpi, out int w, out int ht, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_get_rendered_image_data", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_get_rendered_image_data(IntPtr img, out int len, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_rendered_image_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void pdf_rendered_image_free(IntPtr img);
    [DllImport("pdf_oxide", EntryPoint = "pdf_document_get_embedded_images", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_document_get_embedded_images(IntPtr h, int page, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_oxide_image_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int pdf_oxide_image_count(IntPtr im);
    [DllImport("pdf_oxide", EntryPoint = "pdf_oxide_image_get_format", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_oxide_image_get_format(IntPtr im, int index, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_oxide_image_get_data", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pdf_oxide_image_get_data(IntPtr im, int index, out int dataLen, out int err);
    [DllImport("pdf_oxide", EntryPoint = "pdf_oxide_image_list_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void pdf_oxide_image_list_free(IntPtr im);
    [DllImport("pdf_oxide", EntryPoint = "free_string", CallingConvention = CallingConvention.Cdecl)]
    private static extern void free_string(IntPtr p);

    private static string TakeString(IntPtr p)
    {
        if (p == IntPtr.Zero) return "";
        try { return Marshal.PtrToStringUTF8(p) ?? ""; }
        finally { free_string(p); }
    }

    private const int MaxHandles = 64;
    private static readonly IntPtr[] _slots = new IntPtr[MaxHandles];

    private static IntPtr Handle(int h)
    {
        if (h <= 0 || h > MaxHandles) return IntPtr.Zero;
        return _slots[h - 1];
    }

    public static int Open(string path)
    {
        var doc = pdf_document_open(path, out _);
        if (doc == IntPtr.Zero) return 0;
        for (int i = 0; i < MaxHandles; i++)
        {
            if (_slots[i] == IntPtr.Zero) { _slots[i] = doc; return i + 1; }
        }
        pdf_document_free(doc);
        return 0;
    }

    public static void Close(int h)
    {
        var doc = Handle(h);
        if (doc != IntPtr.Zero) { pdf_document_free(doc); _slots[h - 1] = IntPtr.Zero; }
    }

    public static int PageCount(int h)
    {
        var doc = Handle(h);
        return doc != IntPtr.Zero ? pdf_document_get_page_count(doc, out _) : 0;
    }

    public static string Markdown(int h, int page)
    {
        var doc = Handle(h);
        if (doc == IntPtr.Zero) return "";
        return page < 0 ? TakeString(pdf_document_to_markdown_all(doc, out _)) : TakeString(pdf_document_to_markdown(doc, page, out _));
    }

    public static string Text(int h, int page)
    {
        var doc = Handle(h);
        return doc != IntPtr.Zero ? TakeString(pdf_document_to_plain_text(doc, page < 0 ? 0 : page, out _)) : "";
    }

    public static string Html(int h, int page)
    {
        var doc = Handle(h);
        return doc != IntPtr.Zero ? TakeString(pdf_document_to_html(doc, page < 0 ? 0 : page, out _)) : "";
    }

    public static string Metadata(int h)
    {
        var doc = Handle(h);
        if (doc == IntPtr.Zero) return "";
        int pages = pdf_document_get_page_count(doc, out _);
        pdf_document_get_version(doc, out byte maj, out byte min);
        return $"pages={pages}; version={maj}.{min}";
    }

    /* Estrae la PRIMA immagine embedded della pagina e ne ritorna il path (o ""). */
    public static string ExtractFirstImage(int h, int page, string dir)
    {
        var doc = Handle(h);
        if (doc == IntPtr.Zero || string.IsNullOrEmpty(dir)) return "";
        Directory.CreateDirectory(dir);
        var im = pdf_document_get_embedded_images(doc, page, out _);
        if (im == IntPtr.Zero) return "";
        int n = pdf_oxide_image_count(im);
        string path = "";
        for (int i = 0; i < n; i++)
        {
            var data = pdf_oxide_image_get_data(im, i, out int dataLen, out _);
            if (data == IntPtr.Zero || dataLen <= 0) continue;
            var bytes = new byte[dataLen];
            Marshal.Copy(data, bytes, 0, dataLen);
            var fmt = TakeString(pdf_oxide_image_get_format(im, i, out _));
            var ext = string.IsNullOrEmpty(fmt) ? "img" : fmt;
            path = Path.Combine(dir, $"img_{page}_{i}.{ext}");
            File.WriteAllBytes(path, bytes);
            break;
        }
        pdf_oxide_image_list_free(im);
        return path;
    }

    public static string Extract(string path)
    {
        var h = Open(path);
        if (h == 0) return "";
        var md = Markdown(h, -1);
        Close(h);
        return md;
    }

    public static string Search(int h, string pattern)
    {
        var doc = Handle(h);
        if (doc == IntPtr.Zero || string.IsNullOrEmpty(pattern)) return "";
        var r = pdf_document_search_all(doc, pattern, 0, out _);
        if (r == IntPtr.Zero) return "";
        int n = pdf_oxide_search_result_count(r);
        var sb = new System.Text.StringBuilder();
        for (int i = 0; i < n; i++)
        {
            int page = pdf_oxide_search_result_get_page(r, i, out _);
            sb.Append("p").Append(page).Append('|').Append(TakeString(pdf_oxide_search_result_get_text(r, i, out _))).Append('\n');
        }
        pdf_oxide_search_result_free(r);
        return sb.ToString();
    }

    /* Estrae le immagini incorporate della pagina in out_dir; ritorna il conteggio */
    public static int Images(int h, int page, string dir)
    {
        var doc = Handle(h);
        if (doc == IntPtr.Zero || string.IsNullOrEmpty(dir)) return 0;
        Directory.CreateDirectory(dir);
        var im = pdf_document_get_embedded_images(doc, page, out _);
        if (im == IntPtr.Zero) return 0;
        int n = pdf_oxide_image_count(im);
        int written = 0;
        for (int i = 0; i < n; i++)
        {
            var data = pdf_oxide_image_get_data(im, i, out int dataLen, out _);
            if (data == IntPtr.Zero || dataLen <= 0) continue;
            var bytes = new byte[dataLen];
            Marshal.Copy(data, bytes, 0, dataLen);
            var fmt = TakeString(pdf_oxide_image_get_format(im, i, out _));
            var ext = string.IsNullOrEmpty(fmt) ? "bin" : fmt;
            var path = Path.Combine(dir, $"img_{page}_{i}.{ext}");
            File.WriteAllBytes(path, bytes);
            written++;
        }
        pdf_oxide_image_list_free(im);
        return written;
    }

    /* Render → PNG (writer PNG8 RGBA minimale, zlib DeflateStream) */
    /* Render → PNG con downscale a maxWidth (PaddleOCR-VL riconosce testo a ~1280px;
     * immagini più grandi → 'OCR REQUIRED'/vuoto). Nearest-neighbor, mantiene aspect. */
    public static int RenderPngMaxWidth(int h, int page, int dpi, string outPath, int maxWidth)
    {
        var doc = Handle(h);
        if (doc == IntPtr.Zero) return -1;
        var img = pdf_render_page_raw(doc, page, dpi, out int w, out int ht, out _);
        if (img == IntPtr.Zero) return -1;
        var data = pdf_get_rendered_image_data(img, out int len, out _);
        var rgba = new byte[len];
        Marshal.Copy(data, rgba, 0, len);
        pdf_rendered_image_free(img);
        if (w <= maxWidth) { WritePng(outPath, rgba, w, ht); return 0; }
        int nw = maxWidth;
        int nh = Math.Max(1, ht * nw / w);
        var dst = new byte[nw * nh * 4];
        for (int y = 0; y < nh; y++)
        {
            int sy = Math.Min(ht - 1, y * ht / nh);
            for (int x = 0; x < nw; x++)
            {
                int sx = Math.Min(w - 1, x * w / nw);
                Buffer.BlockCopy(rgba, (sy * w + sx) * 4, dst, (y * nw + x) * 4, 4);
            }
        }
        WritePng(outPath, dst, nw, nh);
        return 0;
    }

    public static int RenderPng(int h, int page, int dpi, string outPath)
    {
        var doc = Handle(h);
        if (doc == IntPtr.Zero) return -1;
        var img = pdf_render_page_raw(doc, page, dpi, out int w, out int ht, out _);
        if (img == IntPtr.Zero) return -1;
        var data = pdf_get_rendered_image_data(img, out int len, out _);
        var rgba = new byte[len];
        Marshal.Copy(data, rgba, 0, len);
        pdf_rendered_image_free(img);
        WritePng(outPath, rgba, w, ht);
        return 0;
    }

    private static void WritePng(string path, byte[] rgba, int w, int h)
    {
        using var f = File.Create(path);
        f.Write(new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 });
        // IHDR
        var ihdr = new byte[13];
        ihdr[0] = (byte)(w >> 24); ihdr[1] = (byte)(w >> 16); ihdr[2] = (byte)(w >> 8); ihdr[3] = (byte)w;
        ihdr[4] = (byte)(h >> 24); ihdr[5] = (byte)(h >> 16); ihdr[6] = (byte)(h >> 8); ihdr[7] = (byte)h;
        ihdr[8] = 8; ihdr[9] = 6;
        WriteChunk(f, "IHDR", ihdr);
        // IDAT: filter 0 + RGBA, deflate
        int row = w * 4;
        var raw = new byte[row * h + h];
        for (int y = 0; y < h; y++)
        {
            raw[y * (row + 1)] = 0;
            Buffer.BlockCopy(rgba, y * row, raw, y * (row + 1) + 1, row);
        }
        using var ms = new MemoryStream();
        using (var ds = new System.IO.Compression.DeflateStream(ms, System.IO.Compression.CompressionLevel.Optimal))
            ds.Write(raw, 0, raw.Length);
        var deflated = ms.ToArray();
        // IDAT richiede un flusso ZLIB (RFC 1950), NON raw deflate: DeflateStream produce
        // solo raw deflate → PNG invalido per i decoder (Vision/sips) finché non avvolgiamo
        // con header zlib (0x78 0x9C) + checksum Adler-32 del dato non compresso.
        var idat = new byte[2 + deflated.Length + 4];
        idat[0] = 0x78; idat[1] = 0x9C;
        Buffer.BlockCopy(deflated, 0, idat, 2, deflated.Length);
        uint a1 = 1, a2 = 0;
        foreach (byte b in raw) { a1 = (a1 + b) % 65521; a2 = (a2 + a1) % 65521; }
        uint adler = (a2 << 16) | a1;
        idat[idat.Length - 4] = (byte)(adler >> 24);
        idat[idat.Length - 3] = (byte)(adler >> 16);
        idat[idat.Length - 2] = (byte)(adler >> 8);
        idat[idat.Length - 1] = (byte)adler;
        WriteChunk(f, "IDAT", idat);
        WriteChunk(f, "IEND", Array.Empty<byte>());
    }

    private static void WriteChunk(Stream s, string type, byte[] data)
    {
        var len = BitConverter.GetBytes(data.Length);
        if (BitConverter.IsLittleEndian) Array.Reverse(len);
        s.Write(len, 0, 4);
        var t = System.Text.Encoding.ASCII.GetBytes(type);
        s.Write(t, 0, 4);
        if (data.Length > 0) s.Write(data, 0, data.Length);
        uint crc = 0xFFFFFFFFu;
        Crc32(ref crc, t); Crc32(ref crc, data);
        crc ^= 0xFFFFFFFFu;
        var c = BitConverter.GetBytes(crc);
        if (BitConverter.IsLittleEndian) Array.Reverse(c);
        s.Write(c, 0, 4);
    }

    private static void Crc32(ref uint crc, byte[] data)
    {
        foreach (var b in data)
        {
            crc ^= b;
            for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
}
