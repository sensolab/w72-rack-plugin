// Ghisa: a 16-stage shift register whose taps are a control. Eight shape buttons
// pick stages from an eight-stage window that DEPTH slides through the register;
// the taps leave as a poly CHORD, a per-voice CHANGE trigger or gate and a mono BASS.
// Everything the outputs read is latched on the clock; BASS CLOCK, when patched,
// gives BASS a clock of its own.
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
// Two voltages this close are the same note, for CHANGE and for Duplicate notes.
static const float SAME_VOLTAGE = 1e-3f;
// A main clock this soon after a bass clock counts as the same edge: every cable
// in Rack delays by a sample, so two clocks from one source rarely arrive together.
static const float BASS_GRACE = 1e-3f;
// Choices for HOLD's loop length, in stages.
static const int HOLD_LENGTHS[] = {16, 12, 8, 6, 4, 3};
static const int HOLD_LENGTHS_LEN = sizeof(HOLD_LENGTHS) / sizeof(HOLD_LENGTHS[0]);

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

static bool sameNote(float a, float b) {
	return std::fabs(a - b) <= SAME_VOLTAGE;
}

// Drops each note that equals an earlier one, keeping the order; returns the new count.
static int skipDuplicates(float* v, int n) {
	int kept = 0;
	for (int i = 0; i < n; i++) {
		bool seen = false;
		for (int j = 0; j < kept; j++)
			seen = seen || sameNote(v[i], v[j]);
		if (!seen)
			v[kept++] = v[i];
	}
	return kept;
}

// Raises each note that equals an earlier one by whole octaves until it matches none,
// so a bumped note that lands on another keeps climbing.
static void raiseDuplicates(float* v, int n) {
	for (int i = 1; i < n; i++) {
		for (int j = 0; j < i; j++) {
			if (sameNote(v[i], v[j])) {
				v[i] += 1.f;
				j = -1;
			}
		}
	}
}

// Folds a V/oct voltage by whole octaves into the window of `octaves` octaves
// starting at floorV. The small offset keeps an exact C on its own octave's floor.
static float foldInto(float v, float floorV, int octaves) {
	float x = v - floorV;
	return floorV + x - octaves * std::floor((x + 1e-4f) / octaves);
}

static bool isString(json_t* j, const char* value) {
	const char* s = json_string_value(j);
	return s && std::string(s) == value;
}

struct Ghisa : Module {
	enum ParamId { ENUMS(SHAPE_PARAM, TAPS), DEPTH_PARAM, HOLD_PARAM, PARAMS_LEN };
	// New inputs go at the end: patches store cables by input index.
	enum InputId { IN_INPUT, CLOCK_INPUT, DEPTH_INPUT, HOLD_INPUT, BASS_CLOCK_INPUT, INPUTS_LEN };
	enum OutputId { CHORD_OUTPUT, CHANGE_OUTPUT, BASS_OUTPUT, OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	enum BassMode { BASS_LOWEST, BASS_ROOT };
	enum ChordOrder { ORDER_STAGE, ORDER_PITCH };
	enum DuplicateMode { DUP_KEEP, DUP_SKIP, DUP_OCTAVE };
	enum ChangeMode { CHANGE_TRIGGER, CHANGE_GATE };

	// Stage 1 is reg[0], the newest.
	float reg[STAGES] = {};

	// Latched on the clock: what the outputs and the register display show.
	bool shape[TAPS] = {};
	int depth = 0;
	float chord[TAPS] = {};
	// The sounding chord's taps before the fold, in stage order: BASS reads these.
	// With Duplicate notes on Skip there can be fewer channels than taps.
	float taps[TAPS] = {};
	int tapCount = 0;
	int channels = 0;
	float bass = 0.f;
	bool latchPending = true;

	// Right-click modes. Bass octave is 0 for off, or 1 to 3 to fold above C1 to C3.
	// Chord range is 0 for off, or 1 to 4 to fold above C2 to C5; chord width is 0
	// for one octave, 1 for two.
	int bassMode = BASS_LOWEST;
	int bassOctave = 0;
	int chordRange = 0;
	int chordWidth = 0;
	int chordOrder = ORDER_STAGE;
	int duplicates = DUP_KEEP;
	int changeMode = CHANGE_TRIGGER;
	// An index into HOLD_LENGTHS: how many stages HOLD loops.
	int holdLength = 0;

