' mqtt_demo.bas — pacchetto `mqtt`: client MQTT 3.1.1 (C puro, self-contained).
' Richiede un broker MQTT su localhost:1883 (es. `docker run -p 1883:1883 eclipse-mosquitto:2`).
REQUIRE "mqtt"
IMPORT MQTT WITH ALIAS

SUB Main()
  h = MQTT.CONNECT("127.0.0.1", 1883, "gmb-demo")
  IF h <= 0 THEN PRINT "broker non disponibile su 127.0.0.1:1883" : EXIT SUB
  MQTT.SUBSCRIBE(h, "gmb/demo")
  MQTT.PUBLISH(h, "gmb/demo", "ciao-dal-pacchetto-mqtt")
  IF MQTT.RECEIVE(h, 3000) = 1 THEN
    PRINT "topic=" + MQTT.LASTTOPIC(h)
    PRINT "payload=" + MQTT.LASTPAYLOAD(h)
  ELSE
    PRINT "nessun messaggio"
  END IF
  MQTT.DISCONNECT(h)
END SUB
