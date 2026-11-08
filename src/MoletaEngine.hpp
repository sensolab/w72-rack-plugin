// Moleta's engine: the beat clock, the ratios, the coins and the lengths.
// Standard library only, so tests/moleta_engine_test.cpp can build it on the host;
// Moleta and Moleta+ run the same output engine. The random source is injected:
// any callable returning a uniform float in [0, 1).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace moleta {

// p pulses every q beats: ×N is N/1, /N is 1/N.
struct Ratio {
	int p;
	int q;
	const char* name;
};

static const int RATIO_COUNT = 67;
// Every whole divider and multiplier from /32 to ×32, and four fractions around ×1.
static const Ratio RATIOS[RATIO_COUNT] = {
	{1, 32, "/32"}, {1, 31, "/31"}, {1, 30, "/30"}, {1, 29, "/29"}, {1, 28, "/28"}, {1, 27, "/27"}, {1, 26, "/26"}, {1, 25, "/25"},
	{1, 24, "/24"}, {1, 23, "/23"}, {1, 22, "/22"}, {1, 21, "/21"}, {1, 20, "/20"}, {1, 19, "/19"}, {1, 18, "/18"}, {1, 17, "/17"},
	{1, 16, "/16"}, {1, 15, "/15"}, {1, 14, "/14"}, {1, 13, "/13"}, {1, 12, "/12"}, {1, 11, "/11"}, {1, 10, "/10"}, {1, 9, "/9"},
	{1, 8, "/8"}, {1, 7, "/7"}, {1, 6, "/6"}, {1, 5, "/5"}, {1, 4, "/4"}, {1, 3, "/3"}, {1, 2, "/2"},
	{2, 3, "2/3"}, {3, 4, "3/4"}, {1, 1, "×1"}, {4, 3, "4/3"}, {3, 2, "3/2"},
	{2, 1, "×2"}, {3, 1, "×3"}, {4, 1, "×4"}, {5, 1, "×5"}, {6, 1, "×6"}, {7, 1, "×7"}, {8, 1, "×8"}, {9, 1, "×9"},
	{10, 1, "×10"}, {11, 1, "×11"}, {12, 1, "×12"}, {13, 1, "×13"}, {14, 1, "×14"}, {15, 1, "×15"}, {16, 1, "×16"}, {17, 1, "×17"},
	{18, 1, "×18"}, {19, 1, "×19"}, {20, 1, "×20"}, {21, 1, "×21"}, {22, 1, "×22"}, {23, 1, "×23"}, {24, 1, "×24"}, {25, 1, "×25"},
	{26, 1, "×26"}, {27, 1, "×27"}, {28, 1, "×28"}, {29, 1, "×29"}, {30, 1, "×30"}, {31, 1, "×31"}, {32, 1, "×32"},
};
// Outputs 1 to 4 of a fresh Moleta: ×2, /2, /3, /4.
static const int DEFAULT_RATIOS[4] = {36, 30, 29, 28};
// Outputs 5 to 8 of a fresh Moleta+: ×4, ×1, /8, 3/2, the other common ratios and one odd one.
static const int PLUS_DEFAULT_RATIOS[4] = {38, 33, 24, 35};

inline int clampRatio(int i) {
	return std::max(0, std::min(i, RATIO_COUNT - 1));
}

// The step whose ratio is nearest p/q (by octaves), so a saved ratio the list no
// longer holds still lands somewhere sensible.
inline int nearestRatio(int p, int q) {
	int best = 0;
	double bestDistance = INFINITY;
	for (int i = 0; i < RATIO_COUNT; i++) {
		double d = std::fabs(std::log2((double) p / q) - std::log2((double) RATIOS[i].p / RATIOS[i].q));
		if (d < bestDistance) {
			bestDistance = d;
			best = i;
		}
	}
	return best;
}

static const float TRIGGER = 1e-3f;
// LENGTH below this is the trigger step; above it, a gate.
static const float LENGTH_TRIGGER_TOP = 0.01f;

// The share of an output's period a gate lasts, 5% to 95% (50% in the middle of
// the knob), or 0 for the trigger step.
inline float gateFraction(float length) {
	if (length < LENGTH_TRIGGER_TOP)
		return 0.f;
	return std::min(0.05f + 0.9f * length, 0.95f);
}

