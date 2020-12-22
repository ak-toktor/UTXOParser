#include <iostream>
#include <stdexcept>
#include <memory>
#include <fstream>

#include "DbWrapper.h"
#include "utils.h"
#include "DbWrapperException.h"

DBWrapper::DBWrapper(const std::filesystem::path& dbName) 
	: m_dbName(dbName)
{
	m_options = leveldb::Options();
	openDB();
	setObfuscationKey();
}

DBWrapper::~DBWrapper()
{
	delete m_db;
}

void DBWrapper::setObfuscationKey()
{
	m_obfuscationKeyKey = {0x0e, 0x00};
	m_obfuscationKeyKey += "obfuscate_key";
	std::string obfuscationKeyString;
	read(m_obfuscationKeyKey, obfuscationKeyString);
	utils::stringToHexBytes(obfuscationKeyString, m_obfuscationKey);
	m_obfuscationKey.erase(m_obfuscationKey.begin());
}

void DBWrapper::openDB()
{
	if (m_dbName.empty()) {
		throw std::invalid_argument{"No database specified"};
	}

	// Check that the provided path exists
	std::filesystem::path p = m_dbName;
	p.append("LOCK");
	if (!std::filesystem::exists(p)) {
		throw DbWrapperException("The provided path is not a LevelDB database.");
	}
	
	leveldb::Status status = leveldb::DB::Open(m_options, m_dbName.string(), &m_db);
	if (!status.ok()) {
		throw DbWrapperException("Can't open the specified database.");
	}
	m_readOptions.verify_checksums = true;
}

void DBWrapper::read(const std::string& key, std::string& val)
{
	leveldb::Status status = m_db->Get(m_readOptions, key, &val);
	if (!m_status.ok()) {
		std::string errMsg("Error reading obfuscation key");
		errMsg += ". ";
		errMsg += m_status.ToString();
		throw DbWrapperException(errMsg.c_str());
	}
}

void DBWrapper::dumpAllUTXOs(const std::filesystem::path& path) {
	std::unique_ptr<leveldb::Iterator> it(m_db->NewIterator(m_readOptions));
	std::ofstream file(path);
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		const size_t keySize = 33;
		auto key = it->key();
		const char* keyData = key.data();
		if (keyData[0] == 'C') { // from the https://en.bitcoin.it/wiki/Bitcoin_Core_0.11_(ch_2):_Data_Storage
			BytesVec deObfuscatedValue;
			deObfuscatedValue.reserve(it->value().size());
			deObfuscate(it->value(), deObfuscatedValue);
			Varint v(deObfuscatedValue);
			BytesVec txid;
			assert(key.size() > keySize);
			txid.insert(txid.begin(), keyData + 1, keyData + keySize);
			utils::switchEndianness(txid);
			UTXO u(v);
			u.setTXID(txid);
			if (u.getAmount()) {
				const auto& pubKey = u.getPublicKey();
				std::string scriptPubKeyStr;
				utils::bytesToHexstring(pubKey, scriptPubKeyStr);
				file << scriptPubKeyStr << "," << u.getAmount() << std::endl;
			}
		}
	}
	if (!it->status().ok()) {
		throw DbWrapperException("Can't parse all UTXOS");
	}
}
