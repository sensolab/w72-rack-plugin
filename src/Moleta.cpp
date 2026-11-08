// Moleta: a master clock whose four outputs are each a ratio, a chance and a
// length. It runs on its BPM or follows CLOCK; a coin per output thins the pulses,
// and REPEAT loops the coin flips into a phrase. The DSP is in MoletaEngine.hpp,
// shared with the Moleta+ expander, which gets the clock through MoletaMessage.
#include "plugin.hpp"
#include "MoletaPanel.hpp"
#include "MoletaEngine.hpp"
#include "MoletaExpander.hpp"
#include "widgets.hpp"

static const int OUTS = MOLETA_OUTS;
static const int PULSES_PER_BEAT[] = {1, 4, 24};
static const int BEATS_PER_BAR[] = {2, 3, 4, 5, 6, 7, 8, 12};
static const int COIN_LOOPS[] = {8, 16};

// The expander's model: only Moleta+ modules on the right count as expanders.
static Model* expanderModel() {
	return modelMoletaPlus;
}

static float uniform() {
	return random::uniform();
}

// Index of value in a list, or the fallback when it is not there.
template <size_t N>
static int indexOf(const int (&list)[N], int value, int fallback) {
	for (size_t i = 0; i < N; i++) {
		if (list[i] == value)
			return (int) i;
	}
	return fallback;
}

struct BpmQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return string::f("%.1f", getDisplayValue());
	}
};

// "Trigger" at the bottom step, then the gate as a share of the output's period.
struct LengthQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float f = moleta::gateFraction(getValue());
		return f > 0.f ? string::f("%.0f%%", f * 100.f) : "Trigger";
	}

	void setDisplayValueString(std::string s) override {
		if (string::lowercase(s).find("t") == 0) {
			setValue(0.f);
			return;
		}
		float pct = std::atof(s.c_str());
		setValue(clamp((pct / 100.f - 0.05f) / 0.9f, moleta::LENGTH_TRIGGER_TOP, 1.f));
	}
};

struct Moleta : Module {
	enum ParamId { RUN_PARAM, RESET_PARAM, BPM_PARAM, REPEAT_PARAM, ENUMS(RATIO_PARAM, OUTS), ENUMS(CHANCE_PARAM, OUTS), ENUMS(LENGTH_PARAM, OUTS), PARAMS_LEN };
	// New inputs and outputs go at the end: patches store cables by index.
	enum InputId { RUN_INPUT, CLOCK_INPUT, RESET_INPUT, DENSITY_INPUT, INPUTS_LEN };
	enum OutputId { ENUMS(OUT_OUTPUT, OUTS), BEAT_OUTPUT, RESET_OUTPUT, CLICK_OUTPUT, OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	moleta::BeatClock clock;
	moleta::OutputEngine outs[OUTS];
	moleta::Pulse beatPulse;
	moleta::Pulse resetPulse;
	moleta::Click click;
	// Every output, 1 to 4, BEAT, RESET and CLICK, delayed together while Moleta+ is attached.
	moleta::SampleDelay<OUTS + 3> delay;

	// Right-click choices, as indexes into the lists above. follow[0] is unused:
	// output 1 has nothing above it.
	int clockInput = 0;
	int beatsPerBar = 2;
	int coinLoop = 1;
	bool follow[OUTS] = {};

	dsp::SchmittTrigger runTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger clockTrigger;
	dsp::BooleanTrigger resetButton;
	// True at start, so a patch that loads running keeps its saved coin pointers.
	bool wasRunning = true;
	// The tempo the readout shows while following CLOCK, 0 while unknown.
	float measuredBpm = 0.f;