// What the beat clock tells the outputs each sample. It is also what the base
// sends Moleta+ (MoletaExpander.hpp), so it holds everything an output needs.
struct Tick {
	// Running, and the clock active: outputs may fire.
	bool live = false;
	bool beat = false;
	// RESET, or RUN turned on: the coin pointers restart, and the cycles wait for beat 0.
	bool restart = false;
	// Index of the latest beat edge since the restart, 0 for the first.
	int64_t beatIndex = -1;
	// On a beat edge, how far past the true edge this sample is, in beats (internal clock only).
	double offset = 0.0;
	// The beat period T in seconds, 0 while unknown.
	double period = 0.0;
};

// The module-wide controls the outputs share.
struct Shared {
	float repeat = 0.f;
	// The DENSITY input, in volts.
	float density = 0.f;
	int loopLength = 16;
};

struct ClockIn {
	bool running = true;
	bool restart = false;
	// A cable in CLOCK.
	bool external = false;
	// A rising edge on CLOCK this sample.
	bool edge = false;
	float bpm = 120.f;
	int pulsesPerBeat = 1;
};

// The beat, from the BPM or from CLOCK. External: every pulsesPerBeat-th rising
// edge is a beat, the first edge after connecting or a restart being edge 0; T is
// the time between the last two beats, and the clock goes inactive when no beat
// comes for two periods.
struct BeatClock {
	bool external = false;
	int64_t beats = -1;
	// Seconds since the latest beat edge.
	double sinceBeat = 0.0;

	// Internal: beats since the latest beat edge, and whether the next sample is one.
	double phase = 0.0;
	bool beatNow = true;

	// External.
	int edges = 0;
	bool started = false;
	// The measured beat period, 0 until two beats have arrived.
	double measured = 0.0;
	// The latest beat came after an inactive gap, so the next one is measured whatever its length.
	bool gapped = false;

	void restart() {
		beats = -1;
		sinceBeat = 0.0;
		phase = 0.0;
		beatNow = true;
		edges = 0;
		started = false;
	}

	// Pulling the cable hands back to the BPM, its next beat one period after the
	// last external beat (or at once if that is past).
	void handOver(const ClockIn& in) {
		external = in.external;
		if (external) {
			edges = 0;
			started = false;
			measured = 0.0;
		}
		else if (started) {
			phase = std::min(sinceBeat * in.bpm / 60.0, 1.0);
			beatNow = false;
		}
		else {
			phase = 0.0;
			beatNow = true;
		}
	}

	Tick process(double dt, const ClockIn& in) {
		Tick t;
		if (in.restart)
			restart();
		t.restart = in.restart;
		if (in.external != external)
			handOver(in);
		if (!in.running) {
			t.beatIndex = beats;
			t.period = external ? measured : 60.0 / in.bpm;
			return t;
		}

		if (!external) {
			if (beatNow) {
				beatNow = false;
				phase = 0.0;
				t.beat = true;
			}
			else {
				phase += dt * in.bpm / 60.0;
				if (phase >= 1.0) {
					phase -= 1.0;
					t.beat = true;
				}
			}
			t.offset = t.beat ? phase : 0.0;
			t.period = 60.0 / in.bpm;
			t.live = true;
		}
		else {
			if (started)
				sinceBeat += dt;
			if (in.edge) {
				int k = std::max(in.pulsesPerBeat, 1);
				if (edges % k == 0) {
					t.beat = true;
					if (started) {
						bool late = measured > 0.0 && sinceBeat > 2.0 * measured;
						if (!late || gapped)
							measured = sinceBeat;
						gapped = late && !gapped;
					}
					started = true;
				}
				edges = (edges + 1) % k;
			}
			t.period = measured;
			t.live = started && !(measured > 0.0 && !t.beat && sinceBeat > 2.0 * measured);
		}

		if (t.beat) {
			beats++;
			sinceBeat = 0.0;
		}
		else if (!external) {
			sinceBeat += dt;
		}
		t.beatIndex = beats;
		return t;
	}
};

// A loop of stored random numbers. On each due pulse the slot at the pointer is
// redrawn with probability 1 − REPEAT, and the pulse passes if the slot is below
// the threshold; the pointer advances either way.
struct Coin {
	static const int MAX_LOOP = 16;
	float slots[MAX_LOOP] = {};
	int pointer = 0;

	template <class Rng>
	void fill(Rng& rng) {
		for (float& s : slots)
			s = rng();
		pointer = 0;
	}

	template <class Rng>
	bool flip(float threshold, float repeat, int loopLength, Rng& rng) {
		int n = std::max(1, std::min(loopLength, MAX_LOOP));
		pointer %= n;
		if (rng() < 1.f - repeat)
			slots[pointer] = rng();
		bool pass = slots[pointer] < threshold;
		pointer = (pointer + 1) % n;
		return pass;
	}
};

