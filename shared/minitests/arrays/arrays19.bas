' ASORT con comparatore custom su array di UDT (4 backend, parity).
' Il predicato booleano BASIC (`a.Fatturato > b.Fatturato`, TRUE = -1) ordina DECRESCENTE.
' Nota: si stampa un campo NUMERICO (ID) per evitare il padding di STRING*N, che e' un
' asse di parita' separato.
' EXPECT: 2 : 2000|1 : 500
TYPE Cliente
  ID AS LONG
  Nome AS STRING * 30
  Fatturato AS CURRENCY
END TYPE

SUB Main()
  DIM Clienti(1) AS Cliente
  Clienti(0).ID = 1 : Clienti(0).Nome = "Zorro" : Clienti(0).Fatturato = 500
  Clienti(1).ID = 2 : Clienti(1).Nome = "Abaco" : Clienti(1).Fatturato = 2000
  ASORT Clienti, { |a, b| RETURN a.Fatturato > b.Fatturato }
  PRINT Clienti(0).ID; ":"; Clienti(0).Fatturato
  PRINT Clienti(1).ID; ":"; Clienti(1).Fatturato
END SUB
