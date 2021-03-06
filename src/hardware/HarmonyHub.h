#pragma once

#include "../Hardware.h"
#include "../Utils.h"

extern "C" {
	#include "mongoose.h"
} // extern "C"

#define HARMONY_HUB_BUSY_WAIT_MSEC  60000 // how long to wait for result
#define HARMONY_HUB_BUSY_BLOCK_MSEC 3000  // how long to block activies while waiting for result

namespace micasa {

	class HarmonyHub final : public Hardware {
		
	public:
		enum class ConnectionState: unsigned short {
			CLOSED = 1,
			CONNECTING,
			WAIT_FOR_CURRENT_ACTIVITY,
			WAIT_FOR_ACTIVITIES,
			IDLE,
		}; // enum class ConnectionState
		ENUM_UTIL( ConnectionState );
		
		HarmonyHub( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~HarmonyHub() { };
		
		void start() override;
		void stop() override;
		
		std::string getLabel() const throw() override { return "Harmony Hub"; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;
		
	private:
		mg_connection* m_connection;
		volatile ConnectionState m_connectionState = ConnectionState::CLOSED;
		std::string m_currentActivityId = "-1";
	
		void _disconnect( const std::string message_ );
		void _processConnection( const bool ready_ );
		
	}; // class HarmonyHub

}; // namespace micasa
