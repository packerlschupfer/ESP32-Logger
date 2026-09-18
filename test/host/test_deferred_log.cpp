// Host-side tests for DeferredLog (ring, argument capture, replay, concurrency).
// Builds DeferredLog.cpp against stub ESP-IDF headers; run with `make -C test/host`.
#define LOG_DEFERRED_FORMAT 1
#define CONFIG_LOG_DEFERRED_RING_SIZE 1024
#include "DeferredLog.cpp"
#include <cstdio>
#include <cstdarg>
#include <string>
#include <thread>
#include <vector>
#include <cassert>
#include <cstdlib>

thread_local const char* g_taskName = "main";
using namespace DeferredLog;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

__attribute__((format(printf, 3, 4)))
static bool pushf(esp_log_level_t lvl, const char* tag, const char* fmt, ...) {
    va_list a; va_start(a, fmt); bool r = push(lvl, tag, fmt, a, 0); va_end(a); return r;
}
__attribute__((format(printf, 1, 2)))
static std::string ref(const char* fmt, ...) {
    char b[256]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a); return b;
}
static std::string popOne(Record* out = nullptr) {
    Record r; char m[256];
    auto res = pop(r, m, sizeof m, false);
    if (res != PopResult::OK) return "<none>";
    if (out) *out = r;
    return m;
}
static void simulateReset() {
    s_initialized = false; s_survived = false; s_recovered = s_incomplete = s_preResetPending = 0;
    init();
}

#define PARITY(...) do { CHECK(pushf(ESP_LOG_INFO, "T", __VA_ARGS__)); std::string got = popOne(), want = ref(__VA_ARGS__); \
    if (got != want) { printf("MISMATCH\n  got : [%s]\n  want: [%s]\n", got.c_str(), want.c_str()); failures++; } } while (0)

