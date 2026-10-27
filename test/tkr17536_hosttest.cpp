// Host test for tukuri patch tkr17536 (6.2.6-tkr1)
// The block between "namespace tkr17536 {" and "static bool should_check_fault_status"
// is taken unchanged from ModbusControl.cpp (patched_block.inc) and run against mocks.
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
// ---- mocks ----
static uint32_t g_now = 0;
static uint32_t ticks_ms() { return g_now; }
struct CHOPCONF_t { static constexpr uint8_t address = 0x6C; };
constexpr uint32_t PUSH_MARK = 0xFFFF; // marker in the write sequence for push()
struct MockStepper {
    std::deque<uint32_t> gstat_reads; uint32_t drv = 0; int pushes = 0; int chop_zero_writes = 0;
    std::vector<std::pair<uint32_t, uint32_t>> seq; // (address, value), push() = (PUSH_MARK, 0)
    std::function<void(uint8_t, uint32_t)> on_write; // simulates the Modbus thread preempting a write
    uint32_t read(uint8_t a) { assert(a == 0x01); if (gstat_reads.empty()) return 0; uint32_t v = gstat_reads.front(); gstat_reads.pop_front(); return v; }
    uint32_t DRV_STATUS() { return drv; }
    void write(uint8_t a, uint32_t v) { if (a == 0x6C && v == 0) chop_zero_writes++; seq.push_back({a, v}); if (on_write) on_write(a, v); }
    void push() { pushes++; seq.push_back({PUSH_MARK, 0}); }
} stepperE0;
struct { float Get24VVoltage() { return 23.95f; } } advancedpower;
static std::vector<std::string> g_logs;
static void logf(const char *sev, const char *fmt, ...) { char b[256]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); g_logs.push_back(std::string(sev) + " " + b); }
#define log_warning(c, ...) logf("W", __VA_ARGS__)
#define log_error(c, ...) logf("E", __VA_ARGS__)
namespace dwarf_shared::errors { enum class FaultStatusMask : uint16_t { NO_FAULT = 0, MARLIN_KILLED = 1, TMC_FAULT = 4 }; }
namespace ModbusRegisters { enum class SystemInputRegister { fault_status }; static uint16_t fault = 0; static void SetRegValue(SystemInputRegister, uint16_t v) { fault = v; } }
#include "patched_block.inc"
static void reset_all() {
    using namespace tkr17536;
    drv_err_count=0; drv_err_last_decay_ms=0; last_event_log_ms=0; last_glitch_log_ms=0; event_logged_once=false; glitch_logged_once=false;
    events_total=0; events_not_logged=0; glitches_total=0; stop_issued=false;
    for (size_t i=0;i<BUDDY_REG_COUNT;i++) { buddy_value[i]=0; }
    buddy_valid_mask=0; buddy_generation=0;
    stepperE0 = MockStepper{}; ModbusRegisters::fault=0; g_logs.clear(); g_now=1000;
}
// the buddy's push() as seen in log_20261023_T2 (first three) plus the rest of TMC2130Stepper::push()
static void buddy_push(uint32_t ihold_irun) {
    using tkr17536::record_buddy_tmc_write;
    const uint8_t regs[] = {0x00,0x10,0x11,0x13,0x14,0x15,0x2D,0x33,0x6C,0x6D,0x6E,0x70,0x72};
    for (uint8_t r : regs) {
        uint32_t v = (r==0x10) ? ihold_irun : (r==0x11) ? 128u : (r==0x6C) ? 0x10410153u : (r==0x00) ? 0u : (0x1000u + r);
        record_buddy_tmc_write(static_cast<uint8_t>(r | 0x80), v); // the buddy sends address | 0x80
    }
}
static uint32_t last_written(uint32_t addr) { uint32_t v = 0xDEAD; bool f=false; for (auto &p : stepperE0.seq) if (p.first==addr) { v=p.second; f=true; } return f ? v : 0xDEAD; }
static size_t index_after_push() { for (size_t i=0;i<stepperE0.seq.size();i++) if (stepperE0.seq[i].first==PUSH_MARK) return i+1; return 0; }
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main() {
    // ===== part A: event handling, glitch filter and stop conditions =====
    reset_all(); for (int i=0;i<1000;i++){ g_now++; update_fault_status(); }
    CHECK(stepperE0.pushes==0 && ModbusRegisters::fault==0 && g_logs.empty()); printf("A1 normal ok\n");
    reset_all(); stepperE0.gstat_reads={0x01}; update_fault_status();
    CHECK(stepperE0.pushes==1 && stepperE0.chop_zero_writes==1 && ModbusRegisters::fault==0 && g_logs.size()==1); printf("A2 reset ok: %s\n", g_logs[0].c_str());
    reset_all(); for (int i=0;i<3000;i++){ g_now++; stepperE0.gstat_reads={0x04}; update_fault_status(); }
    CHECK(ModbusRegisters::fault==0 && stepperE0.pushes==3000 && g_logs.size()<=4); printf("A3 uv_cp x3000 ok, logs=%zu\n", g_logs.size());
    reset_all(); stepperE0.gstat_reads={0xFFFFFFFF,0x0}; update_fault_status();
    CHECK(stepperE0.pushes==0 && ModbusRegisters::fault==0 && g_logs.size()==1); printf("A4 glitch ok\n");
    reset_all(); stepperE0.gstat_reads={0xDEADBEEF,0x01}; update_fault_status();
    CHECK(stepperE0.pushes==1 && ModbusRegisters::fault==0 && g_logs.size()==2); printf("A5 glitch+reset ok\n");
    reset_all(); stepperE0.drv = 0x08000000; int stopped_at=-1; for (int i=0;i<50;i++){ g_now++; stepperE0.gstat_reads={0x02}; update_fault_status(); if (ModbusRegisters::fault==4 && stopped_at<0) stopped_at=i+1; }
    CHECK(stopped_at==10); printf("A6 persistent drv_err stops at event %d ok\n", stopped_at);
    reset_all(); stepperE0.drv = 0x08000000; for (int t=0;t<600000;t++){ g_now++; if (t%2000==0) stepperE0.gstat_reads={0x02}; update_fault_status(); }
    CHECK(ModbusRegisters::fault==0); printf("A7 sporadic drv_err no stop ok\n");
    reset_all(); for (int i=0;i<9;i++){ g_now++; stepperE0.gstat_reads={0x02}; update_fault_status(); } for (int i=0;i<20000;i++){ g_now++; update_fault_status(); }
    CHECK(ModbusRegisters::fault==0 && tkr17536::drv_err_count==0); printf("A8 burst 9 + decay ok\n");
    reset_all(); stepperE0.drv = 0x02000000; stepperE0.gstat_reads={0x02}; update_fault_status();
    CHECK(ModbusRegisters::fault==4); printf("A9 ot immediate stop ok\n");
    reset_all(); stepperE0.drv = 0xFFFFFFFF; stepperE0.gstat_reads={0x02}; update_fault_status();
    CHECK(ModbusRegisters::fault==0); printf("A10 invalid DRV not ot ok\n");
    reset_all(); g_now=0xFFFFFF00u; for (int i=0;i<600;i++){ g_now++; stepperE0.gstat_reads={0x02}; update_fault_status(); } CHECK(ModbusRegisters::fault==4); printf("A11 wraparound ok\n");

    // ===== part B: replay of the buddy values (item 5) =====
    // B1 no buddy write yet -> nothing replayed, r=0
    reset_all(); stepperE0.gstat_reads={0x01}; update_fault_status();
    CHECK(index_after_push()==stepperE0.seq.size()); CHECK(g_logs.back().find(" r=0")!=std::string::npos); printf("B1 no record -> r=0 ok: %s\n", g_logs.back().c_str());
    // B2 buddy push recorded -> after push(): 13 registers in push order, IHOLD_IRUN 0x000A1A1A (450 mA)
    reset_all(); buddy_push(0x000A1A1Au); stepperE0.gstat_reads={0x01}; update_fault_status();
    {
        size_t k = index_after_push(); CHECK(k>0); CHECK(stepperE0.seq[0].first==0x6C && stepperE0.seq[0].second==0); // CHOPCONF=0 first
        const uint32_t order[] = {0x00,0x10,0x11,0x13,0x14,0x15,0x2D,0x33,0x6C,0x6D,0x6E,0x70,0x72};
        CHECK(stepperE0.seq.size()-k==13);
        for (size_t i=0;i<13;i++) CHECK(stepperE0.seq[k+i].first==order[i]);
        CHECK(last_written(0x10)==0x000A1A1Au && last_written(0x11)==128u && last_written(0x00)==0u);
        CHECK(g_logs.back().find(" r=13")!=std::string::npos);
    }
    printf("B2 replay after push, 13 regs in push order, IHOLD_IRUN=0x000A1A1A ok: %s\n", g_logs.back().c_str());
    // B3 latest value wins; address with and without 0x80 map to the same register
    reset_all(); tkr17536::record_buddy_tmc_write(0x90, 0x00081717u); tkr17536::record_buddy_tmc_write(0x10, 0x000A1A1Au); stepperE0.gstat_reads={0x01}; update_fault_status();
    CHECK(stepperE0.seq.size()-index_after_push()==1 && last_written(0x10)==0x000A1A1Au); printf("B3 latest value wins ok\n");
    // B4 addresses outside the push set (GSTAT 0x01, IOIN 0x04, DRV_STATUS 0x6F, 0x7F) are ignored
    reset_all(); tkr17536::record_buddy_tmc_write(0x81, 1); tkr17536::record_buddy_tmc_write(0x84, 1); tkr17536::record_buddy_tmc_write(0xEF, 1); tkr17536::record_buddy_tmc_write(0xFF, 1);
    CHECK(tkr17536::buddy_valid_mask==0 && tkr17536::buddy_generation==0); printf("B4 other addresses ignored ok\n");
    // B5 buddy write lands during the replay (Modbus thread preempts) -> second round, new value is the last one written
    reset_all(); buddy_push(0x00081717u);
    static bool fired; fired=false; stepperE0.on_write = [](uint8_t a, uint32_t){ if (!fired && a==0x2D) { fired=true; tkr17536::record_buddy_tmc_write(0x90, 0x000A1A1Au); } };
    stepperE0.gstat_reads={0x01}; update_fault_status();
    { size_t k=index_after_push(); CHECK(stepperE0.seq.size()-k==26); CHECK(last_written(0x10)==0x000A1A1Au); }
    printf("B5 write during replay -> replayed again (26 writes), final IHOLD_IRUN=0x000A1A1A ok\n");
    // B6 buddy writing on every replay write (worst case) -> bounded to 3 rounds
    reset_all(); buddy_push(0x000A1A1Au); stepperE0.on_write = [](uint8_t, uint32_t){ tkr17536::buddy_generation = tkr17536::buddy_generation + 1; };
    stepperE0.gstat_reads={0x01}; update_fault_status();
    CHECK(stepperE0.seq.size()-index_after_push()==39); printf("B6 endless buddy writes -> bounded to 3 rounds (39 writes) ok\n");
    // B7 1000 resets in 1 s with a recorded buddy config -> every event replays, still no stop
    reset_all(); buddy_push(0x000A1A1Au); for (int i=0;i<1000;i++){ g_now++; stepperE0.seq.clear(); stepperE0.gstat_reads={0x01}; update_fault_status(); CHECK(stepperE0.seq.size()-index_after_push()==13); }
    CHECK(ModbusRegisters::fault==0); printf("B7 1000 resets, each replayed, no stop ok\n");
    // B8 replay also on drv_err; the stop at 10 is unchanged
    reset_all(); buddy_push(0x000A1A1Au); stepperE0.drv=0x08000000; stopped_at=-1; for (int i=0;i<20;i++){ g_now++; stepperE0.seq.clear(); stepperE0.gstat_reads={0x02}; update_fault_status(); CHECK(last_written(0x10)==0x000A1A1Au); if (ModbusRegisters::fault==4 && stopped_at<0) stopped_at=i+1; }
    CHECK(stopped_at==10); printf("B8 drv_err replays and still stops at %d ok\n", stopped_at);
    // B9 longest log line
    size_t mx=0; reset_all(); buddy_push(0x000A1A1Au); stepperE0.drv=0xFFFFFFFE; stepperE0.gstat_reads={0x07}; g_now=4000000000u; tkr17536::events_total=4000000000u; tkr17536::events_not_logged=4000000000u; update_fault_status(); for (auto&s:g_logs) mx = s.size()>mx? s.size():mx;
    CHECK(mx < 128); printf("B9 longest line %zu chars (<128): %s\n", mx, g_logs.front().c_str());
    printf("ALL PASS\n"); return 0;
}
