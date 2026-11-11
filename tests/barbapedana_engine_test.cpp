// Host tests of Barbapedana's engine (src/BarbapedanaEngine.hpp), with no Rack: `make test-engine`.
// Plain checks, no framework; exits non-zero if any fails.
#include "../src/BarbapedanaEngine.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace barbapedana;

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

static bool near(float a, float b, float eps = 1e-5f) {
	return std::fabs(a - b) < eps;
}

// The module, minus Rack: a clock edge every `period` samples.
struct Rig {
	Engine e;
	Knobs k;
	Frame f;
	Rng rng;
	long period;

	explicit Rig(long period = 1000, uint32_t seed = 1) : rng(seed), period(period) {
		e.fill(rng);
	}

	// One sample, with or without a clock edge.
	Out tick(bool edge) {
		f.clockEdge = edge;
		Out o = e.process(DT, f, k, rng);
		f.clockEdge = false;
		f.reset = false;
		return o;
	}

	// One clocked step: the edge sample and the rest of the period. Returns the
	// edge sample's output, and counts the samples the gate was high.
	Out step(int* gateSamples = nullptr, std::vector<int>* stepGate = nullptr) {
		Out first = tick(true);
		int high = first.gate ? 1 : 0;
		if (stepGate) {
			stepGate->assign(STEPS, 0);
			for (int i = 0; i < STEPS; i++)
				(*stepGate)[i] += first.gates[i];
		}
		for (long n = 1; n < period; n++) {
			Out o = tick(false);
			high += o.gate ? 1 : 0;
			if (stepGate) {
				for (int i = 0; i < STEPS; i++)
					(*stepGate)[i] += o.gates[i];
			}
		}
		if (gateSamples)
			*gateSamples = high;
		return first;
	}
};

static void testStepsAndLength() {
	Rig r;
	r.k.length = 5;
	std::vector<int> seen;
	for (int n = 0; n < 12; n++) {
		r.step();
		seen.push_back(r.e.step);
	}
	int want[] = {0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1};
	bool ok = true;
	for (int n = 0; n < 12; n++)
		ok = ok && seen[n] == want[n];
	CHECK(ok, "LENGTH 5 plays steps 1 to 5 and wraps");

	// Shortening LENGTH past the playhead wraps on the next clock.
	r.k.length = 16;
	for (int n = 0; n < 6; n++)
		r.step();
	CHECK(r.e.step == 7, "step 8 after six more clocks, got %d", r.e.step + 1);
	r.k.length = 4;
	r.step();
	CHECK(r.e.step == 0, "past LENGTH wraps to step 1, got %d", r.e.step + 1);

	// RESET: the next clock plays step 1, also when both arrive together.
	r.k.length = 16;
	r.step();
	r.step();
	r.f.reset = true;
	r.tick(false);
	r.step();
	CHECK(r.e.step == 0, "the clock after RESET plays step 1, got %d", r.e.step + 1);
	r.step();
	r.f.reset = true;
	r.step();
	CHECK(r.e.step == 0, "RESET with a clock plays step 1, got %d", r.e.step + 1);
}

