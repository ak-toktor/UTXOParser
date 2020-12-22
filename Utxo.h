#pragma once
#include <vector>
#include <string>
#include "Varint.h"

class UTXO {
public:
	UTXO(const UTXO& src);
	UTXO(Varint<std::vector<unsigned char>>& inputValue);
	void scriptDescription(size_t type, std::string& desc);
	void getDbValue(std::string& dbValue);
	void setTXID(const std::vector<unsigned char>& txid);
    uint64_t getAmount() const;
    const std::vector<unsigned char>& getPublicKey() const;

private:	
	uint64_t DecompressAmount(uint64_t x);
	void setHeight();
	void setAmount();
	void setScriptPubKey();

 private:
	std::vector<unsigned char> m_vout;
	std::vector<unsigned char> m_txid; 
	Varint<std::vector<unsigned char>> m_inputValue;
	std::vector<unsigned char> m_scriptPubKey;
	bool m_coinbase;
	uint64_t m_height;
	uint64_t m_amount = 0;
	unsigned char m_scriptType;
   	int m_scriptStart;
};