	Moleta() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run", {"Stopped", "Running"});
		getParamQuantity(RUN_PARAM)->randomizeEnabled = false;
		configButton(RESET_PARAM, "Reset");
		configParam<BpmQuantity>(BPM_PARAM, 30.f, 300.f, 120.f, "Tempo", " BPM");
		configParam(REPEAT_PARAM, 0.f, 1.f, 0.f, "Repeat", "%", 0.f, 100.f)->description =
			"Fully left, every pulse flips a fresh coin; fully right, each output's coin loop is locked";
		std::vector<std::string> names;
		for (const moleta::Ratio& r : moleta::RATIOS)
			names.push_back(r.name);
		for (int i = 0; i < OUTS; i++) {
			configSwitch(RATIO_PARAM + i, 0.f, moleta::RATIO_COUNT - 1, moleta::DEFAULT_RATIOS[i], string::f("Ratio %d", i + 1), names);
			configParam(CHANCE_PARAM + i, 0.f, 1.f, 1.f, string::f("Chance %d", i + 1), "%", 0.f, 100.f);
			configParam<LengthQuantity>(LENGTH_PARAM + i, 0.f, 1.f, 0.5f, string::f("Length %d", i + 1))->description =
				"Fully left, a 1 ms trigger; then a gate of 5% to 95% of the output's period";
		}
		configInput(RUN_INPUT, "Run")->description = "A trigger toggles RUN";
		configInput(CLOCK_INPUT, "Clock")->description = "Rising edges; 1, 4 or 24 per beat (right-click menu). Unpatched, BPM sets the tempo";
		configInput(RESET_INPUT, "Reset");
		configInput(DENSITY_INPUT, "Density")->description = "10% per volt, added to every CHANCE";
		for (int i = 0; i < OUTS; i++)
			configOutput(OUT_OUTPUT + i, string::f("Output %d", i + 1));
		configOutput(BEAT_OUTPUT, "Beat")->description = "A trigger on every beat, with no coin";
		configOutput(RESET_OUTPUT, "Reset")->description = "A trigger whenever Moleta resets";
		configOutput(CLICK_OUTPUT, "Click")->description = "Audio: a metronome click, accented on beat 1 of the bar";
		fillCoins();
	}

	void fillCoins() {
		for (moleta::OutputEngine& o : outs)
			o.coin.fill(uniform);
	}

	void onReset() override {
		clock = moleta::BeatClock();
		for (moleta::OutputEngine& o : outs)
			o = moleta::OutputEngine();
		fillCoins();
		clockInput = 0;
		beatsPerBar = 2;
		coinLoop = 1;
		std::fill(follow, follow + OUTS, false);
	}

	// Hands this sample's tick to every Moleta+ chained on the right, up to the cap,
	// each with its place. Returns whether one is attached.
	bool pushToExpander(const moleta::Tick& tick, const moleta::Shared& shared) {
		Model* plus = expanderModel();
		if (!plus)
			return false;
		int n = 0;
		for (Module* at = rightExpander.module; n < MOLETA_PLUS_MAX && at && at->model == plus; at = at->rightExpander.module) {
			MoletaMessage* message = (MoletaMessage*) at->leftExpander.producerMessage;
			if (!message)
				break;
			message->linked = true;
			message->place = n;
			message->tick = tick;
			message->shared = shared;
			at->leftExpander.requestMessageFlip();
			n++;
		}
		return n > 0;
	}

