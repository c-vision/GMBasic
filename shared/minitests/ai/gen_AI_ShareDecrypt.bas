' EXPECT: ERROR: The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.
IMPORT AI

SUB Main()
    PRINT AI.SHAREDECRYPT("x", "x")
END SUB
