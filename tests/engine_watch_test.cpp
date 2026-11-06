// Host tests of Brumista's engine watch (src/BrumistaEngineWatch.hpp), with no Rack: `make test-engine`.
// Plain checks, no framework; exits non-zero if any fails.
#include "../src/BrumistaEngineWatch.hpp"
#include <cstdint>
#include <cstdio>

using brumista::EngineWatch;

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

// Rack's window steps at 60 Hz; the engine advances 800 frames a step at 48 kHz.
static const double UI_DT = 1.0 / 60.0;
static const int64_t FRAMES_PER_STEP = 800;

// Steps the watch for seconds of window time from t, with the counter moving or still.
static double run(EngineWatch& w, double t, double seconds, int64_t& frame, bool moving) {
	for (double end = t + seconds; t < end; t += UI_DT) {
		if (moving)
			frame += FRAMES_PER_STEP;
		w.observe(frame, t);
	}
	return t;
}

// No warning before the watch has seen anything.
static void testStartsRunning() {
	EngineWatch w;
	CHECK(w.running(), "a new watch reports a stopped engine");
}

static void testMovingCounterRuns() {
	EngineWatch w;
	int64_t frame = 0;
	run(w, 0.0, 2.0, frame, true);
	CHECK(w.running(), "a moving counter reads as stopped");
}

static void testStillCounterStopsAfterStall() {
	EngineWatch w;
	int64_t frame = 0;
	double t = run(w, 0.0, 1.0, frame, true);
	run(w, t, EngineWatch::STALL - 0.1, frame, false);
	CHECK(w.running(), "stopped before the counter stood still for STALL");
	run(w, t + EngineWatch::STALL - 0.1, 0.2, frame, false);
	CHECK(!w.running(), "still running after the counter stood still for more than STALL");
}

// A patch that loads with the engine already stopped: the counter never moves, from 0.
static void testStillFromTheStart() {
	EngineWatch w;
	int64_t frame = 0;
	run(w, 0.0, EngineWatch::STALL + 0.1, frame, false);
	CHECK(!w.running(), "a counter that never moved reads as running");
}

static void testResumes() {
	EngineWatch w;
	int64_t frame = 0;
	double t = run(w, 0.0, 1.0, frame, false);
	CHECK(!w.running(), "setup: the engine should read as stopped");
	frame += FRAMES_PER_STEP;
	w.observe(frame, t);
	CHECK(w.running(), "still stopped after the counter moved again");
}

// The window can stall (a file dialog blocks it) while the engine runs on. The verdict
// rests on what the watch saw, not on how long it went without looking.
static void testWindowStallKeepsEngineRunning() {
	EngineWatch w;
	int64_t frame = 0;
	double t = run(w, 0.0, 1.0, frame, true);
	frame += 48000 * 10;
	w.observe(frame, t + 10.0);
	CHECK(w.running(), "a window stall with the engine running reads as a stopped engine");
}

int main() {
	testStartsRunning();
	testMovingCounterRuns();
	testStillCounterStopsAfterStall();
	testStillFromTheStart();
	testResumes();
	testWindowStallKeepsEngineRunning();
	std::printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
