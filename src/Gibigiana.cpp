// Gibigiana: a clocked echo lane. Four taps read one mono voice back on the clock
// grid, each with a chance; the coins roll once per hit and ride with the audio, so
// a note and its tail share one fate. Wet only, no feedback. The DSP is in
// GibigianaEngine.hpp, and the coin loop is Moleta's.
#include "plugin.hpp"
#include "GibigianaPanel.hpp"
#include "GibigianaEngine.hpp"
#include "widgets.hpp"

// Four taps on one panel until the lanes move to Gibigiana +; the engine holds up to gibigiana::LANES.
static const int TAPS = 4;
static const int COIN_LOOPS[] = {8, 16};

static float uniform() {
	return random::uniform();
}

// "Open" at the top, where the band is bypassed; otherwise the band's span.
struct WidthQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float w = getValue();
		if (w >= gibigiana::WIDTH_OPEN)
			return "Open";
		return string::f("%.1f octaves", 2.f * gibigiana::BAND_OCTAVES * w);
	}
};

struct Gibigiana : Module {
	enum ParamId { ENUMS(STEP_PARAM, TAPS), ENUMS(CHANCE_PARAM, TAPS), ENUMS(LEVEL_PARAM, TAPS), ENUMS(PAN_PARAM, TAPS), CENTRE_PARAM, WIDTH_PARAM, REPEAT_PARAM, DUCK_PARAM, PARAMS_LEN };
	// New inputs and outputs go at the end: patches store cables by index.
	enum InputId { IN_INPUT, CLOCK_INPUT, GATE_INPUT, RESET_INPUT, DENSITY_INPUT, DUCK_INPUT, INPUTS_LEN };
	enum OutputId { LEFT_OUTPUT, RIGHT_OUTPUT, ENUMS(TAP_OUTPUT, TAPS), OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	gibigiana::Engine engine;
	gibigiana::Knobs knobs;

	// Right-click choices: an index into COIN_LOOPS, and whether a patched tap stays in the sum.
	int coinLoop = 1;
	bool keepPatched = false;

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger gateTrigger;
	dsp::SchmittTrigger resetTrigger;

	Gibigiana() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> names;
		for (const gibigiana::Step& s : gibigiana::STEPS)
			names.push_back(s.name);
		for (int i = 0; i < TAPS; i++) {
			configSwitch(STEP_PARAM + i, 0.f, gibigiana::STEP_COUNT - 1, gibigiana::DEFAULT_STEPS[i], string::f("Step %d", i + 1), names)->description =
				"Clock steps behind the input";
			configParam(CHANCE_PARAM + i, 0.f, 1.f, 1.f, string::f("Chance %d", i + 1), "%", 0.f, 100.f)->description =
				"The chance that a hit comes back at this tap";
			configParam(LEVEL_PARAM + i, 0.f, 1.f, 0.5f, string::f("Level %d", i + 1), " dB", -10.f, 20.f);
			configParam(PAN_PARAM + i, -1.f, 1.f, 0.f, string::f("Pan %d", i + 1), "%", 0.f, 100.f);
		}
		configParam(CENTRE_PARAM, 0.f, 1.f, 0.5f, "Band centre", " Hz", gibigiana::BAND_RANGE, gibigiana::BAND_LOW);
		configParam<WidthQuantity>(WIDTH_PARAM, 0.f, 1.f, 1.f, "Band width")->description = "Fully right, the band is open";
		configParam(REPEAT_PARAM, 0.f, 1.f, 0.f, "Repeat", "%", 0.f, 100.f)->description =
			"Fully left, every hit flips a fresh coin; fully right, each tap's coin loop is locked";
		configParam(DUCK_PARAM, 0.f, 1.f, 0.8f, "Duck amount", "%", 0.f, 100.f);
		configInput(IN_INPUT, "Audio");
		configInput(CLOCK_INPUT, "Clock")->description = "One step per pulse; the taps are silent until two pulses have set the step";
		configInput(GATE_INPUT, "Gate")->description = "Each rising edge is a hit and rolls the coins. Unpatched, every clock pulse is a hit";
		configInput(RESET_INPUT, "Reset")->description = "Restarts the coin loops";
		configInput(DENSITY_INPUT, "Density")->description = "10% per volt, added to every CHANCE";
		configInput(DUCK_INPUT, "Duck")->description = "Pulls the reflections down by up to the DUCK amount";
		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
		for (int i = 0; i < TAPS; i++)
			configOutput(TAP_OUTPUT + i, string::f("Tap %d", i + 1))->description = "Patched, the tap leaves the sum (right-click menu)";
		engine.fillCoins(uniform);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		engine.setSampleRate(e.sampleRate);
	}

