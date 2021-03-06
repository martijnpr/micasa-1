#pragma once

#include <thread>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

#include "Worker.h"
#include "Utils.h"

extern "C" {
	#include "mongoose.h"
} // extern "C"

#define MG_EV_SHUTDOWN 999

namespace micasa {
	
	class Network final : public Worker {
		
	public:
		typedef std::function<bool( mg_connection* connection_, int event_, void* data_ )> t_handler;
		
		enum class Event: unsigned short {
			SUCCESS = 1,
			FAILURE,
			DATA,
			TIMER,
			CLOSE
		}; // enum class Event
		ENUM_UTIL( Event );

		Network();
		~Network();
		friend std::ostream& operator<<( std::ostream& out_, const Network* ) { out_ << "Network"; return out_; }

		void start();
		void stop();
		
		typedef std::function<void( mg_connection* connection_, int event_, void* data_ )> t_callback;
		
		mg_connection* bind( const std::string port_, const t_callback callback_ );
		mg_connection* bind( const std::string port_, const std::string cert_, const std::string key_, const t_callback callback_ );
		mg_connection* connect( const std::string uri_, const t_callback callback_ );

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ );
		
	private:
		mg_mgr m_manager;
		std::thread m_worker;
		std::set<mg_connection*> m_bindings;
		
		Network::t_handler* _newHandler( const Network::t_callback callback_, const bool binding_ );
		
	}; // class WebServer
	
}; // namespace micasa
