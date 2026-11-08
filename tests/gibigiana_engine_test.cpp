// Host tests of Gibigiana's engine (src/GibigianaEngine.hpp), with no Rack: `make test-engine`.
// Plain checks, no framework; exits non-zero if any fails.
#include "../src/GibigianaEngine.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

using namespace gibigiana;

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

static const float FS = 48000.f;
static const double PI = 3.14159265358979;

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

// The module, minus Rack: a clock every `period` samples, GATE unpatched unless
// gate edges are given, and the step's input from a function of the sample index.
struct Rig {
	Engine e;
	Knobs k;
	Frame f;
	Rng rng;
	long n = 0;
	long period;
	// The hit decisions, in order.
	std::vector<uint8_t> hits;

	explicit Rig(long period, uint32_t seed = 1) : rng(seed), period(period) {
		e.setSampleRate(FS);
		e.fillCoins(rng);
		f.clock = true;
		k.lanes = 4;
		for (int i = 0; i < LANES; i++) {
			k.chance[i] = 0.f;
			k.level[i] = 1.f;
			k.pan[i] = -1.f;
		}
		k.chance[0] = 1.f;
		k.step[0] = 3; // 1 step
	}

	Out step(float in, bool gateEdge = false) {
		f.in = in;
		f.clockEdge = period > 0 && n % period == 0;
		f.gateEdge = gateEdge;
		bool hit = f.gate ? gateEdge : f.clock && f.clockEdge;
		Out o = e.process(f, k, rng);
		if (hit)
			hits.push_back(e.hitMask);
		n++;
		f.reset = false;
		return o;
	}

	void run(long samples) {
		for (long i = 0; i < samples; i++)
			step(0.f);
	}
};

static void testSteps() {
	CHECK(STEP_COUNT == 14, "%d steps", STEP_COUNT);
	for (int i = 1; i < STEP_COUNT; i++)
		CHECK(STEPS[i].p * STEPS[i - 1].q > STEPS[i - 1].p * STEPS[i].q, "step %d not above step %d", i, i - 1);
	for (int i = 0; i < STEP_COUNT; i++)
		CHECK(nearestStep(STEPS[i].p, STEPS[i].q) == i, "step %d does not round-trip", i);
	CHECK(nearestStep(5, 4) == 4, "5/4 lands on %d, want 1 1/3", nearestStep(5, 4));
	CHECK(std::strcmp(STEPS[DEFAULT_STEPS[0]].name, "1 1/2") == 0, "tap 1 defaults to %s", STEPS[DEFAULT_STEPS[0]].name);
}

static void testClockPeriod() {
	ClockPeriod c;
	// The first call after a clear starts with an edge; later calls continue from the last one.
	auto edges = [&](const std::vector<long>& intervals) {
		if (!c.started)
			c.process(true);
		for (long iv : intervals) {
			for (long i = 1; i < iv; i++)
				c.process(false);
			c.process(true);
		}
	};
	edges({});
	CHECK(c.period == 0.0, "period known after one edge");
	c.clear();
	edges({1000, 1000});
	CHECK(c.period == 1000.0, "steady clock reads %.1f", c.period);
	c.clear();
	edges({999, 1001, 999, 1001});
	CHECK(c.period == 1000.0, "jittery clock reads %.1f", c.period);
	c.clear();
	edges({1200, 800, 1200, 800});
	CHECK(c.period == 1000.0, "swung clock reads %.1f, want its average", c.period);
	c.clear();
	edges({1000, 1000, 5000});
	CHECK(c.period == 1000.0, "a stopped clock's gap changed the period to %.1f", c.period);
	edges({1000});
	CHECK(c.period == 1000.0, "after a gap, %.1f", c.period);
	c.clear();
	edges({1000, 1000, 3000, 3000});
	CHECK(c.period == 3000.0, "a slower tempo, two long intervals in a row, reads %.1f", c.period);
	c.clear();
	edges({1000, 1000, 500});
	CHECK(c.period == 750.0, "a faster tempo's first interval reads %.1f", c.period);
	edges({500});
	CHECK(c.period == 500.0, "a faster tempo settles at %.1f", c.period);
}

static void testSilentUntilPeriod() {
	Rig r(12000);
	float peak = 0.f;
	for (long i = 0; i < 12000; i++) {
		Out o = r.step((float) std::sin(i * 0.05));
		peak = std::max(peak, std::fabs(o.tap[0]) + std::fabs(o.left));
	}
	CHECK(peak == 0.f, "a tap sounded before the period was known (%.3f)", peak);
}

