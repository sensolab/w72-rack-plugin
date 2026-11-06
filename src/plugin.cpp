#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelBrumista);
	p->addModel(modelGhisa);
	p->addModel(modelMoleta);
	p->addModel(modelMoletaPlus);
	p->addModel(modelGibigiana);
}
