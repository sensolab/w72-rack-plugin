// Host tests of Moleta's engine (src/MoletaEngine.hpp), with no Rack: `make test-engine`.
// Plain checks, no framework; exits non-zero if any fails.
#include "../src/MoletaEngine.hpp"
#include "../src/MoletaExpander.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace moleta;

static int checks = 0;
static int failures = 0;

#define CHECK(cond, ...) \
	do { \
		checks++; \
		if (!(cond)) { \
			failures++; \
			std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
			std::printf(__VA_ARGS__); \
			std::printf("\n"); \
		} \
	} while (0)

static const double FS = 48000.0;
static const double DT = 1.0 / FS;

// xorshift32: a fixed, repeatable uniform source in [0, 1).
struct Rng {
	uint32_t s;
	explicit Rng(uint32_t seed) : s(seed ? seed : 1) {}
	float operator()() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return (s >> 8) * (1.f / 16777216.f);
	}
};

// The base, minus Rack: a beat clock and four outputs in a chain.
struct Rig {
	BeatClock clock;
	OutputEngine outs[4];
	Controls c[4];
	Shared shared;
	ClockIn in;
	Tick tick;
	Rng rng;
	float v[4] = {};
	long n = 0;
	std::vector<long> rises[4];
	// Length of each finished high stretch, per output.
	std::vector<long> highs[4];
	long highFor[4] = {};

	explicit Rig(uint32_t seed = 1) : rng(seed) {
		for (int i = 0; i < 4; i++) {
			c[i].length = 0.f;
			outs[i].coin.fill(rng);
		}
	}

	void step() {
		tick = clock.process(DT, in);
		in.restart = false;
		in.edge = false;
		float prev[4];
		for (int i = 0; i < 4; i++)
			prev[i] = v[i];
		processChain(outs, c, 4, DT, tick, shared, rng, v);
		for (int i = 0; i < 4; i++) {
			if (v[i] > 5.f && prev[i] < 5.f)
				rises[i].push_back(n);
			if (v[i] > 5.f) {
				highFor[i]++;
			}
			else if (highFor[i] > 0) {
				highs[i].push_back(highFor[i]);
				highFor[i] = 0;
			}
		}
		n++;
	}

	void run(long samples) {
		for (long k = 0; k < samples; k++)
			step();
	}

	// Steps to the given sample, with a CLOCK edge on each listed one.
	void runEdges(const std::vector<long>& edges, long end) {
		size_t e = 0;
		while (n < end) {
			while (e < edges.size() && edges[e] < n)
				e++;
			if (e < edges.size() && edges[e] == n)
				in.edge = true;
			step();
		}
	}
};

static int ratioIndex(const char* name) {
	for (int i = 0; i < RATIO_COUNT; i++) {
		if (std::string(RATIOS[i].name) == name)
			return i;
	}
	return -1;
}

static long countIn(const std::vector<long>& v, long from, long to) {
	long k = 0;
	for (long x : v)
		k += x >= from && x < to;
	return k;
}

static bool contains(const std::vector<long>& v, long x) {
	for (long y : v) {
		if (y == x)
			return true;
	}
	return false;
}