// High for a set time. A pulse landing on one still high drops the output for a
// sample first, so every pulse is a fresh rising edge.
struct Pulse {
	float left = 0.f;
	bool gap = false;

	void trigger(float seconds) {
		gap = left > 0.f;
		left = seconds;
	}

	void cut() {
		left = 0.f;
		gap = false;
	}

	bool process(float dt) {
		bool high = left > 0.f && !gap;
		gap = false;
		if (left > 0.f)
			left -= dt;
		return high;
	}
};

// One output's knobs and menu flag.
struct Controls {
	int ratio = 10;
	float chance = 1.f;
	float length = 0.5f;
	bool follow = false;
};

// One output: a cycle of q beats holding p pulses, a coin and a pulse. The cycle
// restarts on the beat edges whose index is a multiple of q, with a pulse; the
// others fall where the position, advancing at 1/T beats a second, crosses a
// multiple of q/p. A pulse still pending at the restart is dropped, and the
// position waits at the cycle's end when the clock is late.
struct OutputEngine {
	double pos = 0.0;
	// The next pulse in the cycle, 1 to p - 1; p when none is left.
	int next = 0;
	// After a restart, until beat 0 starts a cycle.
	bool waiting = true;
	int ratio = -1;
	Coin coin;
	Pulse pulse;
	// A pulse was due this sample.
	bool due = false;
	// The final result of the latest due pulse, coin and follow both: what the output below follows.
	bool passed = true;

	void restart() {
		coin.pointer = 0;
		passed = true;
		waiting = true;
	}

	template <class Rng>
	bool process(double dt, const Tick& t, const Shared& s, const Controls& c, bool abovePassed, Rng& rng) {
		due = false;
		if (t.restart)
			restart();
		if (!t.live) {
			pulse.cut();
			return false;
		}
		const Ratio& r = RATIOS[clampRatio(c.ratio)];
		// A new ratio mid-cycle skips the pulses its position has already passed.
		if (c.ratio != ratio) {
			ratio = c.ratio;
			pos = std::min(pos, (double) r.q);
			next = (int) std::floor(pos * r.p / r.q + 1e-9) + 1;
		}

		if (t.beat && t.beatIndex % r.q == 0) {
			pos = t.offset;
			next = 1;
			waiting = false;
			due = true;
		}
		else if (!waiting && t.period > 0.0) {
			pos = std::min(pos + dt / t.period, (double) r.q);
			if (next < r.p && pos * r.p >= next * r.q - 1e-9) {
				next++;
				due = true;
			}
		}

		if (due) {
			float threshold = std::max(0.f, std::min(c.chance + 0.1f * s.density, 1.f));
			bool pass = coin.flip(threshold, s.repeat, s.loopLength, rng);
			if (c.follow && !abovePassed)
				pass = false;
			passed = pass;
			if (pass) {
				float f = gateFraction(c.length);
				pulse.trigger(f > 0.f && t.period > 0.0 ? (float) (f * t.period * r.q / r.p) : TRIGGER);
			}
		}
		return pulse.process((float) dt);
	}
};

// Runs n outputs in index order, so each one's follow sees the output above's
// result from this same sample. The chain starts fresh at outs[0]. Writes 10 V or 0 V.
template <class Rng>
void processChain(OutputEngine* outs, const Controls* controls, int n, double dt, const Tick& t, const Shared& s, Rng& rng, float* volts) {
	bool above = true;
	for (int i = 0; i < n; i++) {
		volts[i] = outs[i].process(dt, t, s, controls[i], above, rng) ? 10.f : 0.f;
		above = outs[i].passed;
	}
}

// The metronome: a short decaying sine burst, an octave higher and louder on beat 1.
struct Click {
	double phase = 0.0;
	float freq = 0.f;
	float amp = 0.f;
	float env = 0.f;

	void trigger(bool accent) {
		phase = 0.0;
		freq = accent ? 1760.f : 880.f;
		amp = accent ? 5.f : 2.5f;
		env = 1.f;
	}

	float process(float dt) {
		if (env < 1e-3f)
			return 0.f;
		float out = amp * env * (float) std::sin(6.283185307179586 * phase);
		phase += freq * dt;
		phase -= std::floor(phase);
		// About 8 ms to fall by e.
		env *= std::exp(-dt / 8e-3f);
		return out;
	}
};

// Delays n values by one sample while on. It keeps the last sample either way, so
// turning it on carries on without a gap.
template <int N>
struct SampleDelay {
	float last[N] = {};

	void process(float* v, bool on) {
		for (int i = 0; i < N; i++) {
			float x = v[i];
			if (on)
				v[i] = last[i];
			last[i] = x;
		}
	}
};

} // namespace moleta
