// Gibigiana: a clocked echo lane. Each lane reads one mono voice back on the clock
// grid with its own chance; the coins roll once per hit and ride with the audio, so
// a note and its tail share one fate. Wet only, no feedback. Gibigiana is the
// system and lane 1; each Gibigiana + on its right adds a lane, up to 8 in all, and
// every lane's sound is made here (GibigianaEngine.hpp, GibigianaExpander.hpp).
#include "plugin.hpp"
#include "GibigianaPanel.hpp"
#include "GibigianaEngine.hpp"
#include "GibigianaExpander.hpp"
#include "widgets.hpp"

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

// The Gibigiana + modules chained on a module's right, nearest first, up to the cap.
static int chainedLanes(Module* m, Module** last = NULL) {
	int n = 0;
	Module* at = m;
	while (n < GIBIGIANA_PLUS_MAX && at->rightExpander.module && at->rightExpander.module->model == modelGibigianaPlus) {
		at = at->rightExpander.module;
		n++;
	}
	if (last)
		*last = at;
	return n;
}

struct Gibigiana : Module {
	enum ParamId { STEP_PARAM, CHANCE_PARAM, LEVEL_PARAM, PAN_PARAM, CENTRE_PARAM, WIDTH_PARAM, REPEAT_PARAM, DUCK_PARAM, ADD_LANE_PARAM, PARAMS_LEN };
	// New inputs and outputs go at the end: patches store cables by index.
	enum InputId { IN_INPUT, CLOCK_INPUT, GATE_INPUT, RESET_INPUT, DENSITY_INPUT, DUCK_INPUT, CHANCE_INPUT, LEVEL_INPUT, PAN_INPUT, INPUTS_LEN };
	enum OutputId { LEFT_OUTPUT, RIGHT_OUTPUT, TAP_OUTPUT, OUTPUTS_LEN };
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
		configSwitch(STEP_PARAM, 0.f, gibigiana::STEP_COUNT - 1, gibigiana::DEFAULT_STEPS[0], "Step", names)->description =
			"Clock steps behind the input";
		configParam(CHANCE_PARAM, 0.f, 1.f, 1.f, "Chance", "%", 0.f, 100.f)->description =
			"The chance that a hit comes back at this tap";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.5f, "Level", " dB", -10.f, 20.f);
		configParam(PAN_PARAM, -1.f, 1.f, 0.f, "Pan", "%", 0.f, 100.f);
		configParam(CENTRE_PARAM, 0.f, 1.f, 0.5f, "Band centre", " Hz", gibigiana::BAND_RANGE, gibigiana::BAND_LOW);
		configParam<WidthQuantity>(WIDTH_PARAM, 0.f, 1.f, 1.f, "Band width")->description = "Fully right, the band is open";
		configParam(REPEAT_PARAM, 0.f, 1.f, 0.f, "Repeat", "%", 0.f, 100.f)->description =
			"Fully left, every hit flips a fresh coin; fully right, each lane's coin loop is locked";
		configParam(DUCK_PARAM, 0.f, 1.f, 0.8f, "Duck amount", "%", 0.f, 100.f);
		configButton(ADD_LANE_PARAM, "Add lane")->description = "Places a Gibigiana + after the last lane, up to 8 lanes";
		configInput(IN_INPUT, "Audio");
		configInput(CLOCK_INPUT, "Clock")->description = "One step per pulse; the taps are silent until two pulses have set the step";
		configInput(GATE_INPUT, "Gate")->description = "Each rising edge is a hit and rolls the coins. Unpatched, every clock pulse is a hit";
		configInput(RESET_INPUT, "Reset")->description = "Restarts the coin loops";
		configInput(DENSITY_INPUT, "Density")->description = "10% per volt, added to every lane's CHANCE";
		configInput(DUCK_INPUT, "Duck")->description = "Pulls the reflections down by up to the DUCK amount";
		configInput(CHANCE_INPUT, "Chance CV")->description = "10% per volt, added to CHANCE";
		configInput(LEVEL_INPUT, "Level CV")->description = "10% per volt, added to LEVEL";
		configInput(PAN_INPUT, "Pan CV")->description = "±5 V sweeps from side to side, added to PAN";
		configOutput(LEFT_OUTPUT, "Left")->description = "Every lane in the chain";
		configOutput(RIGHT_OUTPUT, "Right")->description = "Every lane in the chain";
		configOutput(TAP_OUTPUT, "Tap")->description = "Patched, the tap leaves the sum (right-click menu)";
		// Each Gibigiana + on the right writes its lanes here; Rack swaps the two at the end of each sample.
		rightExpander.producerMessage = new GibigianaLanes;
		rightExpander.consumerMessage = new GibigianaLanes;
		engine.fillCoins(uniform);
	}

	~Gibigiana() {
		delete (GibigianaLanes*) rightExpander.producerMessage;
		delete (GibigianaLanes*) rightExpander.consumerMessage;
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

	// The lanes the Gibigiana + chain sent: none unless one is on the right.
	int readLanes() {
		GibigianaLanes* message = (GibigianaLanes*) rightExpander.consumerMessage;
		Module* right = rightExpander.module;
		if (!right || right->model != modelGibigianaPlus) {
			// Stale once the chain is gone: a Gibigiana + placed back reads as empty until it writes.
			message->count = 0;
			return 0;
		}
		int n = clamp(message->count, 0, GIBIGIANA_PLUS_MAX);
		for (int j = 0; j < n; j++) {
			const GibigianaLane& l = message->lane[j];
			int i = j + 1;
			knobs.step[i] = l.step;
			knobs.chance[i] = l.chance;
			knobs.level[i] = l.level;
			knobs.pan[i] = l.pan;
			knobs.chanceCv[i] = l.chanceCv;
			knobs.levelCv[i] = l.levelCv;
			knobs.panCv[i] = l.panCv;
			knobs.patched[i] = l.patched;
			// One sample per hop: lane i hears its tap i samples late.
			knobs.lag[i] = i;
		}
		return n;
	}

	// Sends the expanders their taps, nearest first.
	void pushTaps(const gibigiana::Out& o) {
		Module* right = rightExpander.module;
		if (!right || right->model != modelGibigianaPlus || !right->leftExpander.producerMessage)
			return;
		GibigianaTaps* message = (GibigianaTaps*) right->leftExpander.producerMessage;
		message->linked = true;
		message->place = 1;
		for (int j = 0; j < GIBIGIANA_PLUS_MAX; j++) {
			message->tap[j] = o.tap[j + 1];
			message->reach[j] = engine.taps[j + 1].reach;
		}
		right->leftExpander.requestMessageFlip();
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

		knobs.step[0] = (int) std::round(params[STEP_PARAM].getValue());
		knobs.chance[0] = params[CHANCE_PARAM].getValue();
		knobs.level[0] = params[LEVEL_PARAM].getValue();
		knobs.pan[0] = params[PAN_PARAM].getValue();
		knobs.chanceCv[0] = inputs[CHANCE_INPUT].getVoltage();
		knobs.levelCv[0] = inputs[LEVEL_INPUT].getVoltage();
		knobs.panCv[0] = inputs[PAN_INPUT].getVoltage();
		knobs.patched[0] = outputs[TAP_OUTPUT].isConnected();
		knobs.lag[0] = 0;
		knobs.lanes = 1 + readLanes();
		knobs.centre = params[CENTRE_PARAM].getValue();
		knobs.width = params[WIDTH_PARAM].getValue();
		knobs.repeat = params[REPEAT_PARAM].getValue();
		knobs.duck = params[DUCK_PARAM].getValue();
		knobs.loopLength = COIN_LOOPS[coinLoop];
		knobs.keepPatched = keepPatched;

		gibigiana::Out o = engine.process(f, knobs, uniform);
		outputs[LEFT_OUTPUT].setVoltage(o.left);
		outputs[RIGHT_OUTPUT].setVoltage(o.right);
		outputs[TAP_OUTPUT].setVoltage(o.tap[0]);
		pushTaps(o);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// The step itself, not the knob's position, so the list can grow.
		const gibigiana::Step& s = gibigiana::STEPS[gibigiana::clampStep((int) std::round(params[STEP_PARAM].getValue()))];
		json_t* stepJ = json_object();
		json_object_set_new(stepJ, "p", json_integer(s.p));
		json_object_set_new(stepJ, "q", json_integer(s.q));
		json_object_set_new(rootJ, "step", stepJ);
		// The coin loops belong to the places in the chain, so Gibigiana keeps all eight.
		json_t* coinsJ = json_array();
		for (const moleta::Coin& c : engine.coins) {
			json_t* cJ = json_object();
			json_t* slotsJ = json_array();
			for (float v : c.slots)
				json_array_append_new(slotsJ, json_real(v));
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
		json_t* stepJ = json_object_get(rootJ, "step");
		int p = (int) json_integer_value(json_object_get(stepJ, "p"));
		int q = (int) json_integer_value(json_object_get(stepJ, "q"));
		if (p > 0 && q > 0)
			params[STEP_PARAM].setValue(gibigiana::nearestStep(p, q));
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

// Lane 1's step, over its number; dim when the step reaches past the buffer.
struct GibigianaReadout : w72::Readout {
	Gibigiana* module;

	GibigianaReadout(Gibigiana* module)
		: Readout(GibigianaPanel::READOUT_WELL, GibigianaPanel::READOUT, 1, GibigianaPanel::READOUT_VALUE_SIZE), module(module) {
		nameBaseline = GibigianaPanel::READOUT_NAME_BASELINE - GibigianaPanel::READOUT_WELL.pos.y;
	}

	std::string text(int i) override {
		if (!module)
			return GibigianaPanel::READOUT_SAMPLE[i];
		int s = gibigiana::clampStep((int) std::round(module->params[Gibigiana::STEP_PARAM].getValue()));
		return gibigiana::STEPS[s].shown;
	}

	std::string name(int i) override {
		return "1";
	}

	bool dim(int i) override {
		return module && !module->engine.taps[0].reach;
	}
};

// ADD LANE: momentary, lit while held.
struct AddLaneButton : w72::FlatButton {
	AddLaneButton() {
		momentary = true;
	}
};

struct GibigianaWidget : ModuleWidget {
	bool addHeld = false;

	GibigianaWidget(Gibigiana* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gibigiana.svg")));

		for (Vec pos : GibigianaPanel::SCREWS)
			addChild(createWidget<ScrewBlack>(pos));

		addChild(new GibigianaReadout(module));
		addParam(w72::createFlatButton<AddLaneButton>(GibigianaPanel::ADD_LANE, GibigianaPanel::BUTTON_SIZE, GibigianaPanel::FLAT_LIGHT_SIZE, module, Gibigiana::ADD_LANE_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(GibigianaPanel::REPEAT, module, Gibigiana::REPEAT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(GibigianaPanel::DUCK_AMOUNT, module, Gibigiana::DUCK_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(GibigianaPanel::CENTRE, module, Gibigiana::CENTRE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(GibigianaPanel::WIDTH, module, Gibigiana::WIDTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(GibigianaPanel::STEP, module, Gibigiana::STEP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(GibigianaPanel::CHANCE, module, Gibigiana::CHANCE_PARAM));
		addParam(createParamCentered<Trimpot>(GibigianaPanel::LEVEL, module, Gibigiana::LEVEL_PARAM));
		addParam(createParamCentered<Trimpot>(GibigianaPanel::PAN, module, Gibigiana::PAN_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::IN, module, Gibigiana::IN_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::CLOCK, module, Gibigiana::CLOCK_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::GATE, module, Gibigiana::GATE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::RESET, module, Gibigiana::RESET_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::DENSITY, module, Gibigiana::DENSITY_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::DUCK, module, Gibigiana::DUCK_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::CHANCE_CV, module, Gibigiana::CHANCE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::LEVEL_CV, module, Gibigiana::LEVEL_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPanel::PAN_CV, module, Gibigiana::PAN_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(GibigianaPanel::OUT_L, module, Gibigiana::LEFT_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(GibigianaPanel::OUT_R, module, Gibigiana::RIGHT_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(GibigianaPanel::OUT, module, Gibigiana::TAP_OUTPUT));
	}

	// The scene changes only on the UI thread, so the button is read here, not in process().
	void step() override {
		if (module) {
			bool held = module->params[Gibigiana::ADD_LANE_PARAM].getValue() > 0.f;
			if (held && !addHeld)
				addLane();
			addHeld = held;
		}
		ModuleWidget::step();
	}

	// Places a Gibigiana + after the last lane and pushes the modules there aside, in
	// one undo action. At the cap it does nothing.
	void addLane() {
		Module* last = NULL;
		int n = chainedLanes(module, &last);
		if (n >= GIBIGIANA_PLUS_MAX)
			return;
		RackWidget* rack = APP->scene->rack;
		ModuleWidget* lastWidget = rack->getModule(last->id);
		if (!lastWidget)
			return;
		rack->updateModuleOldPositions();

		Module* m = modelGibigianaPlus->createModule();
		APP->engine->addModule(m);
		ModuleWidget* mw = modelGibigianaPlus->createModuleWidget(m);
		rack->addModule(mw);
		rack->setModulePosForce(mw, Vec(lastWidget->box.getRight(), lastWidget->box.pos.y));
		mw->loadTemplate();
		// A new lane's step follows the first defaults by its place: 3, 4, then 8.
		m->params[GIBIGIANA_PLUS_STEP_PARAM].setValue(gibigiana::DEFAULT_STEPS[n + 1]);

		// Undo removes the lane first, then moves the neighbours back.
		history::ComplexAction* h = new history::ComplexAction;
		h->name = "add Gibigiana lane";
		h->push(rack->getModuleDragAction());
		history::ModuleAdd* add = new history::ModuleAdd;
		add->setModule(mw);
		h->push(add);
		APP->history->push(h);
	}

	void appendContextMenu(Menu* menu) override {
		Gibigiana* m = getModule<Gibigiana>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		bool full = chainedLanes(m) >= GIBIGIANA_PLUS_MAX;
		menu->addChild(createMenuItem("Add lane", full ? "8 lanes" : "", [=]() { addLane(); }, full));
		menu->addChild(createIndexPtrSubmenuItem("Coin loop", {"8 hits", "16 hits"}, &m->coinLoop));
		menu->addChild(createBoolPtrMenuItem("Patched tap stays in the sum", "", &m->keepPatched));
	}
};

Model* modelGibigiana = createModel<Gibigiana, GibigianaWidget>("Gibigiana");