	bool holding = false;
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger holdGate;
	dsp::SchmittTrigger bassClockTrigger;
	// Time left in which a main clock re-reads the bass after its shift.
	float bassGrace = 0.f;
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
		configInput(HOLD_INPUT, "Hold")->description = "While high, the last stage of the loop feeds stage 1 (stage 16 unless Hold loop length says less)";
		configInput(BASS_CLOCK_INPUT, "Bass clock")->description = "Normalled to CLOCK. When patched, BASS changes only on its rising edges";
		configOutput(CHORD_OUTPUT, "Chord");
		configOutput(CHANGE_OUTPUT, "Change")->description = "Per channel, a trigger (or a gate until the next clock) when that channel's voltage changed";
		configOutput(BASS_OUTPUT, "Bass");
	}

	void onReset() override {
		std::fill(reg, reg + STAGES, 0.f);
		bassMode = BASS_LOWEST;
		bassOctave = 0;
		chordRange = 0;
		chordWidth = 0;
		chordOrder = ORDER_STAGE;
		duplicates = DUP_KEEP;
		changeMode = CHANGE_TRIGGER;
		holdLength = 0;
		latchPending = true;
	}

	// C4 is 0 V, so C1 is -3 V.
	float foldBass(float v) {
		return bassOctave == 0 ? v : foldInto(v, bassOctave - 4.f, 1);
	}

	// C2 is -2 V.
	float foldChord(float v) {
		return chordRange == 0 ? v : foldInto(v, chordRange - 3.f, chordWidth + 1);
	}

	// BASS from the taps of the chord that is sounding, before the chord's fold,
	// by the Bass mode and octave.
	float bassOf() {
		if (tapCount == 0)
			return 0.f;
		float sorted[TAPS];
		std::copy(taps, taps + tapCount, sorted);
		sortLowToHigh(sorted, tapCount);
		return foldBass(bassMode == BASS_ROOT ? rootOf(sorted, tapCount) : sorted[0]);
	}

	// Reads the shape and DEPTH, taps the register, folds the taps by the chord
	// range and handles duplicate notes. On a clock, CHANGE fires for each channel
	// that is new or whose voltage moved by more than 1 mV: a trigger,
	// or a gate held until the next clock. BASS follows, unless BASS CLOCK is patched
	// (it's still read when the module loads or resets, so it never starts empty).
	void latch(bool clocked) {
		latchPending = false;
		for (int k = 0; k < TAPS; k++)
			shape[k] = params[SHAPE_PARAM + k].getValue() > 0.5f;
		float d = params[DEPTH_PARAM].getValue() + inputs[DEPTH_INPUT].getVoltage() / VOLTS_PER_STAGE;
		depth = clamp((int) std::round(d), 0, MAX_DEPTH);

		float raw[TAPS];
		int n = 0;
		for (int k = 0; k < TAPS; k++) {
			if (shape[k])
				raw[n++] = reg[depth + k];
		}
		// Duplicates go after the fold, which can make two taps equal, and before By
		// pitch, which orders after both, so channel N is the Nth lowest note that sounds.
		// n counts the taps and stays so; the chord can end up with fewer channels.
		float next[TAPS];
		for (int c = 0; c < n; c++)
			next[c] = foldChord(raw[c]);
		int count = n;
		if (duplicates == DUP_SKIP)
			count = skipDuplicates(next, n);
		else if (duplicates == DUP_OCTAVE)
			raiseDuplicates(next, n);
		if (chordOrder == ORDER_PITCH)
			sortLowToHigh(next, count);

		gateMode = changeMode == CHANGE_GATE;
		if (clocked) {
			for (int c = 0; c < TAPS; c++) {
				bool changed = c < count && (c >= channels || !sameNote(next[c], chord[c]));
				if (changed && (!gateMode || changeGate[c]))
					changePulse[c].trigger(1e-3f);
				changeGate[c] = changed && gateMode;
			}
		}
		std::copy(next, next + count, chord);
		std::copy(raw, raw + n, taps);
		tapCount = n;
		channels = count;

		if (!clocked || !inputs[BASS_CLOCK_INPUT].isConnected())
			bass = bassOf();
	}

