// Gibigiana's engine: the buffer and its coin mask, the clock period, the lanes,
// the band, the duck and the sum. Every lane in the chain is computed here, the
// expanders' too: a Gibigiana + only sends its settings in and plays its tap back. Standard library only, so
// tests/gibigiana_engine_test.cpp can build it on the host. The coin loop is
// Moleta's (MoletaEngine.hpp), one coin model across the family. The random source
// is injected: any callable returning a uniform float in [0, 1).
#pragma once
#include "MoletaEngine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace gibigiana {

// The chain holds at most 8 lanes, Gibigiana's own and 7 Gibigiana +, so a sample's
// decisions fit in one byte of the mask.
static const int LANES = 8;
// A lane's tap reads early by its place in the chain; a ring of this many samples
// puts its share of the sum back on the grid.
static const int LAG_RING = 8;
static const int MAX_LAG = LAG_RING - 1;

// A tap sits p/q clock steps behind the write head. The readout shows `shown`;
// `name` is for menus and tooltips. Inter Tight has ⅓ and ½ but no ⅔, so the
// readout composes every fraction from superior digits, the fraction slash and
// inferior digits, which look the same as its own fractions.
struct Step {
	int p;
	int q;
	const char* name;
	const char* shown;
};

static const int STEP_COUNT = 14;
static const Step STEPS[STEP_COUNT] = {
	{1, 3, "1/3", "¹⁄₃"}, {1, 2, "1/2", "¹⁄₂"}, {2, 3, "2/3", "²⁄₃"}, {1, 1, "1", "1"},
	{4, 3, "1 1/3", "1¹⁄₃"}, {3, 2, "1 1/2", "1¹⁄₂"}, {2, 1, "2", "2"}, {8, 3, "2 2/3", "2²⁄₃"},
	{3, 1, "3", "3"}, {4, 1, "4", "4"}, {6, 1, "6", "6"}, {8, 1, "8", "8"}, {12, 1, "12", "12"}, {16, 1, "16", "16"},
};
// A fresh lane's step by its place in the chain: 1½, 3, 4, then 8 steps.
static const int DEFAULT_STEPS[LANES] = {5, 8, 9, 11, 11, 11, 11, 11};

inline int clampStep(int i) {
	return std::max(0, std::min(i, STEP_COUNT - 1));
}

// The step nearest p/q (by octaves), so a saved step the list no longer holds
// still lands somewhere sensible.
inline int nearestStep(int p, int q) {
	int best = 0;
	double bestDistance = INFINITY;
	for (int i = 0; i < STEP_COUNT; i++) {
		double d = std::fabs(std::log2((double) p / q) - std::log2((double) STEPS[i].p / STEPS[i].q));
		if (d < bestDistance) {
			bestDistance = d;
			best = i;
		}
	}
	return best;
}

static const float BUFFER_SECONDS = 20.f;
// A coin decision fades in or out over this long, ending exactly where the hit falls.
static const float GATE_RAMP = 0.002f;
// A tap that moves (a new tempo or STEP) crossfades from where it was over this long.
static const float CROSSFADE = 0.03f;
// The duck's envelope: fixed attack and release. A follower at DUCK_FULL ducks by the whole AMOUNT.
static const float DUCK_ATTACK = 0.002f;
static const float DUCK_RELEASE = 0.15f;
static const float DUCK_FULL = 5.f;
// The sum is linear up to the knee and bends smoothly towards the ceiling.
static const float CLIP_KNEE = 5.f;
static const float CLIP_CEILING = 10.f;
// The band's centre sweeps 20 Hz to 20 kHz; fully open, WIDTH spans 5 octaves each side.
static const float BAND_LOW = 20.f;
static const float BAND_RANGE = 1000.f;
static const float BAND_OCTAVES = 5.f;
static const float WIDTH_OPEN = 0.999f;
// The lane CVs add to their knobs: CHANCE and LEVEL by 10% of their range per volt
// (as DENSITY does), PAN by 20% per volt, so ±5 V sweeps it from side to side.
static const float CHANCE_PER_VOLT = 0.1f;
static const float LEVEL_PER_VOLT = 0.1f;
static const float PAN_PER_VOLT = 0.2f;

inline float softClip(float x) {
	float a = std::fabs(x);
	if (a <= CLIP_KNEE)
		return x;
	float room = CLIP_CEILING - CLIP_KNEE;
	float y = CLIP_KNEE + room * std::tanh((a - CLIP_KNEE) / room);
	return x < 0.f ? -y : y;
}

// The CLOCK period in samples, 0 until two edges have arrived: the mean of the last
// two intervals, so a swung clock reads as its average. An interval over 2.5
// periods is a gap (the clock stopped) and is ignored once; a second one in a row
// is the new tempo.
struct ClockPeriod {
	bool started = false;
	int64_t since = 0;
	double last = 0.0;
	double period = 0.0;
	bool gapped = false;

