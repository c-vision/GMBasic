// enc.swift — PNG sequence -> MP4 H.264 (AVFoundation), senza ffmpeg.
// uso: enc <cartella> <out.mp4> <fps> <square 0|1>
import Foundation
import AVFoundation
import CoreGraphics
import ImageIO
import CoreVideo

func loadCGImage(_ path: String) -> CGImage? {
    guard let src = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil) else { return nil }
    return CGImageSourceCreateImageAtIndex(src, 0, nil)
}

let a = CommandLine.arguments
guard a.count >= 5 else { print("uso: enc <dir> <out.mp4> <fps> <square>"); exit(2) }
let dir = a[1], outPath = a[2]
let fps = Int32(a[3]) ?? 24
let square = a[4] == "1"

let all = (try? FileManager.default.contentsOfDirectory(atPath: dir)) ?? []
let files = all.filter { $0.hasSuffix(".png") && $0.lowercased().hasPrefix("f") }.sorted {
    (Int($0.filter { $0.isNumber }) ?? 0) < (Int($1.filter { $0.isNumber }) ?? 0)
}
guard let first = files.first.flatMap({ loadCGImage(dir + "/" + $0) }) else { print("nessun frame"); exit(3) }
let fw = first.width, fh = first.height
let W = square ? 1080 : fw
let H = square ? 1080 : fh

try? FileManager.default.removeItem(atPath: outPath)
let writer = try! AVAssetWriter(outputURL: URL(fileURLWithPath: outPath), fileType: .mp4)
let settings: [String: Any] = [
    AVVideoCodecKey: AVVideoCodecType.h264,
    AVVideoWidthKey: W, AVVideoHeightKey: H,
    AVVideoCompressionPropertiesKey: [
        AVVideoAverageBitRateKey: square ? 6_000_000 : 2_500_000,
        AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel,
        AVVideoMaxKeyFrameIntervalKey: 24
    ]
]
let input = AVAssetWriterInput(mediaType: .video, outputSettings: settings)
input.expectsMediaDataInRealTime = false
let adaptor = AVAssetWriterInputPixelBufferAdaptor(
    assetWriterInput: input,
    sourcePixelBufferAttributes: [
        kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA,
        kCVPixelBufferWidthKey as String: W,
        kCVPixelBufferHeightKey as String: H
    ])
writer.add(input)
guard writer.startWriting() else { print("startWriting fallito: \(String(describing: writer.error))"); exit(4) }
writer.startSession(atSourceTime: .zero)

var written = 0
for (i, f) in files.enumerated() {
    guard let img = loadCGImage(dir + "/" + f) else { continue }
    while !input.isReadyForMoreMediaData { usleep(2000) }
    var pb: CVPixelBuffer?
    if let pool = adaptor.pixelBufferPool { CVPixelBufferPoolCreatePixelBuffer(nil, pool, &pb) }
    guard let buf = pb else { continue }
    CVPixelBufferLockBaseAddress(buf, [])
    let ctx = CGContext(data: CVPixelBufferGetBaseAddress(buf), width: W, height: H,
                        bitsPerComponent: 8, bytesPerRow: CVPixelBufferGetBytesPerRow(buf),
                        space: CGColorSpaceCreateDeviceRGB(),
                        bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue)
    ctx?.interpolationQuality = .high
    if square {
        let s = max(CGFloat(W) / CGFloat(fw), CGFloat(H) / CGFloat(fh))
        let dw = CGFloat(fw) * s, dh = CGFloat(fh) * s
        ctx?.draw(img, in: CGRect(x: (CGFloat(W) - dw) / 2, y: (CGFloat(H) - dh) / 2, width: dw, height: dh))
    } else {
        ctx?.draw(img, in: CGRect(x: 0, y: 0, width: W, height: H))
    }
    CVPixelBufferUnlockBaseAddress(buf, [])
    if adaptor.append(buf, withPresentationTime: CMTime(value: CMTimeValue(i), timescale: fps)) { written += 1 }
}
input.markAsFinished()
let sem = DispatchSemaphore(value: 0)
writer.finishWriting { sem.signal() }
sem.wait()
if writer.status == .completed {
    print("ok \(outPath) — \(written) frame, \(W)x\(H) @\(fps)fps")
} else {
    print("errore: \(String(describing: writer.error))")
    exit(5)
}