static void testTable() {
	CHECK(RATIO_COUNT == 21, "21 ratios");
	const char* order[] = {"/16", "/8", "/7", "/6", "/5", "/4", "/3", "/2", "2/3", "3/4", "×1", "4/3", "3/2", "×2", "×3", "×4", "×5", "×6", "×7", "×8", "×16"};
	for (int i = 0; i < RATIO_COUNT; i++)
		CHECK(std::string(RATIOS[i].name) == order[i], "ratio %d is %s", i, order[i]);
	CHECK(std::string(RATIOS[DEFAULT_RATIOS[0]].name) == "×2", "default 1 is ×2");
	CHECK(std::string(RATIOS[DEFAULT_RATIOS[1]].name) == "/2", "default 2 is /2");
	CHECK(std::string(RATIOS[DEFAULT_RATIOS[2]].name) == "/3", "default 3 is /3");
	CHECK(std::string(RATIOS[DEFAULT_RATIOS[3]].name) == "/4", "default 4 is /4");
	CHECK(std::string(RATIOS[PLUS_DEFAULT_RATIOS[0]].name) == "×4", "default 5 is ×4");
	CHECK(std::string(RATIOS[PLUS_DEFAULT_RATIOS[1]].name) == "×1", "default 6 is ×1");
	CHECK(std::string(RATIOS[PLUS_DEFAULT_RATIOS[2]].name) == "/8", "default 7 is /8");
	CHECK(std::string(RATIOS[PLUS_DEFAULT_RATIOS[3]].name) == "3/2", "default 8 is 3/2");
	for (int i = 0; i < RATIO_COUNT; i++)
		CHECK(nearestRatio(RATIOS[i].p, RATIOS[i].q) == i, "a listed ratio maps back to its own step (%s)", RATIOS[i].name);
	CHECK(nearestRatio(4, 2) == ratioIndex("×2"), "4/2 lands on ×2");
	CHECK(nearestRatio(32, 1) == ratioIndex("×16"), "×32 lands on ×16");
	CHECK(nearestRatio(10, 1) == ratioIndex("×8"), "×10 lands on ×8, the nearer by octaves");
}

// Every ratio on the internal clock: p pulses per q beats, each within a sample
// of its ideal time, over two cycles.
static void testRatios() {
	const float bpms[] = {60.f, 120.f, 137.5f, 300.f};
	for (float bpm : bpms) {
		double ts = 60.0 / bpm * FS;
		for (int i = 0; i < RATIO_COUNT; i++) {
			const Ratio& r = RATIOS[i];
			Rig rig;
			rig.in.bpm = bpm;
			rig.c[0].ratio = i;
			double spacing = ts * r.q / r.p;
			rig.run((long) (2 * r.q * ts - spacing / 2));
			const std::vector<long>& got = rig.rises[0];
			CHECK((long) got.size() == 2 * r.p, "%s at %.1f BPM: %ld pulses over two cycles, want %d", r.name, bpm, (long) got.size(), 2 * r.p);
			double worst = 0.0;
			for (size_t k = 0; k < got.size(); k++)
				worst = std::max(worst, std::fabs(got[k] - k * spacing));
			CHECK(worst <= 1.0, "%s at %.1f BPM: a pulse %.2f samples from its time", r.name, bpm, worst);
		}
	}
}

// Beat edges whose period swings ±30% around base, a few percent per beat.
static std::vector<long> movingEdges(double base, int beats) {
	std::vector<long> e;
	double t = 100.0;
	for (int b = 0; b < beats; b++) {
		e.push_back((long) std::round(t));
		t += base * (1.0 + 0.3 * std::sin(2.0 * 3.141592653589793 * b / 40.0));
	}
	return e;
}

// A moving clock into CLOCK: dividers land on their edges, and every cycle holds
// exactly its p pulses, starting on its edge, so nothing drifts.
static void testMovingClock() {
	const char* names[4] = {"/4", "×3", "3/2", "4/3"};
	Rig rig;
	rig.in.external = true;
	for (int i = 0; i < 4; i++)
		rig.c[i].ratio = ratioIndex(names[i]);
	std::vector<long> edges = movingEdges(12000.0, 200);
	rig.runEdges(edges, edges.back() + 1);

	for (int i = 0; i < 4; i++) {
		const Ratio& r = RATIOS[rig.c[i].ratio];
		int badStart = 0, badCount = 0, cycles = 0;
		// The first cycle runs before T is known; check from beat 4 (a multiple of every q here but 3) on.
		for (size_t b = 4; b + r.q < edges.size(); b++) {
			if (b % r.q != 0)
				continue;
			cycles++;
			if (!contains(rig.rises[i], edges[b]))
				badStart++;
			if (countIn(rig.rises[i], edges[b], edges[b + r.q]) != r.p)
				badCount++;
		}
		CHECK(cycles > 40, "%s: %d cycles checked", r.name, cycles);
		CHECK(badStart == 0, "%s: %d cycles did not start on their edge", r.name, badStart);
		CHECK(badCount == 0, "%s: %d cycles without exactly %d pulses", r.name, badCount, r.p);
	}
	// /4 pulses only on its edges.
	long off = 0;
	for (long x : rig.rises[0]) {
		bool on = false;
		for (size_t b = 0; b < edges.size(); b += 4)
			on = on || edges[b] == x;
		off += !on;
	}
	CHECK(off == 0, "/4: %ld pulses off its edges", off);
}

