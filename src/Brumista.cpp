// Brumista: the bridge module. The TCP server comes in milestone 1; until then the link light stays off.
#include "plugin.hpp"
#include "BrumistaPanel.hpp"

struct Brumista : Module {
	enum ParamId { PARAMS_LEN };
	enum InputId { INPUTS_LEN };
	enum OutputId { OUTPUTS_LEN };
	enum LightId { LINK_LIGHT, LIGHTS_LEN };

	Brumista() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}
};

struct BrumistaWidget : ModuleWidget {
	BrumistaWidget(Brumista* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Brumista.svg")));

		for (Vec pos : BrumistaPanel::SCREWS)
			addChild(createWidget<ScrewBlack>(pos));

		addChild(createLightCentered<MediumLight<WhiteLight>>(BrumistaPanel::LINK_LIGHT, module, Brumista::LINK_LIGHT));
	}
};

Model* modelBrumista = createModel<Brumista, BrumistaWidget>("Brumista");
