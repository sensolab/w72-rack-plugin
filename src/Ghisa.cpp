// Ghisa: a 16-stage shift register whose taps are a control. Eight shape buttons
// pick stages from an eight-stage window that DEPTH slides through the register;
// the taps leave as a poly CHORD, a per-voice CHANGE trigger or gate and a mono BASS.
// Everything the outputs read is latched on the clock.
//
// The Root bass scores intervals as cvly bss's Harmonic mode does
// (cvly-modules by Lyqst, GPL-3.0, https://github.com/Lyqst/cvly-modules).
#include "plugin.hpp"
#include "GhisaPanel.hpp"
#include "widgets.hpp"
#include <algorithm>
#include <climits>

static const int STAGES = 16;
static const int TAPS = 8;
static const int MAX_DEPTH = STAGES - TAPS;
static const float VOLTS_PER_STAGE = 1.25f;

// Pitch class of a V/oct voltage, 0 (C) to 11.
static int pitchClass(float v) {
	int semitones = (int) std::round(v * 12.f);
	return ((semitones % 12) + 12) % 12;
}

// bss's score for a note this many semitones above a candidate root: lower fits better.
static const int ROOT_SCORE[12] = {0, 12, 6, 2, 2, 12, 6, 0, 12, 12, 4, 4};

// The tap that best explains the others as a chord built on it. Notes are
// sorted low to high, so a tie goes to the lower note.
static float rootOf(const float* sorted, int n) {
	int best = 0;
	int bestScore = INT_MAX;
	for (int i = 0; i < n; i++) {
		int score = 0;
		for (int j = 0; j < n; j++)
			score += ROOT_SCORE[(pitchClass(sorted[j]) - pitchClass(sorted[i]) + 12) % 12];
		if (score < bestScore) {
			bestScore = score;
			best = i;
		}
	}
	return sorted[best];
}

// At most eight values: an insertion sort, which also keeps GCC's bounds warnings about std::sort quiet.
static void sortLowToHigh(float* v, int n) {
	for (int i = 1; i < n; i++) {
		float x = v[i];
		int j = i;
		for (; j > 0 && v[j - 1] > x; j--)
			v[j] = v[j - 1];
		v[j] = x;
	}
}

static bool isString(json_t* j, const char* value) {
	const char* s = json_string_value(j);
	return s && std::string(s) == value;
}

struct Ghisa : Module {
	enum ParamId { ENUMS(SHAPE_PARAM, TAPS), DEPTH_PARAM, HOLD_PARAM, PARAMS_LEN };
	enum InputId { IN_INPUT, CLOCK_INPUT, DEPTH_INPUT, HOLD_INPUT, INPUTS_LEN };
	enum OutputId { CHORD_OUTPUT, CHANGE_OUTPUT, BASS_OUTPUT, OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	enum BassMode { BASS_LOWEST, BASS_ROOT };
	enum ChordOrder { ORDER_STAGE, ORDER_PITCH };
	enum ChangeMode { CHANGE_TRIGGER, CHANGE_GATE };

	// Stage 1 is reg[0], the newest.
	float reg[STAGES] = {};

	// Latched on the clock: what the outputs and the register display show.
	bool shape[TAPS] = {};
	int depth = 0;
	float chord[TAPS] = {};
	int channels = 0;
	float bass = 0.f;
	bool latchPending = true;

	// Right-click modes. Bass octave is 0 for off, or 1 to 3 to fold above C1 to C3.
	int bassMode = BASS_LOWEST;
	int bassOctave = 0;
	int chordOrder = ORDER_STAGE;
	int changeMode = CHANGE_TRIGGER;

	bool holding = false;
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger holdGate;
	// A trigger's 1 ms pulse; in gate mode, the 1 ms gap that retriggers a gate already high.
	dsp::PulseGenerator changePulse[TAPS];
	bool changeGate[TAPS] = {};
	bool gateMode = false;