// A sudden speed-up drops the pulse still pending at the restart instead of
// firing it late; a stopped clock goes quiet and inactive after two periods.
static void testDropAndStop() {
	Rig rig;
	rig.in.external = true;
	rig.c[0].ratio = ratioIndex("×4");
	std::vector<long> edges = {0, 24000, 48000, 60000, 72000, 84000};
	rig.runEdges(edges, 84000 + 10);
	CHECK(countIn(rig.rises[0], 48000, 60000) == 2, "×4 on a sudden speed-up: %ld pulses before the early edge, want 2", countIn(rig.rises[0], 48000, 60000));
	CHECK(contains(rig.rises[0], 60000), "×4 restarts on the early edge");
	CHECK(countIn(rig.rises[0], 60001, 63000) == 0, "×4 fires nothing late after the early edge");
	CHECK(countIn(rig.rises[0], 60000, 72000) == 4, "×4 holds 4 pulses once the new period is measured");

	long last = edges.back();
	while (rig.n < last + 24000 - 10)
		rig.step();
	CHECK(rig.tick.live, "still active just under two periods after the last edge");
	while (rig.n < last + 24000 + 10)
		rig.step();
	CHECK(!rig.tick.live, "inactive two periods after the last edge");
	rig.run(48000);
	CHECK(countIn(rig.rises[0], last + 12000, rig.n) == 0, "no pulse after the last cycle ends");

	// The clock comes back: active on its first edge, cycles running again.
	std::vector<long> again;
	for (int k = 0; k < 4; k++)
		again.push_back(rig.n + 10 + k * 12000);
	rig.runEdges(again, again.back() + 12000);
	CHECK(contains(rig.rises[0], again[0]), "the first edge after a stop pulses again");
	CHECK(countIn(rig.rises[0], again[2], again[3]) == 4, "×4 back to 4 pulses a beat after a stop");
}

// Before the second beat, T is unknown: only cycle restarts fire. Then pulling
// the cable hands back to BPM within one period.
static void testExternalStartAndHandOver() {
	Rig rig;
	rig.in.external = true;
	rig.in.pulsesPerBeat = 4;
	rig.c[0].ratio = ratioIndex("×1");
	rig.c[1].ratio = ratioIndex("×2");
	std::vector<long> edges;
	for (int k = 0; k < 16; k++)
		edges.push_back(1000 + k * 3000);
	rig.runEdges(edges, edges.back() + 100);
	// Beats on edges 0, 4, 8, 12.
	std::vector<long> want = {edges[0], edges[4], edges[8], edges[12]};
	CHECK(rig.rises[0] == want, "×1 on 4 pulses a beat fires on every fourth edge (%ld pulses)", (long) rig.rises[0].size());
	CHECK(countIn(rig.rises[1], edges[0], edges[4]) == 1, "×2 before T is known: only the restart");
	CHECK(countIn(rig.rises[1], edges[4], edges[8]) == 2, "×2 once T is known: two a beat");
	CHECK(std::fabs(rig.clock.measured - 12000 * DT) < 1e-9, "T is the time between beats");

	long lastBeat = edges[12];
	rig.in.external = false;
	rig.in.bpm = 120.f;
	long before = (long) rig.rises[0].size();
	rig.run(lastBeat + 24000 + 2 - rig.n);
	CHECK((long) rig.rises[0].size() == before + 1, "the BPM's first beat comes one period after the last external beat");
	if ((long) rig.rises[0].size() == before + 1)
		CHECK(std::labs(rig.rises[0].back() - (lastBeat + 24000)) <= 1, "and lands on it (%ld)", rig.rises[0].back() - lastBeat);
}

