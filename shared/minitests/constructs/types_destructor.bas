' EXPECT: main|connesso db|fine-main|disconnesso db
' Oracle = golden: la SUB DESTRUCTOR (SENZA parentesi) viene invocata a fine programma.
' Sintassi canone: samples/Classes/ClassesDestructor.bas
Public Class Connessione
    Public Dim Nome As String
    Public Sub New(n As String)
        SELF.Nome = n
        Print "connesso "; SELF.Nome
    End Sub
    Public Sub Destructor
        Print "disconnesso "; SELF.Nome
    End Sub
End Class
Sub Main()
    Print "main"
    Dim c = New Connessione("db")
    Print "fine-main"
End Sub
