// tools/swift/pngs2mp4.swift — monta una sequenza di PNG in un MP4 H.264 con AVFoundation.
//
// Uso:
//   swift tools/swift/pngs2mp4.swift <cartella-png> <output.mp4> [fps]
//
// I file sono ordinati per il primo numero contenuto nel nome (`f_1.png`, `f_2.png`, …),
// quindi non serve lo zero-padding. La dimensione del video è quella del primo PNG.
// Serve quando i fotogrammi li produce un programma BASIC (una finestra grafica o una
// pagina CANVAS, salvata con `CANVAS SAVE 0, …`) e manca ffmpeg: su macOS AVFoundation
// è già nel sistema.

import Foundation
import AVFoundation
import CoreGraphics
import ImageIO

func fail(_ message: String) -> Never {
    FileHandle.standardError.write((message + "\n").data(using: .utf8)!)
    exit(1)
}

let args = CommandLine.arguments
guard args.count >= 3 else { fail("uso: pngs2mp4 <cartella-png> <output.mp4> [fps]") }
let directory = args[1]
let output = args[2]
let fps = Int32(args.count > 3 ? args[3] : "30") ?? 30

let fm = FileManager.default
let all = (try? fm.contentsOfDirectory(atPath: directory)) ?? []
let pngs = all.filter { $0.lowercased().hasSuffix(".png") }
guard !pngs.isEmpty else { fail("nessun PNG in \(directory)") }

// Ordine numerico: il primo gruppo di cifre nel nome del file.
func frameIndex(_ name: String) -> Int {
    let digits = name.drop { !$0.isNumber }.prefix { $0.isNumber }
    return Int(digits) ?? 0
}
let frames = pngs.sorted { frameIndex($0) < frameIndex($1) }

func load(_ path: String) -> CGImage? {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil) else { return nil }
    return CGImageSourceCreateImageAtIndex(source, 0, nil)
}

let firstPath = directory + "/" + frames[0]
guard let first = load(firstPath) else { fail("primo PNG illeggibile: \(firstPath)") }
let width = first.width
let height = first.height

try? fm.removeItem(atPath: output)
guard let writer = try? AVAssetWriter(outputURL: URL(fileURLWithPath: output), fileType: .mp4) else {
    fail("AVAssetWriter non creato per \(output)")
}

let settings: [String: Any] = [
    AVVideoCodecKey: AVVideoCodecType.h264,
    AVVideoWidthKey: width,
    AVVideoHeightKey: height,
    AVVideoCompressionPropertiesKey: [
        AVVideoAverageBitRateKey: 6_000_000,
        AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel,
    ],
]
let input = AVAssetWriterInput(mediaType: .video, outputSettings: settings)
input.expectsMediaDataInRealTime = false
let adaptor = AVAssetWriterInputPixelBufferAdaptor(
    assetWriterInput: input,
    sourcePixelBufferAttributes: [
        kCVPixelBufferPixelFormatTypeKey as String: Int(kCVPixelFormatType_32ARGB),
        kCVPixelBufferWidthKey as String: width,
        kCVPixelBufferHeightKey as String: height,
    ]
)
writer.add(input)
guard writer.startWriting() else { fail("startWriting fallito: \(writer.error?.localizedDescription ?? "?")") }
writer.startSession(atSourceTime: .zero)

let colorSpace = CGColorSpaceCreateDeviceRGB()
var written = 0
for (i, name) in frames.enumerated() {
    let path = directory + "/" + name
    guard let image = load(path) else { continue }
    var buffer: CVPixelBuffer?
    if let pool = adaptor.pixelBufferPool { CVPixelBufferPoolCreatePixelBuffer(nil, pool, &buffer) }
    guard let pixelBuffer = buffer else { fail("pixel buffer non allocato al fotogramma \(i)") }

    CVPixelBufferLockBaseAddress(pixelBuffer, [])
    let context = CGContext(
        data: CVPixelBufferGetBaseAddress(pixelBuffer),
        width: width, height: height,
        bitsPerComponent: 8,
        bytesPerRow: CVPixelBufferGetBytesPerRow(pixelBuffer),
        space: colorSpace,
        bitmapInfo: CGImageAlphaInfo.noneSkipFirst.rawValue
    )
    context?.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
    CVPixelBufferUnlockBaseAddress(pixelBuffer, [])

    while !input.isReadyForMoreMediaData { usleep(2000) }
    if !adaptor.append(pixelBuffer, withPresentationTime: CMTime(value: CMTimeValue(i), timescale: fps)) {
        fail("append fallito al fotogramma \(i): \(writer.error?.localizedDescription ?? "?")")
    }
    written += 1
}

input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()

if writer.status == .completed {
    print("scritto \(output) — \(written) fotogrammi, \(width)x\(height), \(fps) fps, \(Double(written) / Double(fps)) s")
    exit(0)
}
fail("scrittura fallita: \(writer.error?.localizedDescription ?? "?")")
