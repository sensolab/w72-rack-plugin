// Barbapedana: a 16-step sequencer crossed with a Turing-style random loop. Each
// step plays its VALUE or the loop's value by its KEEP chance, unless it is
// anchored, and fires its gate by its CHANCE. Pressing a free step's anchor captures
// the loop's value into VALUE. The 16 knobs show one page of 48 parameters at a
// time (BarbapedanaEngine.hpp).
#include "plugin.hpp"
#include "BarbapedanaPanel.hpp"
#include "BarbapedanaEngine.hpp"
#include "widgets.hpp"

using barbapedana::STEPS;

static float uniform() {
	return random::uniform();
}

static const char* const PAGE_NAMES[] = {"VALUE", "KEEP", "CHANCE"};

struct Barbapedana : Module {
	// New params, inputs and outputs go at the end: patches store them by index.
	enum ParamId {
		ENUMS(VALUE_PARAM, STEPS),
		ENUMS(KEEP_PARAM, STEPS),
		ENUMS(CHANCE_PARAM, STEPS),
		ENUMS(ANCHOR_PARAM, STEPS),
		PAGE_PARAM,
		LENGTH_PARAM,
		MUTATE_PARAM,
		RANGE_PARAM,
		OFFSET_PARAM,
		PARAMS_LEN
	};
	enum InputId { CLOCK_INPUT, RESET_INPUT, MUTATE_INPUT, CHANCE_INPUT, TURING_INPUT, KEEP_INPUT, INPUTS_LEN };
	enum OutputId { CV_OUTPUT, GATE_OUTPUT, ENUMS(STEP_OUTPUT, STEPS), OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	barbapedana::Engine engine;
	barbapedana::Knobs knobs;

	// Right-click choices: 0 is the default of each.
	int gateMode = 0;  // gate (half a step) or trigger
	int silentMode = 0;  // CV on silent steps: hold or move
	int draw = barbapedana::WALK;

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;

	// Shows the volts the step sends: OFFSET + value × RANGE.
	struct ValueQuantity : ParamQuantity {
		float volts(float v) {
			return module->params[OFFSET_PARAM].getValue() + v * module->params[RANGE_PARAM].getValue();
		}
		float getDisplayValue() override {
			return volts(getValue());
		}
		void setDisplayValue(float d) override {
			float range = module->params[RANGE_PARAM].getValue();
			if (range > 0.f)
				setValue((d - module->params[OFFSET_PARAM].getValue()) / range);
		}
	};

