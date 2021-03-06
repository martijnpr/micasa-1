#pragma once

#include <chrono>

#include "../Hardware.h"

namespace micasa {

	class WeatherUnderground final : public Hardware {

	public:
		WeatherUnderground( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~WeatherUnderground() { };

		void start() override;
		void stop() override;
		
		std::string getLabel() const throw() override { return "Weather Underground"; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;
		
	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;
		
	private:
		bool m_first = true;
		
		void _processHttpReply( const std::string& body_ );
	
	}; // class WeatherUnderground

}; // namespace micasa
