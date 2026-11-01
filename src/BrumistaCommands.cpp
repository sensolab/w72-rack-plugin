// Brumista's commands: the dispatch table and the read-only handlers.
// Shapes and error strings follow limen (forsitan modulare, GPL-3.0-or-later, by gosub),
// recorded in protocol/limen/commands.json; differences are noted in protocol/commands.json.
#include "BrumistaCommands.hpp"
#include "plugin.hpp"
#include <tag.hpp>

namespace brumista {

namespace {

// Commands that touch the engine or the scene run on the main thread, through the
// queue; the rest read plugin data that doesn't change while Rack runs.
enum class Thread { SERVER, MAIN };

const auto MAIN_TIMEOUT = std::chrono::seconds(2);

struct CommandError : std::runtime_error {
	using std::runtime_error::runtime_error;
};

// The request's integer field key, or CommandError(missing) when absent.
int64_t requireInt(json_t* req, const char* key, const char* missing) {
	json_t* j = json_object_get(req, key);
	if (!json_is_integer(j))
		throw CommandError(missing);
	return json_integer_value(j);
}

engine::Module* requireModule(int64_t id) {
	engine::Module* m = APP->engine->getModule(id);
	if (!m)
		throw CommandError("module not found");
	return m;
}

std::string portName(const std::vector<engine::PortInfo*>& infos, int i) {
	if (i >= 0 && i < (int) infos.size() && infos[i])
		return infos[i]->name;
	return "";
}

json_t* moduleJson(engine::Module* m) {
	json_t* obj = json_object();
	json_object_set_new(obj, "id", json_integer(m->id));
	if (m->model) {
		json_object_set_new(obj, "plugin", json_string(m->model->plugin ? m->model->plugin->slug.c_str() : ""));
		json_object_set_new(obj, "model", json_string(m->model->slug.c_str()));
		json_object_set_new(obj, "name", json_string(m->model->name.c_str()));
	}
	json_object_set_new(obj, "numParams", json_integer(m->getNumParams()));
	json_object_set_new(obj, "numInputs", json_integer(m->getNumInputs()));
	json_object_set_new(obj, "numOutputs", json_integer(m->getNumOutputs()));
	if (app::ModuleWidget* mw = APP->scene->rack->getModule(m->id)) {
		math::Vec p = mw->getGridPosition();
		json_t* pos = json_object();
		json_object_set_new(pos, "x", json_integer(std::lround(p.x)));
		json_object_set_new(pos, "y", json_integer(std::lround(p.y)));
		json_object_set_new(obj, "pos", pos);
		json_object_set_new(obj, "hp", json_integer(std::lround(mw->getGridSize().x)));
	}
	return obj;
}

json_t* paramJson(engine::Module* m, int i) {
	json_t* obj = json_object();
	json_object_set_new(obj, "id", json_integer(i));
	json_object_set_new(obj, "value", json_real(APP->engine->getParamValue(m, i)));
	if (engine::ParamQuantity* pq = m->getParamQuantity(i)) {
		json_object_set_new(obj, "name", json_string(pq->name.c_str()));
		json_object_set_new(obj, "min", json_real(pq->minValue));
		json_object_set_new(obj, "max", json_real(pq->maxValue));
		json_object_set_new(obj, "unit", json_string(pq->unit.c_str()));
	}
	return obj;
}

std::string pluginFilter(json_t* req) {
	json_t* j = json_object_get(req, "plugin");
	return json_is_string(j) ? json_string_value(j) : "";
}

json_t* hello(json_t* req);

json_t* listPlugins(json_t*) {
	json_t* arr = json_array();
	for (plugin::Plugin* p : plugin::plugins) {
		json_t* obj = json_object();
		json_object_set_new(obj, "slug", json_string(p->slug.c_str()));
		json_object_set_new(obj, "name", json_string(p->name.c_str()));
		json_object_set_new(obj, "version", json_string(p->version.c_str()));
		json_array_append_new(arr, obj);
	}
	return arr;
}

json_t* listModels(json_t* req) {
	std::string filter = pluginFilter(req);
	json_t* arr = json_array();
	for (plugin::Plugin* p : plugin::plugins) {
		if (!filter.empty() && p->slug != filter)
			continue;
		for (plugin::Model* mdl : p->models) {
			json_t* obj = json_object();
			json_object_set_new(obj, "plugin", json_string(p->slug.c_str()));
			json_object_set_new(obj, "slug", json_string(mdl->slug.c_str()));
			json_object_set_new(obj, "name", json_string(mdl->name.c_str()));
			json_object_set_new(obj, "description", json_string(mdl->description.c_str()));
			json_array_append_new(arr, obj);
		}
	}
	return arr;
}

json_t* listModules(json_t* req) {
	std::string filter = pluginFilter(req);
	json_t* arr = json_array();
	for (int64_t id : APP->engine->getModuleIds()) {
		engine::Module* m = APP->engine->getModule(id);
		if (!m)
			continue;
		if (!filter.empty() && !(m->model && m->model->plugin && m->model->plugin->slug == filter))
			continue;
		json_array_append_new(arr, moduleJson(m));
	}
	return arr;
}

json_t* getModule(json_t* req) {
	return moduleJson(requireModule(requireInt(req, "id", "missing id")));
}

json_t* getModuleInfo(json_t* req) {
	int64_t id = requireInt(req, "id", "missing id");
	plugin::Model* mdl = requireModule(id)->model;
	if (!mdl)
		throw CommandError("module has no model");

	json_t* model = json_object();
	json_object_set_new(model, "slug", json_string(mdl->slug.c_str()));
	json_object_set_new(model, "name", json_string(mdl->name.c_str()));
	json_object_set_new(model, "description", json_string(mdl->description.c_str()));
	json_t* tags = json_array();
	for (int tagId : mdl->tagIds)
		json_array_append_new(tags, json_string(tag::getTag(tagId).c_str()));
	json_object_set_new(model, "tags", tags);
	json_object_set_new(model, "manualUrl", json_string(mdl->manualUrl.c_str()));
	json_object_set_new(model, "modularGridUrl", json_string(mdl->modularGridUrl.c_str()));

	json_t* plug = json_object();
	if (plugin::Plugin* p = mdl->plugin) {
		const std::pair<const char*, const std::string*> fields[] = {
			{"slug", &p->slug}, {"name", &p->name}, {"brand", &p->brand},
			{"version", &p->version}, {"license", &p->license}, {"author", &p->author},
			{"authorUrl", &p->authorUrl}, {"pluginUrl", &p->pluginUrl}, {"manualUrl", &p->manualUrl},
			{"sourceUrl", &p->sourceUrl}, {"donateUrl", &p->donateUrl}, {"changelogUrl", &p->changelogUrl},
		};
		for (auto& f : fields)
			json_object_set_new(plug, f.first, json_string(f.second->c_str()));
	}

	json_t* obj = json_object();
	json_object_set_new(obj, "id", json_integer(id));
	json_object_set_new(obj, "model", model);
	json_object_set_new(obj, "plugin", plug);
	return obj;
}

json_t* listParams(json_t* req) {
	engine::Module* m = requireModule(requireInt(req, "id", "missing id"));
	json_t* arr = json_array();
	for (int i = 0; i < m->getNumParams(); i++)
		json_array_append_new(arr, paramJson(m, i));
	return arr;
}

json_t* getParam(json_t* req) {
	int64_t id = requireInt(req, "id", "missing id or param");
	int64_t param = requireInt(req, "param", "missing id or param");
	engine::Module* m = requireModule(id);
	if (param < 0 || param >= m->getNumParams())
		throw CommandError("param not found");
	return paramJson(m, (int) param);
}

json_t* listPorts(json_t* req) {
	engine::Module* m = requireModule(requireInt(req, "id", "missing id"));
	auto ports = [](const std::vector<engine::PortInfo*>& infos, int count) {
		json_t* arr = json_array();
		for (int i = 0; i < count; i++) {
			json_t* obj = json_object();
			json_object_set_new(obj, "id", json_integer(i));
			json_object_set_new(obj, "name", json_string(portName(infos, i).c_str()));
			json_array_append_new(arr, obj);
		}
		return arr;
	};
	json_t* obj = json_object();
	json_object_set_new(obj, "inputs", ports(m->inputInfos, m->getNumInputs()));
	json_object_set_new(obj, "outputs", ports(m->outputInfos, m->getNumOutputs()));
	return obj;
}

json_t* listCables(json_t* req) {
	json_t* idJ = json_object_get(req, "id");
	bool filtered = json_is_integer(idJ);
	int64_t filter = filtered ? json_integer_value(idJ) : 0;
	bool verbose = json_is_true(json_object_get(req, "verbose"));

	json_t* arr = json_array();
	for (int64_t id : APP->engine->getCableIds()) {
		engine::Cable* c = APP->engine->getCable(id);
		if (!c || !c->outputModule || !c->inputModule)
			continue;
		if (filtered && c->outputModule->id != filter && c->inputModule->id != filter)
			continue;
		json_t* obj = json_object();
		json_object_set_new(obj, "id", json_integer(id));
		json_object_set_new(obj, "outputModule", json_integer(c->outputModule->id));
		json_object_set_new(obj, "outputPort", json_integer(c->outputId));
		json_object_set_new(obj, "inputModule", json_integer(c->inputModule->id));
		json_object_set_new(obj, "inputPort", json_integer(c->inputId));
		if (app::CableWidget* cw = APP->scene->rack->getCable(id))
			json_object_set_new(obj, "color", json_string(color::toHexString(cw->color).c_str()));
		if (verbose) {
			engine::Module* om = c->outputModule;
			engine::Module* im = c->inputModule;
			json_object_set_new(obj, "outputModuleName", json_string(om->model ? om->model->name.c_str() : ""));
			json_object_set_new(obj, "outputPortName", json_string(portName(om->outputInfos, c->outputId).c_str()));
			json_object_set_new(obj, "inputModuleName", json_string(im->model ? im->model->name.c_str() : ""));
			json_object_set_new(obj, "inputPortName", json_string(portName(im->inputInfos, c->inputId).c_str()));
		}
		json_array_append_new(arr, obj);
	}
	return arr;
}

struct Command {
	const char* name;
	Thread thread;
	json_t* (*run)(json_t* req);
};

// One line per command, in protocol/commands.json's order; tools/check-commands.sh reads it.
const Command COMMANDS[] = {
	{"hello", Thread::SERVER, hello},
	{"list_plugins", Thread::SERVER, listPlugins},
	{"list_models", Thread::SERVER, listModels},
	{"list_modules", Thread::MAIN, listModules},
	{"get_module", Thread::MAIN, getModule},
	{"get_module_info", Thread::MAIN, getModuleInfo},
	{"list_ports", Thread::MAIN, listPorts},
	{"list_params", Thread::MAIN, listParams},
	{"get_param", Thread::MAIN, getParam},
	{"list_cables", Thread::MAIN, listCables},
};

json_t* hello(json_t*) {
	json_t* names = json_array();
	for (const Command& c : COMMANDS)
		json_array_append_new(names, json_string(c.name));
	json_t* obj = json_object();
	json_object_set_new(obj, "server", json_string("Brumista"));
	json_object_set_new(obj, "version", json_string(pluginInstance->version.c_str()));
	json_object_set_new(obj, "commands", names);
	return obj;
}

const Command* findCommand(const std::string& name) {
	for (const Command& c : COMMANDS)
		if (name == c.name)
			return &c;
	return nullptr;
}

std::string run(const Command* command, json_t* req) {
	try {
		return okReply(command->run(req));
	}
	catch (CommandError& e) {
		return errorReply(e.what());
	}
	catch (std::exception& e) {
		return errorReply(std::string("internal error: ") + e.what());
	}
}

} // namespace

std::string handleLine(Server& server, const std::string& line) {
	json_error_t err;
	json_t* req = json_loads(line.c_str(), 0, &err);
	if (!req)
		return errorReply("invalid JSON");
	std::string out;
	json_t* cmd = json_object_get(req, "cmd");
	const Command* command = json_is_string(cmd) ? findCommand(json_string_value(cmd)) : nullptr;
	if (!json_is_string(cmd))
		out = errorReply("missing cmd");
	else if (!command)
		out = errorReply("unknown cmd");
	else if (command->thread == Thread::MAIN)
		// A cancelled task never runs, and a started one is waited for, so req outlives it.
		out = server.runOnMain([command, req] { return run(command, req); }, MAIN_TIMEOUT);
	else
		out = run(command, req);
	json_decref(req);
	return out;
}

} // namespace brumista