// An impulse 100 samples after each clock returns exactly one period later, at full level.
static void testDelayExact() {
	const long N = 4800;
	Rig r(N);
	std::vector<float> out;
	for (long i = 0; i < 8 * N; i++) {
		float in = i % N == 100 ? 1.f : 0.f;
		out.push_back(r.step(in).tap[0]);
	}
	for (long c = 3; c < 7; c++) {
		long at = c * N + 100;
		CHECK(std::fabs(out[at] - 1.f) < 1e-6f, "impulse at %ld came back as %.4f", at - N, out[at]);
		CHECK(out[at - 1] == 0.f && out[at + 1] == 0.f, "impulse at %ld smeared", at - N);
	}
	// 1½ steps: the same impulse 1.5 periods later.
	Rig s(N);
	s.k.step[0] = 5;
	out.clear();
	for (long i = 0; i < 8 * N; i++)
		out.push_back(s.step(i == 3 * N + 100 ? 1.f : 0.f).tap[0]);
	CHECK(std::fabs(out[3 * N + 100 + 3 * N / 2] - 1.f) < 1e-6f, "1 1/2 steps: %.4f", out[3 * N + 100 + 3 * N / 2]);
}

static long e_ramp() {
	return (long) std::lround(GATE_RAMP * FS);
}

// The coin rides with the audio: a held tone comes back hit by hit, each hit's
// stretch whole or silent as its decision says, with the fade ending on the hit.
static void testCoinPerHit() {
	const long N = 4800;
	Rig r(N, 7);
	r.k.chance[0] = 0.5f;
	std::vector<float> out;
	for (long i = 0; i < 40 * N; i++)
		out.push_back(r.step(1.f).tap[0]);
	int on = 0, off = 0;
	for (long h = 3; h < 38; h++) {
		bool pass = r.hits[h] & 1;
		pass ? on++ : off++;
		// Hit h's audio, [hN, (h+1)N), comes back over [(h+1)N, (h+2)N).
		long start = (h + 1) * N;
		float mid = out[start + N / 2];
		CHECK(std::fabs(mid - (pass ? 1.f : 0.f)) < 1e-6f, "hit %ld (%s) came back at %.3f", h, pass ? "on" : "off", mid);
		bool prev = r.hits[h - 1] & 1;
		if (prev && !pass) {
			CHECK(out[start] == 0.f, "hit %ld: the fade out has not ended on the hit (%.3f)", h, out[start]);
			CHECK(out[start - e_ramp()] > 0.98f, "hit %ld: the fade out started early", h);
		}
		if (!prev && pass)
			CHECK(out[start] == 1.f, "hit %ld: the attack is not whole (%.3f)", h, out[start]);
	}
	CHECK(on > 8 && off > 8, "a 50%% chance passed %d of %d hits", on, on + off);
	// The other taps roll their own coins.
	Rig two(N, 7);
	two.k.chance[0] = two.k.chance[1] = 0.5f;
	two.run(40 * N);
	int differ = 0;
	for (uint8_t m : two.hits)
		differ += ((m & 1) != ((m >> 1) & 1));
	CHECK(differ > 5, "taps 1 and 2 disagreed on only %d of %zu hits", differ, two.hits.size());
}

static void testGate() {
	const long N = 4800;
	Rig r(N, 3);
	r.f.gate = true;
	r.k.chance[0] = 0.5f;
	const long edges[] = {1000, 1700, 9000, 9100, 30000};
	size_t next = 0;
	for (long i = 0; i < 40000; i++) {
		bool edge = next < 5 && i == edges[next];
		if (edge)
			next++;
		r.step(0.f, edge);
	}
	CHECK(r.hits.size() == 5, "%zu hits from 5 gate edges and many clocks", r.hits.size());
}