	Barbapedana() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int k = 0; k < STEPS; k++) {
			std::string n = string::f("Step %d", k + 1);
			configParam<ValueQuantity>(VALUE_PARAM + k, 0.f, 1.f, 0.5f, n + " value", " V")->description =
				"What the step plays when it is yours: OFFSET + value × RANGE";
			configParam(KEEP_PARAM + k, 0.f, 1.f, 0.f, n + " keep", "%", 0.f, 100.f)->description =
				"The chance the step plays its VALUE rather than the loop";
			configParam(CHANCE_PARAM + k, 0.f, 1.f, 1.f, n + " chance", "%", 0.f, 100.f)->description =
				"The chance the step fires its gate";
			configSwitch(ANCHOR_PARAM + k, 0.f, 1.f, 0.f, n + " anchor", {"Free", "Anchored"})->description =
				"Anchored, the step always plays its VALUE. Pressed on a free step, it captures the loop's value into VALUE";
			getParamQuantity(ANCHOR_PARAM + k)->randomizeEnabled = false;
			configOutput(STEP_OUTPUT + k, n + " gate");
		}
		configSwitch(PAGE_PARAM, 0.f, 2.f, 0.f, "Page", {"Value", "Keep", "Chance"})->description =
			"What the 16 knobs edit";
		getParamQuantity(PAGE_PARAM)->randomizeEnabled = false;
		configParam(LENGTH_PARAM, 1.f, STEPS, STEPS, "Length", " steps")->snapEnabled = true;
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.1f, "Mutate", "%", 0.f, 100.f)->description =
			"The chance a free step's loop value is redrawn as the playhead reaches it";
		configParam(RANGE_PARAM, 0.f, 10.f, 2.f, "Range", " V")->description = "The span the values cover";
		getParamQuantity(RANGE_PARAM)->randomizeEnabled = false;
		configParam(OFFSET_PARAM, -5.f, 5.f, 0.f, "Offset", " V")->description = "Where the span starts";
		getParamQuantity(OFFSET_PARAM)->randomizeEnabled = false;
		configInput(CLOCK_INPUT, "Clock")->description = "Advances one step per pulse";
		configInput(RESET_INPUT, "Reset")->description = "The next clock plays step 1";
		configInput(MUTATE_INPUT, "Mutate CV")->description = "10% per volt, added to MUTATE";
		configInput(CHANCE_INPUT, "Chance CV")->description = "10% per volt, added to every step's CHANCE";
		configInput(TURING_INPUT, "Turing")->description =
			"Patched, a redrawn loop value is sampled from here instead: the voltage the step will send";
		configInput(KEEP_INPUT, "Keep CV")->description = "10% per volt, added to every free step's KEEP";
		configOutput(CV_OUTPUT, "CV")->description = "Moves when a gate fires, unless the right-click menu says otherwise";
		configOutput(GATE_OUTPUT, "Gate");
		engine.fill(uniform);
	}

	void onReset() override {
		engine = barbapedana::Engine();
		engine.fill(uniform);
		gateMode = 0;
		silentMode = 0;
		draw = barbapedana::WALK;
	}

	void onRandomize(const RandomizeEvent& e) override {
		Module::onRandomize(e);
		engine.fill(uniform);
	}

	void process(const ProcessArgs& args) override {
		barbapedana::Frame f;
		f.clockEdge = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		f.reset = resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		f.turing = inputs[TURING_INPUT].isConnected();
		f.turingIn = inputs[TURING_INPUT].getVoltage();

		for (int k = 0; k < STEPS; k++) {
			knobs.value[k] = params[VALUE_PARAM + k].getValue();
			knobs.keep[k] = params[KEEP_PARAM + k].getValue();
			knobs.chance[k] = params[CHANCE_PARAM + k].getValue();
			knobs.anchor[k] = params[ANCHOR_PARAM + k].getValue() > 0.5f;
		}
		knobs.length = (int) std::round(params[LENGTH_PARAM].getValue());
		knobs.mutate = params[MUTATE_PARAM].getValue() + 0.1f * inputs[MUTATE_INPUT].getVoltage();
		knobs.keepShift = 0.1f * inputs[KEEP_INPUT].getVoltage();
		knobs.chanceShift = 0.1f * inputs[CHANCE_INPUT].getVoltage();
		knobs.range = params[RANGE_PARAM].getValue();
		knobs.offset = params[OFFSET_PARAM].getValue();
		knobs.draw = draw;
		knobs.trigger = gateMode == 1;
		knobs.moveOnSilent = silentMode == 1;

		barbapedana::Out o = engine.process(args.sampleTime, f, knobs, uniform);
		outputs[CV_OUTPUT].setVoltage(o.cv);
		outputs[GATE_OUTPUT].setVoltage(o.gate ? 10.f : 0.f);
		for (int k = 0; k < STEPS; k++)
			outputs[STEP_OUTPUT + k].setVoltage(o.gates[k] ? 10.f : 0.f);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_t* loopJ = json_array();
		for (float v : engine.loop)
			json_array_append_new(loopJ, json_real(v));
		json_object_set_new(rootJ, "loop", loopJ);
		json_object_set_new(rootJ, "bits", json_integer(engine.bits));
		json_object_set_new(rootJ, "gateMode", json_integer(gateMode));
		json_object_set_new(rootJ, "silentMode", json_integer(silentMode));
		json_object_set_new(rootJ, "draw", json_integer(draw));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* loopJ = json_object_get(rootJ, "loop");
		for (int k = 0; k < STEPS; k++) {
			if (json_t* vJ = json_array_get(loopJ, k))
				engine.loop[k] = clamp((float) json_number_value(vJ), 0.f, 1.f);
		}
		if (json_t* bJ = json_object_get(rootJ, "bits"))
			engine.bits = (uint16_t) json_integer_value(bJ);
		gateMode = clamp((int) json_integer_value(json_object_get(rootJ, "gateMode")), 0, 1);
		silentMode = clamp((int) json_integer_value(json_object_get(rootJ, "silentMode")), 0, 1);
		draw = clamp((int) json_integer_value(json_object_get(rootJ, "draw")), 0, barbapedana::DRAWS_LEN - 1);
	}

	int page() {
		return clamp((int) std::round(params[PAGE_PARAM].getValue()), 0, 2);
	}
};

// An anchor. Pressed on a free step it captures the loop's value into VALUE and
// anchors the step; pressed again it frees the step. Either is one undo action.
// Anchoring by other means (a MIDI map, Brumista) locks the step without a capture.
struct AnchorButton : w72::FlatButton {
	int step = 0;

