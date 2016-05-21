
#include <iostream>

#include "Async.hpp"

#include "Ll/Tls.hpp"

#include <botan/rsa.h>
#include <botan/x509self.h>
#include <botan/pkcs8.h>

namespace Ll {

TlsServer::TlsServer() : p_sessionManager(p_rng), p_credentials(p_rng) {
}

// --------------------------------------------------------
// TlsServer::Channel
// --------------------------------------------------------

TlsServer::Channel::Channel(TlsServer *server) : p_server(server),
		p_botanServer(ASYNC_MEMBER(this, &Channel::onWriteRaw),
			ASYNC_MEMBER(this, &Channel::onReadTls),
			ASYNC_MEMBER(this, &Channel::onAlert),
			ASYNC_MEMBER(this, &Channel::onHandshake),
			server->p_sessionManager, server->p_credentials,
				server->p_policy, server->p_rng) { }

void TlsServer::Channel::readRaw(int size, const char *buffer) {
	p_botanServer.received_data((const Botan::byte *)buffer, size);
}
void TlsServer::Channel::writeTls(int size, const char *buffer) {
	p_botanServer.send((const Botan::byte *)buffer, size);
}

void TlsServer::Channel::setWriteRawCallback(Async::Callback<void(int, const char*)> callback) {
	p_writeRawCallback = callback;
}
void TlsServer::Channel::setReadTlsCallback(Async::Callback<void(int, const char*)> callback) {
	p_readTlsCallback = callback;
}

void TlsServer::Channel::onWriteRaw(const Botan::byte buffer[], size_t size) {
	p_writeRawCallback(size, (const char *)buffer);
}
void TlsServer::Channel::onReadTls(const Botan::byte buffer[], size_t size) {
	p_readTlsCallback(size, (const char *)buffer);
}
void TlsServer::Channel::onAlert(Botan::TLS::Alert alert, const Botan::byte buffer[], size_t size) {
	// ignore tls alerts
}
bool TlsServer::Channel::onHandshake(const Botan::TLS::Session &session) {
	return true;
}

// --------------------------------------------------------
// TlsServer::Credentials
// --------------------------------------------------------

TlsServer::Credentials::Credentials(Botan::RandomNumberGenerator &rng)
	: p_certificate("server.crt"), p_privateKey(Botan::PKCS8::load_key("server.key", rng)) { }

std::vector<Botan::X509_Certificate> TlsServer::Credentials::cert_chain(const std::vector<std::string> &cert_key_types,
		const std::string& type, const std::string& context) {
	if(std::find(cert_key_types.begin(), cert_key_types.end(), p_privateKey->algo_name())
			!= cert_key_types.end())
		return std::vector<Botan::X509_Certificate> { p_certificate };
	return std::vector<Botan::X509_Certificate>();
}

Botan::Private_Key* TlsServer::Credentials::private_key_for(const Botan::X509_Certificate& cert,
			const std::string& type, const std::string& context) {
	if(cert == p_certificate)
		return p_privateKey;
	return nullptr;
}

} // namespace Ll

