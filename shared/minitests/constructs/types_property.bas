' EXPECT: c|r1= 10|r2= 10
' Oracle = golden: PROPERTY GET/SET su classe (campo privato + validazione).
' Sintassi canone: samples/Community/Classes/ClassesHierarchy.bas
Public Class Cerchio
    Public Dim _raggio As Double
    Public Property Raggio As Double
        Get
            Return SELF._raggio
        End Get
        Set(BYVAL value As Double)
            IF value > 0 THEN
                SELF._raggio = value
            END IF
        End Set
    End Property
End Class
Sub Main()
    Print "c"
    Dim c = New Cerchio()
    c.Raggio = 10
    Print "r1="; c.Raggio
    c.Raggio = -3
    Print "r2="; c.Raggio
End Sub
