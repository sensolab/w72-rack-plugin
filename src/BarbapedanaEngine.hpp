// Barbapedana's engine: 16 steps crossed with a Turing-style random loop. Each step
// plays its VALUE or the loop's value by its KEEP coin, unless it is anchored, and
// fires its gate by its CHANCE coin. Standard library only, so
// tests/barbapedana_engine_test.cpp can build it on the host. The random source is
// injected: any callable returning a uniform float in [0, 1).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace barbapedana {

static const int STEPS = 16;

// What a redraw of a loop value does (the *Loop draws* menu).
enum Draw { WALK, ANYWHERE, BITS, DRAWS_LEN };
// Walk lands within this much of the old value, either way.
static const float WALK_SPAN = 0.125f;

// Before two clocks have measured a step, a gate lasts this long.
static const float FIRST_GATE = 0.1f;
static const float TRIGGER = 1e-3f;

inline float clamp01(float v) {
	return std::max(0.f, std::min(v, 1.f));
}

// The controls, read each sample. CVs are already scaled: KEEP CV and CHANCE CV
// shift the chances by 10% a volt, MUT CV is added into mutate.
struct Knobs {
	float value[STEPS];
	float keep[STEPS];
	float chance[STEPS];
	bool anchor[STEPS];
	int length = STEPS;
	float mutate = 0.1f;
	float keepShift = 0.f;
	float chanceShift = 0.f;
	float range = 2.f;
	float offset = 0.f;
	int draw = WALK;
	bool trigger = false;
	// CV on silent steps: hold (false) or move (true).
	bool moveOnSilent = false;

	Knobs() {
		for (int i = 0; i < STEPS; i++) {
			value[i] = 0.5f;
			keep[i] = 0.f;
			chance[i] = 1.f;
			anchor[i] = false;
		}
	}
};

struct Frame {
	bool clockEdge = false;
	bool reset = false;
	// TURING IN: patched, a redraw samples its voltage instead of drawing.
	bool turing = false;
	float turingIn = 0.f;
};

struct Out {
	float cv = 0.f;
	bool gate = false;
	bool gates[STEPS] = {};
};

struct Engine {
	float loop[STEPS];
	// Turing bits: a 16-bit circular register; step k reads the byte starting at bit k.
	uint16_t bits = 0;
	// The step playing, or -1 before the first clock and after a reset.
	int step = -1;
	// What the CV holds: a step and whether it played VALUE (followed live, so
	// turning the knob of a held step is heard) or the loop value it took.
	int heldStep = -1;
	bool heldFromValue = false;
	float heldLoop = 0.f;
	// The step whose gate is open, and the gate's state.
	int gateStep = -1;
	double gateLeft = 0.0;
	// The gate was still open when the next one fired: one sample low between them.
	bool rearm = false;
	// Seconds since the last clock, and the last measured step, 0 while unknown.
	double sinceClock = 0.0;
	double period = 0.0;
	bool clocked = false;

	Engine() {
		for (int i = 0; i < STEPS; i++)
			loop[i] = 0.5f;
	}

	template <class Rng>
	void fill(Rng& rng) {
		for (int i = 0; i < STEPS; i++)
			loop[i] = rng();
		bits = 0;
		for (int i = 0; i < STEPS; i++) {
			if (rng() < 0.5f)
				bits |= (uint16_t) (1u << i);
		}
	}

	// The value the register gives step k: bits k to k+7, the first the most significant.
	float bitsValue(int k) const {
		int byte = 0;
		for (int j = 0; j < 8; j++)
			byte = (byte << 1) | ((bits >> ((k + j) % STEPS)) & 1);
		return byte / 255.f;
	}

	void readBits() {
		for (int i = 0; i < STEPS; i++)
			loop[i] = bitsValue(i);
	}

	// A new loop value for step k. TURING IN overrides every draw: its voltage is
	// what the step will send, within the span RANGE and OFFSET give.
	template <class Rng>
	void redraw(int k, const Frame& f, const Knobs& kn, Rng& rng) {
		if (f.turing) {
			loop[k] = kn.range > 0.f ? clamp01((f.turingIn - kn.offset) / kn.range) : 0.f;
			return;
		}
		switch (kn.draw) {
			case ANYWHERE:
				loop[k] = rng();
				break;
			case BITS:
				bits ^= (uint16_t) (1u << k);
				readBits();
				break;
			default: {
				// Reflected at the edges, so a walk never sticks to 0 or 1.
				float v = loop[k] + (2.f * rng() - 1.f) * WALK_SPAN;
				if (v < 0.f)
					v = -v;
				if (v > 1.f)
					v = 2.f - v;
				loop[k] = clamp01(v);
			}
		}
	}

	// The chance a free step plays VALUE this pass.
	static float keepChance(const Knobs& kn, int k) {
		return clamp01(kn.keep[k] + kn.keepShift);
	}

	float heldValue(const Knobs& kn) const {
		if (heldStep < 0)
			return 0.f;
		return heldFromValue ? clamp01(kn.value[heldStep]) : heldLoop;
	}

	template <class Rng>
	void advance(double dt, const Frame& f, const Knobs& kn, Rng& rng) {
		int length = std::max(1, std::min(kn.length, STEPS));
		step = step + 1 >= length ? 0 : step + 1;
		int k = step;
		// A free step's loop value may be redrawn as the playhead reaches it.
		if (!kn.anchor[k] && rng() < clamp01(kn.mutate))
			redraw(k, f, kn, rng);
		bool fromValue = kn.anchor[k] || rng() < keepChance(kn, k);
		bool fire = rng() < clamp01(kn.chance[k] + kn.chanceShift);
		if (fire || kn.moveOnSilent) {
			heldStep = k;
			heldFromValue = fromValue;
			heldLoop = loop[k];
		}
		if (fire) {
			rearm = gateLeft > 0.5 * dt;
			gateStep = k;
			gateLeft = kn.trigger ? TRIGGER : (period > 0.0 ? 0.5 * period : FIRST_GATE);
		}
	}

	template <class Rng>
	Out process(double dt, const Frame& f, const Knobs& kn, Rng& rng) {
		if (f.reset)
			step = -1;
		if (f.clockEdge) {
			if (clocked)
				period = sinceClock;
			clocked = true;
			sinceClock = 0.0;
			advance(dt, f, kn, rng);
		}
		Out o;
		o.cv = kn.offset + heldValue(kn) * kn.range;
		// Open to the nearest sample, so half a step of 1000 samples is 500.
		bool open = gateLeft > 0.5 * dt && !rearm;
		o.gate = open;
		if (open && gateStep >= 0)
			o.gates[gateStep] = true;
		if (!rearm)
			gateLeft -= dt;
		rearm = false;
		sinceClock += dt;
		return o;
	}

	// Pressing a free step's anchor: the loop value it would play becomes its VALUE.
	float capture(int k) const {
		return loop[std::max(0, std::min(k, STEPS - 1))];
	}
};

} // namespace barbapedana
