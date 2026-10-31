// Security P2 prep (2026-09-02): FileUpload core condiviso (estensione whitelist, magic-number,
// nome file sicuro). Unit test puri di CommonLib.Http.FileUpload.
using Xunit;
using CommonLib.Http;

namespace Basic.IlBackend.Tests;

public class FileUploadTests
{
    [Fact]
    public void AllowedExtension_CaseInsensitive()
    {
        Assert.True(FileUpload.HasAllowedExtension("photo.PNG", new[] { "png", "jpg" }));
        Assert.False(FileUpload.HasAllowedExtension("evil.exe", new[] { "png", "jpg" }));
        Assert.False(FileUpload.HasAllowedExtension("noext", new[] { "txt" }));
    }

    [Fact]
    public void MagicSniff_Detects_Common_Types()
    {
        Assert.Equal("image/png", FileUpload.DetectContentType([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 1, 2, 3]));
        Assert.Equal("image/jpeg", FileUpload.DetectContentType([0xFF, 0xD8, 0xFF, 0xE0, 0, 0]));
        Assert.Equal("application/pdf", FileUpload.DetectContentType(System.Text.Encoding.ASCII.GetBytes("%PDF-1.7")));
        Assert.Equal("application/zip", FileUpload.DetectContentType([0x50, 0x4B, 0x03, 0x04, 0, 0]));
        Assert.Null(FileUpload.DetectContentType([0x00, 0x01, 0x02]));
    }

    [Fact]
    public void SecureFileName_Strips_Path_And_Dots()
    {
        Assert.Equal("a.txt", FileUpload.SecureFileName("../../etc/a.txt"));
        Assert.Equal("x.bin", FileUpload.SecureFileName(@"C:\temp\..\x.bin"));
        Assert.Equal("file", FileUpload.SecureFileName("...."));
        Assert.Equal("f.txt", FileUpload.SecureFileName("f.txt"));
    }
}
