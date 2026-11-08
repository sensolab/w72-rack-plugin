// What Moleta sends each Moleta+ in the chain on its right, every sample: the
// beat clock's tick and the shared controls, all the expander's output engines
// need, and the expander's place. Plain data, no Rack includes.
//
// Moleta walks the chain and writes into every Moleta+'s leftExpander directly,
// not hop by hop, so each one reads the same tick one sample late, wherever it
// sits; the base delays its own outputs by a sample while one is attached. A
// Moleta+ clears what it read, so once nothing writes to it (the chain is broken
// or past the cap) it reads as unlinked on the next sample.
#pragma once
#include "MoletaEngine.hpp"

// The Moleta+ modules a chain holds: outputs 5 to 20.
static const int MOLETA_PLUS_MAX = 4;
// The outputs a Moleta, and each Moleta+, has.
static const int MOLETA_OUTS = 4;

struct MoletaMessage {
	// Written by a Moleta this sample.
	bool linked = false;
	// The expander's place in the chain, 0 for the one next to Moleta.
	int place = 0;
	moleta::Tick tick;
	moleta::Shared shared;
};

// The panel number of output i (from 0) of the Moleta+ at a place: 5 to 8 on the first.
inline int moletaPlusOutput(int place, int i) {
	return MOLETA_OUTS * (place + 1) + i + 1;
}