	void process(const ProcessArgs& args) override {
		if (runTrigger.process(inputs[RUN_INPUT].getVoltage(), 0.1f, 1.f))
			params[RUN_PARAM].setValue(params[RUN_PARAM].getValue() > 0.5f ? 0.f : 1.f);
		bool running = params[RUN_PARAM].getValue() > 0.5f;
		bool resetPressed = resetButton.process(params[RESET_PARAM].getValue() > 0.5f);
		bool resetIn = resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		bool reset = resetPressed || resetIn;

		moleta::ClockIn in;
		in.running = running;
		in.restart = reset || (running && !wasRunning);
		wasRunning = running;
		in.external = inputs[CLOCK_INPUT].isConnected();
		in.edge = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		in.bpm = params[BPM_PARAM].getValue();
		in.pulsesPerBeat = PULSES_PER_BEAT[clockInput];
		moleta::Tick tick = clock.process(args.sampleTime, in);
		measuredBpm = clock.measured > 0.0 ? (float) (60.0 / clock.measured) : 0.f;

		moleta::Shared shared;
		shared.repeat = params[REPEAT_PARAM].getValue();
		shared.density = inputs[DENSITY_INPUT].getVoltage();
		shared.loopLength = COIN_LOOPS[coinLoop];
		bool attached = pushToExpander(tick, shared);

		moleta::Controls controls[OUTS];
		for (int i = 0; i < OUTS; i++) {
			controls[i].ratio = (int) std::round(params[RATIO_PARAM + i].getValue());
			controls[i].chance = params[CHANCE_PARAM + i].getValue();
			controls[i].length = params[LENGTH_PARAM + i].getValue();
			controls[i].follow = follow[i];
		}
		float v[OUTS + 3];
		moleta::processChain(outs, controls, OUTS, args.sampleTime, tick, shared, uniform, v);

		bool clicking = outputs[CLICK_OUTPUT].isConnected();
		if (tick.live && tick.beat) {
			beatPulse.trigger(moleta::TRIGGER);
			if (clicking)
				click.trigger(tick.beatIndex % BEATS_PER_BAR[beatsPerBar] == 0);
		}
		if (!tick.live)
			beatPulse.cut();
		if (reset)
			resetPulse.trigger(moleta::TRIGGER);
		v[OUTS] = beatPulse.process(args.sampleTime) ? 10.f : 0.f;
		v[OUTS + 1] = resetPulse.process(args.sampleTime) ? 10.f : 0.f;
		v[OUTS + 2] = clicking ? click.process(args.sampleTime) : 0.f;

		delay.process(v, attached);
		for (int i = 0; i < OUTS; i++)
			outputs[OUT_OUTPUT + i].setVoltage(v[i]);
		outputs[BEAT_OUTPUT].setVoltage(v[OUTS]);
		outputs[RESET_OUTPUT].setVoltage(v[OUTS + 1]);
		outputs[CLICK_OUTPUT].setVoltage(v[OUTS + 2]);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// The ratio itself, not the knob's step, so the list can grow.
		json_t* ratiosJ = json_array();
		for (int i = 0; i < OUTS; i++) {
			const moleta::Ratio& r = moleta::RATIOS[moleta::clampRatio((int) std::round(params[RATIO_PARAM + i].getValue()))];
			json_t* rJ = json_object();
			json_object_set_new(rJ, "p", json_integer(r.p));
			json_object_set_new(rJ, "q", json_integer(r.q));
			json_array_append_new(ratiosJ, rJ);
		}
		json_object_set_new(rootJ, "ratios", ratiosJ);
		json_t* coinsJ = json_array();
		for (const moleta::OutputEngine& o : outs) {
			json_t* cJ = json_object();
			json_t* slotsJ = json_array();
			for (float s : o.coin.slots)
				json_array_append_new(slotsJ, json_real(s));
			json_object_set_new(cJ, "slots", slotsJ);
			json_object_set_new(cJ, "pointer", json_integer(o.coin.pointer));
			json_array_append_new(coinsJ, cJ);
		}
		json_object_set_new(rootJ, "coins", coinsJ);
		json_object_set_new(rootJ, "clockInput", json_integer(PULSES_PER_BEAT[clockInput]));
		json_object_set_new(rootJ, "beatsPerBar", json_integer(BEATS_PER_BAR[beatsPerBar]));
		json_object_set_new(rootJ, "coinLoop", json_integer(COIN_LOOPS[coinLoop]));
		json_t* followJ = json_array();
		for (int i = 1; i < OUTS; i++)
			json_array_append_new(followJ, json_boolean(follow[i]));
		json_object_set_new(rootJ, "follow", followJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Rack loads the params first, so a saved ratio wins over the saved step.
		json_t* ratiosJ = json_object_get(rootJ, "ratios");
		for (int i = 0; i < OUTS; i++) {
			json_t* rJ = json_array_get(ratiosJ, i);
			int p = (int) json_integer_value(json_object_get(rJ, "p"));
			int q = (int) json_integer_value(json_object_get(rJ, "q"));
			if (p > 0 && q > 0)
				params[RATIO_PARAM + i].setValue(moleta::nearestRatio(p, q));
		}
		json_t* coinsJ = json_object_get(rootJ, "coins");
		for (int i = 0; i < OUTS; i++) {
			json_t* cJ = json_array_get(coinsJ, i);
			json_t* slotsJ = json_object_get(cJ, "slots");
			moleta::Coin& coin = outs[i].coin;
			for (int k = 0; k < moleta::Coin::MAX_LOOP; k++) {
				if (json_t* sJ = json_array_get(slotsJ, k))
					coin.slots[k] = clamp((float) json_number_value(sJ), 0.f, 0.9999999f);
			}
			if (json_t* pJ = json_object_get(cJ, "pointer"))
				coin.pointer = clamp((int) json_integer_value(pJ), 0, moleta::Coin::MAX_LOOP - 1);
		}
		clockInput = indexOf(PULSES_PER_BEAT, (int) json_integer_value(json_object_get(rootJ, "clockInput")), 0);
		beatsPerBar = indexOf(BEATS_PER_BAR, (int) json_integer_value(json_object_get(rootJ, "beatsPerBar")), 2);
		coinLoop = indexOf(COIN_LOOPS, (int) json_integer_value(json_object_get(rootJ, "coinLoop")), 1);
		json_t* followJ = json_object_get(rootJ, "follow");
		for (int i = 1; i < OUTS; i++)
			follow[i] = json_is_true(json_array_get(followJ, i - 1));
	}
};

// BPM, then each output's ratio. The cell's name stays BPM while following CLOCK.
struct MoletaReadout : w72::Readout {
	Moleta* module;