	void clear() {
		*this = ClockPeriod();
	}

	void process(bool edge) {
		if (started)
			since++;
		if (!edge)
			return;
		if (started) {
			double interval = (double) since;
			bool late = period > 0.0 && interval > 2.5 * period;
			if (late && !gapped) {
				gapped = true;
				last = 0.0;
			}
			else {
				gapped = false;
				period = last > 0.0 ? 0.5 * (last + interval) : interval;
				last = interval;
			}
		}
		started = true;
		since = 0;
	}
};

// A 12 dB/octave state-variable filter (Simper's trapezoidal form), Butterworth Q.
struct Svf {
	float a1 = 0.f, a2 = 0.f, a3 = 0.f;
	float ic1 = 0.f, ic2 = 0.f;
	static constexpr float K = 1.41421356f;

	void tune(float hz, float sampleRate) {
		float g = std::tan(3.14159265f * hz / sampleRate);
		a1 = 1.f / (1.f + g * (g + K));
		a2 = g * a1;
		a3 = g * a2;
	}

	void clear() {
		ic1 = ic2 = 0.f;
	}

	// Returns the low-pass; high is the high-pass of the same input.
	float process(float v0, float& high) {
		float v3 = v0 - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		high = v0 - K * v1 - v2;
		return v2;
	}
};

// One read head: how far behind the write head it reads, and its coin gate.
struct Head {
	// In samples; -1 is silent.
	int delay = -1;
	float gate = 0.f;
};

// A lane's tap reads through one head, or two while it crossfades to a new delay.
struct Tap {
	Head cur;
	Head next;
	bool fading = false;
	float fade = 0.f;
	// Where the tap is heading, on the grid: a delay in samples, or -1 for silent.
	// The heads read `lag` samples nearer than this.
	int target = -1;
	// The tap's step fits in the buffer (or no period is known yet).
	bool reach = true;
	// The band-pass: a high-pass, then a low-pass.
	Svf high;
	Svf low;
	// The lane's place in the chain fades it in and out: 1 while it is there.
	float presence = 0.f;
	// When the lane joined the chain, in samples written: older audio carries no
	// decision for it, so it plays nothing written before.
	int64_t since = 0;
	// The lane's last samples, so its share of the sum lands on the grid.
	float past[LAG_RING] = {};
	int pastAt = 0;

	// Silent and back at the start, keeping the band's tuning.
	void stop() {
		cur = Head();
		next = Head();
		fading = false;
		fade = 0.f;
		target = -1;
		reach = true;
		high.clear();
		low.clear();
		presence = 0.f;
		since = 0;
		for (float& p : past)
			p = 0.f;
		pastAt = 0;
	}
};

// The knobs and CVs of every lane, the shared knobs, the menu choices and which
// tap outputs are patched. Lane i is the i-th in the chain, 0 being Gibigiana's own.
struct Knobs {
	// How many lanes the chain holds now; the rest fade out and stay silent.
	int lanes = 1;
	int step[LANES] = {DEFAULT_STEPS[0], DEFAULT_STEPS[1], DEFAULT_STEPS[2], DEFAULT_STEPS[3],
		DEFAULT_STEPS[4], DEFAULT_STEPS[5], DEFAULT_STEPS[6], DEFAULT_STEPS[7]};
	float chance[LANES] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
	// Amplitude, 0 to 1; 0.5 is -6 dB.
	float level[LANES] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
	// -1 (left) to 1 (right).
	float pan[LANES] = {};
	// The lane CVs, in volts.
	float chanceCv[LANES] = {};
	float levelCv[LANES] = {};
	float panCv[LANES] = {};
	// How many samples late the lane's tap out reaches its jack: its place in the
	// chain, one sample per expander hop. Its tap reads that much early, so the jack
	// is on the grid, and its share of the sum is held back as much, so the sum is too.
	int lag[LANES] = {};
	float centre = 0.5f;
	float width = 1.f;
	float repeat = 0.f;
	float duck = 0.8f;
	int loopLength = 16;
	bool keepPatched = false;
	bool patched[LANES] = {};
};

// One sample of the inputs. Edges are rising edges this sample.
struct Frame {
	float in = 0.f;
	bool clock = false;
	bool clockEdge = false;
	bool gate = false;
	bool gateEdge = false;
	bool reset = false;
	// Volts.
	float density = 0.f;
	float duck = 0.f;
};

struct Out {
	float left = 0.f;
	float right = 0.f;
	// Each lane's tap, `lag` samples early: on the grid once it has crossed the chain.
	float tap[LANES] = {};
};

