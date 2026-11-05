// W72's own parts, drawn with NanoVG. The panel SVG draws their wells and bezels;
// these draw what lights up, on Rack's light layer, so it stays lit when the room is dimmed.
#pragma once
#include "plugin.hpp"

namespace w72 {

// Display ink, and the colour of a flat light that is off (design-system.md, Displays).
static const NVGcolor INK = nvgRGB(0xf1, 0xf3, 0xf4);
static const NVGcolor LIGHT_OFF = nvgRGB(0x2b, 0x2d, 0x30);
static const float DIM = 0.4f;

inline void drawFlat(NVGcontext* vg, math::Rect r, NVGcolor color) {
	nvgBeginPath(vg);
	nvgRect(vg, r.pos.x, r.pos.y, r.size.x, r.size.y);
	nvgFillColor(vg, color);
	nvgFill(vg);
}

// A latching button with a flat square light in the middle. Lit follows the
// parameter; sample is what the module browser shows, where there is no module.
struct FlatButton : app::Switch {
	float lightSize = 0.f;
	bool sample = false;

	virtual bool isLit() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? pq->getValue() > 0.5f : sample;
	}

	math::Rect lightBox() {
		return math::Rect(box.size.minus(math::Vec(lightSize, lightSize)).div(2), math::Vec(lightSize, lightSize));
	}

	void draw(const DrawArgs& args) override {
		drawFlat(args.vg, lightBox(), LIGHT_OFF);
		app::Switch::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && isLit())
			drawFlat(args.vg, lightBox(), INK);
		app::Switch::drawLayer(args, layer);
	}
};

// Places a flat button by its centre, at the sizes the panel header gives.
template <class TButton = FlatButton>
TButton* createFlatButton(math::Vec centre, float size, float lightSize, engine::Module* module, int paramId) {
	TButton* o = createParam<TButton>(centre.minus(math::Vec(size, size).div(2)), module, paramId);
	o->box.size = math::Vec(size, size);
	o->lightSize = lightSize;
	return o;
}

// A row of flat bars in a display well. Each bar is off (0), dim or full (1).
// The boxes are in panel px, as the header lists them.
struct FlatBars : widget::TransparentWidget {
	std::vector<math::Rect> bars;

	FlatBars(math::Rect well, const math::Rect* boxes, int count) {
		box = well;
		for (int i = 0; i < count; i++)
			bars.push_back(math::Rect(boxes[i].pos.minus(well.pos), boxes[i].size));
	}

	virtual float level(int i) = 0;

	void draw(const DrawArgs& args) override {
		for (const math::Rect& r : bars)
			drawFlat(args.vg, r, LIGHT_OFF);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			for (int i = 0; i < (int) bars.size(); i++) {
				float l = level(i);
				if (l > 0.f)
					drawFlat(args.vg, bars[i], nvgTransRGBAf(INK, l));
			}
		}
		widget::TransparentWidget::drawLayer(args, layer);
	}
};

// A row of signage values in a readout well: Inter Tight SemiBold in display ink,
// centred on each cell's baseline point. The panel draws the well and the names;
// this draws only the values, on the light layer. The points are in panel px, as
// the header lists them.
struct Readout : widget::TransparentWidget {
	std::vector<math::Vec> baselines;
	float fontSize;

	Readout(math::Rect well, const math::Vec* points, int count, float fontSize) : fontSize(fontSize) {
		box = well;
		for (int i = 0; i < count; i++)
			baselines.push_back(points[i].minus(well.pos));
	}

	virtual std::string text(int i) = 0;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			// A missing font draws nothing rather than failing.
			std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/InterTight-SemiBold.ttf"));
			if (font && font->handle >= 0) {
				nvgFontFaceId(args.vg, font->handle);
				nvgFontSize(args.vg, fontSize);
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
				nvgFillColor(args.vg, INK);
				for (int i = 0; i < (int) baselines.size(); i++) {
					std::string s = text(i);
					nvgText(args.vg, baselines[i].x, baselines[i].y, s.c_str(), NULL);
				}
			}
		}
		widget::TransparentWidget::drawLayer(args, layer);
	}
};

} // namespace w72