// The pass results of one output's due pulses, collected over a run.
static std::vector<bool> flips(Rig& rig, long samples, int out = 0) {
	std::vector<bool> got;
	for (long k = 0; k < samples; k++) {
		rig.step();
		if (rig.outs[out].due)
			got.push_back(rig.outs[out].passed);
	}
	return got;
}

static void testCoin() {
	long beat = 24000;
	{
		Rig rig;
		rig.c[0].ratio = ratioIndex("×4");
		rig.c[0].chance = 0.f;
		rig.run(8 * beat);
		CHECK(rig.rises[0].empty(), "CHANCE 0 passes nothing");
	}
	{
		Rig rig;
		rig.c[0].ratio = ratioIndex("×4");
		rig.run(8 * beat - 10);
		CHECK(rig.rises[0].size() == 32, "CHANCE 100%% passes everything (%ld of 32)", (long) rig.rises[0].size());
	}
	{
		// REPEAT fully left: fresh coins, no loop.
		Rig rig;
		rig.c[0].ratio = ratioIndex("×1");
		rig.c[0].chance = 0.5f;
		std::vector<bool> f = flips(rig, 64 * beat);
		int same = 0;
		for (size_t i = 0; i + 16 < f.size(); i++)
			same += f[i] == f[i + 16];
		CHECK(same < (int) f.size() - 16, "REPEAT fully left does not loop");
	}
	const int loops[2] = {16, 8};
	for (int loop : loops) {
		Rig rig;
		rig.c[0].ratio = ratioIndex("×1");
		rig.c[0].chance = 0.5f;
		rig.shared.repeat = 1.f;
		rig.shared.loopLength = loop;
		std::vector<bool> f = flips(rig, 4 * loop * beat);
		int differ = 0, passes = 0;
		for (size_t i = 0; i + loop < f.size(); i++)
			differ += f[i] != f[i + loop];
		for (bool b : f)
			passes += b;
		CHECK(f.size() >= (size_t) 3 * loop, "loop %d: %ld flips", loop, (long) f.size());
		CHECK(differ == 0, "REPEAT fully right repeats a pattern of %d (%d differ)", loop, differ);
		CHECK(passes > 0 && passes < (int) f.size(), "a locked pattern at 50%% has both results");
	}
	{
		// Lowering CHANCE on a locked pattern only removes pulses; raising it puts them back.
		Rig rig;
		rig.c[0].ratio = ratioIndex("×1");
		rig.shared.repeat = 1.f;
		rig.c[0].chance = 0.7f;
		std::vector<bool> high = flips(rig, 16 * beat);
		rig.c[0].chance = 0.4f;
		std::vector<bool> low = flips(rig, 16 * beat);
		rig.c[0].chance = 0.7f;
		std::vector<bool> back = flips(rig, 16 * beat);
		bool subset = high.size() == 16 && low.size() == 16;
		int removed = 0;
		for (size_t i = 0; subset && i < 16; i++) {
			subset = !low[i] || high[i];
			removed += high[i] && !low[i];
		}
		CHECK(subset, "the pulses at 40%% are a subset of those at 70%%");
		CHECK(removed > 0, "lowering CHANCE removed some pulses");
		CHECK(back == high, "raising CHANCE back restores the same phrase");
	}
	{
		// DENSITY adds 10% per volt, clamped.
		Rig rig;
		rig.c[0].ratio = ratioIndex("×1");
		rig.c[0].chance = 0.5f;
		rig.shared.density = 5.f;
		std::vector<bool> up = flips(rig, 16 * beat);
		rig.shared.density = -5.f;
		std::vector<bool> down = flips(rig, 16 * beat);
		int u = 0, d = 0;
		for (bool b : up)
			u += b;
		for (bool b : down)
			d += b;
		CHECK(u == (int) up.size() && !up.empty(), "+5 V of DENSITY over 50%% passes everything");
		CHECK(d == 0, "-5 V of DENSITY under 50%% passes nothing");
	}
	{
		// RESET restarts the pointers: a locked loop replays from its first slot.
		Rig rig;
		rig.c[0].ratio = ratioIndex("×1");
		rig.c[0].chance = 0.5f;
		rig.shared.repeat = 1.f;
		std::vector<bool> first = flips(rig, 16 * beat - 10);
		flips(rig, 5 * beat);
		CHECK(rig.outs[0].coin.pointer == 5, "pointer at 5 before the reset (%d)", rig.outs[0].coin.pointer);
		rig.in.restart = true;
		std::vector<bool> after = flips(rig, 16 * beat - 10);
		CHECK(first.size() == 16 && after == first, "after RESET the loop starts again from its first flip");
	}
}

