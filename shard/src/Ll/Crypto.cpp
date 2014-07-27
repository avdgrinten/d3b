
#include "Ll/Crypto.hpp"

#include <botan/md5.h>

namespace Ll {

void Md5::hash(int size, const char *data, char *hash) {
	Botan::MD5 algorithm;
	algorithm.update((const Botan::byte *)data, size);
	Botan::secure_vector<Botan::byte> result = algorithm.final();
	memcpy(hash, result.data(), 16);
}

} // namespace Ll

