' MiniTest: AI.VideoInfo con file inesistente -> errore esplicito (non silenzio).
' (Con un video reale servirebbe una fixture mp4: ffmpeg assente in ambiente.)
' EXPECT: vinfo=[AI Video: file not found]
IMPORT AI WITH ALIAS
SUB Main()
  PRINT "vinfo="; AI.VideoInfo("nonexistent-zzz.mp4")
END SUB