struct Engine {
	float sampleRate = 0.f;
	std::vector<float> audio;
	// One bit per lane: the decision of the hit the sample belongs to.
	std::vector<uint8_t> mask;
	int size = 0;
	// Where the next sample is written, and how many have been written.
	int write = 0;
	int64_t written = 0;
	int ramp = 1;
	float rampStep = 1.f;
	float fadeStep = 1.f;
	float attack = 1.f;
	float release = 1.f;

	ClockPeriod clock;
	// The coin loops belong to the place in the chain, not to a module.
	moleta::Coin coins[LANES];
	uint8_t hitMask = 0;
	Tap taps[LANES];
	float env = 0.f;

	// The band and the pan gains, recomputed only when their knobs move.
	float bandCentre = -1.f;
	float bandWidth = -1.f;
	bool bandOn = false;
	float panAt[LANES] = {2.f, 2.f, 2.f, 2.f, 2.f, 2.f, 2.f, 2.f};
	float panLeft[LANES] = {};
	float panRight[LANES] = {};

	// Allocates the buffer for a sample rate and empties it. The clock period is in
	// samples, so it is measured again.
	void setSampleRate(float sr) {
		sampleRate = sr;
		size = (int) std::ceil(BUFFER_SECONDS * sr) + 2;
		audio.assign(size, 0.f);
		mask.assign(size, 0);
		write = 0;
		written = 0;
		ramp = std::max(1, (int) std::lround(GATE_RAMP * sr));
		rampStep = 1.f / ramp;
		fadeStep = 1.f / std::max(1.f, CROSSFADE * sr);
		attack = 1.f - std::exp(-1.f / (DUCK_ATTACK * sr));
		release = 1.f - std::exp(-1.f / (DUCK_RELEASE * sr));
		clock.clear();
		hitMask = 0;
		for (Tap& t : taps)
			t = Tap();
		// Forces the band to be tuned again for the new rate.
		bandCentre = bandWidth = -1.f;
		bandOn = false;
		env = 0.f;
	}

	template <class Rng>
	void fillCoins(Rng& rng) {
		for (moleta::Coin& c : coins)
			c.fill(rng);
	}

	// The longest delay the buffer holds, in samples.
	int maxDelay() const {
		return size - 1;
	}

	// A tap's delay for its step at the current period: -1 while the period is
	// unknown or when the step reaches past the buffer.
	int delayFor(int step, bool& reach) const {
		reach = true;
		if (clock.period <= 0.0)
			return -1;
		const Step& s = STEPS[clampStep(step)];
		double d = std::round(clock.period * s.p / s.q);
		if (d > maxDelay()) {
			reach = false;
			return -1;
		}
		return std::max(1, (int) d);
	}

	// The decision for lane `bit` of the sample `ahead` samples after the one `delay`
	// back. Audio written before the lane joined (`since`) has none, so it is off.
	bool decision(int delay, int ahead, int bit, int64_t since) const {
		if (written - delay + ahead < since)
			return false;
		int i = write - delay + ahead;
		if (i < 0)
			i += size;
		return (mask[i] >> bit) & 1;
	}

	// Reads one head and moves its gate towards the decision `ramp` samples ahead,
	// so a fade ends exactly where a hit falls: the previous tail fades out before
	// the hit, and the hit's attack arrives whole.
	float read(Head& h, int bit, int64_t since) {
		if (h.delay < 0)
			return 0.f;
		int i = write - h.delay;
		if (i < 0)
			i += size;
		float target = decision(h.delay, std::min(ramp, h.delay), bit, since) ? 1.f : 0.f;
		if (h.gate < target)
			h.gate = std::min(target, h.gate + rampStep);
		else if (h.gate > target)
			h.gate = std::max(target, h.gate - rampStep);
		return audio[i] * h.gate;
	}

	// A fresh head starts with the gate its first sample's decision gives.
	void startHead(Head& h, int delay, int bit, int64_t since) {
		h.delay = delay;
		h.gate = 0.f;
		if (delay < 0)
			return;
		h.gate = decision(delay, std::min(ramp, delay), bit, since) ? 1.f : 0.f;
	}

	float processTap(int k, int lag) {
		Tap& t = taps[k];
		int want = t.target < 0 ? -1 : std::max(1, t.target - lag);
		if (!t.fading && want != t.cur.delay) {
			startHead(t.next, want, k, t.since);
			t.fading = true;
			t.fade = 0.f;
		}
		if (!t.fading)
			return read(t.cur, k, t.since);
		// Equal power: the two heads read different moments, so they add as uncorrelated.
		float a = read(t.cur, k, t.since);
		float b = read(t.next, k, t.since);
		float th = t.fade * 1.57079633f;
		float y = a * std::cos(th) + b * std::sin(th);
		t.fade += fadeStep;
		if (t.fade >= 1.f) {
			t.cur = t.next;
			t.fading = false;
		}
		return y;
	}

