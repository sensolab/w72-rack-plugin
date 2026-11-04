// Brumista's commands: the dispatch table and the handlers. Writes run on the main
// thread and record a Rack undo step.
// Shapes and error strings follow limen (forsitan modulare, GPL-3.0-or-later, by gosub),
// recorded in protocol/limen/commands.json; differences are noted in protocol/commands.json.
#include "BrumistaCommands.hpp"
#include "BrumistaShot.hpp"
#include "plugin.hpp"
#include <tag.hpp>
#include <patch.hpp>
#include <settings.hpp>
#include <cctype>

namespace brumista {

namespace {

// Commands that touch the engine or the scene run on the main thread, through the
// queue; the rest read plugin data that doesn't change while Rack runs.
enum class Thread { SERVER, MAIN };

// Commands that change the patch flash Brumista's CMD light full; the rest flash it dim.
enum class Effect { READ, EDIT };

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

// Undo. Every write records its step here. A batch opens a group so that its
// writes undo together; outside one, each write is its own step in Edit > Undo.
history::ComplexAction* undoGroup = nullptr;

void record(history::Action* h) {
	if (undoGroup)
		undoGroup->push(h);
	else
		APP->history->push(h);
}

// Takes back into the open group the step a Rack call pushed to the history itself.
void adoptLastStep(size_t sizeBefore) {
	history::State* hs = APP->history;
	if (!undoGroup || hs->actions.size() <= sizeBefore || hs->actionIndex != (int) hs->actions.size())
		return;
	history::Action* h = hs->actions.back();
	hs->actions.pop_back();
	hs->actionIndex--;
	undoGroup->push(h);
}

// A ComplexAction that may hold nothing, such as a move that changed no position.
void recordAll(history::ComplexAction* h) {
	if (h->isEmpty())
		delete h;
	else
		record(h);
}

enum class Mode { NEAREST, FORCE, SQUEEZE, STRICT };

// The request's placement mode, nearest when absent. Checked before anything
// changes; limen only notices a bad mode after it has created the module.
Mode requireMode(json_t* req) {
	json_t* j = json_object_get(req, "mode");
	if (!j)
		return Mode::NEAREST;
	std::string s = json_is_string(j) ? json_string_value(j) : "";
	if (s == "nearest")
		return Mode::NEAREST;
	if (s == "force")
		return Mode::FORCE;
	if (s == "squeeze")
		return Mode::SQUEEZE;
	if (s == "strict")
		return Mode::STRICT;
	throw CommandError("unknown mode (expected nearest, force, squeeze, or strict)");
}

// Moves mw to a grid position under one of Rack's placement policies; false when
// strict finds the place taken. The caller has called updateModuleOldPositions(),
// which squeeze starts from and the undo step is read against.
bool place(app::ModuleWidget* mw, math::Vec grid, Mode mode) {
	app::RackWidget* rack = APP->scene->rack;
	// Grid to pixels through setGridPosition, so the mapping is Rack's own.
	math::Vec saved = mw->box.pos;
	mw->setGridPosition(grid);
	math::Vec px = mw->box.pos;
	mw->box.pos = saved;
	switch (mode) {
		case Mode::NEAREST: rack->setModulePosNearest(mw, px); return true;
		case Mode::FORCE: rack->setModulePosForce(mw, px); return true;
		case Mode::SQUEEZE: rack->setModulePosSqueeze(mw, px); return true;
		case Mode::STRICT: return rack->requestModulePos(mw, px);
	}
	return false;
}

bool isHexColor(const std::string& s) {
	if ((s.size() != 7 && s.size() != 9) || s[0] != '#')
		return false;
	for (size_t i = 1; i < s.size(); i++)
		if (!std::isxdigit((unsigned char) s[i]))
			return false;
	return true;
}

std::string portName(const std::vector<engine::PortInfo*>& infos, int i) {
	if (i >= 0 && i < (int) infos.size() && infos[i])
		return infos[i]->name;
	return "";
}

json_t* posJson(app::ModuleWidget* mw) {
	math::Vec p = mw->getGridPosition();
	json_t* pos = json_object();
	json_object_set_new(pos, "x", json_integer(std::lround(p.x)));
	json_object_set_new(pos, "y", json_integer(std::lround(p.y)));
	return pos;
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
		json_object_set_new(obj, "pos", posJson(mw));
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
		json_object_set_new(obj, "default", json_real(pq->getDefaultValue()));
		// The tooltip's text, from the module's own code, which may override it. A
		// broken override costs this one field, not the reply.
		try {
			json_object_set_new(obj, "display", json_string((pq->getDisplayValueString() + pq->getUnit()).c_str()));
		}
		catch (...) {
		}
		if (engine::SwitchQuantity* sq = dynamic_cast<engine::SwitchQuantity*>(pq)) {
			if (!sq->labels.empty()) {
				json_t* labels = json_array();
				for (const std::string& l : sq->labels)
					json_array_append_new(labels, json_string(l.c_str()));
				json_object_set_new(obj, "labels", labels);
			}
		}
	}
	return obj;
}

// One port with its instant voltages. The engine writes the channel count and the
// voltages apart, so the count is read once and capped; at worst one value is stale.
template <class P>
json_t* portJson(const std::vector<engine::PortInfo*>& infos, P& port, int i) {
	int channels = math::clamp((int) port.getChannels(), 0, PORT_MAX_CHANNELS);
	json_t* voltages = json_array();
	for (int c = 0; c < channels; c++)
		json_array_append_new(voltages, json_real(port.getVoltage(c)));
	json_t* obj = json_object();
	json_object_set_new(obj, "id", json_integer(i));
	json_object_set_new(obj, "name", json_string(portName(infos, i).c_str()));
	json_object_set_new(obj, "channels", json_integer(channels));
	json_object_set_new(obj, "voltages", voltages);
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
	json_t* inputs = json_array();
	for (int i = 0; i < m->getNumInputs(); i++)
		json_array_append_new(inputs, portJson(m->inputInfos, m->inputs[i], i));
	json_t* outputs = json_array();
	for (int i = 0; i < m->getNumOutputs(); i++)
		json_array_append_new(outputs, portJson(m->outputInfos, m->outputs[i], i));
	json_t* obj = json_object();
	json_object_set_new(obj, "inputs", inputs);
	json_object_set_new(obj, "outputs", outputs);
	return obj;
}

// The state a module saves in the patch beyond its params, live: what its
// dataToJson() writes under "data" in a .vcv file.
json_t* getModuleData(json_t* req) {
	engine::Module* m = requireModule(requireInt(req, "id", "missing id"));
	json_t* data = m->dataToJson();
	json_t* obj = json_object();
	json_object_set_new(obj, "id", json_integer(m->id));
	json_object_set_new(obj, "data", data ? data : json_null());
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

json_t* setParam(json_t* req) {
	json_t* idJ = json_object_get(req, "id");
	json_t* paramJ = json_object_get(req, "param");
	json_t* valueJ = json_object_get(req, "value");
	if (!json_is_integer(idJ) || !json_is_integer(paramJ) || !json_is_number(valueJ))
		throw CommandError("missing id, param, or value");
	engine::Module* m = requireModule(json_integer_value(idJ));
	int64_t param = json_integer_value(paramJ);
	if (param < 0 || param >= m->getNumParams())
		throw CommandError("param not found");
	int p = (int) param;
	float oldValue = APP->engine->getParamValue(m, p);
	// The quantity clamps to the range and snaps stepped params, as a knob would.
	if (engine::ParamQuantity* pq = m->getParamQuantity(p))
		pq->setImmediateValue((float) json_number_value(valueJ));
	else
		APP->engine->setParamValue(m, p, (float) json_number_value(valueJ));
	float newValue = APP->engine->getParamValue(m, p);
	if (newValue != oldValue) {
		history::ParamChange* h = new history::ParamChange;
		h->moduleId = m->id;
		h->paramId = p;
		h->oldValue = oldValue;
		h->newValue = newValue;
		record(h);
	}
	return paramJson(m, p);
}

json_t* addModule(json_t* req) {
	json_t* pluginJ = json_object_get(req, "plugin");
	json_t* modelJ = json_object_get(req, "model");
	if (!json_is_string(pluginJ) || !json_is_string(modelJ))
		throw CommandError("missing plugin or model");
	json_t* xJ = json_object_get(req, "x");
	json_t* yJ = json_object_get(req, "y");
	bool placed = xJ || yJ;
	if (placed && !(json_is_number(xJ) && json_is_number(yJ)))
		throw CommandError("x and y must both be numbers");
	Mode mode = requireMode(req);
	plugin::Plugin* p = plugin::getPlugin(json_string_value(pluginJ));
	if (!p)
		throw CommandError("plugin not found");
	plugin::Model* mdl = p->getModel(json_string_value(modelJ));
	if (!mdl)
		throw CommandError("model not found");

	engine::Module* m = mdl->createModule();
	if (!m)
		throw CommandError("failed to create module");
	APP->engine->addModule(m);
	app::ModuleWidget* mw = nullptr;
	try {
		mw = mdl->createModuleWidget(m);
	}
	catch (...) {
		APP->engine->removeModule(m);
		delete m;
		throw;
	}
	if (!mw) {
		APP->engine->removeModule(m);
		delete m;
		throw CommandError("failed to create module widget");
	}

	// As Rack's own paste does: record positions, place the widget, then add it, so
	// one undo step holds the new module and any modules force or squeeze pushed aside.
	// Deleting a widget that was never added also removes and deletes its module.
	app::RackWidget* rack = APP->scene->rack;
	rack->updateModuleOldPositions();
	if (placed) {
		if (!place(mw, math::Vec(json_number_value(xJ), json_number_value(yJ)), mode)) {
			delete mw;
			throw CommandError("position occupied");
		}
	}
	else {
		// Next to an existing module, so it lands in view, as limen does.
		std::vector<app::ModuleWidget*> mws = rack->getModules();
		rack->setModulePosNearest(mw, mws.empty() ? math::Vec() : mws.front()->box.pos);
	}
	try {
		rack->addModule(mw);
	}
	catch (...) {
		delete mw;
		throw;
	}

	history::ComplexAction* h = new history::ComplexAction;
	h->name = "add module";
	h->push(rack->getModuleDragAction());
	history::ModuleAdd* add = new history::ModuleAdd;
	add->setModule(mw);
	h->push(add);
	record(h);

	json_t* obj = json_object();
	json_object_set_new(obj, "id", json_integer(m->id));
	json_object_set_new(obj, "pos", posJson(mw));
	json_object_set_new(obj, "hp", json_integer(std::lround(mw->getGridSize().x)));
	return obj;
}

json_t* removeModule(json_t* req) {
	int64_t id = requireInt(req, "id", "missing id");
	app::ModuleWidget* mw = APP->scene->rack->getModule(id);
	if (!mw)
		throw CommandError("module not found");
	// Commands run in Brumista's widget step(): removing a Brumista from there would
	// delete the widget mid-step and wait forever for the server thread serving this call.
	if (mw->getModel() == modelBrumista)
		throw CommandError("Brumista can't remove a Brumista module");
	// Rack's own Delete: cables, the gap squeeze closes, and the module, as one undo step.
	// It pushes to the history itself, so inside a batch the group adopts that step.
	size_t sizeBefore = APP->history->actions.size();
	mw->removeAction();
	adoptLastStep(sizeBefore);
	return json_null();
}

json_t* moveModule(json_t* req) {
	json_t* idJ = json_object_get(req, "id");
	json_t* xJ = json_object_get(req, "x");
	json_t* yJ = json_object_get(req, "y");
	if (!json_is_integer(idJ) || !json_is_number(xJ) || !json_is_number(yJ))
		throw CommandError("missing id, x, or y");
	Mode mode = requireMode(req);
	app::RackWidget* rack = APP->scene->rack;
	app::ModuleWidget* mw = rack->getModule(json_integer_value(idJ));
	if (!mw)
		throw CommandError("module not found");
	rack->updateModuleOldPositions();
	if (!place(mw, math::Vec(json_number_value(xJ), json_number_value(yJ)), mode))
		throw CommandError("position occupied");
	history::ComplexAction* h = rack->getModuleDragAction();
	h->name = "move module";
	recordAll(h);

	// Every mode but strict may land the module elsewhere, so report where it is.
	json_t* obj = json_object();
	json_object_set_new(obj, "id", json_integer(mw->module->id));
	json_object_set_new(obj, "pos", posJson(mw));
	return obj;
}

json_t* addCable(json_t* req) {
	json_t* omJ = json_object_get(req, "outputModule");
	json_t* opJ = json_object_get(req, "outputPort");
	json_t* imJ = json_object_get(req, "inputModule");
	json_t* ipJ = json_object_get(req, "inputPort");
	if (!json_is_integer(omJ) || !json_is_integer(opJ) || !json_is_integer(imJ) || !json_is_integer(ipJ))
		throw CommandError("missing outputModule, outputPort, inputModule, or inputPort");
	json_t* colorJ = json_object_get(req, "color");
	if (colorJ && !(json_is_string(colorJ) && isHexColor(json_string_value(colorJ))))
		throw CommandError("color must be a string \"#rrggbb\" or \"#rrggbbaa\"");

	engine::Module* outM = APP->engine->getModule(json_integer_value(omJ));
	if (!outM)
		throw CommandError("output module not found");
	engine::Module* inM = APP->engine->getModule(json_integer_value(imJ));
	if (!inM)
		throw CommandError("input module not found");
	int64_t op = json_integer_value(opJ);
	int64_t ip = json_integer_value(ipJ);
	if (op < 0 || op >= outM->getNumOutputs())
		throw CommandError("output port out of range");
	if (ip < 0 || ip >= inM->getNumInputs())
		throw CommandError("input port out of range");
	// A module may declare ports it has no jack for; a cable to one throws in
	// CableWidget::setCable, after the engine already holds it. Check the panel first.
	app::RackWidget* rack = APP->scene->rack;
	app::ModuleWidget* outMw = rack->getModule(outM->id);
	if (!outMw || !outMw->getOutput((int) op))
		throw CommandError("output port has no jack on the panel");
	app::ModuleWidget* inMw = rack->getModule(inM->id);
	if (!inMw || !inMw->getInput((int) ip))
		throw CommandError("input port has no jack on the panel");

	engine::Cable* cable = new engine::Cable;
	cable->outputModule = outM;
	cable->outputId = (int) op;
	cable->inputModule = inM;
	cable->inputId = (int) ip;
	APP->engine->addCable(cable);
	app::CableWidget* cw = new app::CableWidget;
	cw->setCable(cable);
	cw->color = colorJ ? color::fromHexString(json_string_value(colorJ)) : rack->getNextCableColor();
	rack->addCable(cw);

	history::CableAdd* h = new history::CableAdd;
	h->setCable(cw);
	record(h);

	json_t* obj = json_object();
	json_object_set_new(obj, "id", json_integer(cable->id));
	json_object_set_new(obj, "color", json_string(color::toHexString(cw->color).c_str()));
	return obj;
}

json_t* removeCable(json_t* req) {
	int64_t id = requireInt(req, "id", "missing id");
	app::CableWidget* cw = APP->scene->rack->getCable(id);
	if (!cw)
		throw CommandError("cable not found");
	history::CableRemove* h = new history::CableRemove;
	h->setCable(cw);
	APP->scene->rack->removeCable(cw);
	// Deleting the widget removes and deletes its engine cable.
	delete cw;
	record(h);
	return json_null();
}

bool isAbsolutePath(const std::string& p) {
#if defined ARCH_WIN
	if (p.size() >= 3 && std::isalpha((unsigned char) p[0]) && p[1] == ':' && (p[2] == '\\' || p[2] == '/'))
		return true;
	return p.size() >= 2 && (p[0] == '\\' || p[0] == '/') && p[0] == p[1];
#else
	return !p.empty() && p[0] == '/';
#endif
}

// The request's target file: absolute, in a folder that exists, ending in .vcv as
// Rack's Save as makes it, and not an existing file unless overwrite is set.
std::string requireTarget(json_t* req) {
	json_t* pathJ = json_object_get(req, "path");
	std::string path = json_is_string(pathJ) ? json_string_value(pathJ) : "";
	if (path.empty())
		throw CommandError("missing path");
	if (!isAbsolutePath(path))
		throw CommandError("path must be absolute");
	if (system::isDirectory(path))
		throw CommandError("path is a folder");
	if (system::getExtension(path) != ".vcv")
		path += ".vcv";
	if (!system::isDirectory(system::getDirectory(path)))
		throw CommandError("folder not found");
	if (system::exists(path)) {
		if (system::isDirectory(path))
			throw CommandError("path is a folder");
		if (!json_is_true(json_object_get(req, "overwrite")))
			throw CommandError("file exists; set overwrite to replace it");
	}
	return path;
}

// Writes the patch to path. The file records whether the patch had unsaved changes,
// so the history is marked saved first, as Rack's Save does; keepSaved false puts
// the mark back afterwards, for a copy. A failed save always puts it back.
void writePatch(const std::string& path, bool keepSaved) {
	history::State* hs = APP->history;
	int savedIndex = hs->savedIndex;
	hs->setSaved();
	try {
		APP->patch->save(path);
	}
	catch (Exception& e) {
		hs->savedIndex = savedIndex;
		throw CommandError(e.what());
	}
	catch (...) {
		hs->savedIndex = savedIndex;
		throw CommandError("failed to save patch");
	}
	if (!keepSaved)
		hs->savedIndex = savedIndex;
}

json_t* pathJson(const std::string& path) {
	json_t* obj = json_object();
	json_object_set_new(obj, "path", json_string(path.c_str()));
	return obj;
}

// Rack's Save, or with a path a copy that leaves the patch's own file and its
// unsaved state alone.
json_t* savePatch(json_t* req) {
	if (json_object_get(req, "path")) {
		std::string path = requireTarget(req);
		writePatch(path, false);
		return pathJson(path);
	}
	std::string path = APP->patch->path;
	if (path.empty())
		throw CommandError("patch has never been saved; use save_patch_as with a path");
	writePatch(path, true);
	return pathJson(path);
}

// Rack's Save as: the file becomes the patch's own and joins the recent patches.
json_t* savePatchAs(json_t* req) {
	std::string path = requireTarget(req);
	writePatch(path, true);
	APP->patch->path = path;
	settings::lastPatchDirectory = system::getDirectory(path);
	APP->patch->pushRecentPath(path);
	return pathJson(path);
}

// The window, not the patch: no undo step.
json_t* setFullscreen(json_t* req) {
	json_t* onJ = json_object_get(req, "on");
	if (!json_is_boolean(onJ))
		throw CommandError("missing on");
	APP->window->setFullScreen(json_is_true(onJ));
	json_t* obj = json_object();
	json_object_set_new(obj, "fullscreen", json_boolean(APP->window->isFullScreen()));
	return obj;
}

// Frames left in which stepView() repeats Rack's F4.
int zoomFrames = 0;

// Rack's F4. One call sets the zoom, but the offset is clamped against the scroll
// area's old size until it catches up in the next frames, so the fit is repeated
// there too, as limen does.
json_t* zoomToModules(json_t*) {
	APP->scene->rackScroll->zoomToModules();
	zoomFrames = 2;
	return json_null();
}

// The request's zoom, in pixels per Rack pixel, or def when absent.
float requireZoom(json_t* req, float def) {
	json_t* j = json_object_get(req, "zoom");
	if (!j)
		return def;
	double zoom = json_is_number(j) ? json_number_value(j) : 0;
	if (!(zoom >= 0.1 && zoom <= 4))
		throw CommandError("zoom must be a number from 0.1 to 4");
	return (float) zoom;
}

// Refused before anything is drawn: a large image holds up Rack's window while it encodes.
void checkShotSize(math::Vec size, float zoom) {
	int width = (int) std::ceil(size.x * zoom);
	int height = (int) std::ceil(size.y * zoom);
	if (std::max(width, height) > SHOT_MAX_SIDE)
		throw CommandError(string::f("the image would be %d x %d px, over %d px a side; lower zoom", width, height, SHOT_MAX_SIDE));
}

json_t* shotJson(const Shot& shot, float zoom) {
	json_t* obj = json_object();
	json_object_set_new(obj, "width", json_integer(shot.width));
	json_object_set_new(obj, "height", json_integer(shot.height));
	json_object_set_new(obj, "zoom", json_real(zoom));
	json_object_set_new(obj, "png", json_string(base64(shot.png).c_str()));
	return obj;
}

json_t* screenshotModule(json_t* req) {
	int64_t id = requireInt(req, "id", "missing id");
	float zoom = requireZoom(req, 2.f);
	app::ModuleWidget* mw = APP->scene->rack->getModule(id);
	if (!mw)
		throw CommandError("module not found");
	checkShotSize(mw->box.size, zoom);
	json_t* obj = shotJson(shotModule(mw, zoom), zoom);
	json_object_set_new(obj, "id", json_integer(id));
	return obj;
}

// The default zoom fits the rack's long side in this many pixels, about what a vision
// model reads at full detail.
const float RACK_FIT = 1568.f;

json_t* screenshotRack(json_t* req) {
	app::RackWidget* rack = APP->scene->rack;
	std::vector<app::ModuleWidget*> mws;
	json_t* idsJ = json_object_get(req, "ids");
	if (idsJ) {
		if (!json_is_array(idsJ) || json_array_size(idsJ) == 0)
			throw CommandError("ids must be a non-empty array of module ids");
		size_t i;
		json_t* idJ;
		json_array_foreach(idsJ, i, idJ) {
			if (!json_is_integer(idJ))
				throw CommandError("ids must be a non-empty array of module ids");
			app::ModuleWidget* mw = rack->getModule(json_integer_value(idJ));
			if (!mw)
				throw CommandError("module not found");
			mws.push_back(mw);
		}
	}
	else {
		mws = rack->getModules();
		if (mws.empty())
			throw CommandError("the patch has no modules");
	}
	math::Rect region = mws.front()->box;
	for (app::ModuleWidget* mw : mws)
		region = region.expand(mw->box);
	region = region.grow(math::Vec(RACK_GRID_WIDTH, RACK_GRID_WIDTH));

	float zoom = requireZoom(req, std::min(1.f, RACK_FIT / std::max(region.size.x, region.size.y)));
	checkShotSize(region.size, zoom);
	return shotJson(shotRack(region, zoom), zoom);
}

json_t* batch(json_t* req);

struct Command {
	const char* name;
	Thread thread;
	Effect effect;
	json_t* (*run)(json_t* req);
};

// One line per command, in protocol/commands.json's order; tools/check-commands.sh reads it.
const Command COMMANDS[] = {
	{"hello", Thread::SERVER, Effect::READ, hello},
	{"list_plugins", Thread::SERVER, Effect::READ, listPlugins},
	{"list_models", Thread::SERVER, Effect::READ, listModels},
	{"list_modules", Thread::MAIN, Effect::READ, listModules},
	{"get_module", Thread::MAIN, Effect::READ, getModule},
	{"get_module_info", Thread::MAIN, Effect::READ, getModuleInfo},
	{"list_ports", Thread::MAIN, Effect::READ, listPorts},
	{"list_params", Thread::MAIN, Effect::READ, listParams},
	{"get_param", Thread::MAIN, Effect::READ, getParam},
	{"get_module_data", Thread::MAIN, Effect::READ, getModuleData},
	{"list_cables", Thread::MAIN, Effect::READ, listCables},
	{"set_param", Thread::MAIN, Effect::EDIT, setParam},
	{"add_module", Thread::MAIN, Effect::EDIT, addModule},
	{"remove_module", Thread::MAIN, Effect::EDIT, removeModule},
	{"move_module", Thread::MAIN, Effect::EDIT, moveModule},
	{"add_cable", Thread::MAIN, Effect::EDIT, addCable},
	{"remove_cable", Thread::MAIN, Effect::EDIT, removeCable},
	{"batch", Thread::MAIN, Effect::EDIT, batch},
	{"save_patch", Thread::MAIN, Effect::READ, savePatch},
	{"save_patch_as", Thread::MAIN, Effect::READ, savePatchAs},
	{"set_fullscreen", Thread::MAIN, Effect::READ, setFullscreen},
	{"zoom_to_modules", Thread::MAIN, Effect::READ, zoomToModules},
	{"screenshot_module", Thread::MAIN, Effect::READ, screenshotModule},
	{"screenshot_rack", Thread::MAIN, Effect::READ, screenshotRack},
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

// The reply to one command of a batch, as the object it would have been on the wire.
json_t* runEntry(json_t* entry) {
	std::string out;
	json_t* cmd = json_is_object(entry) ? json_object_get(entry, "cmd") : nullptr;
	const Command* command = json_is_string(cmd) ? findCommand(json_string_value(cmd)) : nullptr;
	if (!json_is_object(entry))
		out = errorReply("command must be an object");
	else if (!json_is_string(cmd))
		out = errorReply("missing cmd");
	else if (!command)
		out = errorReply("unknown cmd");
	else if (command->run == batch)
		out = errorReply("batch cannot be nested");
	// An image inside a batch's results would reach the client as text.
	else if (command->run == screenshotModule || command->run == screenshotRack)
		out = errorReply("screenshots can't run inside a batch");
	else
		out = run(command, entry);
	json_error_t err;
	json_t* obj = json_loads(out.c_str(), 0, &err);
	return obj ? obj : json_pack("{s:b, s:s}", "ok", 0, "error", "malformed response");
}

// Runs the commands in order in one main-thread task, so no frame falls between
// them, and records their undo steps as one.
json_t* batch(json_t* req) {
	json_t* commands = json_object_get(req, "commands");
	if (!json_is_array(commands))
		throw CommandError("missing commands array");
	json_t* stopJ = json_object_get(req, "stopOnError");
	bool stopOnError = json_is_boolean(stopJ) ? json_is_true(stopJ) : true;

	// Closes the group on the way out, even past an exception, so the steps of the
	// commands that ran reach the history. Commands that ran before a failure stay
	// done, as in limen; they undo together.
	struct Grouping {
		history::ComplexAction* group = new history::ComplexAction;
		Grouping() {
			group->name = "batch";
			undoGroup = group;
		}
		~Grouping() {
			undoGroup = nullptr;
			recordAll(group);
		}
	};

	json_t* results = json_array();
	int failed = 0;
	int stopped = -1;
	{
		Grouping grouping;
		size_t i;
		json_t* entry;
		json_array_foreach(commands, i, entry) {
			json_t* one = runEntry(entry);
			bool ok = json_is_true(json_object_get(one, "ok"));
			json_array_append_new(results, one);
			if (!ok) {
				failed++;
				if (stopOnError) {
					stopped = (int) i;
					break;
				}
			}
		}
	}

	json_t* obj = json_object();
	json_object_set_new(obj, "count", json_integer((json_int_t) json_array_size(results)));
	json_object_set_new(obj, "failed", json_integer(failed));
	if (stopped >= 0)
		json_object_set_new(obj, "stopped", json_integer(stopped));
	json_object_set_new(obj, "results", results);
	return obj;
}

} // namespace

void stepView() {
	if (zoomFrames > 0) {
		zoomFrames--;
		APP->scene->rackScroll->zoomToModules();
	}
}

std::string handleLine(Server& server, const std::string& line, bool& edits) {
	edits = false;
	json_error_t err;
	json_t* req = json_loads(line.c_str(), 0, &err);
	if (!req)
		return errorReply("invalid JSON");
	std::string out;
	json_t* cmd = json_object_get(req, "cmd");
	const Command* command = json_is_string(cmd) ? findCommand(json_string_value(cmd)) : nullptr;
	if (command)
		edits = command->effect == Effect::EDIT;
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
