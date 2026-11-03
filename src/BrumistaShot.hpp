// Brumista's screenshots: panels and the rack drawn offscreen by Rack's own widgets, as PNG.
#pragma once
#include <rack.hpp>

namespace brumista {

// The longest side a screenshot may have, in pixels.
const int SHOT_MAX_SIDE = 4096;

// A PNG file's bytes and its size in pixels.
struct Shot {
	std::string png;
	int width = 0;
	int height = 0;
};

// One module's panel with its live state: knobs, lights and displays, without cables,
// drawn as Rack's -t draws its module images. zoom is pixels per Rack pixel. Main thread.
Shot shotModule(rack::app::ModuleWidget* mw, float zoom);

// A region of the rack, in the rack's own coordinates, as the rack draws it: rails,
// modules, plugs and cables, wherever the view is. Main thread.
Shot shotRack(rack::math::Rect region, float zoom);

// RFC 4648 base64, for carrying a PNG in a JSON reply.
std::string base64(const std::string& data);

} // namespace brumista
