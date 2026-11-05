// What Moleta sends the Moleta+ on its right, every sample: the beat clock's tick
// and the shared controls, all the expander's output engines need. Plain data, no
// Rack includes. Moleta+ allocates both buffers in its leftExpander; Rack flips
// them at the end of the sample, so the expander reads this one sample late, and
// the base delays its own outputs by a sample while one is attached.
#pragma once
#include "MoletaEngine.hpp"

struct MoletaMessage {
	moleta::Tick tick;
	moleta::Shared shared;
};
