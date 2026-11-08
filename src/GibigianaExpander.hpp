// What passes along a Gibigiana chain, every sample. Plain data, no Rack includes.
// All of the sound is made in Gibigiana (GibigianaEngine.hpp); a Gibigiana + only
// sends its lane's settings in and plays its own tap back.
//
// Leftward, each Gibigiana + writes into the rightExpander of the module on its
// left: its own lane first, then the lanes it got from its right. Rightward,
// Gibigiana writes into the leftExpander of the Gibigiana + on its right: the
// expanders' taps in chain order, and each Gibigiana + takes the first and passes
// the rest on. Rack flips the buffers at the end of each sample, so a lane n
// places down the chain hears its tap n samples late; the engine reads that lane n
// samples early to make up for it.
#pragma once
#include "GibigianaEngine.hpp"

// The expanders a chain holds, after Gibigiana's own lane.
static const int GIBIGIANA_PLUS_MAX = gibigiana::LANES - 1;
// The STEP knob is a Gibigiana +'s first param, so Gibigiana can set a new lane's
// step by its place when ADD LANE makes it.
static const int GIBIGIANA_PLUS_STEP_PARAM = 0;

struct GibigianaLane {
	int step = gibigiana::DEFAULT_STEPS[1];
	float chance = 1.f;
	float level = 0.5f;
	float pan = 0.f;
	// Volts.
	float chanceCv = 0.f;
	float levelCv = 0.f;
	float panCv = 0.f;
	bool patched = false;
};

// Leftward: the lanes to the right of the module that receives it, nearest first.
struct GibigianaLanes {
	int count = 0;
	GibigianaLane lane[GIBIGIANA_PLUS_MAX];
};

// Rightward: the taps of the receiver and of the lanes beyond it, nearest first.
struct GibigianaTaps {
	// A Gibigiana starts the chain this message came down.
	bool linked = false;
	// The receiver's lane, counted from 0 at Gibigiana's own.
	int place = 0;
	float tap[GIBIGIANA_PLUS_MAX] = {};
	// The lane's step fits in the buffer; its readout is dim when it doesn't.
	bool reach[GIBIGIANA_PLUS_MAX] = {};
};
