// Brumista: the bridge module. Empty for now; the TCP server comes in milestone 1.
#include "plugin.hpp"

struct Brumista : Module {
	enum ParamId { PARAMS_LEN };
	enum InputId { INPUTS_LEN };
	enum OutputId { OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	Brumista() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}
};

struct BrumistaWidget : ModuleWidget {
	BrumistaWidget(Brumista* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Brumista.svg")));

		addChild(createWidget<ScrewSilver>(Vec(0, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
};

Model* modelBrumista = createModel<Brumista, BrumistaWidget>("Brumista");