	void onReset() override {
		engine.setSampleRate(engine.sampleRate);
		engine.fillCoins(uniform);
		coinLoop = 1;
		keepPatched = false;
	}

	void process(const ProcessArgs& args) override {
		if (engine.sampleRate != args.sampleRate)
			engine.setSampleRate(args.sampleRate);

		gibigiana::Frame f;
		f.in = inputs[IN_INPUT].getVoltage();
		f.clock = inputs[CLOCK_INPUT].isConnected();
		f.clockEdge = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		f.gate = inputs[GATE_INPUT].isConnected();
		f.gateEdge = gateTrigger.process(inputs[GATE_INPUT].getVoltage(), 0.1f, 1.f);
		f.reset = resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		f.density = inputs[DENSITY_INPUT].getVoltage();
		f.duck = inputs[DUCK_INPUT].getVoltage();

		for (int i = 0; i < TAPS; i++) {
			knobs.step[i] = (int) std::round(params[STEP_PARAM + i].getValue());
			knobs.chance[i] = params[CHANCE_PARAM + i].getValue();
			knobs.level[i] = params[LEVEL_PARAM + i].getValue();
			knobs.pan[i] = params[PAN_PARAM + i].getValue();
			knobs.patched[i] = outputs[TAP_OUTPUT + i].isConnected();
		}
		knobs.centre = params[CENTRE_PARAM].getValue();
		knobs.width = params[WIDTH_PARAM].getValue();
		knobs.repeat = params[REPEAT_PARAM].getValue();
		knobs.duck = params[DUCK_PARAM].getValue();
		knobs.lanes = TAPS;
		knobs.loopLength = COIN_LOOPS[coinLoop];
		knobs.keepPatched = keepPatched;

		gibigiana::Out o = engine.process(f, knobs, uniform);
		outputs[LEFT_OUTPUT].setVoltage(o.left);
		outputs[RIGHT_OUTPUT].setVoltage(o.right);
		for (int i = 0; i < TAPS; i++)
			outputs[TAP_OUTPUT + i].setVoltage(o.tap[i]);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// The step itself, not the knob's position, so the list can grow.
		json_t* stepsJ = json_array();
		for (int i = 0; i < TAPS; i++) {
			const gibigiana::Step& s = gibigiana::STEPS[gibigiana::clampStep((int) std::round(params[STEP_PARAM + i].getValue()))];
			json_t* sJ = json_object();
			json_object_set_new(sJ, "p", json_integer(s.p));
			json_object_set_new(sJ, "q", json_integer(s.q));
			json_array_append_new(stepsJ, sJ);
		}
		json_object_set_new(rootJ, "steps", stepsJ);
		json_t* coinsJ = json_array();
		for (const moleta::Coin& c : engine.coins) {
			json_t* cJ = json_object();
			json_t* slotsJ = json_array();
			for (float s : c.slots)
				json_array_append_new(slotsJ, json_real(s));
			json_object_set_new(cJ, "slots", slotsJ);
			json_object_set_new(cJ, "pointer", json_integer(c.pointer));
			json_array_append_new(coinsJ, cJ);
		}
		json_object_set_new(rootJ, "coins", coinsJ);
		json_object_set_new(rootJ, "coinLoop", json_integer(COIN_LOOPS[coinLoop]));
		json_object_set_new(rootJ, "keepPatched", json_boolean(keepPatched));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Rack loads the params first, so a saved step wins over the knob's position.
		json_t* stepsJ = json_object_get(rootJ, "steps");
		for (int i = 0; i < TAPS; i++) {
			json_t* sJ = json_array_get(stepsJ, i);
			int p = (int) json_integer_value(json_object_get(sJ, "p"));
			int q = (int) json_integer_value(json_object_get(sJ, "q"));
			if (p > 0 && q > 0)
				params[STEP_PARAM + i].setValue(gibigiana::nearestStep(p, q));
		}
		json_t* coinsJ = json_object_get(rootJ, "coins");
		for (int i = 0; i < gibigiana::LANES; i++) {
			json_t* cJ = json_array_get(coinsJ, i);
			json_t* slotsJ = json_object_get(cJ, "slots");
			moleta::Coin& coin = engine.coins[i];
			for (int k = 0; k < moleta::Coin::MAX_LOOP; k++) {
				if (json_t* sJ = json_array_get(slotsJ, k))
					coin.slots[k] = clamp((float) json_number_value(sJ), 0.f, 0.9999999f);
			}
			if (json_t* pJ = json_object_get(cJ, "pointer"))
				coin.pointer = clamp((int) json_integer_value(pJ), 0, moleta::Coin::MAX_LOOP - 1);
		}
		int loop = (int) json_integer_value(json_object_get(rootJ, "coinLoop"));
		coinLoop = loop == 8 ? 0 : 1;
		keepPatched = json_is_true(json_object_get(rootJ, "keepPatched"));
	}
};

