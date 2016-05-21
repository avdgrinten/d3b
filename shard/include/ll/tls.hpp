
#include <botan/x509cert.h>
#include <botan/pk_keys.h>
#include <botan/tls_server.h>
#include <botan/tls_alert.h>
#include <botan/tls_session.h>
#include <botan/auto_rng.h>

namespace Ll {

class TlsServer {
public:
	class Channel {
	public:
		Channel(TlsServer *server);

		void readRaw(int size, const char *buffer);
		void writeTls(int size, const char *buffer);

		void setWriteRawCallback(Async::Callback<void(int, const char *)> callback);
		void setReadTlsCallback(Async::Callback<void(int, const char *)> callback);
	
	private:
		void onWriteRaw(const Botan::byte buffer[], size_t size);
		void onReadTls(const Botan::byte buffer[], size_t size);
		void onAlert(Botan::TLS::Alert alert, const Botan::byte buffer[], size_t size);
		bool onHandshake(const Botan::TLS::Session &session);

		Async::Callback<void(int, const char *)> p_writeRawCallback;
		Async::Callback<void(int, const char *)> p_readTlsCallback;

		TlsServer *p_server;
		Botan::TLS::Server p_botanServer;
	};
	
	TlsServer();

private:
	class Credentials : public Botan::Credentials_Manager {
	public:
		Credentials(Botan::RandomNumberGenerator &rng);

		virtual std::vector<Botan::X509_Certificate> cert_chain(const std::vector<std::string> &cert_key_types,
				const std::string& type, const std::string& context);
		virtual Botan::Private_Key* private_key_for(const Botan::X509_Certificate& cert,
				const std::string& type, const std::string& context);
	
	private:
		Botan::X509_Certificate p_certificate;
		Botan::Private_Key *p_privateKey;
	};

	Botan::AutoSeeded_RNG p_rng;
	Botan::TLS::Policy p_policy;
	Botan::TLS::Session_Manager_In_Memory p_sessionManager;
	Credentials p_credentials;
};

} // namespace Ll

