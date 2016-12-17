#pragma once

#include "../Hardware.h"
#include "../Network.h"

namespace micasa {

	class SolarEdge final : public Hardware {

	public:
		SolarEdge( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ ) : Hardware( id_, type_, reference_, parent_, label_ ) { };
		~SolarEdge() { };

		void start() override;
		void stop() override;
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };

	protected:
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ );
		
	private:
		void _processHttpReply( mg_connection* connection_, const http_message* message_ );
		
	}; // class SolarEdge

}; // namespace micasa
