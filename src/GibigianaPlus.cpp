// Gibigiana +: one more lane for the Gibigiana on its left, and nothing else. It
// sends its knobs and CVs leftward and plays back the tap Gibigiana sends it
// (GibigianaExpander.hpp); all of the sound is made in Gibigiana. On its own, with
// no Gibigiana at the head of its chain, it is silent and its readout dim.
#include "plugin.hpp"
#include "GibigianaPlusPanel.hpp"
#include "GibigianaEngine.hpp"
#include "GibigianaExpander.hpp"
#include "widgets.hpp"

struct GibigianaPlus : Module {
	enum ParamId { STEP_PARAM, CHANCE_PARAM, LEVEL_PARAM, PAN_PARAM, PARAMS_LEN };
	// New inputs and outputs go at the end: patches store cables by index.
	enum InputId { CHANCE_INPUT, LEVEL_INPUT, PAN_INPUT, INPUTS_LEN };
	enum OutputId { TAP_OUTPUT, OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	// What the chain said last sample: whether a Gibigiana heads it, this lane's place
	// (0 is Gibigiana's own), and whether its step fits the buffer. Read by the readout.
	bool linked = false;
	int place = 0;
	bool reach = true;
	// A module made from the browser takes its place's default step once it learns
	// its place; one loaded from a patch or a preset keeps what it has.
	bool fresh = true;

	GibigianaPlus() {
		static_assert(STEP_PARAM == GIBIGIANA_PLUS_STEP_PARAM, "Gibigiana sets a new lane's STEP by this index");
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> names;
		for (const gibigiana::Step& s : gibigiana::STEPS)
			names.push_back(s.name);
		configSwitch(STEP_PARAM, 0.f, gibigiana::STEP_COUNT - 1, gibigiana::DEFAULT_STEPS[1], "Step", names)->description =
			"Clock steps behind the input";
		configParam(CHANCE_PARAM, 0.f, 1.f, 1.f, "Chance", "%", 0.f, 100.f)->description =
			"The chance that a hit comes back at this tap";
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.5f, "Level", " dB", -10.f, 20.f);
		configParam(PAN_PARAM, -1.f, 1.f, 0.f, "Pan", "%", 0.f, 100.f);
		configInput(CHANCE_INPUT, "Chance CV")->description = "10% per volt, added to CHANCE";
		configInput(LEVEL_INPUT, "Level CV")->description = "10% per volt, added to LEVEL";
		configInput(PAN_INPUT, "Pan CV")->description = "±5 V sweeps from side to side, added to PAN";
		configOutput(TAP_OUTPUT, "Tap")->description = "Patched, the tap leaves Gibigiana's sum (its right-click menu). Silent unless a Gibigiana heads the chain";
		// Taps come from the left, lanes from the right; the neighbours write the
		// producers and Rack swaps each pair at the end of the sample.
		leftExpander.producerMessage = new GibigianaTaps;
		leftExpander.consumerMessage = new GibigianaTaps;
		rightExpander.producerMessage = new GibigianaLanes;
		rightExpander.consumerMessage = new GibigianaLanes;
	}

	~GibigianaPlus() {
		delete (GibigianaTaps*) leftExpander.producerMessage;
		delete (GibigianaTaps*) leftExpander.consumerMessage;
		delete (GibigianaLanes*) rightExpander.producerMessage;
		delete (GibigianaLanes*) rightExpander.consumerMessage;
	}

	static bool inChain(Module* m) {
		return m && (m->model == modelGibigiana || m->model == modelGibigianaPlus);
	}

