' Oracle = golden: OPERATOR CTYPE — conversione utente definita (CLASS -> STRING).
' Sintassi canone: samples/Community/Types/TypesConversionOperators.bas
' Verificato: IL/PCode/VM-C/C99 gate identici. Gap C99Native: `Dim s$ = p` non emesso
' (type mismatch in emissione — conversione implicita via CTYPE non inferita, esplicito).
' EXPECT: s=( 5, 3)
Public Class Punto
    Public Dim X As Double
    Public Dim Y As Double
    Public Static Operator CType(p As Punto) As String
        Operator = "(" + Str(p.X) + "," + Str(p.Y) + ")"
    End Operator
End Class

Sub Main()
    Dim p = New Punto()
    p.X = 5 : p.Y = 3
    ' Conversione implicita via OPERATOR CTYPE. NOTA: forma esplicita CTYPE(p, STRING) NON
    ' usata (IL la rifiuta: conversione numerica->String non supportata; PCode/VMC wrong).
    ' Gap implicito su C99Native: `Dim s$ = p` -> type mismatch in emissione (documentato).
    Dim s$ = p
    Print "s="; s$
End Sub
