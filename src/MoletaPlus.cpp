// Moleta+: four more outputs for the Moleta on its left, each a ratio, a chance and
// a length: 5 to 8 next to Moleta, 9 to 12 on a second one, up to four in a chain.
// It has no clock of its own: every sample Moleta sends it the beat clock's tick,
// the shared controls and its place (MoletaExpander.hpp), and it runs the same
// output engine on them, with its own coin loops. The message arrives one sample
// late, and Moleta delays its own outputs to match while one is attached.
#include "plugin.hpp"
#include "MoletaPlusPanel.hpp"
#include "MoletaEngine.hpp"
#include "MoletaExpander.hpp"
#include "widgets.hpp"

namespace {

const int OUTS = MOLETA_OUTS;

float uniform() {
	return random::uniform();
}

// As on Moleta: "Trigger" at the bottom step, then the gate as a share of the output's period.
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

} // namespace

struct MoletaPlus : Module {
	enum ParamId { ENUMS(RATIO_PARAM, OUTS), ENUMS(CHANCE_PARAM, OUTS), ENUMS(LENGTH_PARAM, OUTS), PARAMS_LEN };
	// New inputs and outputs go at the end: patches store cables by index.
	enum InputId { INPUTS_LEN };
	enum OutputId { ENUMS(OUT_OUTPUT, OUTS), OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	moleta::OutputEngine outs[OUTS];
	// follow[0] is unused: each Moleta+'s first output starts a fresh chain.
	bool follow[OUTS] = {};
	// A Moleta wrote to this one last sample, and where in the chain it sits.
	bool attached = false;
	int place = 0;

	MoletaPlus() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> names;
		for (const moleta::Ratio& r : moleta::RATIOS)
			names.push_back(r.name);
		for (int i = 0; i < OUTS; i++) {
			configSwitch(RATIO_PARAM + i, 0.f, moleta::RATIO_COUNT - 1, moleta::PLUS_DEFAULT_RATIOS[i], "", names);
			configParam(CHANCE_PARAM + i, 0.f, 1.f, 1.f, "", "%", 0.f, 100.f);
			configParam<LengthQuantity>(LENGTH_PARAM + i, 0.f, 1.f, 0.5f, "")->description =
				"Fully left, a 1 ms trigger; then a gate of 5% to 95% of the output's period";
		}
		for (int i = 0; i < OUTS; i++)
			configOutput(OUT_OUTPUT + i, "")->description = "Silent unless a Moleta, or a chain of Moleta + from one, is on the left";
		name(0);
		// Moleta writes the producer buffer; Rack swaps the two at the end of each sample.
		leftExpander.producerMessage = new MoletaMessage;
		leftExpander.consumerMessage = new MoletaMessage;
		fillCoins();
	}

	~MoletaPlus() {
		delete (MoletaMessage*) leftExpander.producerMessage;
		delete (MoletaMessage*) leftExpander.consumerMessage;
	}

	// Names the knobs and outputs by their numbers at a place. Called from the UI
	// thread (the widget's step), where Rack reads the names.
	void name(int at) {
		for (int i = 0; i < OUTS; i++) {
			int k = moletaPlusOutput(at, i);
			paramQuantities[RATIO_PARAM + i]->name = string::f("Ratio %d", k);
			paramQuantities[CHANCE_PARAM + i]->name = string::f("Chance %d", k);
			paramQuantities[LENGTH_PARAM + i]->name = string::f("Length %d", k);
			outputInfos[OUT_OUTPUT + i]->name = string::f("Output %d", k);
		}
	}

	void fillCoins() {
		for (moleta::OutputEngine& o : outs)
			o.coin.fill(uniform);
	}

	void onReset() override {
		for (moleta::OutputEngine& o : outs)
			o = moleta::OutputEngine();
		fillCoins();
		std::fill(follow, follow + OUTS, false);
	}

