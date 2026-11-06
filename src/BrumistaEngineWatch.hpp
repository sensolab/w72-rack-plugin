// Brumista's watch on Rack's engine: whether its frame counter still moves. The engine stops
// when the primary audio module has no device to drive it; modules then don't process and
// knobs set targets the engine never reaches, while Brumista goes on answering from its own
// threads. No Rack includes, so it can be tested on the host (tests/engine_watch_test.cpp).
#pragma once
#include <cstdint>

namespace brumista {

// Fed from the window's step on the main thread. The verdict rests only on what the watch
// has seen, so a window that stops stepping (a file dialog blocks it) doesn't make a running
// engine look stopped.
struct EngineWatch {
	// Seconds the counter may stand still before the engine counts as stopped: well above
	// one audio block, so a large block size doesn't read as a stop.
	static constexpr double STALL = 0.5;

	void observe(int64_t frame, double now) {
		if (!seen || frame != lastFrame) {
			seen = true;
			lastFrame = frame;
			lastMove = now;
		}
		stopped = now - lastMove >= STALL;
	}

	// Running until the watch has seen the counter stand still for STALL.
	bool running() const {
		return !stopped;
	}

private:
	bool seen = false;
	int64_t lastFrame = 0;
	double lastMove = 0.0;
	bool stopped = false;
};

} // namespace brumista
