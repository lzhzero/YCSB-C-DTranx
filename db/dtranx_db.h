/*
 * Author: Ning Gao(nigo9731@colorado.edu)
 */

#ifndef YCSB_C_DTRANX_DB_H_
#define YCSB_C_DTRANX_DB_H_

#include "core/db.h"

#include <mutex>
#include <iostream>
#include <fstream>
#include "DTranx/Client/ClientTranx.h"

namespace ycsbc {

using std::cout;
using std::endl;

class DtranxDB {
public:
	typedef std::pair<std::string, std::string> KVPair;
	//Not using unordered_map for clients because incompatibility between g++4.6 and g++4.9
	void Init(std::vector<std::string> ips,
			std::vector<DTranx::Client::Client*> clients) {
		std::lock_guard<std::mutex> lock(mutex_);
		cout << "A new thread begins working." << endl;
		/*
		 * do not create context in each thread, since that creates too many file descriptors
		 */
		//std::shared_ptr<zmq::context_t> context = std::make_shared<
		//		zmq::context_t>(1);
		clientTranx = new DTranx::Client::ClientTranx("60000", NULL, ips);
		for (size_t i = 0; i < clients.size(); ++i) {
			clientTranx->InitClients(ips[i], clients[i]);
		}
	}

	void Close() {
		if (clientTranx) {
			delete clientTranx;
		}
	}

	int Read(std::vector<std::string> keys) {
		std::lock_guard<std::mutex> lock(mutex_);
		std::string value;
		DTranx::Storage::Status status;

		for (auto it = keys.begin(); it != keys.end(); ++it) {
			status = clientTranx->Read(const_cast<std::string&>(*it), value);
			if (status == DTranx::Storage::Status::OK) {
			} else {
				clientTranx->Clear();
				std::cout << "reading " << (*it) << " failure" << std::endl;
				return DB::kErrorNoData;
			}
		}

		bool success = clientTranx->Commit();
		clientTranx->Clear();
		if (success) {
			return DB::kOK;
		} else {
			return DB::kErrorConflict;
		}
	}

	int ReadSnapshot(std::vector<std::string> keys) {
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto it = keys.begin(); it != keys.end(); ++it) {
			std::string value;
			DTranx::Storage::Status status = clientTranx->Read(
					const_cast<std::string&>(*it), value, true);
			if (status == DTranx::Storage::Status::OK) {
			} else if (status
					== DTranx::Storage::Status::SNAPSHOT_NOT_CREATED) {
				clientTranx->Clear();
				std::cout << "reading " << (*it) << " failure, no snapshot"
						<< std::endl;
				return DB::kErrorNoData;
			} else {
				clientTranx->Clear();
				std::cout << "reading " << (*it) << " failure" << std::endl;
				return DB::kErrorNoData;
			}
		}
		bool success = clientTranx->Commit();
		clientTranx->Clear();
		if (success) {
			return DB::kOK;
		} else {
			return DB::kErrorConflict;
		}
	}

	int Write(std::vector<KVPair> writes) {
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto it = writes.begin(); it != writes.end(); ++it) {
			clientTranx->Write(it->first, it->second);
			cout << "update write key: " << it->first << " and the value is "
					<< it->second << endl;
		}
		bool success = clientTranx->Commit();
		clientTranx->Clear();
		if (success) {
			return DB::kOK;
		} else {
			return DB::kErrorConflict;
		}
	}

	int Update(std::vector<std::string> reads, std::vector<KVPair> writes) {
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto it = reads.begin(); it != reads.end(); ++it) {
			std::string value;
			DTranx::Storage::Status status = clientTranx->Read(
					const_cast<std::string&>(*it), value);
			if (status == DTranx::Storage::Status::OK) {
			} else {
				clientTranx->Clear();
				return DB::kErrorNoData;
			}
		}
		for (auto it = writes.begin(); it != writes.end(); ++it) {
			clientTranx->Write(it->first, it->second);
		}
		bool success = clientTranx->Commit();
		clientTranx->Clear();
		if (success) {
			return DB::kOK;
		} else {
			return DB::kErrorConflict;
		}
	}

	int Insert(std::vector<KVPair> writes) {
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto it = writes.begin(); it != writes.end(); ++it) {
			cout << "insert write key: " << it->first << " and the value is "
					<< it->second << endl;
			clientTranx->Write(it->first, it->second);
		}
		bool success = clientTranx->Commit();
		clientTranx->Clear();
		if (success) {
			cout << "commit success" << endl;
			return DB::kOK;
		} else {
			cout << "commit failure" << endl;
			return DB::kErrorConflict;
		}
	}
private:
	std::mutex mutex_;
	DTranx::Client::ClientTranx *clientTranx;
};

} // ycsbc

#endif /* YCSB_C_DTRANX_DB_H_ */