	MoletaReadout(Moleta* module)
		: Readout(MoletaPanel::READOUT_WELL, MoletaPanel::READOUT, 1 + OUTS, MoletaPanel::READOUT_VALUE_SIZE), module(module) {}

	std::string text(int i) override {
		if (!module)
			return MoletaPanel::READOUT_SAMPLE[i];
		if (i == 0) {
			if (!module->inputs[Moleta::CLOCK_INPUT].isConnected())
				return string::f("%.1f", module->params[Moleta::BPM_PARAM].getValue());
			return module->measuredBpm > 0.f ? string::f("%.1f", module->measuredBpm) : "---";
		}
		int r = moleta::clampRatio((int) std::round(module->params[Moleta::RATIO_PARAM + i - 1].getValue()));
		return moleta::RATIOS[r].name;
	}
};

struct MoletaWidget : ModuleWidget {
	MoletaWidget(Moleta* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Moleta.svg")));

		for (Vec pos : MoletaPanel::SCREWS)
			addChild(createWidget<ScrewBlack>(pos));

		addChild(new MoletaReadout(module));

		using MoletaPanel::BUTTON_SIZE;
		using MoletaPanel::FLAT_LIGHT_SIZE;
		w72::FlatButton* run = w72::createFlatButton(MoletaPanel::RUN, BUTTON_SIZE, FLAT_LIGHT_SIZE, module, Moleta::RUN_PARAM);
		run->sample = true;
		addParam(run);
		w72::FlatButton* reset = w72::createFlatButton(MoletaPanel::RESET, BUTTON_SIZE, FLAT_LIGHT_SIZE, module, Moleta::RESET_PARAM);
		reset->momentary = true;
		addParam(reset);
		addParam(createParamCentered<RoundLargeBlackKnob>(MoletaPanel::BPM, module, Moleta::BPM_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(MoletaPanel::REPEAT, module, Moleta::REPEAT_PARAM));

		const Vec ratio[OUTS] = {MoletaPanel::RATIO_1, MoletaPanel::RATIO_2, MoletaPanel::RATIO_3, MoletaPanel::RATIO_4};
		const Vec chance[OUTS] = {MoletaPanel::CHANCE_1, MoletaPanel::CHANCE_2, MoletaPanel::CHANCE_3, MoletaPanel::CHANCE_4};
		const Vec length[OUTS] = {MoletaPanel::LENGTH_1, MoletaPanel::LENGTH_2, MoletaPanel::LENGTH_3, MoletaPanel::LENGTH_4};
		const Vec out[OUTS] = {MoletaPanel::OUT_1, MoletaPanel::OUT_2, MoletaPanel::OUT_3, MoletaPanel::OUT_4};
		for (int i = 0; i < OUTS; i++) {
			addParam(createParamCentered<RoundBlackKnob>(ratio[i], module, Moleta::RATIO_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(chance[i], module, Moleta::CHANCE_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(length[i], module, Moleta::LENGTH_PARAM + i));
		}

		addInput(createInputCentered<DarkPJ301MPort>(MoletaPanel::RUN_IN, module, Moleta::RUN_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(MoletaPanel::CLOCK, module, Moleta::CLOCK_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(MoletaPanel::RESET_IN, module, Moleta::RESET_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(MoletaPanel::DENSITY, module, Moleta::DENSITY_INPUT));

		for (int i = 0; i < OUTS; i++)
			addOutput(createOutputCentered<DarkPJ301MPort>(out[i], module, Moleta::OUT_OUTPUT + i));
		addOutput(createOutputCentered<DarkPJ301MPort>(MoletaPanel::BEAT, module, Moleta::BEAT_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(MoletaPanel::RESET_OUT, module, Moleta::RESET_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(MoletaPanel::CLICK, module, Moleta::CLICK_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Moleta* m = getModule<Moleta>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Clock input", {"1 pulse per beat", "4 pulses per beat", "24 pulses per beat"}, &m->clockInput));
		menu->addChild(createIndexPtrSubmenuItem("Beats per bar", {"2", "3", "4", "5", "6", "7", "8", "12"}, &m->beatsPerBar));
		menu->addChild(createIndexPtrSubmenuItem("Coin loop", {"8 flips", "16 flips"}, &m->coinLoop));
		menu->addChild(createSubmenuItem("Follow the output above", "", [=](Menu* sub) {
			for (int i = 1; i < OUTS; i++)
				sub->addChild(createBoolPtrMenuItem(string::f("Output %d follows %d", i + 1, i), "", &m->follow[i]));
		}));
	}
};

Model* modelMoleta = createModel<Moleta, MoletaWidget>("Moleta");