	// A new delay counts only when it moves by more than clock jitter would.
	static bool moved(int from, int to) {
		if (from < 0 || to < 0)
			return from != to;
		return std::abs(to - from) > std::max(4, from / 1000);
	}

	void tuneBand(float centre, float width) {
		if (centre == bandCentre && width == bandWidth)
			return;
		bool wasOn = bandOn;
		bandCentre = centre;
		bandWidth = width;
		bandOn = width < WIDTH_OPEN;
		if (!bandOn)
			return;
		float c = BAND_LOW * std::pow(BAND_RANGE, std::max(0.f, std::min(centre, 1.f)));
		float half = BAND_OCTAVES * std::max(0.f, width);
		float top = 0.45f * sampleRate;
		float lo = std::max(10.f, std::min(c / std::exp2(half), top));
		float hi = std::max(10.f, std::min(c * std::exp2(half), top));
		for (Tap& t : taps) {
			t.high.tune(lo, sampleRate);
			t.low.tune(hi, sampleRate);
			if (!wasOn) {
				t.high.clear();
				t.low.clear();
			}
		}
	}

	template <class Rng>
	Out process(const Frame& f, const Knobs& k, Rng& rng) {
		Out o;
		if (size == 0)
			return o;

		if (f.reset) {
			for (moleta::Coin& c : coins)
				c.pointer = 0;
		}
		if (f.clock)
			clock.process(f.clockEdge);
		else
			clock.clear();

		// A hit rolls the coin of every lane in the chain; the decision rides with the
		// audio until the next hit. A lane that leaves is silent from now on, and one
		// that joins stays silent until a hit has rolled for it.
		int lanes = std::max(0, std::min(k.lanes, LANES));
		hitMask &= (uint8_t) ((1 << lanes) - 1);
		bool hit = f.gate ? f.gateEdge : f.clock && f.clockEdge;
		if (hit) {
			hitMask = 0;
			for (int i = 0; i < lanes; i++) {
				float chance = k.chance[i] + CHANCE_PER_VOLT * (k.chanceCv[i] + f.density);
				float threshold = std::max(0.f, std::min(chance, 1.f));
				if (coins[i].flip(threshold, k.repeat, k.loopLength, rng))
					hitMask |= 1 << i;
			}
		}
		audio[write] = f.in;
		mask[write] = hitMask;

		tuneBand(k.centre, k.width);
		float x = std::fabs(f.duck) / DUCK_FULL;
		env += (x - env) * (x > env ? attack : release);
		float duck = 1.f - std::max(0.f, std::min(k.duck, 1.f)) * std::min(env, 1.f);

		float left = 0.f, right = 0.f;
		for (int i = 0; i < LANES; i++) {
			Tap& t = taps[i];
			// A lane fades in and out of the chain over the coin ramp; gone, it costs nothing.
			bool here = i < lanes;
			if (!here && t.presence <= 0.f)
				continue;
			if (here && t.presence <= 0.f)
				t.since = written;
			t.presence = here ? std::min(1.f, t.presence + rampStep) : t.presence - rampStep;
			if (t.presence <= 0.f) {
				t.stop();
				continue;
			}
			int lag = std::max(0, std::min(k.lag[i], MAX_LAG));
			int d = delayFor(k.step[i], t.reach);
			if (moved(t.target, d))
				t.target = d;
			float s = processTap(i, lag);
			if (bandOn) {
				float high, unused;
				t.high.process(s, high);
				s = t.low.process(high, unused);
			}
			float level = std::max(0.f, std::min(k.level[i] + LEVEL_PER_VOLT * k.levelCv[i], 1.f));
			s *= level * duck * t.presence;
			o.tap[i] = s;
			// The sum takes the sample `lag` steps back, which is on the grid.
			t.past[t.pastAt] = s;
			float onGrid = t.past[(t.pastAt - lag + LAG_RING) % LAG_RING];
			t.pastAt = (t.pastAt + 1) % LAG_RING;
			if (k.patched[i] && !k.keepPatched)
				continue;
			float pan = std::max(-1.f, std::min(k.pan[i] + PAN_PER_VOLT * k.panCv[i], 1.f));
			if (pan != panAt[i]) {
				panAt[i] = pan;
				float th = (pan + 1.f) * 0.785398163f;
				panLeft[i] = std::cos(th);
				panRight[i] = std::sin(th);
			}
			left += onGrid * panLeft[i];
			right += onGrid * panRight[i];
		}
		o.left = softClip(left);
		o.right = softClip(right);

		write = write + 1 == size ? 0 : write + 1;
		written++;
		return o;
	}
};

} // namespace gibigiana
