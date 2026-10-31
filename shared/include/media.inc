' FILE: media.inc — binding FFI gmbmedia (namespace MEDIA) per il pacchetto gmbsuite 'media'.
' Uso: REQUIRE "media" (oppure IMPORT MEDIA). Firme: LONG per handle/scalari, STRING per path/testo.
' Gli handle immagine/audio sono LONG (>=1); -1 = errore. Direzione unica: la logica sta in gmbmedia.
NAMESPACE MEDIA

PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbmedia" ALIAS "gmbmedia_available" () AS LONG

' ── immagini ──
PUBLIC DECLARE FUNCTION IMGREAD LIB "gmbmedia" ALIAS "gmbmedia_img_read" (path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION IMGREADS LIB "gmbmedia" ALIAS "gmbmedia_img_read_scaled" (path AS STRING, shrink AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGW LIB "gmbmedia" ALIAS "gmbmedia_img_w" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGH LIB "gmbmedia" ALIAS "gmbmedia_img_h" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGCHANNELS LIB "gmbmedia" ALIAS "gmbmedia_img_channels" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGGET LIB "gmbmedia" ALIAS "gmbmedia_img_get" (h AS LONG, x AS LONG, y AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGGETA LIB "gmbmedia" ALIAS "gmbmedia_img_get_a" (h AS LONG, x AS LONG, y AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGSET LIB "gmbmedia" ALIAS "gmbmedia_img_set" (h AS LONG, x AS LONG, y AS LONG, rgb AS LONG, a AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGWRITE LIB "gmbmedia" ALIAS "gmbmedia_img_write" (h AS LONG, path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION IMGWRITEQ LIB "gmbmedia" ALIAS "gmbmedia_img_write_q" (h AS LONG, path AS STRING, quality AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGCONVERT LIB "gmbmedia" ALIAS "gmbmedia_img_convert" (src AS STRING, dst AS STRING) AS LONG
PUBLIC DECLARE FUNCTION IMGCOMPRESS LIB "gmbmedia" ALIAS "gmbmedia_img_compress" (src AS STRING, dst AS STRING, max_short AS LONG, quality AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGFORMATS LIB "gmbmedia" ALIAS "gmbmedia_img_formats" () AS STRING
PUBLIC DECLARE FUNCTION IMGFREE LIB "gmbmedia" ALIAS "gmbmedia_img_free" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGRESIZE LIB "gmbmedia" ALIAS "gmbmedia_img_resize" (h AS LONG, w AS LONG, hh AS LONG, mode AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGCROP LIB "gmbmedia" ALIAS "gmbmedia_img_crop" (h AS LONG, x AS LONG, y AS LONG, w AS LONG, hh AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGROTATE LIB "gmbmedia" ALIAS "gmbmedia_img_rotate" (h AS LONG, turns AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGROTATEA LIB "gmbmedia" ALIAS "gmbmedia_img_rotate_angle" (h AS LONG, angle_cdeg AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGFLIP LIB "gmbmedia" ALIAS "gmbmedia_img_flip" (h AS LONG, axis AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGGRAY LIB "gmbmedia" ALIAS "gmbmedia_img_gray" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGCHANNELS2 LIB "gmbmedia" ALIAS "gmbmedia_img_to_channels" (h AS LONG, ch AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGBLUR LIB "gmbmedia" ALIAS "gmbmedia_img_blur" (h AS LONG, radius AS LONG, kind AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGMORPH LIB "gmbmedia" ALIAS "gmbmedia_img_morph" (h AS LONG, radius AS LONG, mode AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGDITHER LIB "gmbmedia" ALIAS "gmbmedia_img_dither" (h AS LONG, levels AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGSHARPEN LIB "gmbmedia" ALIAS "gmbmedia_img_sharpen" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGSOBEL LIB "gmbmedia" ALIAS "gmbmedia_img_sobel" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGBLEND LIB "gmbmedia" ALIAS "gmbmedia_img_blend" (base AS LONG, over AS LONG, alpha_pct AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGWARP LIB "gmbmedia" ALIAS "gmbmedia_img_warp" (h AS LONG, m_csv AS STRING, ow AS LONG, oh AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGINVERT LIB "gmbmedia" ALIAS "gmbmedia_img_invert" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGBRIGHTNESS LIB "gmbmedia" ALIAS "gmbmedia_img_brightness" (h AS LONG, delta AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGCONTRAST LIB "gmbmedia" ALIAS "gmbmedia_img_contrast" (h AS LONG, factor_pct AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGTHRESHOLD LIB "gmbmedia" ALIAS "gmbmedia_img_threshold" (h AS LONG, t AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGFILL LIB "gmbmedia" ALIAS "gmbmedia_img_fill" (h AS LONG, rgb AS LONG, a AS LONG) AS LONG
PUBLIC DECLARE FUNCTION IMGPASTE LIB "gmbmedia" ALIAS "gmbmedia_img_paste" (dst AS LONG, src AS LONG, x AS LONG, y AS LONG) AS LONG

' ── audio ──
PUBLIC DECLARE FUNCTION AUDIOREAD LIB "gmbmedia" ALIAS "gmbmedia_audio_read" (path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION AUDIOFRAMES LIB "gmbmedia" ALIAS "gmbmedia_audio_frames" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIOCHANNELS LIB "gmbmedia" ALIAS "gmbmedia_audio_channels" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIORATE LIB "gmbmedia" ALIAS "gmbmedia_audio_rate" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIOSAMPLE LIB "gmbmedia" ALIAS "gmbmedia_audio_sample" (h AS LONG, frame AS LONG, ch AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIOSET LIB "gmbmedia" ALIAS "gmbmedia_audio_set" (h AS LONG, frame AS LONG, ch AS LONG, val16 AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIOWRITE LIB "gmbmedia" ALIAS "gmbmedia_audio_write" (h AS LONG, path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION AUDIOFREE LIB "gmbmedia" ALIAS "gmbmedia_audio_free" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIOGAIN LIB "gmbmedia" ALIAS "gmbmedia_audio_gain" (h AS LONG, gain_cdb AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIONORMALIZE LIB "gmbmedia" ALIAS "gmbmedia_audio_normalize" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIOFADE LIB "gmbmedia" ALIAS "gmbmedia_audio_fade" (h AS LONG, fade_in AS LONG, fade_out AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIOREVERSE LIB "gmbmedia" ALIAS "gmbmedia_audio_reverse" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIOTOMONO LIB "gmbmedia" ALIAS "gmbmedia_audio_to_mono" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIORESAMPLE LIB "gmbmedia" ALIAS "gmbmedia_audio_resample" (h AS LONG, out_frames AS LONG, kind AS LONG) AS LONG
PUBLIC DECLARE FUNCTION AUDIOMIX LIB "gmbmedia" ALIAS "gmbmedia_audio_mix" (a_h AS LONG, b_h AS LONG, wa_pct AS LONG, wb_pct AS LONG) AS LONG
PUBLIC DECLARE FUNCTION WAVCHANNELS LIB "gmbmedia" ALIAS "gmbmedia_wav_channels" (path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION WAVRATE LIB "gmbmedia" ALIAS "gmbmedia_wav_rate" (path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION WAVBITS LIB "gmbmedia" ALIAS "gmbmedia_wav_bits" (path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION WAVFRAMES LIB "gmbmedia" ALIAS "gmbmedia_wav_frames" (path AS STRING) AS LONG
