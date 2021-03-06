
#include <cstdint>
#include <string>
#include <iostream>
#include <boost/program_options.hpp>

#include <botan/init.h>
#include <v8.h>
#include <libplatform/libplatform.h>

#include "async.hpp"
#include "os/linux.hpp"
#include "ll/tasks.hpp"

#include "db/types.hpp"
#include "db/storage-driver.hpp"
#include "db/view-driver.hpp"
#include "db/engine.hpp"

#include "db/flex-storage.hpp"
#include "db/js-view.hpp"

#include "api/server.hpp"

namespace po = boost::program_options;

Botan::LibraryInitializer botanInit;

volatile bool running = true;

void shutdown() {
	running = false;
}

int main(int argc, char **argv) {
	v8::V8::InitializeICU();
	v8::V8::InitializeExternalStartupData(".");
	v8::V8::InitializePlatform(v8::platform::CreateDefaultPlatform());
	v8::V8::Initialize();

	OS::LocalAsyncHost::set(new OS::LocalAsyncHost());
	LocalTaskQueue::set(new LocalTaskQueue(OS::LocalAsyncHost::get()));
	LocalTaskQueue::get()->process();

	WorkerThread worker1;
	WorkerThread worker2;

	Db::globStorageRegistry.addDriver(new Db::FlexStorage::Factory);
	Db::globViewRegistry.addDriver(new Db::JsView::Factory);

	po::options_description desc("Options");
	desc.add_options()
		("create", "create database instance instead of loading from disc")
		("help", "print help message")
		("path", po::value<std::string>(), "root path for this database backend");
	
	po::variables_map opts;
	po::store(po::parse_command_line(argc, argv, desc), opts);

	if(opts.count("help")) {
		std::cout << desc << std::endl;
		return EXIT_SUCCESS;
	}

	if(!opts.count("path")) {
		std::cout << "No --path option specified" << std::endl;
		return EXIT_FAILURE;
	}
	
	Db::Engine engine;
	engine.setPath(opts["path"].as<std::string>());
	
	engine.getIoPool()->addWorker(worker1.getTaskQueue());
	engine.getIoPool()->addWorker(worker2.getTaskQueue());
	engine.getProcessPool()->addWorker(worker1.getTaskQueue());
	engine.getProcessPool()->addWorker(worker2.getTaskQueue());
	
	if(opts.count("create")) {
		engine.createConfig();
	}else{
		engine.loadConfig();
	}
	
	/* NOTE: for now all methods called during --create
		are synchronous. if we change that in the future
		we have to run osIntf->processIO() also in the
		--create case */
	engine.process();

	if(!opts.count("create")) {
		Api::Server server(&engine);
		server.setShutdownCallback(Async::Callback<void()>::make<&shutdown>());
		server.start();

		std::cout << "Server is running!" << std::endl;
		while(running)
			OS::LocalAsyncHost::get()->process();
	}

	worker1.shutdown();
	worker2.shutdown();
	worker1.getThread().join();
	worker2.getThread().join();

	std::cout << "Exited gracefully" << std::endl;
}

