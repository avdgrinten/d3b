
#include <map>
#include <algorithm>

#include "Os/Linux.h"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Transact.hpp"
#include "Db/Engine.hpp"

namespace Db {

std::tuple<Error, trid_type> TransactManager::transaction(StateCallback callback) {
	trid_type trid = p_nextTrid;
	p_nextTrid++;
	
	TransactInfo *info = new TransactInfo(callback);
	p_transactMap.insert(std::make_pair(trid, info));
	return std::make_tuple(Error(true), trid);
}

void TransactManager::submit(trid_type trid, const Proto::Update &update) {
	TransactInfo *tr_info = p_getTransact(trid);
	if(tr_info == nullptr)
		throw std::runtime_error("Illegal transaction id");
	if(tr_info->state != kIsInitial)
		throw std::runtime_error("Illegal state for submit()");
	
	UpdateInfo *up_info = new UpdateInfo(trid, update);

	if(!up_info->desc.has_storage_idx()) {
		const std::string &name = up_info->desc.storage_name();
		int index = p_engine->getStorage(name);
		up_info->desc.set_storage_idx(index);
	}
	if(up_info->desc.action() == Proto::kActInsert) {
		id_type id;
		p_engine->allocId(up_info->desc.storage_idx(), &id);
		up_info->desc.set_id(id);
	}

	tr_info->updateStack.push_back(up_info);
}

void TransactManager::fix(trid_type trid) {
	TransactInfo *tr_info = p_getTransact(trid);
	if(tr_info == nullptr)
		throw std::runtime_error("Illegal transaction id");
	if(tr_info->state != kIsInitial)
		throw std::runtime_error("Illegal state for fix()");
	
	tr_info->state = kIsQueued;
	tr_info->fixSuccess = 0;
	
	for(auto it = tr_info->updateStack.begin();
			it != tr_info->updateStack.end(); ++it) {
		UpdateInfo *up_info = *it;
		assert(up_info->desc.has_id());
		
		id_type id = up_info->desc.id();
		ObjectInfo *obj_info = p_allocObject(id);
		assert(!obj_info->touched);
		obj_info->fixQueue.push_back(up_info);
		obj_info->touched = true;
		p_touchedStack.push_back(id);
	}
	
	p_processTouched();
}

void TransactManager::commit(trid_type trid) {
	TransactInfo *tr_info = p_getTransact(trid);
	if(tr_info == nullptr)
		throw std::runtime_error("Illegal transaction id");
	if(tr_info->state != kIsFixed)
		throw std::runtime_error("Illegal state for commit()");
	
	for(auto it = tr_info->updateStack.begin();
			it != tr_info->updateStack.end(); ++it) {
		UpdateInfo *up_info = *it;
		assert(up_info->desc.has_id());
		
		id_type id = up_info->desc.id();
		ObjectInfo *obj_info = p_getObject(id);
		assert(obj_info != nullptr);
		assert(obj_info->fixState &&
				obj_info->fixQueue.front() == up_info);
		
		/* commit the fixed object to the database */
		if(up_info->desc.action() == Proto::kActInsert) {
			p_engine->insert(up_info->desc.storage_idx(), id,
					up_info->desc.buffer().c_str(),
					up_info->desc.buffer().size());
		}else throw std::logic_error("commit(): Illegal action!");
	}
	
	p_unfixTransact(trid);
	tr_info->callback(kStCommit);
	p_processTouched();
}

void TransactManager::rollback(trid_type trid) {
	TransactInfo *tr_info = p_getTransact(trid);
	if(tr_info == nullptr)
		throw std::runtime_error("Illegal transaction id");
	if(tr_info->state != kIsQueued && tr_info->state != kIsFixed)
		throw std::runtime_error("Illegal state for rollback()");

	p_unfixTransact(trid);
	tr_info->callback(kStRollback);
	p_processTouched();
}

void TransactManager::cleanup(trid_type trid) {
	TransactInfo *tr_info = p_getTransact(trid);
	if(tr_info == nullptr)
		throw std::runtime_error("Illegal transaction id");
	if(tr_info->state != kIsInitial)
		throw std::runtime_error("Illegal state for rollback()");

	tr_info->callback(kStCleanup);
	p_releaseTransact(trid);
}

void TransactManager::p_processTouched() {
	while(!p_touchedStack.empty()) {
		id_type id = p_touchedStack.back();
		p_touchedStack.pop_back();

		ObjectInfo *obj_info = p_getObject(id);
		assert(obj_info->touched);
		assert(!obj_info->fixState);
		obj_info->touched = false;
		
		if(!obj_info->fixQueue.empty()) {
			UpdateInfo *up_info = obj_info->fixQueue.front();
			TransactInfo *tr_info = p_getTransact(up_info->trid);

			/* compute the actual fixed object */
			bool success = true;
			if(success) {
				obj_info->fixState = true;
				tr_info->fixSuccess++;
				
				if(tr_info->fixSuccess == tr_info->updateStack.size()) {
					assert(tr_info->state == kIsQueued);
					tr_info->state = kIsFixed;
					tr_info->callback(kStFixSuccess);
				}
			}else{
				p_unfixTransact(up_info->trid);
				tr_info->callback(kStFixFailure);
			}
		}else{
			p_releaseObject(id);
		}
	}
}

void TransactManager::p_unfixTransact(trid_type trid) {
	TransactInfo *tr_info = p_getTransact(trid);
	assert(tr_info != nullptr);
	assert(tr_info->state == kIsQueued || tr_info->state == kIsFixed);

	for(auto it = tr_info->updateStack.begin();
			it != tr_info->updateStack.end(); ++it) {
		UpdateInfo *up_info = *it;
		assert(up_info->desc.has_id());
		
		id_type id = up_info->desc.id();
		ObjectInfo *obj_info = p_getObject(id);
		assert(obj_info != nullptr);
		assert(!obj_info->fixQueue.empty());
		
		if(obj_info->fixState &&
				obj_info->fixQueue.front() == up_info) {
			obj_info->fixState = false;
			obj_info->fixQueue.pop_front();
		}else{
			auto queue_it = std::find(obj_info->fixQueue.begin(),
					obj_info->fixQueue.end(), up_info);
			assert(queue_it != obj_info->fixQueue.end());
			obj_info->fixQueue.erase(queue_it);
		}
		if(!obj_info->touched) {
			obj_info->touched = true;
			p_touchedStack.push_back(id);
		}
	}

	tr_info->state = kIsInitial;
}

TransactManager::TransactInfo *TransactManager::p_getTransact(trid_type trid) {
	auto info_it = p_transactMap.find(trid);
	if(info_it == p_transactMap.end())
		return nullptr;
	return info_it->second;
}
void TransactManager::p_releaseTransact(trid_type trid) {
	auto info_it = p_transactMap.find(trid);
	assert(info_it != p_transactMap.end());

	TransactInfo *info = info_it->second;
	assert(info->state == kIsInitial);
	for(auto it = info->updateStack.begin();
			it != info->updateStack.end(); ++it)
		delete *it;
	delete info;
	p_transactMap.erase(info_it);
}

TransactManager::ObjectInfo *TransactManager::p_allocObject(id_type id) {
	auto info_it = p_objectMap.find(id);
	if(info_it != p_objectMap.end())
		return info_it->second;

	ObjectInfo *obj_info = new ObjectInfo;
	p_objectMap.insert(std::make_pair(id, obj_info));
	return obj_info;
}
TransactManager::ObjectInfo *TransactManager::p_getObject(id_type id) {
	auto info_it = p_objectMap.find(id);
	if(info_it == p_objectMap.end())
		return nullptr;
	return info_it->second;
}
void TransactManager::p_releaseObject(id_type id) {
	auto info_it = p_objectMap.find(id);
	assert(info_it != p_objectMap.end());

	ObjectInfo *info = info_it->second;
	assert(!info->fixState && info->fixQueue.empty());
	delete info;
	p_objectMap.erase(info_it);
}

};

