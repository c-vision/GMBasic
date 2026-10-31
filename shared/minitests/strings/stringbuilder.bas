' EXPECT: Hello World|Hello Beautiful World|Nome: Mario, 30|21|ABGH|A|B
IMPORT STRINGS
DIM sb AS STRINGBUILDER
sb.Append("Hello")
sb.Append(" ")
sb.Append("World")
PRINT sb.ToString()
DIM sb2 AS STRINGBUILDER
sb2.Append("Hello World")
sb2.Insert(5, " Beautiful")
PRINT sb2.ToString()
DIM sb3 AS STRINGBUILDER
sb3.AppendFormat("Nome: {0}, {1}", "Mario", 30)
PRINT sb3.ToString()
PRINT sb2.Length
DIM sb4 AS STRINGBUILDER
sb4.Append("ABCDEFGH")
sb4.Remove(2, 4)
PRINT sb4.ToString()
DIM sb5 AS STRINGBUILDER
sb5.AppendLine("A")
sb5.AppendLine("B")
PRINT sb5.ToString()