	Ghisa() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int k = 0; k < TAPS; k++) {
			bool lit = k == 0 || k == 2 || k == 5;
			configSwitch(SHAPE_PARAM + k, 0.f, 1.f, lit ? 1.f : 0.f, string::f("Shape %d", k + 1), {"Off", "On"});
		}
		configParam(DEPTH_PARAM, 0.f, MAX_DEPTH, 0.f, "Depth", " stages");
		getParamQuantity(DEPTH_PARAM)->snapEnabled = true;
		configSwitch(HOLD_PARAM, 0.f, 1.f, 0.f, "Hold", {"Off", "On"});
		getParamQuantity(HOLD_PARAM)->randomizeEnabled = false;
		configInput(IN_INPUT, "In");
		configInput(CLOCK_INPUT, "Clock");
		configInput(DEPTH_INPUT, "Depth")->description = "1.25 V per stage, added to the knob";
		configInput(HOLD_INPUT, "Hold")->description = "While high, stage 16 feeds stage 1";
		configOutput(CHORD_OUTPUT, "Chord");
		configOutput(CHANGE_OUTPUT, "Change")->description = "Per channel, a trigger (or a gate until the next clock) when that channel's voltage changed";
		configOutput(BASS_OUTPUT, "Bass");
	}

	void onReset() override {
		std::fill(reg, reg + STAGES, 0.f);
		bassMode = BASS_LOWEST;
		bassOctave = 0;
		chordOrder = ORDER_STAGE;
		changeMode = CHANGE_TRIGGER;
		latchPending = true;
	}

	float foldBass(float v) {
		if (bassOctave == 0)
			return v;
		// C4 is 0 V, so C1 is -3 V. The small offset keeps an exact C on its own octave's floor.
		float floorV = bassOctave - 4.f;
		float x = v - floorV;
		return floorV + x - std::floor(x + 1e-4f);
	}

	// Reads the shape and DEPTH and taps the register. On a clock, CHANGE fires for
	// each channel that is new or whose voltage moved by more than 1 mV: a trigger,
	// or a gate held until the next clock.
	void latch(bool clocked) {
		latchPending = false;
		for (int k = 0; k < TAPS; k++)
			shape[k] = params[SHAPE_PARAM + k].getValue() > 0.5f;
		float d = params[DEPTH_PARAM].getValue() + inputs[DEPTH_INPUT].getVoltage() / VOLTS_PER_STAGE;
		depth = clamp((int) std::round(d), 0, MAX_DEPTH);

		float next[TAPS];
		int n = 0;
		for (int k = 0; k < TAPS; k++) {
			if (shape[k])
				next[n++] = reg[depth + k];
		}
		float sorted[TAPS];
		std::copy(next, next + n, sorted);
		sortLowToHigh(sorted, n);
		if (chordOrder == ORDER_PITCH)
			std::copy(sorted, sorted + n, next);

		gateMode = changeMode == CHANGE_GATE;
		if (clocked) {
			for (int c = 0; c < TAPS; c++) {
				bool changed = c < n && (c >= channels || std::fabs(next[c] - chord[c]) > 1e-3f);
				if (changed && (!gateMode || changeGate[c]))
					changePulse[c].trigger(1e-3f);
				changeGate[c] = changed && gateMode;
			}
		}
		std::copy(next, next + n, chord);
		channels = n;

		if (n == 0)
			bass = 0.f;
		else
			bass = foldBass(bassMode == BASS_ROOT ? rootOf(sorted, n) : sorted[0]);
	}

	void process(const ProcessArgs& args) override {
		holdGate.process(inputs[HOLD_INPUT].getVoltage(), 0.1f, 1.f);
		holding = params[HOLD_PARAM].getValue() > 0.5f || holdGate.isHigh();

		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			float in = holding ? reg[STAGES - 1] : inputs[IN_INPUT].getVoltage();
			std::copy_backward(reg, reg + STAGES - 1, reg + STAGES);
			reg[0] = in;
			latch(true);
		}
		else if (latchPending) {
			latch(false);
		}

		// Rack keeps a connected output at one channel at least, so an empty shape gives one channel at 0 V.
		outputs[CHORD_OUTPUT].setChannels(channels);
		outputs[CHANGE_OUTPUT].setChannels(channels);
		for (int c = 0; c < TAPS; c++) {
			bool fire = changePulse[c].process(args.sampleTime);
			if (c < channels) {
				outputs[CHORD_OUTPUT].setVoltage(chord[c], c);
				bool high = gateMode ? changeGate[c] && !fire : fire;
				outputs[CHANGE_OUTPUT].setVoltage(high ? 10.f : 0.f, c);
			}
		}
		outputs[BASS_OUTPUT].setVoltage(bass);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_t* regJ = json_array();
		for (float v : reg)
			json_array_append_new(regJ, json_real(v));
		json_object_set_new(rootJ, "register", regJ);
		json_object_set_new(rootJ, "bass", json_string(bassMode == BASS_ROOT ? "root" : "lowest"));
		json_object_set_new(rootJ, "bassOctave", json_integer(bassOctave));
		json_object_set_new(rootJ, "chordOrder", json_string(chordOrder == ORDER_PITCH ? "pitch" : "stage"));
		json_object_set_new(rootJ, "change", json_string(changeMode == CHANGE_GATE ? "gate" : "trigger"));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* regJ = json_object_get(rootJ, "register");
		for (int i = 0; i < STAGES; i++)
			reg[i] = json_number_value(json_array_get(regJ, i));
		if (json_t* j = json_object_get(rootJ, "bass"))
			bassMode = isString(j, "root") ? BASS_ROOT : BASS_LOWEST;
		if (json_t* j = json_object_get(rootJ, "bassOctave"))
			bassOctave = clamp((int) json_integer_value(j), 0, 3);
		if (json_t* j = json_object_get(rootJ, "chordOrder"))
			chordOrder = isString(j, "pitch") ? ORDER_PITCH : ORDER_STAGE;
		if (json_t* j = json_object_get(rootJ, "change"))
			changeMode = isString(j, "gate") ? CHANGE_GATE : CHANGE_TRIGGER;
		latchPending = true;
	}
};