	void onDragStart(const DragStartEvent& e) override {
		Barbapedana* m = dynamic_cast<Barbapedana*>(module);
		if (e.button != GLFW_MOUSE_BUTTON_LEFT || !m) {
			w72::FlatButton::onDragStart(e);
			return;
		}
		history::ComplexAction* h = new history::ComplexAction;
		auto change = [&](int paramId, float value) {
			float old = m->params[paramId].getValue();
			m->getParamQuantity(paramId)->setValue(value);
			history::ParamChange* p = new history::ParamChange;
			p->moduleId = m->id;
			p->paramId = paramId;
			p->oldValue = old;
			p->newValue = m->params[paramId].getValue();
			h->push(p);
		};
		if (m->params[Barbapedana::ANCHOR_PARAM + step].getValue() > 0.5f) {
			h->name = string::f("free Barbapedana step %d", step + 1);
			change(Barbapedana::ANCHOR_PARAM + step, 0.f);
		}
		else {
			h->name = string::f("capture Barbapedana step %d", step + 1);
			change(Barbapedana::VALUE_PARAM + step, m->engine.capture(step));
			change(Barbapedana::ANCHOR_PARAM + step, 1.f);
		}
		APP->history->push(h);
	}
};

// The steps, following PAGE. The top line names the page and shows the step,
// LENGTH and the CV going out. On VALUE each step shows its value as a solid bar
// and the loop's value beside it as an outline (what an anchor would capture); on
// KEEP and CHANCE, one bar for the knob. Anchored steps get a dot and a dim
// outline, the playhead a line, and steps past LENGTH are dim.
struct BarbapedanaDisplay : widget::TransparentWidget {
	Barbapedana* module;

	// Layout, in px from the well's top-left, tuned on the mockup.
	static constexpr float PAD = 6.f;
	static constexpr float LINE = 12.f;
	static constexpr float BARS_TOP = 20.f;
	static constexpr float BARS_BOTTOM = 68.f;
	static constexpr float DOT = BARS_BOTTOM + 5.f;
	static constexpr float PLAYHEAD = BARS_BOTTOM + 10.f;

	// What the module browser shows.
	static constexpr float SAMPLE_VALUE[STEPS] = {.55f, .30f, .70f, .45f, .20f, .62f, .80f, .35f, .50f, .25f, .90f, .40f, .60f, .15f, .75f, .45f};
	static constexpr float SAMPLE_LOOP[STEPS] = {.40f, .65f, .35f, .45f, .85f, .30f, .55f, .70f, .20f, .60f, .45f, .80f, .60f, .50f, .30f, .65f};

	BarbapedanaDisplay(Barbapedana* module) : module(module) {
		box = BarbapedanaPanel::DISPLAY_WELL;
	}

	float pitch() {
		return (box.size.x - 2 * PAD) / STEPS;
	}

	// One bar slot: the bar on KEEP and CHANCE, or the left (value) or right (loop) half on VALUE.
	math::Rect slot(int k, int part) {
		float x = PAD + k * pitch() + pitch() / 2;
		float h = BARS_BOTTOM - BARS_TOP;
		if (part == 0)
			return math::Rect(x - 6.5f, BARS_TOP, 13.f, h);
		if (part == 1)
			return math::Rect(x - 7.f, BARS_TOP, 6.5f, h);
		return math::Rect(x + 0.5f, BARS_TOP, 6.5f, h);
	}

	// A bar's lit part; at least 1 px, so a value of 0 still reads as a floor.
	static math::Rect level(math::Rect r, float v) {
		float h = std::max(r.size.y * clamp(v, 0.f, 1.f), 1.f);
		return math::Rect(r.pos.x, r.pos.y + r.size.y - h, r.size.x, h);
	}

	int page() {
		return module ? module->page() : 0;
	}

	void draw(const DrawArgs& args) override {
		for (int k = 0; k < STEPS; k++) {
			if (page() == 0) {
				w72::drawFlat(args.vg, slot(k, 1), w72::LIGHT_OFF);
				w72::drawFlat(args.vg, slot(k, 2), w72::LIGHT_OFF);
			}
			else
				w72::drawFlat(args.vg, slot(k, 0), w72::LIGHT_OFF);
		}
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			drawLit(args.vg);
		widget::TransparentWidget::drawLayer(args, layer);
	}

