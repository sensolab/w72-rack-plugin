// Brumista: the bridge module. A TCP server on 127.0.0.1 (port 7200 unless the menu sets another)
// that lets Claude read a running Rack.
// The link light is off while the port is unavailable, dim while listening, and lit with a client.
#include "plugin.hpp"
#include "BrumistaPanel.hpp"
#include "BrumistaCommands.hpp"

static const int DEFAULT_PORT = 7200;
// Below 1024 needs privileges on Linux and macOS.
static const int MIN_PORT = 1024;
static const int MAX_PORT = 65535;

struct Brumista : Module {
	enum ParamId { PARAMS_LEN };
	enum InputId { INPUTS_LEN };
	enum OutputId { OUTPUTS_LEN };
	enum LightId { LINK_LIGHT, LIGHTS_LEN };

	brumista::Server server{DEFAULT_PORT, [this](const std::string& line) { return brumista::handleLine(server, line); }};

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

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "port", json_integer(server.getPort()));
		return rootJ;
	}

	// Patches saved before the port was configurable have no "port" and keep the default.
	void dataFromJson(json_t* rootJ) override {
		json_t* portJ = json_object_get(rootJ, "port");
		if (json_is_integer(portJ) && isValidPort(json_integer_value(portJ)))
			server.setPort((int) json_integer_value(portJ));
	}

	static bool isValidPort(long long port) {
		return port >= MIN_PORT && port <= MAX_PORT;
	}
};

// The context menu's port field: Enter applies a valid port and closes the menu. Anything else
// leaves the field open with its text, and the port unchanged.
struct PortField : ui::TextField {
	Brumista* module;

	void onSelectKey(const SelectKeyEvent& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
			e.consume(this);
			std::string t = string::trim(text);
			char* end = nullptr;
			long port = std::strtol(t.c_str(), &end, 10);
			if (t.empty() || *end != '\0' || !Brumista::isValidPort(port))
				return;
			if (port != module->server.getPort())
				module->server.setPort((int) port);
			if (ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>())
				overlay->requestDelete();
			return;
		}
		ui::TextField::onSelectKey(e);
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
		menu->addChild(createMenuLabel("Port (Enter to apply)"));
		PortField* field = new PortField;
		field->module = m;
		field->box.size.x = 100;
		field->placeholder = std::to_string(DEFAULT_PORT);
		field->setText(std::to_string(m->server.getPort()));
		field->selectAll();
		menu->addChild(field);
	}
};

Model* modelBrumista = createModel<Brumista, BrumistaWidget>("Brumista");
