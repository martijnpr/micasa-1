#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <mutex>
#include <chrono>
#include <map>
#include <vector>

#include "Worker.h"
#include "Network.h"

#include "json.hpp"

namespace micasa {

	using namespace nlohmann;

	class WebServer final : public Worker {

	public:
		enum Method {
			// v Retrieve all resources in a collection
			// v Retrieve a single resource
			GET     = 1,
			// v Retrieve all resources in a collection (header only)
			// v Retrieve a single resource (header only)
			HEAD    = 2,
			// v Create a new resource in a collection
			POST    = 4,
			// v Update a resource
			PUT     = 8,
			// v Update a resource
			PATCH   = 16,
			// v Delete a resource
			DELETE  = 32,
			// v Return available methods for resource or collection
			OPTIONS = 64
		}; // enum Method
		
		typedef std::function<void( const json& input_, const Method& method_, json& output_ )> t_callback;
		
		struct ResourceCallback {
			const std::string reference;
			const std::string uri;
			const unsigned int sort;
			const unsigned int rights;
			const unsigned int methods;
			const t_callback callback;
		}; // struct ResourceCallback
		
		// TODO extend from std::runtime_error?
		struct ResourceException {
			const unsigned int code;
			const std::string error;
			const std::string message;
		}; // struct ResourceException

		WebServer();
		~WebServer();
		friend std::ostream& operator<<( std::ostream& out_, const WebServer* ) { out_ << "WebServer"; return out_; }
		
		void addResourceCallback( const ResourceCallback& callback_ );
		void removeResourceCallback( const std::string& reference_ );

		void start();
		void stop();
		
	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) { return std::chrono::milliseconds( 1000 * 60 * 15 ); };
		
	private:
		std::map<std::string, std::vector<std::shared_ptr<ResourceCallback> > > m_resources;
		mutable std::mutex m_resourcesMutex;
		mutable std::mutex m_usersMutex;
		std::string m_publicKey;
		std::string m_privateKey;

		void _processHttpRequest( mg_connection* connection_, http_message* message_ );
		void _updateUserResourceHandlers();

	}; // class WebServer

}; // namespace micasa
