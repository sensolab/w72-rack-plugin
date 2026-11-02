// Brumista's TCP server: 127.0.0.1 only, one client at a time, one JSON object per line.
// The socket loop owes its shape to limen (forsitan modulare, GPL-3.0-or-later, by gosub).
#pragma once
#include <rack.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace brumista {

// Replies, without the trailing newline.
std::string okReply(json_t* result);
std::string errorReply(const std::string& message);

struct Server {
	enum class Status { STOPPED, WAITING_FOR_PORT, LISTENING, CONNECTED };

	// handler turns one request line into one reply. It runs on the server thread.
	Server(int port, std::function<std::string(const std::string&)> handler);
	~Server();

	void start();
	// Never blocks, so it is safe while the engine holds its lock (onRemove).
	void signalStop();
	void join();

	Status getStatus() const { return status.load(); }
	int getPort() const { return port.load(); }
	// Any thread: the server drops its listener and any client, and binds the new port.
	void setPort(int newPort);

	// Server thread: queues fn for the main thread and waits for its reply. A task
	// still waiting when the timeout passes is cancelled and never runs; one the main
	// thread has already started is waited for, since it is already changing things.
	std::string runOnMain(std::function<std::string()> fn, std::chrono::milliseconds timeout);

	// Main thread, once a frame: runs the queued tasks.
	void drain();

private:
	struct Task;

	void loop();
	void serve(intptr_t client, int bound);

	std::atomic<int> port;
	std::function<std::string(const std::string&)> handler;
	std::thread thread;
	std::atomic<bool> stopping{false};
	std::atomic<Status> status{Status::STOPPED};

	std::mutex mutex;
	std::condition_variable cv;
	std::deque<std::shared_ptr<Task>> tasks;
};

} // namespace brumista
