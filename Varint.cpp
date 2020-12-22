#include "Varint.h"

static const int g_maxBits = 8;

UTXO::UTXO(Varint<std::vector<unsigned char>>& inputValue)
{
	m_inputValue = inputValue;
	setHeight();
	setAmount();
	setScriptPubKey();
}

UTXO::UTXO(const UTXO& src)
{
	m_txid = src.m_txid;	
	m_vout = src.m_vout;
	m_height = src.m_height;
	m_coinbase = src.m_coinbase;
	m_scriptPubKey = src.m_scriptPubKey;
	m_amount = src.m_amount;
	m_scriptType = src.m_scriptType;
	m_inputValue = src.m_inputValue;
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
	m_inputValue.remainingBytesFromIndex((size_t) m_scriptStart, in);
	
	// There are 6 special script types. Outside these 6, the entire script is present
//	scriptType = nSize[0] < 6 ? nSize[0] : -1;
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
		break;
		assert(m_scriptType == 6);
		
		// Convert nSize (vector of bytes)
		auto customScriptSize = static_cast<size_t>(utils::toUint64(nSize) - 6);
		if (customScriptSize) {
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

std::string UTXO::getPublicKey() const {
	std::string scriptPubKeyStr;
	utils::bytesToHexstring(m_scriptPubKey, scriptPubKeyStr);
	return scriptPubKeyStr;
}

uint64_t UTXO::getAmount() const {
	return m_amount;
}

template <class T>
Varint<T>::Varint() {
	m_startIndexes = {};
	inputBytes = {};
}

template <class T>
Varint<T>::Varint(T input) : inputBytes(input)
{
	setStartIndexes();
	processBytes();
}

template <class T>
void Varint<T>::getInputBytes(std::vector<unsigned char>& v)
{
	v = inputBytes;
}

template <class T>
void Varint<T>::setStartIndexes()
{
	m_startIndexes.push_back(0);
	for (size_t i = 0; i < inputBytes.size(); i++) {
		if ((inputBytes[i] & 0x80) == 0 && i != (inputBytes.size() - 1)) {
			m_startIndexes.push_back(i + 1);
		}
	}
}

template <class T>
void Varint<T>::remainingBytesFromIndex(size_t startIndex, std::vector<unsigned char>& result)
{
	for (size_t i = startIndex; i < inputBytes.size(); i++) {
		result.push_back(inputBytes[i]);
	}
}

template <class T>
int Varint<T>::decode(size_t start, std::vector<unsigned char>& result)
{
	std::vector<unsigned char> tmp;
	size_t nBytes = 0;
	for (size_t i = m_startIndexes[start]; i < inputBytes.size(); i++) {
		nBytes++;
		if ((inputBytes[i] & 0x80) == 0) {
			tmp.push_back(inputBytes[i] & (0xFF >> 1));
			break;
		}
		tmp.push_back((inputBytes[i] & (0xFF >> 1)) + 1);
	}
	base128To256(tmp, result);

	size_t finalIndex = m_startIndexes[start] + nBytes;
	return finalIndex < inputBytes.size() - 1 ? (int)finalIndex : -1;
}

template <class T>
void Varint<T>::base128To256(const T& b128, T& b256)
{
	size_t nBytes = b128.size();
	unsigned char current = 0, carry = 0;
	for (const auto& b : b128) {
		size_t offset = nBytes - 1;
		// Set least significant bits in current, then shift into correct position 
		current = (b & (0xFF << offset)) >> offset;
		// Most significant bits are the last iterations carry
		current |= carry;
		b256.push_back(current);
		
		// Set carry bits and shift into correct position for next iteration.
		carry = b & ~(0xFF << offset); // nBytes == 3, mask is 0000 0011
		carry <<= (g_maxBits - offset);
		nBytes--;
	}
}
	
template <class T>
void Varint<T>::processBytes()
{
	std::vector<unsigned char> tmp;
	size_t nBytes = 0;
	for (const auto& el : inputBytes) {
		nBytes++;
		// Most significant bit set zero denotes last byte
		if (el <= 0x7F) {
//		if ((el & (1 << 7)) == 0) {
			tmp.push_back(el & (0xff >> 1));
			break;
		}
		// For all bytes except the last, save the last 7 bits + 1
		tmp.push_back((el & (0xff >> 1)) + 1);
	}

	unsigned char current = 0, carry = 0;
	for (const auto& b : tmp) {
		size_t offset = nBytes - 1;
		// Set least significant bits in current, then shift into correct position 
		current = (b & (0xFF << offset)) >> offset;
		// Most significant bits are the last iterations carry
		current |= carry;
		m_result.push_back(current);
		
		// Set carry bits and shift into correct position for next iteration.
		carry = b & ~(0xFF << offset); // nBytes == 3, mask is 0000 0011
		carry <<= (g_maxBits - offset);
		nBytes--;
	}
	shiftAllBytesRight(1);
}

template <class T>
void Varint<T>::outputResult()
{
	for (const auto& el : m_result) {
		std::cout << (int) el << " ";
	}
	std::cout << "\n";
}

template <class T>
void Varint<T>::shiftAllBytesRight(size_t shift)
{
	for (auto& byte : m_result) {
		byte >>= shift;
	}
}	

template <class T>
void Varint<T>::shiftAllBytesRight(T& bytes, size_t shift)
{
	for (auto& byte : bytes) {
		byte >>= shift;
	}
}	

template class Varint<std::vector<unsigned char>>;