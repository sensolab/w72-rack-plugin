// Brumista: the bridge module. A TCP server on 127.0.0.1:7200 that lets Claude read a running Rack.
// The link light is off while the port is unavailable, dim while listening, and lit with a client.
#include "plugin.hpp"
#include "BrumistaPanel.hpp"
#include "BrumistaCommands.hpp"

static const int PORT = 7200;

struct Brumista : Module {
	enum ParamId { PARAMS_LEN };
	enum InputId { INPUTS_LEN };
	enum OutputId { OUTPUTS_LEN };
	enum LightId { LINK_LIGHT, LIGHTS_LEN };

	brumista::Server server{PORT, [this](const std::string& line) { return brumista::handleLine(server, line); }};

	Brumista() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}

	void onAdd(const AddEvent& e) override {
		server.start();
	}

	// The engine holds its lock here, so only signal; the server's destructor joins.
	void onRemove(const RemoveEvent& e) override {
		server.signalStop();
	}

	void process(const ProcessArgs& args) override {
		using Status = brumista::Server::Status;
		Status status = server.getStatus();
		lights[LINK_LIGHT].setBrightness(status == Status::CONNECTED ? 1.f : status == Status::LISTENING ? 0.25f : 0.f);
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

	// Commands that touch the patch run here, on the main thread.
	void step() override {
		ModuleWidget::step();
		if (Brumista* m = getModule<Brumista>()) {
			brumista::stepView();
			m->server.drain();
		}
	}

	void appendContextMenu(Menu* menu) override {
		Brumista* m = getModule<Brumista>();
		if (!m)
			return;
		using Status = brumista::Server::Status;
		std::string address = "127.0.0.1:" + std::to_string(m->server.getPort());
		std::string text;
		switch (m->server.getStatus()) {
			case Status::CONNECTED: text = "Client connected on " + address; break;
			case Status::LISTENING: text = "Listening on " + address; break;
			case Status::WAITING_FOR_PORT: text = address + " in use, retrying"; break;
			default: text = "Stopped"; break;
		}
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(text));
	}
};

Model* modelBrumista = createModel<Brumista, BrumistaWidget>("Brumista");