static void testRepeatAndReset() {
	for (int loop : {16, 8}) {
		Rig r(480, 11);
		r.k.chance[0] = 0.5f;
		r.k.repeat = 1.f;
		r.k.loopLength = loop;
		r.run(480 * (3 * loop + 1));
		bool same = true;
		for (size_t i = loop; i < r.hits.size(); i++)
			same = same && r.hits[i] == r.hits[i - loop];
		CHECK(same, "REPEAT fully right does not loop every %d hits", loop);
		// RESET starts the loop from its first slot.
		std::vector<uint8_t> first(r.hits.begin(), r.hits.begin() + 3);
		r.f.reset = true;
		r.hits.clear();
		long until = r.n + 480 - r.n % 480;
		r.run(until - r.n + 480 * 3);
		CHECK(r.hits.size() >= 3 && r.hits[0] == first[0] && r.hits[1] == first[1] && r.hits[2] == first[2], "RESET did not restart the %d-hit loop", loop);
	}
}

static void testDensity() {
	Rig r(480, 5);
	r.k.chance[0] = 0.f;
	r.f.density = 10.f;
	r.run(480 * 20);
	int all = 0;
	for (uint8_t m : r.hits)
		all += m & 1;
	CHECK(all == (int) r.hits.size(), "10 V of DENSITY over CHANCE 0 passed %d of %zu", all, r.hits.size());
	Rig s(480, 5);
	s.f.density = -10.f;
	s.run(480 * 20);
	int none = 0;
	for (uint8_t m : s.hits)
		none += m & 1;
	CHECK(none == 0, "-10 V of DENSITY under CHANCE 100%% passed %d", none);
}

// A tempo jump: the tap crossfades to its new place, smoothly, and lands exactly there.
static void testTempoJump() {
	Rig r(4800);
	const double w = 2 * PI * 220 / FS;
	float prev = 0.f, worst = 0.f;
	long jump = 6 * 4800;
	for (long i = 0; i < 12 * 4800; i++) {
		if (i == jump)
			r.period = 3600; // the clock speeds up; the next edges come every 3600
		Out o = r.step((float) std::sin(w * i));
		if (i > 4 * 4800)
			worst = std::max(worst, std::fabs(o.tap[0] - prev));
		prev = o.tap[0];
	}
	// A 220 Hz sine moves at most 0.029 per sample; a cut would jump by up to 2.
	CHECK(worst < 0.06f, "the tap jumped by %.3f in one sample across a tempo change", worst);
	CHECK(r.e.taps[0].cur.delay == r.e.taps[0].target && std::abs(r.e.taps[0].target - 3600) <= 1, "the tap settled at %d samples, want 3600", r.e.taps[0].cur.delay);
}

static void testReach() {
	Rig r((long) (1.5f * FS));
	r.k.step[0] = 13; // 16 steps: 24 s
	r.k.step[1] = 12; // 12 steps: 18 s
	r.run((long) (5 * FS));
	CHECK(!r.e.taps[0].reach && r.e.taps[0].cur.delay == -1, "16 steps of 1.5 s did not fall silent");
	CHECK(r.e.taps[1].reach && r.e.taps[1].cur.delay > 0, "12 steps of 1.5 s fell silent");
	CHECK(r.e.maxDelay() >= (int) (BUFFER_SECONDS * FS), "the buffer holds %d samples", r.e.maxDelay());
}

static void testSumAndPatched() {
	const long N = 4800;
	Rig r(N);
	Out o;
	for (long i = 0; i < 5 * N; i++)
		o = r.step(1.f);
	CHECK(std::fabs(o.tap[0] - 1.f) < 1e-6f && std::fabs(o.left - 1.f) < 1e-6f && std::fabs(o.right) < 1e-6f, "hard left: tap %.3f, L %.3f, R %.3f", o.tap[0], o.left, o.right);
	r.k.pan[0] = 0.f;
	o = r.step(1.f);
	CHECK(std::fabs(o.left - 0.7071f) < 1e-3f && std::fabs(o.right - 0.7071f) < 1e-3f, "centred: L %.3f, R %.3f", o.left, o.right);
	r.k.patched[0] = true;
	o = r.step(1.f);
	CHECK(o.left == 0.f && o.right == 0.f && o.tap[0] > 0.99f, "a patched tap stayed in the sum");
	r.k.keepPatched = true;
	o = r.step(1.f);
	CHECK(o.left > 0.7f, "Patched tap stays in the sum: L %.3f", o.left);
}

static void testSoftClip() {
	CHECK(softClip(3.f) == 3.f && softClip(-5.f) == -5.f, "the clip bends below the knee");
	CHECK(softClip(40.f) < CLIP_CEILING && softClip(-40.f) > -CLIP_CEILING, "the clip passes the ceiling");
	CHECK(std::fabs(softClip(5.001f) - 5.001f) < 1e-4f, "the clip has a corner at the knee");
	float last = 0.f;
	bool rising = true;
	for (float x = 0.f; x < 50.f; x += 0.1f) {
		rising = rising && softClip(x) >= last;
		last = softClip(x);
	}
	CHECK(rising, "the clip is not monotonic");
}