	void drawLit(NVGcontext* vg) {
		int p = page();
		int length = module ? (int) std::round(module->params[Barbapedana::LENGTH_PARAM].getValue()) : 12;
		int playing = module ? module->engine.step : 5;
		for (int k = 0; k < STEPS; k++) {
			NVGcolor ink = k < length ? w72::INK : nvgTransRGBAf(w72::INK, w72::DIM);
			bool anchored = module ? module->params[Barbapedana::ANCHOR_PARAM + k].getValue() > 0.5f : (k == 0 || k == 3 || k == 7 || k == 12);
			if (p == 0) {
				float v = module ? module->params[Barbapedana::VALUE_PARAM + k].getValue() : SAMPLE_VALUE[k];
				float l = module ? module->engine.loop[k] : SAMPLE_LOOP[k];
				w72::drawFlat(vg, level(slot(k, 1), v), ink);
				// An anchored step never plays the loop, so its outline is dim.
				NVGcolor outline = anchored ? nvgTransRGBAf(ink, ink.a * w72::DIM) : ink;
				math::Rect o = level(slot(k, 2), l);
				if (o.size.y < 2.f)
					w72::drawFlat(vg, o, outline);
				else {
					nvgBeginPath(vg);
					nvgRect(vg, o.pos.x + 0.5f, o.pos.y + 0.5f, o.size.x - 1.f, o.size.y - 1.f);
					nvgStrokeColor(vg, outline);
					nvgStrokeWidth(vg, 1.f);
					nvgStroke(vg);
				}
			}
			else {
				int base = p == 1 ? Barbapedana::KEEP_PARAM : Barbapedana::CHANCE_PARAM;
				float v = module ? module->params[base + k].getValue() : 0.5f;
				w72::drawFlat(vg, level(slot(k, 0), v), ink);
			}
			float cx = PAD + k * pitch() + pitch() / 2;
			if (anchored) {
				nvgBeginPath(vg);
				nvgCircle(vg, cx, DOT, 1.5f);
				nvgFillColor(vg, ink);
				nvgFill(vg);
			}
			if (k == playing)
				w72::drawFlat(vg, math::Rect(cx - 6.5f, PLAYHEAD, 13.f, 1.5f), w72::INK);
		}

		// The top line. A missing font draws nothing rather than failing.
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/InterTight-SemiBold.ttf"));
		if (!font || font->handle < 0)
			return;
		nvgFontFaceId(vg, font->handle);
		nvgFillColor(vg, w72::INK);
		nvgFontSize(vg, 6.5f);
		nvgTextLetterSpacing(vg, 0.04f * 6.5f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgText(vg, PAD, LINE, PAGE_NAMES[p], NULL);
		nvgTextLetterSpacing(vg, 0.f);
		nvgFontSize(vg, 9.f);
		std::string at = playing >= 0 ? string::f("%d / %d", playing + 1, length) : string::f("– / %d", length);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
		nvgText(vg, box.size.x / 2, LINE, at.c_str(), NULL);
		float cv = module ? module->outputs[Barbapedana::CV_OUTPUT].getVoltage() : 2.35f;
		std::string volts = string::f("%+.2f V", cv);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgText(vg, box.size.x - PAD, LINE, volts.c_str(), NULL);
	}
};

constexpr float BarbapedanaDisplay::SAMPLE_VALUE[];
constexpr float BarbapedanaDisplay::SAMPLE_LOOP[];

struct BarbapedanaWidget : ModuleWidget {
	// The three pages' knobs share the 16 places; PAGE shows one set.
	ParamWidget* knobs[3][STEPS] = {};