static void testLength() {
	CHECK(gateFraction(0.f) == 0.f, "LENGTH fully left is the trigger");
	CHECK(std::fabs(gateFraction(0.5f) - 0.5f) < 1e-6f, "LENGTH at 50%% is half the period");
	CHECK(gateFraction(1.f) < 1.f, "LENGTH never reaches 100%%");
	CHECK(std::fabs(gateFraction(LENGTH_TRIGGER_TOP) - 0.05f) < 0.01f, "the gate starts near 5%%");

	Rig rig;
	rig.c[0].ratio = ratioIndex("×1");
	rig.c[0].length = 0.f;
	rig.c[1].ratio = ratioIndex("×2");
	rig.c[1].length = 0.5f;
	rig.c[2].ratio = ratioIndex("×2");
	rig.c[2].length = 1.f;
	rig.c[3].ratio = ratioIndex("/2");
	rig.c[3].length = 0.25f;
	rig.run(8 * 24000 + 100);
	auto within = [](const std::vector<long>& v, long want) {
		bool ok = !v.empty();
		for (long x : v)
			ok = ok && std::labs(x - want) <= 1;
		return ok;
	};
	CHECK(within(rig.highs[0], 48), "the trigger is 1 ms (%ld samples)", rig.highs[0].empty() ? 0 : rig.highs[0][0]);
	CHECK(within(rig.highs[1], 6000), "×2 at 50%%: a quarter beat (%ld samples)", rig.highs[1].empty() ? 0 : rig.highs[1][0]);
	CHECK(within(rig.highs[2], 11400), "×2 at 95%%: 95%% of its own period (%ld samples)", rig.highs[2].empty() ? 0 : rig.highs[2][0]);
	// Beats 0 to 8 fall in the run: 17 pulses at ×2.
	CHECK(rig.rises[2].size() == 17, "every 95%% gate is its own pulse (%ld of 17)", (long) rig.rises[2].size());
	long g = (long) std::round(gateFraction(0.25f) * 48000);
	CHECK(within(rig.highs[3], g), "/2 at 25%%: %ld samples, want %ld", rig.highs[3].empty() ? 0 : rig.highs[3][0], g);

	// A clock speeding up under a 95% gate: each beat still retriggers.
	Rig fast;
	fast.in.external = true;
	fast.c[0].ratio = ratioIndex("×1");
	fast.c[0].length = 1.f;
	std::vector<long> edges = {0, 24000, 48000, 60000, 66000, 69000, 72000};
	fast.runEdges(edges, 80000);
	CHECK(fast.rises[0].size() == edges.size(), "a gate overlapping the next beat drops for a sample and retriggers (%ld of %ld)",
		(long) fast.rises[0].size(), (long) edges.size());
}