static void testDuck() {
	const long N = 4800;
	for (float amount : {1.f, 0.f}) {
		Rig r(N);
		r.k.duck = amount;
		r.f.duck = 10.f;
		float out = 0.f;
		for (long i = 0; i < 6 * N; i++)
			out = r.step(1.f).tap[0];
		if (amount > 0.f)
			CHECK(std::fabs(out) < 1e-3f, "DUCK fully on left %.3f", out);
		else
			CHECK(std::fabs(out - 1.f) < 1e-6f, "AMOUNT 0 still ducked to %.3f", out);
	}
}

static void testBand() {
	const long N = 4800;
	auto level = [&](float centre, float width, double hz) {
		Rig r(N);
		r.k.centre = centre;
		r.k.width = width;
		double w = 2 * PI * hz / FS;
		float peak = 0.f;
		for (long i = 0; i < 8 * N; i++) {
			float y = r.step((float) std::sin(w * i)).tap[0];
			if (i > 6 * N)
				peak = std::max(peak, std::fabs(y));
		}
		return peak;
	};
	CHECK(std::fabs(level(0.5f, 1.f, 5000) - 1.f) < 1e-4f, "fully open, the band coloured a 5 kHz sine");
	CHECK(level(0.f, 0.f, 5000) < 0.01f, "a 20 Hz band let 5 kHz through at %.3f", level(0.f, 0.f, 5000));
	float atCentre = level(0.5f, 0.3f, 632);
	CHECK(atCentre > 0.6f, "the band's centre came through at only %.3f", atCentre);
}

static void testClockPulled() {
	const long N = 4800;
	Rig r(N);
	r.run(4 * N);
	CHECK(r.e.taps[0].cur.delay == N, "the tap is at %d samples", r.e.taps[0].cur.delay);
	r.f.clock = false;
	float out = 1.f;
	for (long i = 0; i < 4 * N; i++)
		out = r.step(1.f).tap[0];
	CHECK(r.e.taps[0].cur.delay == -1 && out == 0.f, "pulling CLOCK left the tap sounding");
}

// A lane two expanders down the chain: its tap out is early by its lag, so it lands on
// the grid after crossing the chain, and its share of the sum is exactly on the grid.
static void testLag() {
	const long N = 4800;
	const int LAG = 3;
	Rig r(N);
	r.k.chance[0] = 0.f;
	r.k.chance[1] = 1.f;
	r.k.step[1] = 3; // 1 step
	r.k.lag[1] = LAG;
	std::vector<float> tap, left;
	for (long i = 0; i < 8 * N; i++) {
		Out o = r.step(i % N == 100 ? 1.f : 0.f);
		tap.push_back(o.tap[1]);
		left.push_back(o.left);
	}
	for (long c = 3; c < 7; c++) {
		long at = c * N + 100;
		CHECK(std::fabs(tap[at - LAG] - 1.f) < 1e-6f && tap[at] == 0.f, "lag %d: the tap out is not %d samples early (%.3f at the grid)", LAG, LAG, tap[at]);
		CHECK(std::fabs(left[at] - 1.f) < 1e-6f && left[at - LAG] == 0.f, "lag %d: the sum is off the grid (%.3f)", LAG, left[at]);
	}
}