	void process(const ProcessArgs& args) override {
		GibigianaLane mine;
		mine.step = (int) std::round(params[STEP_PARAM].getValue());
		mine.chance = params[CHANCE_PARAM].getValue();
		mine.level = params[LEVEL_PARAM].getValue();
		mine.pan = params[PAN_PARAM].getValue();
		mine.chanceCv = inputs[CHANCE_INPUT].getVoltage();
		mine.levelCv = inputs[LEVEL_INPUT].getVoltage();
		mine.panCv = inputs[PAN_INPUT].getVoltage();
		mine.patched = outputs[TAP_OUTPUT].isConnected();

		// Leftward: this lane, then the lanes on the right.
		Module* left = leftExpander.module;
		Module* right = rightExpander.module;
		bool rightLane = right && right->model == modelGibigianaPlus;
		GibigianaLanes* fromRight = (GibigianaLanes*) rightExpander.consumerMessage;
		if (!rightLane)
			fromRight->count = 0;
		if (inChain(left) && left->rightExpander.producerMessage) {
			GibigianaLanes* toLeft = (GibigianaLanes*) left->rightExpander.producerMessage;
			int n = clamp(fromRight->count, 0, GIBIGIANA_PLUS_MAX - 1);
			toLeft->count = 1 + n;
			toLeft->lane[0] = mine;
			for (int j = 0; j < n; j++)
				toLeft->lane[j + 1] = fromRight->lane[j];
			left->rightExpander.requestMessageFlip();
		}

		// Rightward: this lane's tap from the message, the rest passed on.
		GibigianaTaps* fromLeft = (GibigianaTaps*) leftExpander.consumerMessage;
		if (!inChain(left)) {
			// Stale once the chain is gone: placed back, it reads as unlinked until written.
			*fromLeft = GibigianaTaps();
		}
		linked = inChain(left) && fromLeft->linked && fromLeft->place >= 1 && fromLeft->place <= GIBIGIANA_PLUS_MAX;
		place = linked ? fromLeft->place : 0;
		reach = linked ? fromLeft->reach[0] : true;
		outputs[TAP_OUTPUT].setVoltage(linked ? fromLeft->tap[0] : 0.f);
		if (fresh && linked) {
			// Unless the knob has been turned already.
			if (std::round(params[STEP_PARAM].getValue()) == gibigiana::DEFAULT_STEPS[1])
				params[STEP_PARAM].setValue(gibigiana::DEFAULT_STEPS[place]);
			fresh = false;
		}
		if (rightLane && right->leftExpander.producerMessage) {
			GibigianaTaps* toRight = (GibigianaTaps*) right->leftExpander.producerMessage;
			toRight->linked = linked;
			toRight->place = place + 1;
			for (int j = 0; j < GIBIGIANA_PLUS_MAX - 1; j++) {
				toRight->tap[j] = fromLeft->tap[j + 1];
				toRight->reach[j] = fromLeft->reach[j + 1];
			}
			toRight->tap[GIBIGIANA_PLUS_MAX - 1] = 0.f;
			toRight->reach[GIBIGIANA_PLUS_MAX - 1] = true;
			right->leftExpander.requestMessageFlip();
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// The step itself, not the knob's position, so the list can grow.
		const gibigiana::Step& s = gibigiana::STEPS[gibigiana::clampStep((int) std::round(params[STEP_PARAM].getValue()))];
		json_t* stepJ = json_object();
		json_object_set_new(stepJ, "p", json_integer(s.p));
		json_object_set_new(stepJ, "q", json_integer(s.q));
		json_object_set_new(rootJ, "step", stepJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		fresh = false;
		json_t* stepJ = json_object_get(rootJ, "step");
		int p = (int) json_integer_value(json_object_get(stepJ, "p"));
		int q = (int) json_integer_value(json_object_get(stepJ, "q"));
		if (p > 0 && q > 0)
			params[STEP_PARAM].setValue(gibigiana::nearestStep(p, q));
	}
};

// The lane's step, over its number in the chain; dim with no Gibigiana at the head
// of the chain, or when the step reaches past the buffer.
struct GibigianaPlusReadout : w72::Readout {
	GibigianaPlus* module;

	GibigianaPlusReadout(GibigianaPlus* module)
		: Readout(GibigianaPlusPanel::READOUT_WELL, GibigianaPlusPanel::READOUT, 1, GibigianaPlusPanel::READOUT_VALUE_SIZE), module(module) {
		nameBaseline = GibigianaPlusPanel::READOUT_NAME_BASELINE - GibigianaPlusPanel::READOUT_WELL.pos.y;
	}

	std::string text(int i) override {
		if (!module)
			return GibigianaPlusPanel::READOUT_SAMPLE[i];
		int s = gibigiana::clampStep((int) std::round(module->params[GibigianaPlus::STEP_PARAM].getValue()));
		return gibigiana::STEPS[s].shown;
	}

	std::string name(int i) override {
		if (!module)
			return "2";
		return module->linked ? string::f("%d", module->place + 1) : "";
	}

	bool dim(int i) override {
		return module && (!module->linked || !module->reach);
	}
};

struct GibigianaPlusWidget : ModuleWidget {
	GibigianaPlusWidget(GibigianaPlus* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GibigianaPlus.svg")));

		for (Vec pos : GibigianaPlusPanel::SCREWS)
			addChild(createWidget<ScrewBlack>(pos));

		addChild(new GibigianaPlusReadout(module));

		addParam(createParamCentered<RoundBlackKnob>(GibigianaPlusPanel::STEP, module, GibigianaPlus::STEP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(GibigianaPlusPanel::CHANCE, module, GibigianaPlus::CHANCE_PARAM));
		addParam(createParamCentered<Trimpot>(GibigianaPlusPanel::LEVEL, module, GibigianaPlus::LEVEL_PARAM));
		addParam(createParamCentered<Trimpot>(GibigianaPlusPanel::PAN, module, GibigianaPlus::PAN_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPlusPanel::CHANCE_CV, module, GibigianaPlus::CHANCE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPlusPanel::LEVEL_CV, module, GibigianaPlus::LEVEL_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GibigianaPlusPanel::PAN_CV, module, GibigianaPlus::PAN_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(GibigianaPlusPanel::OUT, module, GibigianaPlus::TAP_OUTPUT));
	}
};

Model* modelGibigianaPlus = createModel<GibigianaPlus, GibigianaPlusWidget>("GibigianaPlus");
