// Brumista's TCP server and main-thread queue.
// The socket loop owes its shape to limen (forsitan modulare, GPL-3.0-or-later, by gosub).
#include "BrumistaServer.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace brumista {

namespace {

const intptr_t NO_SOCKET = -1;
// A request longer than this closes the connection instead of growing the buffer.
const size_t MAX_LINE = 1 << 20;

#ifdef _WIN32
SOCKET toNative(intptr_t s) { return (SOCKET) s; }
void closeSocket(intptr_t s) { closesocket(toNative(s)); }
#else
int toNative(intptr_t s) { return (int) s; }
void closeSocket(intptr_t s) { close(toNative(s)); }
#endif

// Linux raises SIGPIPE on a send to a closed peer, which would take Rack down.
#ifdef MSG_NOSIGNAL
const int SEND_FLAGS = MSG_NOSIGNAL;
#else
const int SEND_FLAGS = 0;
#endif

// 1 when s can be read (or accepted), 0 on timeout, -1 on error. Polling keeps the
// thread able to stop: closing a socket from another thread doesn't reliably wake it.
int waitReadable(intptr_t s, int ms) {
	fd_set set;
	FD_ZERO(&set);
	FD_SET(toNative(s), &set);
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = ms * 1000;
	int n = select((int) s + 1, &set, nullptr, nullptr, &tv);
	return n > 0 ? 1 : n;
}

intptr_t openListener(int port) {
	intptr_t s = (intptr_t) socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
	if (toNative(s) == INVALID_SOCKET)
		return NO_SOCKET;
	// SO_REUSEADDR on Windows lets a second socket take a bound port; refuse that instead.
	BOOL yes = TRUE;
	setsockopt(toNative(s), SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*) &yes, sizeof(yes));
#else
	if (s < 0)
		return NO_SOCKET;
	int yes = 1;
	setsockopt(toNative(s), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons((uint16_t) port);
	if (bind(toNative(s), (sockaddr*) &addr, sizeof(addr)) != 0 || listen(toNative(s), 1) != 0) {
		closeSocket(s);
		return NO_SOCKET;
	}
	return s;
}

bool sendAll(intptr_t s, const std::string& data) {
	const char* p = data.data();
	size_t left = data.size();
	while (left > 0) {
		int n = send(toNative(s), p, (int) left, SEND_FLAGS);
		if (n <= 0)
			return false;
		p += n;
		left -= n;
	}
	return true;
}

std::string reply(bool ok, json_t* result, const std::string& message) {
	json_t* root = json_object();
	json_object_set_new(root, "ok", json_boolean(ok));
	if (ok)
		json_object_set_new(root, "result", result ? result : json_null());
	else
		json_object_set_new(root, "error", json_string(message.c_str()));
	char* s = json_dumps(root, JSON_COMPACT);
	std::string out = s ? s : "{\"ok\":false,\"error\":\"could not encode reply\"}";
	free(s);
	json_decref(root);
	return out;
}

} // namespace

std::string okReply(json_t* result) {
	return reply(true, result, "");
}

std::string errorReply(const std::string& message) {
	return reply(false, nullptr, message);
}

struct Server::Task {
	enum State { PENDING, RUNNING, DONE, CANCELLED };
	std::function<std::string()> fn;
	std::string reply;
	State state = PENDING;
};

Server::Server(int port, std::function<std::string(const std::string&)> handler)
	: port(port), handler(std::move(handler)) {}

Server::~Server() {
	signalStop();
	join();
}

void Server::start() {
	if (thread.joinable())
		return;
#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
	stopping = false;
	rack::Context* ctx = rack::contextGet();
	thread = std::thread([this, ctx] {
		rack::contextSet(ctx);
		loop();
	});
}

void Server::signalStop() {
	{
		std::lock_guard<std::mutex> lock(mutex);
		stopping = true;
	}
	cv.notify_all();
}

void Server::join() {
	if (!thread.joinable())
		return;
	thread.join();
#ifdef _WIN32
	WSACleanup();
#endif
}

std::string Server::runOnMain(std::function<std::string()> fn, std::chrono::milliseconds timeout) {
	auto task = std::make_shared<Task>();
	task->fn = std::move(fn);
	std::unique_lock<std::mutex> lock(mutex);
	tasks.push_back(task);
	cv.wait_for(lock, timeout, [&] { return task->state == Task::DONE || stopping; });
	if (task->state == Task::PENDING) {
		task->state = Task::CANCELLED;
		return errorReply(stopping ? "server stopping" : "timeout waiting for main thread");
	}
	// Started on the main thread: let it finish rather than report a change as failed.
	cv.wait(lock, [&] { return task->state == Task::DONE; });
	return task->reply;
}

void Server::drain() {
	std::deque<std::shared_ptr<Task>> batch;
	{
		std::lock_guard<std::mutex> lock(mutex);
		batch.swap(tasks);
	}
	for (auto& task : batch) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (task->state != Task::PENDING)
				continue;
			task->state = Task::RUNNING;
		}
		std::string out;
		try {
			out = task->fn();
		}
		catch (std::exception& e) {
			out = errorReply(std::string("internal error: ") + e.what());
		}
		catch (...) {
			out = errorReply("internal error");
		}
		{
			std::lock_guard<std::mutex> lock(mutex);
			task->reply = std::move(out);
			task->state = Task::DONE;
		}
		cv.notify_all();
	}
}

void Server::loop() {
	while (!stopping) {
		intptr_t listener = openListener(port);
		if (listener == NO_SOCKET) {
			// Another Brumista, or something else, has the port: try again each second.
			status = Status::WAITING_FOR_PORT;
			std::unique_lock<std::mutex> lock(mutex);
			cv.wait_for(lock, std::chrono::seconds(1), [&] { return stopping.load(); });
			continue;
		}
		INFO("Brumista listening on 127.0.0.1:%d", port);
		status = Status::LISTENING;
		while (!stopping) {
			int ready = waitReadable(listener, 100);
			if (ready == 0)
				continue;
			if (ready < 0)
				break;
			intptr_t client = (intptr_t) accept(toNative(listener), nullptr, nullptr);
#ifdef _WIN32
			if (toNative(client) == INVALID_SOCKET)
				continue;
#else
			if (client < 0)
				continue;
#endif
#ifdef SO_NOSIGPIPE
			int yes = 1;
			setsockopt(toNative(client), SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
			status = Status::CONNECTED;
			serve(client);
			closeSocket(client);
			status = Status::LISTENING;
		}
		closeSocket(listener);
	}
	status = Status::STOPPED;
}

void Server::serve(intptr_t client) {
	std::string buffer;
	char chunk[4096];
	while (!stopping) {
		int ready = waitReadable(client, 100);
		if (ready == 0)
			continue;
		if (ready < 0)
			return;
		int n = recv(toNative(client), chunk, sizeof(chunk), 0);
		if (n <= 0)
			return;
		buffer.append(chunk, n);
		size_t end;
		while ((end = buffer.find('\n')) != std::string::npos) {
			std::string line = buffer.substr(0, end);
			buffer.erase(0, end + 1);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty())
				continue;
			if (!sendAll(client, handler(line) + "\n"))
				return;
		}
		if (buffer.size() > MAX_LINE) {
			sendAll(client, errorReply("request too long") + "\n");
			return;
		}
	}
}

} // namespace brumista