int main() {
    // Fresh "power-on": garbage in the ring memory
    memset(&s_ring, 0x5A, sizeof s_ring);
    init();
    CHECK(!ringSurvivedReset());
    CHECK(empty());

    // ---- formatting parity with snprintf
    PARITY("plain text");
    PARITY("int %d neg %d u %u x %x X %08X o %o", 42, -7, 3000000000u, 0xbeef, 0xBEEFu, 8u);
    PARITY("long %ld %lu %li", -123456L, 4000000000UL, 5L);
    PARITY("ll %lld %llu %llx", -1234567890123LL, 18446744073709551615ULL, 0x123456789abcULL);
    PARITY("size %zu ptrdiff %td intmax %jd", (size_t)77, (ptrdiff_t)-3, (intmax_t)-99);
    PARITY("short %hd %hhu", (short)-5, (unsigned char)250);
    PARITY("float %f %.2f %8.3f %-8.1f| %e %E %g %G %a", 3.14159, 2.5f, -1.0/3, 9.99, 12345.678, 0.00012, 1e-10, 1e20, 1.0);
    PARITY("star [%*d] [%-*d] [%.*f] [%*.*f]", 6, 42, 6, 42, 3, 2.0/3, 10, 2, 3.14159);
    PARITY("str [%s] [%10s] [%-10s|] [%.3s] [%.*s]", "hello", "hi", "hi", "abcdef", 2, "xyz");
    PARITY("char %c%c%c pct %%", 'a', 'b', 'c');
    PARITY("ptr %p", (void*)0x1234);
    PARITY("%s", "");
    PARITY("flags [%+d] [% d] [%#x] [%#o] [%-5d] [%05d]", 5, 5, 255, 8, 3, 3);
    PARITY("many %d %d %d %d %d %d %d %d %d %d", 1,2,3,4,5,6,7,8,9,10);
    {   // NULL %s -> "(null)" (glibc prints the same)
        const char* volatile n = nullptr;
        CHECK(pushf(ESP_LOG_INFO, "T", "null [%s]", n));
        CHECK(popOne() == "null [(null)]");
    }
    {   // long strings are capped at CONFIG_LOG_DEFERRED_MAX_STRING
        std::string big(300, 'q');
        CHECK(pushf(ESP_LOG_INFO, "T", "big %s end", big.c_str()));
        std::string got = popOne();
        CHECK(got == "big " + std::string(CONFIG_LOG_DEFERRED_MAX_STRING, 'q') + " end");
    }
    {   // unsupported conversion: prefix formatted, rest raw
        int n = 0;
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wformat"
        CHECK(pushf(ESP_LOG_INFO, "T", "a=%d then %n rest %d", 5, &n, 6));
        #pragma GCC diagnostic pop
        CHECK(popOne() == "a=5 then %n rest %d");
    }
    {   // caller's buffer is copied, not referenced
        char buf[16]; strcpy(buf, "before");
        CHECK(pushf(ESP_LOG_INFO, "T", "buf=%s", buf));
        strcpy(buf, "AFTER!");
        CHECK(popOne() == "buf=before");
    }
    {   // record fields
        g_taskName = "MyTask";
        CHECK(pushf(ESP_LOG_WARN, "SomeTag", "x"));
        Record r; popOne(&r);
        CHECK(r.level == ESP_LOG_WARN && std::string(r.tag) == "SomeTag" && std::string(r.task) == "MyTask" && !r.preReset);
        g_taskName = "main";
    }

    // ---- wrap-around: many push/pop cycles with varying sizes
    for (int i = 0; i < 5000; i++) {
        std::string s(i % 97, 'a' + i % 26);
        CHECK(pushf(ESP_LOG_INFO, "W", "%d:%s:%.1f", i, s.c_str(), i * 0.5));
        if (i % 3 == 2) {
            for (int k = 0; k < 3; k++) { std::string g = popOne(); (void)g; }
        }
    }
    while (!empty()) popOne();

    // ---- fill until full: drops counted, order kept
    uint32_t before = overflowCount();
    int accepted = 0;
    for (int i = 0; i < 200; i++) if (pushf(ESP_LOG_INFO, "F", "fill %d", i)) accepted++;
    CHECK(accepted > 10 && accepted < 200);
    CHECK(overflowCount() - before == (uint32_t)(200 - accepted));
    for (int i = 0; i < accepted; i++) CHECK(popOne() == ref("fill %d", i));
    CHECK(empty());

    // ---- reset replay
    for (int i = 0; i < 5; i++) pushf(ESP_LOG_ERROR, "R", "pre %d", i);
    // one entry whose producer was interrupted mid-write
    uint8_t* half = reserve(64); CHECK(half != nullptr);
    pushf(ESP_LOG_ERROR, "R", "after-half");
    simulateReset();
    CHECK(ringSurvivedReset());
    CHECK(recoveredCount() == 6 && recoveredIncompleteCount() == 1);
    pushf(ESP_LOG_INFO, "N", "new boot");
    for (int i = 0; i < 5; i++) { Record r; std::string m = popOne(&r); CHECK(m == ref("pre %d", i) && r.preReset); }
    { Record r; std::string m = popOne(&r); CHECK(m == "after-half" && r.preReset); }
    { Record r; std::string m = popOne(&r); CHECK(m == "new boot" && !r.preReset); }
    CHECK(empty());

    // ---- replay rejected: different firmware (SHA), corrupted control word
    pushf(ESP_LOG_INFO, "S", "x");
    s_ring.elfSha[0] ^= 1;
    simulateReset();
    CHECK(!ringSurvivedReset() && empty());
    pushf(ESP_LOG_INFO, "S", "y"); pushf(ESP_LOG_INFO, "S", "z");
    s_ring.data[s_ring.tail + 3] ^= 0xFF;  // magic byte of the oldest entry
    simulateReset();
    CHECK(!ringSurvivedReset() && empty());

    // ---- busy entry: consumer waits, then can skip
    uint8_t* stuck = reserve(64); CHECK(stuck != nullptr);
    pushf(ESP_LOG_INFO, "B", "behind");
    { Record r; char m[64]; CHECK(pop(r, m, sizeof m, false) == PopResult::BUSY); }
    { Record r; char m[64]; CHECK(pop(r, m, sizeof m, true) == PopResult::OK); CHECK(std::string(m) == "behind"); }
    CHECK(empty());

    // ---- concurrency: 4 producers, 1 consumer, check per-producer order and content
    const int P = 4, N = 20000;
    std::vector<int> next(P, 0);
    std::atomic<bool> done{false};
    std::atomic<long> dropped{0};
    std::vector<std::thread> producers;
    for (int p = 0; p < P; p++) producers.emplace_back([p, &dropped] {
        static const char* names[] = {"P0", "P1", "P2", "P3"};
        g_taskName = names[p];
        for (int i = 0; i < N; i++) {
            while (!pushf(ESP_LOG_INFO, "C", "%d %d %s %.2f", p, i, "payload-string", i * 0.25)) { dropped++; std::this_thread::yield(); }
        }
    });
    long received = 0, bad = 0;
    std::thread consumer([&] {
        Record r; char m[256];
        while (!done.load() || !empty()) {
            auto res = pop(r, m, sizeof m, false);
            if (res != PopResult::OK) { std::this_thread::yield(); continue; }
            int p, i; char s[32]; double d;
            if (sscanf(m, "%d %d %31s %lf", &p, &i, s, &d) != 4 || p < 0 || p >= P || i != next[p] ||
                strcmp(s, "payload-string") || d != i * 0.25 || std::string(r.task) != "P" + std::to_string(p)) {
                if (bad++ < 5) printf("bad: [%s] task=%s expected i=%d\n", m, r.task, p >= 0 && p < P ? next[p] : -1);
            } else next[p]++;
            received++;
        }
    });
    for (auto& t : producers) t.join();
    done = true; consumer.join();
    CHECK(bad == 0);
    CHECK(received == (long)P * N);
    printf("concurrency: %ld received, %ld retries on full ring\n", received, dropped.load());

    printf(failures ? "FAILED (%d)\n" : "ALL PASSED\n", failures);
    return failures != 0;
}
