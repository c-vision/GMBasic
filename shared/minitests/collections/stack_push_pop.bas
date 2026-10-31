' EXPECT: cnt= 3|peek= 2|pop= 2 , 7 , 5|vuoto= 0
' STACK: Push/Pop LIFO e Peek (VM/C: Push era un no-op, Count restava 0).
SUB Main()
    DIM st AS STACK OF LONG = NEW STACK OF LONG
    st.Push(5)
    st.Push(7)
    st.Push(2)
    PRINT "cnt="; st.Count
    PRINT "peek="; st.Peek()
    PRINT "pop="; st.Pop(); ","; st.Pop(); ","; st.Pop()
    PRINT "vuoto="; st.Count
END SUB