// A lane that leaves fades out over the coin ramp and stops; one that joins stays
// silent until a hit has rolled for it and that hit's audio comes back.
static void testLanesComeAndGo() {
	const long N = 4800;
	const long ramp = e_ramp();
	Rig r(N);
	r.k.lanes = 2;
	r.k.chance[0] = 0.f;
	r.k.chance[1] = 1.f;
	r.k.step[1] = 3;
	float o = 0.f;
	for (long i = 0; i < 5 * N + N / 2; i++)
		o = r.step(1.f).tap[1];
	CHECK(std::fabs(o - 1.f) < 1e-6f, "lane 2 plays at %.3f", o);
	r.k.lanes = 1;
	float last = o;
	bool falling = true;
	for (long i = 0; i < ramp; i++) {
		o = r.step(1.f).tap[1];
		falling = falling && o < last;
		last = o;
	}
	CHECK(falling && o == 0.f, "lane 2 did not fade out over the ramp (%.3f)", o);
	CHECK(r.e.taps[1].cur.delay == -1, "lane 2 still reads after leaving");
	// Back in the chain before the next hit, while the buffer still holds audio its
	// coin passed before it left.
	while (r.n % N != 3 * N / 4)
		r.step(1.f);
	r.k.lanes = 2;
	long next = r.n - r.n % N + N;
	float early = 0.f;
	while (r.n < next + N - ramp)
		early = std::max(early, std::fabs(r.step(1.f).tap[1]));
	CHECK(early == 0.f, "lane 2 sounded before its first hit came back (%.3f)", early);
	while (r.n < next + N + N / 2)
		o = r.step(1.f).tap[1];
	CHECK(std::fabs(o - 1.f) < 1e-6f, "lane 2 came back at %.3f after its first hit", o);
}

static void testLaneCv() {
	const long N = 480;
	Rig up(N, 9);
	up.k.chance[0] = 0.f;
	up.k.chanceCv[0] = 10.f;
	up.run(N * 20);
	int pass = 0;
	for (uint8_t m : up.hits)
		pass += m & 1;
	CHECK(pass == (int) up.hits.size(), "10 V of CHANCE CV over CHANCE 0 passed %d of %zu", pass, up.hits.size());
	Rig down(N, 9);
	down.k.chanceCv[0] = -10.f;
	down.run(N * 20);
	pass = 0;
	for (uint8_t m : down.hits)
		pass += m & 1;
	CHECK(pass == 0, "-10 V of CHANCE CV under CHANCE 100%% passed %d", pass);

	Rig r(4800);
	r.k.level[0] = 0.f;
	r.k.levelCv[0] = 5.f;
	r.k.pan[0] = 0.f;
	r.k.panCv[0] = 5.f;
	Out o;
	for (long i = 0; i < 5 * 4800; i++)
		o = r.step(1.f);
	CHECK(std::fabs(o.tap[0] - 0.5f) < 1e-6f, "5 V of LEVEL CV over LEVEL 0: %.3f", o.tap[0]);
	CHECK(std::fabs(o.left) < 1e-6f && std::fabs(o.right - 0.5f) < 1e-6f, "5 V of PAN CV from the centre: L %.3f, R %.3f", o.left, o.right);
	r.k.levelCv[0] = 20.f;
	r.k.panCv[0] = -20.f;
	o = r.step(1.f);
	CHECK(std::fabs(o.tap[0] - 1.f) < 1e-6f && std::fabs(o.left - 1.f) < 1e-6f && std::fabs(o.right) < 1e-6f, "the CVs pass their range: tap %.3f, L %.3f, R %.3f", o.tap[0], o.left, o.right);
}

// The full chain: eight lanes, eight coins, eight bits in the mask, each lane at its own step.
static void testEightLanes() {
	const long N = 4800;
	Rig r(N, 13);
	r.k.lanes = LANES;
	for (int i = 0; i < LANES; i++) {
		r.k.chance[i] = 0.5f;
		r.k.step[i] = DEFAULT_STEPS[i];
	}
	r.run(40 * N);
	int high = 0;
	for (uint8_t m : r.hits)
		high += (m >> 7) & 1;
	CHECK(high > 5 && high < (int) r.hits.size() - 5, "lane 8 passed %d of %zu hits", high, r.hits.size());
	for (int i = 0; i < LANES; i++) {
		const Step& s = STEPS[DEFAULT_STEPS[i]];
		CHECK(r.e.taps[i].cur.delay == (int) (N * s.p / s.q), "lane %d reads at %d samples", i + 1, r.e.taps[i].cur.delay);
	}
	CHECK(std::strcmp(STEPS[DEFAULT_STEPS[LANES - 1]].name, "8") == 0, "lane 8 defaults to %s", STEPS[DEFAULT_STEPS[LANES - 1]].name);
}

int main() {
	testSteps();
	testClockPeriod();
	testSilentUntilPeriod();
	testDelayExact();
	testCoinPerHit();
	testGate();
	testRepeatAndReset();
	testDensity();
	testTempoJump();
	testReach();
	testSumAndPatched();
	testSoftClip();
	testDuck();
	testBand();
	testClockPulled();
	testLag();
	testLanesComeAndGo();
	testLaneCv();
	testEightLanes();
	std::printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