static void testKeep() {
	Rig r;
	r.k.mutate = 0.f;
	r.k.range = 4.f;
	r.k.offset = -1.f;
	for (int i = 0; i < STEPS; i++) {
		r.k.value[i] = i / 15.f;
		r.k.keep[i] = 1.f;
	}
	bool ok = true;
	for (int n = 0; n < 32; n++) {
		Out o = r.step();
		int s = r.e.step;
		ok = ok && near(o.cv, -1.f + 4.f * r.k.value[s]);
	}
	CHECK(ok, "KEEP 100%%: every step plays VALUE, CV = OFFSET + value × RANGE");

	for (int i = 0; i < STEPS; i++)
		r.k.keep[i] = 0.f;
	ok = true;
	for (int n = 0; n < 32; n++) {
		Out o = r.step();
		ok = ok && near(o.cv, -1.f + 4.f * r.e.loop[r.e.step]);
	}
	CHECK(ok, "KEEP 0%%: every step plays the loop");

	// KEEP CV shifts every free step's KEEP.
	r.k.keepShift = 1.f;
	ok = true;
	for (int n = 0; n < 16; n++) {
		Out o = r.step();
		ok = ok && near(o.cv, -1.f + 4.f * r.k.value[r.e.step]);
	}
	CHECK(ok, "KEEP CV fully up: the free steps play VALUE");

	// Half and half: both sources turn up, roughly evenly.
	r.k.keepShift = 0.f;
	for (int i = 0; i < STEPS; i++) {
		r.k.keep[i] = 0.5f;
		r.k.value[i] = 2.f;  // out of the loop's 0 to 1, so the source is plain
	}
	int fromValue = 0;
	for (int n = 0; n < 1600; n++) {
		r.step();
		fromValue += r.e.heldFromValue ? 1 : 0;
	}
	CHECK(fromValue > 700 && fromValue < 900, "KEEP 50%%: about half the passes play VALUE, got %d of 1600", fromValue);
}

static void testAnchors() {
	Rig r;
	r.k.mutate = 1.f;
	r.k.draw = ANYWHERE;
	r.k.keepShift = -1.f;
	r.k.anchor[2] = true;
	r.k.value[2] = 0.25f;
	float loop2 = r.e.loop[2];
	bool ok = true;
	for (int n = 0; n < 64; n++) {
		Out o = r.step();
		if (r.e.step == 2)
			ok = ok && near(o.cv, r.k.offset + 0.25f * r.k.range);
	}
	CHECK(ok, "an anchored step plays VALUE whatever KEEP and KEEP CV say");
	CHECK(r.e.loop[2] == loop2, "an anchored step's loop value is never redrawn");
	CHECK(r.e.capture(5) == r.e.loop[5], "capture takes the loop value the step would play");
}

static void testMutate() {
	Rig r;
	r.k.mutate = 0.f;
	float before[STEPS];
	for (int i = 0; i < STEPS; i++)
		before[i] = r.e.loop[i];
	for (int n = 0; n < 160; n++)
		r.step();
	bool same = true;
	for (int i = 0; i < STEPS; i++)
		same = same && r.e.loop[i] == before[i];
	CHECK(same, "MUTATE fully left: the loop repeats");

	// Walk: each redraw lands within an eighth of the old value.
	r.k.mutate = 1.f;
	r.k.draw = WALK;
	bool within = true, moved = false;
	for (int n = 0; n < 400; n++) {
		int next = (r.e.step + 1) % STEPS;
		float old = r.e.loop[next];
		r.step();
		float now = r.e.loop[r.e.step];
		within = within && std::fabs(now - old) <= WALK_SPAN + 1e-6f && now >= 0.f && now <= 1.f;
		moved = moved || now != old;
	}
	CHECK(within && moved, "Walk moves within an eighth of the span and stays in 0 to 1");

	// Anywhere: values spread across the span.
	r.k.draw = ANYWHERE;
	float lo = 1.f, hi = 0.f;
	for (int n = 0; n < 400; n++) {
		r.step();
		lo = std::min(lo, r.e.loop[r.e.step]);
		hi = std::max(hi, r.e.loop[r.e.step]);
	}
	CHECK(lo < 0.1f && hi > 0.9f, "Anywhere draws across the span, got %f to %f", lo, hi);

	// Turing bits: a redraw flips step k's bit, and every value reads the register.
	r.k.draw = BITS;
	r.k.length = 16;
	uint16_t bits = r.e.bits;
	int next = (r.e.step + 1) % STEPS;
	r.step();
	CHECK(r.e.bits == (uint16_t) (bits ^ (1u << next)), "Turing bits flips the bit of the step reached");
	bool reads = true;
	for (int i = 0; i < STEPS; i++)
		reads = reads && near(r.e.loop[i], r.e.bitsValue(i));
	CHECK(reads, "Turing bits: each step reads its byte of the register");
	r.e.bits = 0x00ff;
	CHECK(near(r.e.bitsValue(0), 1.f) && near(r.e.bitsValue(8), 0.f) && near(r.e.bitsValue(4), 240 / 255.f),
		"a step reads bits k to k+7, the first the most significant");

	// TURING IN overrides every draw: the step will send its voltage.
	r.k.draw = WALK;
	r.k.range = 2.f;
	r.k.offset = 0.5f;
	r.f.turing = true;
	r.f.turingIn = 1.5f;
	r.step();
	CHECK(near(r.e.loop[r.e.step], 0.5f), "TURING IN 1.5 V with RANGE 2, OFFSET 0.5 gives 0.5, got %f", r.e.loop[r.e.step]);
	r.f.turingIn = 9.f;
	r.step();
	CHECK(near(r.e.loop[r.e.step], 1.f), "TURING IN past the span clamps to its top");
}