	void process(const ProcessArgs& args) override {
		holdGate.process(inputs[HOLD_INPUT].getVoltage(), 0.1f, 1.f);
		holding = params[HOLD_PARAM].getValue() > 0.5f || holdGate.isHigh();

		bool clocked = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		if (clocked) {
			float in = holding ? reg[HOLD_LENGTHS[holdLength] - 1] : inputs[IN_INPUT].getVoltage();
			std::copy_backward(reg, reg + STAGES - 1, reg + STAGES);
			reg[0] = in;
			latch(true);
		}
		else if (latchPending) {
			latch(false);
		}

		// BASS CLOCK holds BASS between its edges. An edge on the same sample as a
		// clock reads after the shift; a clock that follows within BASS_GRACE
		// counts as the same edge, and BASS reads again after its shift.
		if (inputs[BASS_CLOCK_INPUT].isConnected()) {
			if (bassClockTrigger.process(inputs[BASS_CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
				bass = bassOf();
				bassGrace = clocked ? 0.f : BASS_GRACE;
			}
			else if (bassGrace > 0.f) {
				if (clocked) {
					bass = bassOf();
					bassGrace = 0.f;
				}
				else {
					bassGrace -= args.sampleTime;
				}
			}
		}
		else {
			bassGrace = 0.f;
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
		json_object_set_new(rootJ, "chordRange", json_integer(chordRange));
		json_object_set_new(rootJ, "chordRangeWidth", json_integer(chordWidth + 1));
		json_object_set_new(rootJ, "chordOrder", json_string(chordOrder == ORDER_PITCH ? "pitch" : "stage"));
		json_object_set_new(rootJ, "duplicates", json_string(duplicates == DUP_SKIP ? "skip" : duplicates == DUP_OCTAVE ? "octave" : "keep"));
		json_object_set_new(rootJ, "change", json_string(changeMode == CHANGE_GATE ? "gate" : "trigger"));
		json_object_set_new(rootJ, "holdLength", json_integer(HOLD_LENGTHS[holdLength]));
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
		if (json_t* j = json_object_get(rootJ, "chordRange"))
			chordRange = clamp((int) json_integer_value(j), 0, 4);
		if (json_t* j = json_object_get(rootJ, "chordRangeWidth"))
			chordWidth = clamp((int) json_integer_value(j), 1, 2) - 1;
		if (json_t* j = json_object_get(rootJ, "chordOrder"))
			chordOrder = isString(j, "pitch") ? ORDER_PITCH : ORDER_STAGE;
		if (json_t* j = json_object_get(rootJ, "duplicates"))
			duplicates = isString(j, "skip") ? DUP_SKIP : isString(j, "octave") ? DUP_OCTAVE : DUP_KEEP;
		if (json_t* j = json_object_get(rootJ, "change"))
			changeMode = isString(j, "gate") ? CHANGE_GATE : CHANGE_TRIGGER;
		if (json_t* j = json_object_get(rootJ, "holdLength")) {
			holdLength = 0;
			for (int i = 0; i < HOLD_LENGTHS_LEN; i++) {
				if (HOLD_LENGTHS[i] == json_integer_value(j))
					holdLength = i;
			}
		}
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
		addInput(createInputCentered<DarkPJ301MPort>(GhisaPanel::BASS_CLOCK, module, Ghisa::BASS_CLOCK_INPUT));

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
		menu->addChild(createIndexPtrSubmenuItem("Chord range", {"Off", "Above C2", "Above C3", "Above C4", "Above C5"}, &m->chordRange));
		menu->addChild(createIndexPtrSubmenuItem("Chord width", {"1 octave", "2 octaves"}, &m->chordWidth));
		menu->addChild(createIndexPtrSubmenuItem("Chord order", {"By stage", "By pitch"}, &m->chordOrder));
		menu->addChild(createIndexPtrSubmenuItem("Duplicate notes", {"Keep", "Skip", "Octave up"}, &m->duplicates));
		menu->addChild(createIndexPtrSubmenuItem("Change", {"Trigger", "Gate until next clock"}, &m->changeMode));
		menu->addChild(createIndexPtrSubmenuItem("Hold loop length", {"16 stages", "12 stages", "8 stages", "6 stages", "4 stages", "3 stages"}, &m->holdLength));
	}
};

Model* modelGhisa = createModel<Ghisa, GhisaWidget>("Ghisa");