	void process(const ProcessArgs& args) override {
		MoletaMessage* message = (MoletaMessage*) leftExpander.consumerMessage;
		// Moleta writes to every expander it reaches; clearing what was read makes
		// this one read as unlinked once it stops being written.
		MoletaMessage in = *message;
		*message = MoletaMessage();
		if (!in.linked) {
			if (attached) {
				// Wait for the next cycle restart once a Moleta is back, so the
				// outputs fall in line with its beats; the coin loops are kept.
				for (moleta::OutputEngine& o : outs) {
					o.waiting = true;
					o.pulse.cut();
				}
			}
			attached = false;
			for (int i = 0; i < OUTS; i++)
				outputs[OUT_OUTPUT + i].setVoltage(0.f);
			return;
		}
		attached = true;
		place = in.place;

		moleta::Controls controls[OUTS];
		for (int i = 0; i < OUTS; i++) {
			controls[i].ratio = (int) std::round(params[RATIO_PARAM + i].getValue());
			controls[i].chance = params[CHANCE_PARAM + i].getValue();
			controls[i].length = params[LENGTH_PARAM + i].getValue();
			controls[i].follow = follow[i];
		}
		float v[OUTS];
		moleta::processChain(outs, controls, OUTS, args.sampleTime, in.tick, in.shared, uniform, v);
		for (int i = 0; i < OUTS; i++)
			outputs[OUT_OUTPUT + i].setVoltage(v[i]);
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
		json_t* followJ = json_object_get(rootJ, "follow");
		for (int i = 1; i < OUTS; i++)
			follow[i] = json_is_true(json_array_get(followJ, i - 1));
	}
};

// Each output's ratio.
struct MoletaPlusReadout : w72::Readout {
	MoletaPlus* module;

	MoletaPlusReadout(MoletaPlus* module)
		: Readout(MoletaPlusPanel::READOUT_WELL, MoletaPlusPanel::READOUT, OUTS, MoletaPlusPanel::READOUT_VALUE_SIZE), module(module) {
		nameBaseline = MoletaPlusPanel::READOUT_NAME_BASELINE - MoletaPlusPanel::READOUT_WELL.pos.y;
	}

	std::string text(int i) override {
		if (!module)
			return MoletaPlusPanel::READOUT_SAMPLE[i];
		int r = moleta::clampRatio((int) std::round(module->params[MoletaPlus::RATIO_PARAM + i].getValue()));
		return moleta::RATIOS[r].name;
	}

	bool follows(int i) override {
		return module && module->follow[i];
	}

	// The output's number, from the place in the chain; none while unlinked.
	std::string name(int i) override {
		if (!module)
			return string::f("%d", moletaPlusOutput(0, i));
		return module->attached ? string::f("%d", moletaPlusOutput(module->place, i)) : "";
	}
};

struct MoletaPlusWidget : ModuleWidget {
	// The place the knobs and outputs are named for.
	int named = 0;

	MoletaPlusWidget(MoletaPlus* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/MoletaPlus.svg")));

		for (Vec pos : MoletaPlusPanel::SCREWS)
			addChild(createWidget<ScrewBlack>(pos));

		addChild(new MoletaPlusReadout(module));

		const Vec ratio[OUTS] = {MoletaPlusPanel::RATIO_5, MoletaPlusPanel::RATIO_6, MoletaPlusPanel::RATIO_7, MoletaPlusPanel::RATIO_8};
		const Vec chance[OUTS] = {MoletaPlusPanel::CHANCE_5, MoletaPlusPanel::CHANCE_6, MoletaPlusPanel::CHANCE_7, MoletaPlusPanel::CHANCE_8};
		const Vec length[OUTS] = {MoletaPlusPanel::LENGTH_5, MoletaPlusPanel::LENGTH_6, MoletaPlusPanel::LENGTH_7, MoletaPlusPanel::LENGTH_8};
		const Vec out[OUTS] = {MoletaPlusPanel::OUT_5, MoletaPlusPanel::OUT_6, MoletaPlusPanel::OUT_7, MoletaPlusPanel::OUT_8};
		for (int i = 0; i < OUTS; i++) {
			addParam(createParamCentered<RoundBlackKnob>(ratio[i], module, MoletaPlus::RATIO_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(chance[i], module, MoletaPlus::CHANCE_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(length[i], module, MoletaPlus::LENGTH_PARAM + i));
		}
		for (int i = 0; i < OUTS; i++)
			addOutput(createOutputCentered<DarkPJ301MPort>(out[i], module, MoletaPlus::OUT_OUTPUT + i));
	}

	void step() override {
		MoletaPlus* m = getModule<MoletaPlus>();
		if (m && m->attached && m->place != named) {
			named = m->place;
			m->name(named);
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		MoletaPlus* m = getModule<MoletaPlus>();
		if (!m)
			return;
		int at = m->attached ? m->place : 0;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Follow the output above", "", [=](Menu* sub) {
			for (int i = 1; i < OUTS; i++)
				sub->addChild(createBoolPtrMenuItem(string::f("Output %d follows %d", moletaPlusOutput(at, i), moletaPlusOutput(at, i - 1)), "", &m->follow[i]));
		}));
	}
};

Model* modelMoletaPlus = createModel<MoletaPlus, MoletaPlusWidget>("MoletaPlus");
