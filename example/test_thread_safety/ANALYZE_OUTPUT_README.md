# Thread Safety Test Output Analyzer

This Python script analyzes the output from the thread safety test to automatically detect corruption patterns.

## Usage

1. Run the thread safety test and save output:
```bash
cd example/test_thread_safety
pio run -t upload
pio run -t monitor | tee output.log
```

2. Analyze the output:
```bash
python3 analyze_output.py output.log
```

## What It Detects

### 1. Interleaved Messages
When multiple tasks write simultaneously, messages can interleave:
```
[1062][Worker2][I] Worker2: MSG_000_START_The_qu[1063][Worker3][I] Worker3: MSG_000_START_The_quick_brown_fox
```

### 2. Broken Timestamps
Timestamp formatting can be corrupted:
```
[10[1066][Worker0][I] Worker0: MSG_001_START_The_quick_brown_fox
65][Worker5][I] Worker5: MSG_000_START_The_quick_brown_fox
```

### 3. Partial Messages
Messages can be truncated or split:
```
[1070][Worker4][I] Worker4: MSG_001_START_The_quick_brown_fox_jumps_over_the_lazy_dog_END_MSG_
ick_brown_fox_jumps_over_the_lazy_dog_END_MSG_000
```

### 4. Out of Sequence Messages
Worker messages should be numbered 000-049 in order:
```
Worker3: expected 1, got 2
```

### 5. Malformed Lines
Lines that don't match the expected log format.

## Expected Results

### ConsoleBackend
- **Expected**: Some corruption under heavy concurrent load
- **Why**: Serial.write() is not atomic, allowing interleaving

### SynchronizedConsoleBackend
- **Expected**: No corruption
- **Why**: Mutex protects Serial.write() calls

### NonBlockingConsoleBackend
- **Expected**: No corruption (but may drop messages)
- **Why**: Atomic write pattern - writes complete message or nothing

## Sample Output

```
============================================================
THREAD SAFETY TEST ANALYSIS REPORT
============================================================

### ConsoleBackend
Total lines: 2300
Message count: 2300

Interleaved messages: 15
  Example 1: [1062][Worker2][I] Worker2: MSG_000_START_The_qu[1063][Worker3][I]...

❌ RESULT: 15 corruption patterns detected

### SynchronizedConsoleBackend
Total lines: 2300
Message count: 2300

✅ RESULT: No corruption detected

### NonBlockingConsoleBackend
Total lines: 2290
Message count: 2290

✅ RESULT: No corruption detected

============================================================
SUMMARY
============================================================
Backend Comparison:
  ConsoleBackend                 ❌ 15 issues
  SynchronizedConsoleBackend     ✅ CLEAN
  NonBlockingConsoleBackend      ✅ CLEAN
============================================================
```

## Interpreting Results

1. **ConsoleBackend with corruption** - This is expected and proves the test is working
2. **All backends clean** - May indicate:
   - Serial buffer is large enough
   - Baud rate is high enough
   - Tasks aren't truly concurrent
   - Try reducing delays or increasing message size

3. **NonBlockingConsoleBackend with fewer messages** - Expected, it drops messages when buffer is full

## Tips

- Run test multiple times - corruption is timing-dependent
- Try different baud rates to see the effect
- Increase STRESS_TEST_MESSAGES for more aggressive testing
- Check the raw log file for visual inspection