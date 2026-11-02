// Brumista's commands. protocol/commands.json describes them; tools/check-commands.sh
// checks the dispatch table in BrumistaCommands.cpp against it.
#pragma once
#include "BrumistaServer.hpp"

namespace brumista {

// One request line in, one reply out. Server thread.
std::string handleLine(Server& server, const std::string& line);

// Finishes view changes that take more than one frame. Main thread, every frame.
void stepView();

} // namespace brumista
