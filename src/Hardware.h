#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <map>
#include <mutex>
#include <vector>
#include <memory>

#include "Logger.h"
#include "WebServer.h"
#include "Device.h"
#include "Settings.h"

namespace micasa {

	class Hardware : public Worker, public std::enable_shared_from_this<Hardware> {
		
		friend class Controller;
		friend class Device;

	public:
		enum HardwareType {
			HARMONY_HUB = 1,
			OPEN_ZWAVE,
			OPEN_ZWAVE_NODE,
			P1_METER,
			PIFACE,
			PIFACE_BOARD,
			RFXCOM,
			SOLAREDGE,
			SOLAREDGE_INVERTER,
			WEATHER_UNDERGROUND
		};
		
		Hardware( const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string name_ );
		virtual ~Hardware();
		friend std::ostream& operator<<( std::ostream& out_, const Hardware* hardware_ ) { out_ << hardware_->m_name; return out_; }

		virtual void start();
		virtual void stop();
		
		const unsigned int getId() const { return this->m_id; };
		const std::string getReference() const { return this->m_reference; };
		std::string getName() const { return this->m_name; };
		Settings& getSettings() { return this->m_settings; };
		
		virtual bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ ) =0;

	protected:
		mutable std::timed_mutex m_hardwareMutex;
		const unsigned int m_id;
		const std::string m_reference;
		const std::shared_ptr<Hardware> m_parent;
		std::string m_name;
		Settings m_settings;

		std::chrono::milliseconds _work( const unsigned long int iteration_ ) =0;
		std::shared_ptr<Device> _getDevice( const std::string reference_ ) const;
		std::shared_ptr<Device> _getDeviceById( const unsigned int id_ ) const;
		std::shared_ptr<Device> _declareDevice( const Device::DeviceType deviceType_, const std::string reference_, const std::string name_, const std::map<std::string, std::string> settings_ );

	private:
		std::vector<std::shared_ptr<Device> > m_devices;
		mutable std::mutex m_devicesMutex;

		static std::shared_ptr<Hardware> _factory( const HardwareType hardwareType, const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string name_ );
		const nlohmann::json _getResourceJson() const;

	}; // class Hardware

}; // namespace micasa
