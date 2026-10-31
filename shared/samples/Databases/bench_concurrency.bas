SUB Worker(ID AS INTEGER, InC AS CHANNEL OF INTEGER, OutC AS CHANNEL OF INTEGER, WG AS WAITGROUP)
  DIM V AS INTEGER
  DIM I AS INTEGER
  FOR I = 1 TO 2000
    RECEIVE V FROM InC
    SEND V * 2 TO OutC
  NEXT I
  DONE WG
END SUB

SUB Main()
  DIM NW AS INTEGER = 16
  ' Capacita' esplicita >= NW: Main invia un intero batch di NW valori a InC PRIMA di
  ' iniziare a ricevere da OutC (righe 25-27 vs 29-31 sotto). Con una capacita' < NW
  ' (il default storico e' 10) il batch non entra tutto nel buffer: alcuni worker restano
  ' "in volo" (hanno gia' ricevuto da InC ma non possono ancora consegnare a OutC, che
  ' Main non sta ancora drenando) e possono esaurire tutti i worker disponibili prima che
  ' Main finisca di inviare il batch — un'attesa circolare reale (Main aspetta spazio in
  ' InC, gli unici worker che potrebbero liberarlo sono bloccati su OutC pieno). Capacita'
  ' >= NW fa entrare l'intero batch in un colpo solo, eliminando la finestra.
  DIM InC AS CHANNEL OF INTEGER (32)
  DIM OutC AS CHANNEL OF INTEGER (32)
  DIM WG AS WAITGROUP
  DIM I AS INTEGER

  FOR I = 1 TO NW
    GO SUB Worker(I, InC, OutC, WG) IN WG
  NEXT I

  DIM T AS INTEGER
  FOR T = 1 TO 2000
    DIM J AS INTEGER
    FOR J = 1 TO NW
      SEND T TO InC
    NEXT J
    DIM K AS INTEGER
    FOR K = 1 TO NW
      DIM R AS INTEGER
      RECEIVE R FROM OutC
    NEXT K
  NEXT T

  WAIT WG
  PRINT "DONE"
END SUB
