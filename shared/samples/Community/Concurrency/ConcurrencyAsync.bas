IMPORT SYSTEM
IMPORT CONSOLE

ASYNC FUNCTION SimulateFetch(url AS STRING) AS STRING
    AWAIT TaskYield()
    RETURN "Response from " + url + " (status: 200)"
END FUNCTION

ASYNC FUNCTION SimulateFileRead(path AS STRING) AS STRING
    AWAIT TaskYield()
    RETURN "Contents of " + path
END FUNCTION

ASYNC FUNCTION FetchMultiple() AS STRING
    PRINT "  [FetchMultiple] Starting parallel fetches..."
    DIM task1 = SimulateFetch("https://api.example.com/users")
    DIM task2 = SimulateFetch("https://api.example.com/posts")
    DIM task3 = SimulateFileRead("config.json")
    DIM result1 = AWAIT task1
    DIM result2 = AWAIT task2
    DIM result3 = AWAIT task3
    RETURN result1 + " | " + result2 + " | " + result3
END FUNCTION

ASYNC SUB LogAsync(message AS STRING)
    AWAIT TaskYield()
    PRINT "  [LOG] " + message
END SUB

ASYNC FUNCTION HeavyComputation(n AS INTEGER) AS LONG
    DIM result AS LONG = 0
    FOR i = 1 TO n
        result = result + i * i
        IF i MOD 1000 = 0 THEN AWAIT TaskYield()
    NEXT
    RETURN result
END FUNCTION

ASYNC SUB SleepAsync(ms AS INTEGER)
    AWAIT TaskYield()
END SUB

SUB Main()
    CLS
    PRINT "=========================================="
    PRINT "  TEST: Async/Await"
    PRINT "=========================================="
    PRINT
    
    PRINT "--- 1. Simple async call ---"
    PRINT "Fetching data..."
    DIM response = AWAIT SimulateFetch("https://api.example.com/data")
    PRINT response
    PRINT
    
    PRINT "--- 2. Composition ---"
    DIM multiResult = AWAIT FetchMultiple()
    PRINT multiResult
    PRINT
    
    PRINT "--- 3. Async Sub ---"
    LogAsync("Operation started")
    PRINT "Main continues while log runs..."
    AWAIT TaskYield()
    PRINT
    
    PRINT "--- 4. CPU-bound async ---"
    PRINT "Computing sum of squares 1 to 5000..."
    DIM compResult = AWAIT HeavyComputation(5000)
    PRINT "Result: "; compResult
    PRINT
    
    PRINT "--- 5. Parallel composition ---"
    PRINT "Parallel..."
    DIM p1 = SimulateFetch("url1")
    DIM p2 = SimulateFetch("url2")
    DIM p3 = SimulateFetch("url3")
    PRINT "Main continues..."
    DIM r1 = AWAIT p1
    DIM r2 = AWAIT p2
    DIM r3 = AWAIT p3
    PRINT r1
    PRINT r2
    PRINT r3
    PRINT
    
    PRINT "Test completed."
END SUB