	BarbapedanaWidget(Barbapedana* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Barbapedana.svg")));

		for (Vec pos : BarbapedanaPanel::SCREWS)
			addChild(createWidget<ScrewBlack>(pos));

		addChild(new BarbapedanaDisplay(module));
		addParam(createParamCentered<CKSSThreeHorizontal>(BarbapedanaPanel::PAGE, module, Barbapedana::PAGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(BarbapedanaPanel::LENGTH, module, Barbapedana::LENGTH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(BarbapedanaPanel::MUTATE, module, Barbapedana::MUTATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(BarbapedanaPanel::RANGE, module, Barbapedana::RANGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(BarbapedanaPanel::OFFSET, module, Barbapedana::OFFSET_PARAM));

		static const Vec steps[STEPS] = {
			BarbapedanaPanel::STEP_1, BarbapedanaPanel::STEP_2, BarbapedanaPanel::STEP_3, BarbapedanaPanel::STEP_4,
			BarbapedanaPanel::STEP_5, BarbapedanaPanel::STEP_6, BarbapedanaPanel::STEP_7, BarbapedanaPanel::STEP_8,
			BarbapedanaPanel::STEP_9, BarbapedanaPanel::STEP_10, BarbapedanaPanel::STEP_11, BarbapedanaPanel::STEP_12,
			BarbapedanaPanel::STEP_13, BarbapedanaPanel::STEP_14, BarbapedanaPanel::STEP_15, BarbapedanaPanel::STEP_16,
		};
		static const Vec anchors[STEPS] = {
			BarbapedanaPanel::ANCHOR_1, BarbapedanaPanel::ANCHOR_2, BarbapedanaPanel::ANCHOR_3, BarbapedanaPanel::ANCHOR_4,
			BarbapedanaPanel::ANCHOR_5, BarbapedanaPanel::ANCHOR_6, BarbapedanaPanel::ANCHOR_7, BarbapedanaPanel::ANCHOR_8,
			BarbapedanaPanel::ANCHOR_9, BarbapedanaPanel::ANCHOR_10, BarbapedanaPanel::ANCHOR_11, BarbapedanaPanel::ANCHOR_12,
			BarbapedanaPanel::ANCHOR_13, BarbapedanaPanel::ANCHOR_14, BarbapedanaPanel::ANCHOR_15, BarbapedanaPanel::ANCHOR_16,
		};
		static const Vec gates[STEPS] = {
			BarbapedanaPanel::GATE_1, BarbapedanaPanel::GATE_2, BarbapedanaPanel::GATE_3, BarbapedanaPanel::GATE_4,
			BarbapedanaPanel::GATE_5, BarbapedanaPanel::GATE_6, BarbapedanaPanel::GATE_7, BarbapedanaPanel::GATE_8,
			BarbapedanaPanel::GATE_9, BarbapedanaPanel::GATE_10, BarbapedanaPanel::GATE_11, BarbapedanaPanel::GATE_12,
			BarbapedanaPanel::GATE_13, BarbapedanaPanel::GATE_14, BarbapedanaPanel::GATE_15, BarbapedanaPanel::GATE_16,
		};
		static const int pageParams[3] = {Barbapedana::VALUE_PARAM, Barbapedana::KEEP_PARAM, Barbapedana::CHANCE_PARAM};
		for (int k = 0; k < STEPS; k++) {
			for (int p = 0; p < 3; p++) {
				knobs[p][k] = createParamCentered<RoundBlackKnob>(steps[k], module, pageParams[p] + k);
				knobs[p][k]->visible = p == 0;
				addParam(knobs[p][k]);
			}
			AnchorButton* a = w72::createFlatButton<AnchorButton>(anchors[k], BarbapedanaPanel::BUTTON_SIZE, BarbapedanaPanel::FLAT_LIGHT_SIZE, module, Barbapedana::ANCHOR_PARAM + k);
			a->step = k;
			a->sample = k == 0 || k == 3 || k == 7 || k == 12;
			addParam(a);
			addOutput(createOutputCentered<DarkPJ301MPort>(gates[k], module, Barbapedana::STEP_OUTPUT + k));
		}

		addInput(createInputCentered<DarkPJ301MPort>(BarbapedanaPanel::CLOCK, module, Barbapedana::CLOCK_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(BarbapedanaPanel::RESET, module, Barbapedana::RESET_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(BarbapedanaPanel::MUTATE_CV, module, Barbapedana::MUTATE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(BarbapedanaPanel::CHANCE_CV, module, Barbapedana::CHANCE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(BarbapedanaPanel::TURING, module, Barbapedana::TURING_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(BarbapedanaPanel::KEEP_CV, module, Barbapedana::KEEP_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(BarbapedanaPanel::CV, module, Barbapedana::CV_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(BarbapedanaPanel::GATE, module, Barbapedana::GATE_OUTPUT));
	}

	void step() override {
		if (Barbapedana* m = getModule<Barbapedana>()) {
			int page = m->page();
			for (int p = 0; p < 3; p++) {
				for (int k = 0; k < STEPS; k++)
					knobs[p][k]->visible = p == page;
			}
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		Barbapedana* m = getModule<Barbapedana>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Gate outputs", {"Gate (half a step)", "Trigger (1 ms)"}, &m->gateMode));
		menu->addChild(createIndexPtrSubmenuItem("CV on silent steps", {"Hold", "Move"}, &m->silentMode));
		menu->addChild(createIndexPtrSubmenuItem("Loop draws", {"Walk", "Anywhere", "Turing bits"}, &m->draw));
	}
};

Model* modelBarbapedana = createModel<Barbapedana, BarbapedanaWidget>("Barbapedana");