static void testFollow() {
	Rig rig(7);
	rig.c[0].ratio = ratioIndex("/2");
	rig.c[0].chance = 0.5f;
	rig.c[1].ratio = ratioIndex("×2");
	rig.c[1].follow = true;
	rig.c[2].ratio = ratioIndex("×4");
	rig.c[2].chance = 0.7f;
	rig.c[2].follow = true;
	bool prev0 = true;
	int wrong1 = 0, wrong2 = 0, sameSample = 0, due2 = 0, blocked2 = 0;
	for (long k = 0; k < 400L * 24000; k++) {
		rig.step();
		const OutputEngine* o = rig.outs;
		if (o[1].due) {
			// CHANCE 100%: output 2 passes exactly when output 1's latest pulse passed.
			wrong1 += o[1].passed != o[0].passed;
			if (o[0].due && o[0].passed != prev0)
				sameSample++;
		}
		if (o[2].due) {
			due2++;
			wrong2 += o[2].passed && !o[1].passed;
			blocked2 += !o[1].passed;
		}
		if (o[0].due)
			prev0 = o[0].passed;
	}
	CHECK(wrong1 == 0, "output 2 follows output 1 (%d wrong)", wrong1);
	CHECK(sameSample > 10, "same-sample pulses see the new result above (%d cases)", sameSample);
	CHECK(wrong2 == 0, "output 3 follows output 2, so the chain cascades (%d wrong)", wrong2);
	CHECK(blocked2 > 0 && blocked2 < due2, "output 3 blocked by the chain sometimes (%d of %d)", blocked2, due2);

	// A restart sets the followed result back to passed.
	rig.c[0].chance = 0.f;
	rig.run(4 * 24000);
	CHECK(!rig.outs[0].passed, "output 1 at CHANCE 0 failed");
	rig.in.restart = true;
	rig.in.running = false;
	rig.step();
	CHECK(rig.outs[0].passed, "after a restart the chain starts passed");
}

// RUN off silences everything and freezes the clock; on again restarts like RESET.
static void testRun() {
	Rig rig;
	rig.c[0].ratio = ratioIndex("×1");
	rig.c[0].length = 1.f;
	rig.run(36000);
	rig.in.running = false;
	rig.step();
	CHECK(rig.v[0] == 0.f, "stopping cuts a gate");
	rig.run(48000);
	CHECK(countIn(rig.rises[0], 36000, rig.n) == 0, "nothing while stopped");
	rig.in.running = true;
	rig.in.restart = true;
	long on = rig.n;
	rig.run(24000 + 10);
	CHECK(contains(rig.rises[0], on), "RUN on starts a beat at once");
	CHECK(contains(rig.rises[0], on + 24000) || contains(rig.rises[0], on + 24001), "and the next one a period later");
}