// The module browser has no module behind the panel, so it shows a sample:
// the shape 1·2·4·6·8 at DEPTH 5, as on the design mock.
static const bool SAMPLE_SHAPE[TAPS] = {true, true, false, true, false, true, false, true};
static const int SAMPLE_DEPTH = 5;

// Sixteen bars, newest at the left: off outside the window, dim in it, full where tapped.
struct RegisterDisplay : w72::FlatBars {
	Ghisa* module;

	RegisterDisplay(Ghisa* module)
		: FlatBars(GhisaPanel::REGISTER_WELL, GhisaPanel::REGISTER, STAGES), module(module) {}

	float level(int i) override {
		const bool* shape = module ? module->shape : SAMPLE_SHAPE;
		int depth = module ? module->depth : SAMPLE_DEPTH;
		int k = i - depth;
		if (k < 0 || k >= TAPS)
			return 0.f;
		return shape[k] ? 1.f : w72::DIM;
	}
};

// HOLD's light shows that the register is looping, by the button or the gate.
struct HoldButton : w72::FlatButton {
	bool isLit() override {
		Ghisa* m = dynamic_cast<Ghisa*>(module);
		return m ? m->holding : sample;
	}
};

struct GhisaWidget : ModuleWidget {
	GhisaWidget(Ghisa* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ghisa.svg")));

		for (Vec pos : GhisaPanel::SCREWS)
			addChild(createWidget<ScrewBlack>(pos));

		addChild(new RegisterDisplay(module));

		using GhisaPanel::BUTTON_SIZE;
		using GhisaPanel::FLAT_LIGHT_SIZE;
		for (int k = 0; k < TAPS; k++) {
			w72::FlatButton* b = w72::createFlatButton(GhisaPanel::SHAPE[k], BUTTON_SIZE, FLAT_LIGHT_SIZE, module, Ghisa::SHAPE_PARAM + k);
			b->sample = SAMPLE_SHAPE[k];
			addParam(b);
		}
		addParam(createParamCentered<RoundLargeBlackKnob>(GhisaPanel::DEPTH, module, Ghisa::DEPTH_PARAM));
		addParam(w72::createFlatButton<HoldButton>(GhisaPanel::HOLD, BUTTON_SIZE, FLAT_LIGHT_SIZE, module, Ghisa::HOLD_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(GhisaPanel::IN, module, Ghisa::IN_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GhisaPanel::CLOCK, module, Ghisa::CLOCK_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GhisaPanel::DEPTH_CV, module, Ghisa::DEPTH_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(GhisaPanel::HOLD_GATE, module, Ghisa::HOLD_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(GhisaPanel::CHORD, module, Ghisa::CHORD_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(GhisaPanel::CHANGE, module, Ghisa::CHANGE_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(GhisaPanel::BASS, module, Ghisa::BASS_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Ghisa* m = getModule<Ghisa>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Bass", {"Lowest", "Root"}, &m->bassMode));
		menu->addChild(createIndexPtrSubmenuItem("Bass octave", {"Off", "Above C1", "Above C2", "Above C3"}, &m->bassOctave));
		menu->addChild(createIndexPtrSubmenuItem("Chord order", {"By stage", "By pitch"}, &m->chordOrder));
		menu->addChild(createIndexPtrSubmenuItem("Change", {"Trigger", "Gate until next clock"}, &m->changeMode));
	}
};

Model* modelGhisa = createModel<Ghisa, GhisaWidget>("Ghisa");
