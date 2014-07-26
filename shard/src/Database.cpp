
#include <cstdint>
#include <string>
#include <iostream>
#include <boost/program_options.hpp>

#include "Async.hpp"
#include "Os/Linux.hpp"
#include "Ll/Tasks.hpp"

#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

#include "Db/FlexStorage.hpp"
#include "Db/JsView.hpp"

#include "Api/Server.hpp"

namespace po = boost::program_options;

int main(int argc, char **argv) {
	OS::LocalAsyncHost::set(new OS::LocalAsyncHost());
	LocalTaskQueue::set(new LocalTaskQueue(OS::LocalAsyncHost::get()));
	LocalTaskQueue::get()->process();

	WorkerThread worker1;
	WorkerThread worker2;

	TaskPool io_pool;
	io_pool.addWorker(worker1.getTaskQueue());
	io_pool.addWorker(worker2.getTaskQueue());
	
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
	
	Db::Engine engine(&io_pool);
	engine.setPath(opts["path"].as<std::string>());
	
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
		server.start();

		std::cout << "Server is running!" << std::endl;
		while(true)
			OS::LocalAsyncHost::get()->process();
	}
}