// The message round trip: output engines fed by the message one sample late give
// what the base's own engines give, delayed by one sample. A clock that moves,
// stops, resets and switches source, with REPEAT, DENSITY and the loop all moving.
static void testMessage() {
	const uint32_t seed = 11;
	Rig base(seed);
	Rng rng(seed);
	OutputEngine plus[4];
	for (int i = 0; i < 4; i++) {
		// Fill in the same order as the base, so both coin loops and random streams match.
		plus[i].coin.fill(rng);
	}
	const char* names[4] = {"3/2", "×3", "/3", "×16"};
	for (int i = 0; i < 4; i++) {
		base.c[i].ratio = ratioIndex(names[i]);
		base.c[i].chance = 0.6f;
		base.c[i].length = 0.3f * i;
		base.c[i].follow = i > 0;
	}
	base.shared.repeat = 0.5f;
	base.in.external = true;
	std::vector<long> edges = movingEdges(9000.0, 120);

	MoletaMessage producer, consumer;
	SampleDelay<4> delay;
	OutputEngine late[4];
	Rng lateRng(99);
	for (int i = 0; i < 4; i++)
		late[i].coin.fill(lateRng);
	Controls always[4];
	for (int i = 0; i < 4; i++) {
		always[i] = base.c[i];
		always[i].chance = 1.f;
		always[i].follow = false;
	}
	OutputEngine baseAlways[4];
	Rng alwaysRng(5);
	SampleDelay<4> alwaysDelay;
	// Attached at sample 50000; checked from when every cycle (/3 at most ~36000 samples) has restarted twice.
	const long attach = 50000;
	const long lateFrom = attach + 80000;

	int mismatches = 0, lateMismatches = 0;
	size_t e = 0;
	const long end = 1200000;
	for (long n = 0; n < end; n++) {
		while (e < edges.size() && edges[e] < n)
			e++;
		base.in.edge = e < edges.size() && edges[e] == n;
		if (n == 300000)
			base.in.restart = true;
		if (n == 400000)
			base.shared.loopLength = 8;
		if (n == 500000)
			base.in.running = false;
		if (n == 520000) {
			base.in.running = true;
			base.in.restart = true;
		}
		if (n == 900000)
			base.in.external = false;
		base.shared.density = 2.f * (float) std::sin(n * 1e-4);
		base.step();
		producer.tick = base.tick;
		producer.shared = base.shared;

		float plusV[4], baseV[4];
		processChain(plus, base.c, 4, DT, consumer.tick, consumer.shared, rng, plusV);
		for (int i = 0; i < 4; i++)
			baseV[i] = base.v[i];
		delay.process(baseV, true);
		for (int i = 0; i < 4; i++)
			mismatches += plusV[i] != baseV[i];

		// An expander attached mid-run: once its slowest cycle has restarted, it lines up.
		float a[4], b[4];
		// Without DENSITY, CHANCE 100% passes everything whatever the random stream.
		Shared quiet = base.shared;
		quiet.density = 0.f;
		Shared lateQuiet = consumer.shared;
		lateQuiet.density = 0.f;
		processChain(baseAlways, always, 4, DT, base.tick, quiet, alwaysRng, a);
		alwaysDelay.process(a, true);
		if (n >= attach)
			processChain(late, always, 4, DT, consumer.tick, lateQuiet, lateRng, b);
		if (n >= lateFrom && n < 300000) {
			for (int i = 0; i < 4; i++)
				lateMismatches += a[i] != b[i];
		}

		// Rack flips the buffers at the end of the sample.
		consumer = producer;
	}
	CHECK(mismatches == 0, "message round trip: %d samples differ from the base delayed by one", mismatches);
	CHECK(base.rises[0].size() > 50 && base.rises[3].size() > 50, "the round trip had pulses to compare (%ld, %ld)",
		(long) base.rises[0].size(), (long) base.rises[3].size());
	CHECK(lateMismatches == 0, "an expander attached mid-run lines up with the base (%d samples differ)", lateMismatches);

	// The delay itself: one sample while on, none while off, no gap when it turns on.
	SampleDelay<2> d;
	float v[2] = {1.f, 2.f};
	d.process(v, false);
	CHECK(v[0] == 1.f && v[1] == 2.f, "the delay passes through while off");
	float w[2] = {3.f, 4.f};
	d.process(w, true);
	CHECK(w[0] == 1.f && w[1] == 2.f, "the delay turned on gives the previous sample");
}

static void testClick() {
	auto peak = [](bool accent) {
		Click c;
		c.trigger(accent);
		float p = 0.f;
		for (int k = 0; k < 2400; k++)
			p = std::max(p, std::fabs(c.process((float) DT)));
		return p;
	};
	CHECK(peak(true) > peak(false), "the accent is louder");
	Click c;
	c.trigger(true);
	float tail = 0.f;
	for (int k = 0; k < 9600; k++) {
		float x = std::fabs(c.process((float) DT));
		if (k > 4800)
			tail = std::max(tail, x);
	}
	CHECK(tail == 0.f, "the click is over within 100 ms");
}

int main() {
	testTable();
	testRatios();
	testMovingClock();
	testDropAndStop();
	testExternalStartAndHandOver();
	testCoin();
	testLength();
	testFollow();
	testRun();
	testMessage();
	testClick();
	std::printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
