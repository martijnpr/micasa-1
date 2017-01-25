#include "P1Meter.h"

#include "../Logger.h"
#include "../Controller.h"
#include "../WebServer.h"
#include "../User.h"
#include "../Serial.h"
#include "../Utils.h"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;

	P1Meter::P1Meter( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) {
		g_webServer->addResourceCallback( {
			"hardware-" + std::to_string( this->m_id ),
			"^api/hardware/" + std::to_string( this->m_id ) + "$",
			99,
			WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const nlohmann::json& input_, const WebServer::Method& method_, nlohmann::json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::INSTALLER ) {
					return;
				}

				auto settings = extractSettingsFromJson( input_ );
				try {
					this->m_settings->put( "port", settings.at( "port" ) );
				} catch( std::out_of_range exception_ ) { };
				try {
					this->m_settings->put( "baudrate", std::stoi( settings.at( "baudrate" ) ) );
				} catch( std::out_of_range exception_ ) { };
				if ( this->m_settings->isDirty() ) {
					this->m_settings->commit();
					this->m_needsRestart = true;
				}
			} )
		} );
	};

	P1Meter::~P1Meter() {
		g_webServer->removeResourceCallback( "hardware-" + std::to_string( this->m_id ) );
	};

	void P1Meter::start() {
		if ( ! this->m_settings->contains( { "port", "baudrate" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return;
		}

		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		
		this->m_serial = std::make_shared<Serial>(
			this->m_settings->get( "port" ),
			this->m_settings->get<unsigned int>( "baudrate" ),
			Serial::CharacterSize::CHAR_SIZE_8,
			Serial::Parity::PARITY_NONE,
			Serial::StopBits::STOP_BITS_1,
			Serial::FlowControl::FLOW_CONTROL_NONE,
			std::make_shared<Serial::t_callback>( [this]( const std::string& data_ ) {
			
			} )
		);

		try {
			this->m_serial->open();
			this->setState( Hardware::State::READY );
		} catch( Serial::SerialException exception_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, exception_.what() );
			this->setState( Hardware::State::FAILED );
		}

		Hardware::start();
	};
	
	void P1Meter::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		try {
			this->m_serial->close();
		} catch( Serial::SerialException exception_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, exception_.what() );
		}
		this->m_serial = nullptr;

		Hardware::stop();
	};

	json P1Meter::getJson( bool full_ ) const {
		if ( full_ ) {
			json result = Hardware::getJson( full_ );

			result["settings"] = json::array();
	
			json setting = {
				{ "name", "port" },
				{ "label", "Port" },
				{ "type", "string" },
				{ "value", this->m_settings->get( "port", "" ) }
			};

#ifdef _WITH_LIBUDEV
			json options = json::array();
			auto ports = getSerialPorts();
			for ( auto portsIt = ports.begin(); portsIt != ports.end(); portsIt++ ) {
				json option = json::object();
				option["value"] = portsIt->first;
				option["label"] = portsIt->second;
				options += option;
			}

			setting["type"] = "list";
			setting["options"] = options;
#endif // _WITH_LIBUDEV
		
			result["settings"] += setting;
			
			result["settings"] += {
				{ "name", "baudrate" },
				{ "label", "Baudrate" },
				{ "type", "list" },
				{ "value", this->m_settings->get<unsigned int>( "baudrate", 115200 ) },
				{ "options", {
					{
						{ "value", 9600 },
						{ "label", "9600" }
					}, {
						{ "value", 115200 },
						{ "label", "115200" }
					}
				} }
			};

			return result;
		} else {
			return Hardware::getJson( full_ );
		}
	};

}; // namespace micasa
