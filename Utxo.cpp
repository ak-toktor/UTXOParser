#include "Utxo.h"

UTXO::UTXO(Varint<std::vector<unsigned char>>& inputValue)
{
	m_inputValue = inputValue;
	setHeight();
	setAmount();
	setScriptPubKey();
}

void UTXO::setHeight()
{
	std::vector<unsigned char> first;
	m_inputValue.decode(0, first);
	// The first Varint in this context represents the block height and coinbase status.
	// The final bit of the final byte in the decoded varint indicates coinbase status.
	m_coinbase = first.back() & 1 ? true : false;
	
	// The bits preceding the least significant bit need to be shifted right to yield the block height.
	// This is because the protocol reserves the least significant bit of this Varint as a boolean
	// indicator of the coinbase status of this UTXO.
	Varint<std::vector<unsigned char>>::shiftAllBytesRight(first, 1);

	// Convert block height represented by first (a base 256 encoded vector of bytes) to a string 
	// as an intermediate to avoid expensive math operations.
	std::string blockHeight;
	utils::bytesToDecimal(first, blockHeight);

	// Convert to a uint64_t type
	char* pEnd;
	m_height = strtol(blockHeight.c_str(), &pEnd, 10);
}

void UTXO::setAmount()
{
	// The second Varint in the stored database value represents the (compressed) amount.
	std::vector<unsigned char> rawAmount;

	// The decode() method returns the index of the following byte
	m_scriptStart = m_inputValue.decode(1, rawAmount);

	std::string amountDecimalStr;
	
	// Make a decimal string.
	utils::bytesToDecimal(rawAmount, amountDecimalStr);

	// Convert to a uint64_t type to allow for decompression and further numeric processing.
	char* pEnd;
	long int a = strtol(amountDecimalStr.c_str(), &pEnd, 10);
	m_amount = DecompressAmount((uint64_t)a);
}

/**
 * Build the scriptPubKey based on the DecompressScript function

 * See: https://github.com/bitcoin/bitcoin/blob/0.20/src/compressor.cpp#L95
 * */
void UTXO::setScriptPubKey()
{
	std::vector<unsigned char> in;
	
	// nSize is a Varint - nSize receives the decoded value
	std::vector<unsigned char> nSize;

	// scriptStart is the index of the first script byte
	m_scriptStart = m_inputValue.decode(2, nSize);	
	
	// The script is comprised of the remaining bytes in the retrieved value
	m_inputValue.remainingBytesFromIndex(m_scriptStart, in);
	
	// There are 6 special script types. Outside these 6, the entire script is present
	m_scriptType = nSize[0] < 6 ? nSize[0] : 6;

	switch(m_scriptType) {
	case 0x00: // P2PKH Pay to Public Key Hash
		m_scriptPubKey.resize(25);
		m_scriptPubKey[0] = OP_DUP;
		m_scriptPubKey[1] = OP_HASH160;
		m_scriptPubKey[2] = 0x14;
		memcpy(&m_scriptPubKey[3], &in[0], 20);
		m_scriptPubKey[23] = OP_EQUALVERIFY;
		m_scriptPubKey[24] = OP_CHECKSIG;
		break;
	case 0x01: // P2SH Pay to Script Hash
		m_scriptPubKey.resize(23);
		m_scriptPubKey[0] = OP_HASH160;
		m_scriptPubKey[1] = 0x14;
		memcpy(&m_scriptPubKey[2], &in[0], 20);
		m_scriptPubKey[22] = OP_EQUAL;
		break;
	case 0x02: // PKPK: upcoming data is a compressed public key (nsize makes up part of the public key) [y=even]
	case 0x03: // PKPK: upcoming data is a compressed public key (nsize makes up part of the public key) [y=odd]
		m_scriptPubKey.resize(35);
		m_scriptPubKey[0] = 33;
		m_scriptPubKey[1] = m_scriptType;
		memcpy(&m_scriptPubKey[2], in.data(), 32);
		m_scriptPubKey[34] = OP_CHECKSIG;
		break;
	case 0x04:// PKPK: upcoming data is an uncompressed public key
	case 0x05:// PKPK: upcoming data is an uncompressed public key
		break;
	default: // Upcoming script is custom, made up of nSize bytes
		assert(m_scriptType == 6);
		// Convert nSize (vector of bytes)
		auto customScriptSize = static_cast<size_t>(utils::toUint64(nSize) - 6);
		const int minimumScriptPubKeySize = 20;
		if (customScriptSize > minimumScriptPubKeySize) {
			m_scriptPubKey.resize(customScriptSize);
			memcpy(&m_scriptPubKey[0], in.data() + 1, customScriptSize);
		}

	}
}

// See function DecompressAmount from Bitcoin Core `src/compressor.cpp`: https://github.com/bitcoin/bitcoin/blob/0.20/src/compressor.cpp#L168
uint64_t UTXO::DecompressAmount(uint64_t x)
{
	// x = 0  OR  x = 1+10*(9*n + d - 1) + e  OR  x = 1+10*(n - 1) + 9
	if (x == 0)
		return 0;
	x--;
	// x = 10*(9*n + d - 1) + e
	int e = x % 10;
	x /= 10;
	uint64_t n = 0;
	if (e < 9) {
		// x = 9*n + d - 1
		int d = (x % 9) + 1;
		x /= 9;
		// x = n
		n = x*10 + d;
	} else {
		n = x+1;
	}
	while (e) {
		n *= 10;
		e--;
	}
	return n;
}

void UTXO::scriptDescription(size_t type, std::string& desc)
{
	/** Map scriptType to string **/
	static const char* scriptDescription[] = {
		"P2PKH",								// 0
		"P2SH",									// 1
		"P2PKa", // data is a compressed public key, y = even			// 2
		"P2PKb", // data is a compressed public key, y = odd			// 3
		"P2PKc", // Uncompressed pubkey, compressed for levelDB, y = even	// 4
		"P2PKd", // Uncompressed pubkey, compressed for levelDB, y = odd	// 5
	};

	if (type > 5) {
		desc = "Unknown script type.";
	} else {
		desc = scriptDescription[type];	
	}
}

void UTXO::getDbValue(std::string& dbValue)
{
	std::string s;
	std::vector<unsigned char> b;
	m_inputValue.getInputBytes(b);
	utils::bytesToHexstring(b, s);
	dbValue = s;
}

void UTXO::setTXID(const std::vector<unsigned char>& _txid)
{
	m_txid = _txid;
}

const std::vector<unsigned char>& UTXO::getPublicKey() const {
	return m_scriptPubKey;
}

uint64_t UTXO::getAmount() const {
	return m_amount;
}