static void testGates() {
	Rig r(1000);
	r.k.mutate = 0.f;
	int high = 0;
	std::vector<int> per;
	r.step(&high);
	CHECK(high == (int) std::lround(FIRST_GATE * FS) || high == 1000, "before a step is measured the gate lasts FIRST_GATE (or the step), got %d", high);
	r.step(&high, &per);
	CHECK(high == 500, "a gate lasts half a step, got %d samples of 1000", high);
	CHECK(per[r.e.step] == 500, "the step's own output carries the same gate");
	int others = 0;
	for (int i = 0; i < STEPS; i++)
		others += i == r.e.step ? 0 : per[i];
	CHECK(others == 0, "the other step outputs stay low");

	r.k.trigger = true;
	r.step(&high);
	CHECK(high == 48 || high == 49, "a trigger lasts 1 ms, got %d samples", high);
	r.k.trigger = false;

	// CHANCE 0: no gate, and the CV holds through the silent step.
	for (int i = 0; i < STEPS; i++)
		r.k.chance[i] = 0.f;
	r.k.chance[r.e.step + 1] = 1.f;
	for (int i = 0; i < STEPS; i++)
		r.k.keep[i] = 1.f, r.k.value[i] = i / 16.f;
	Out o = r.step(&high);
	float held = o.cv;
	o = r.step(&high);
	CHECK(high == 0, "CHANCE 0 fires no gate");
	CHECK(near(o.cv, held), "CV on silent steps: hold keeps the last note's CV");
	r.k.moveOnSilent = true;
	o = r.step(&high);
	CHECK(near(o.cv, r.k.offset + r.k.value[r.e.step] * r.k.range), "CV on silent steps: move follows every step");

	// CHANCE CV shifts every step's CHANCE.
	r.k.moveOnSilent = false;
	r.k.chanceShift = 1.f;
	r.step(&high);
	CHECK(high == 500, "CHANCE CV fully up fires the silent steps");
	r.k.chanceShift = 0.f;

	// A knob turned on the held step is heard at once.
	for (int i = 0; i < STEPS; i++)
		r.k.chance[i] = 1.f;
	r.step();
	int s = r.e.step;
	r.k.value[s] = 0.9f;
	o = r.tick(false);
	CHECK(near(o.cv, r.k.offset + 0.9f * r.k.range), "turning the playing step's VALUE moves the CV");

	// A clock that speeds up: the gate still open at the next clock drops for one
	// sample, so two firing steps stay two notes.
	Rig q(1000);
	q.step();
	q.tick(true);
	for (int n = 0; n < 100; n++)
		q.tick(false);
	Out edge = q.tick(true);
	Out after = q.tick(false);
	CHECK(!edge.gate && after.gate, "an open gate drops for one sample at the next clock");
}

int main() {
	testStepsAndLength();
	testKeep();
	testAnchors();
	testMutate();
	testGates();
	std::printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
