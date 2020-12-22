#pragma once

#include <vector>
#include "leveldb/db.h"
#include "varint.h"
#include <filesystem>

using BytesVec = std::vector<unsigned char>;

class DBWrapper {
public:
	DBWrapper(const std::filesystem::path& dbName);
	~DBWrapper();
	void read(const std::string& key, std::string& val);
	void dumpAllUTXOs(const std::filesystem::path& path);

private:
	void setObfuscationKey();
	void openDB();
	template <typename T> 
	void deObfuscate(T bytes, BytesVec& plaintext);


private:
	std::string m_obfuscationKeyKey;
	BytesVec m_obfuscationKey;
	leveldb::Options m_options;
	std::filesystem::path m_dbName;
	leveldb::ReadOptions m_readOptions;
	leveldb::DB* m_db;
	leveldb::Status m_status;
};

template <typename T>
void DBWrapper::deObfuscate(T bytes, BytesVec& plaintext)
{
	for (size_t i = 0, j = 0; i < bytes.size(); i++) {
		plaintext.push_back(m_obfuscationKey[j++] ^ bytes[i]);
		if (j == m_obfuscationKey.size()) j = 0;	
	}
}