// Each tap's step; dim when the step reaches past the buffer and the tap is silent.
struct GibigianaReadout : w72::Readout {
	Gibigiana* module;

	GibigianaReadout(Gibigiana* module)
		: Readout(GibigianaPanel::READOUT_WELL, GibigianaPanel::READOUT, TAPS, GibigianaPanel::READOUT_VALUE_SIZE), module(module) {}

	std::string text(int i) override {
		if (!module)
			return GibigianaPanel::READOUT_SAMPLE[i];
		int s = gibigiana::clampStep((int) std::round(module->params[Gibigiana::STEP_PARAM + i].getValue()));
		return gibigiana::STEPS[s].shown;
	}

	bool dim(int i) override {
		return module && !module->engine.taps[i].reach;
	}
};

struct GibigianaWidget : ModuleWidget {
	GibigianaWidget(Gibigiana* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gibigiana.svg")));

		for (Vec pos : GibigianaPanel::SCREWS)
			addChild(createWidget<ScrewBlack>(pos));

		addChild(new GibigianaReadout(module));

		const Vec step[TAPS] = {GibigianaPanel::STEP_1, GibigianaPanel::STEP_2, GibigianaPanel::STEP_3, GibigianaPanel::STEP_4};
		const Vec chance[TAPS] = {GibigianaPanel::CHANCE_1, GibigianaPanel::CHANCE_2, GibigianaPanel::CHANCE_3, GibigianaPanel::CHANCE_4};
		const Vec level[TAPS] = {GibigianaPanel::LEVEL_1, GibigianaPanel::LEVEL_2, GibigianaPanel::LEVEL_3, GibigianaPanel::LEVEL_4};
		const Vec pan[TAPS] = {GibigianaPanel::PAN_1, GibigianaPanel::PAN_2, GibigianaPanel::PAN_3, GibigianaPanel::PAN_4};
		const Vec out[TAPS] = {GibigianaPanel::OUT_1, GibigianaPanel::OUT_2, GibigianaPanel::OUT_3, GibigianaPanel::OUT_4};
		for (int i = 0; i < TAPS; i++) {
			addParam(createParamCentered<RoundBlackKnob>(step[i], module, Gibigiana::STEP_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(chance[i], module, Gibigiana::CHANCE_PARAM + i));
			addParam(createParamCentered<Trimpot>(level[i], module, Gibigiana::LEVEL_PARAM + i));
			addParam(createParamCentered<Trimpot>(pan[i], module, Gibigiana::PAN_PARAM + i));
		}
		addParam(createParamCentered<Trimpot>(GibigianaPanel::CENTRE, module, Gibigiana::CENTRE_PARAM));
		addParam(createParamCentered<Trimpot>(GibigianaPanel::WIDTH, module, Gibigiana::WIDTH_PARAM));
		addParam(createParamCentered<Trimpot>(GibigianaPanel::REPEAT, module, Gibigiana::REPEAT_PARAM));
		addParam(createParamCentered<Trimpot>(GibigianaPanel::DUCK_AMOUNT, module, Gibigiana::DUCK_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::IN, module, Gibigiana::IN_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::CLOCK, module, Gibigiana::CLOCK_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::GATE, module, Gibigiana::GATE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::RESET, module, Gibigiana::RESET_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::DENSITY, module, Gibigiana::DENSITY_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::DUCK, module, Gibigiana::DUCK_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(GibigianaPanel::OUT_L, module, Gibigiana::LEFT_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(GibigianaPanel::OUT_R, module, Gibigiana::RIGHT_OUTPUT));
		for (int i = 0; i < TAPS; i++)
			addOutput(createOutputCentered<DarkPJ301MPort>(out[i], module, Gibigiana::TAP_OUTPUT + i));
	}

	void appendContextMenu(Menu* menu) override {
		Gibigiana* m = getModule<Gibigiana>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Coin loop", {"8 hits", "16 hits"}, &m->coinLoop));
		menu->addChild(createBoolPtrMenuItem("Patched tap stays in the sum", "", &m->keepPatched));
	}
};

Model* modelGibigiana = createModel<Gibigiana, GibigianaWidget>("Gibigiana");
