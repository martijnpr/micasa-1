#pragma once

#include <list>

#include "../Hardware.h"
#include "OpenZWave.h"

#include "Notification.h"

#define OPEN_ZWAVE_NODE_BUSY_WAIT_MSEC  30000 // how long to wait for result
#define OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC 3000  // how long to block node while waiting for result

#define OPENZWAVE_NODE_SETTING_CONFIGURATION "_ozw_configuration"

namespace micasa {

	using namespace nlohmann;

	class OpenZWaveNode final : public Hardware {

		friend class OpenZWave;

	public:
		OpenZWaveNode( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );
		~OpenZWaveNode() { };

		void start() override;
		void stop() override;

		std::string getLabel() const override;
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		json getJson( bool full_ = false ) const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override { return std::chrono::milliseconds( 1000 * 60 * 5 ); }

	private:
		json m_configuration = json::object();
		mutable std::mutex m_configurationMutex;

		static const std::map<std::string, unsigned int> UnitMapping;

		void _handleNotification( const ::OpenZWave::Notification* notification_ );
		void _processValue( const ::OpenZWave::ValueID& valueId_, unsigned int source_ );
		void _updateNodeInfo( const unsigned int& homeId_, const unsigned char& nodeId_ );
		void _installResourceHandlers() const;

	}; // class OpenZWaveNode

}; // namespace micasa
