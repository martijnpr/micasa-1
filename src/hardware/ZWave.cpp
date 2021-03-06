// https://github.com/OpenZWave/open-zwave/wiki/Config-Options
// http://www.openzwave.com/dev/classOpenZWave_1_1Notification.html#a5fa14ba721a25a4c84e0fbbedd767d54a4432a88465416a63cf4eda11ecf28c24

// pipes to store and retrieve the log and config?
// http://stackoverflow.com/questions/2784500/how-to-send-a-simple-string-between-two-programs-using-pipes

#include "ZWave.h"
#include "ZWaveNode.h"

#include "../Logger.h"
#include "../Controller.h"
#include "../User.h"

// OpenZWave includes
#include "Options.h"
#include "Manager.h"
#include "Driver.h"
#include "Node.h"
#include "Group.h"
#include "Notification.h"
#include "value_classes/ValueStore.h"
#include "value_classes/Value.h"
#include "value_classes/ValueBool.h"
#include "platform/Log.h"
#include "Defs.h"

void micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ ) {
	auto target = reinterpret_cast<micasa::ZWave*>( handler_ );
	target->_handleNotification( notification_ );
};

namespace micasa {

	using namespace nlohmann;
	using namespace OpenZWave;

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;

	std::timed_mutex ZWave::g_managerMutex;
	unsigned int ZWave::g_managerWatchers = 0;

	void ZWave::start() {
		if ( ! this->m_settings->contains( { "port" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return;
		}

		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		g_logger->logr( Logger::LogLevel::VERBOSE, this, "OpenZWave Version %s.", Manager::getVersionAsString().c_str() );

		std::unique_lock<std::timed_mutex> lock( ZWave::g_managerMutex );
		if (
			NULL == Manager::Get()
			|| 0 == g_managerWatchers
		) {
			Options::Create( "./lib/open-zwave/config", "./var", "" );
#ifdef _DEBUG
			Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Detail );
			Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
			Options::Get()->AddOptionInt( "DumpTriggerLevel", LogLevel_Error );
			Options::Get()->AddOptionBool( "Logging", true );
#else
			Options::Get()->AddOptionBool( "Logging", false );
#endif // _DEBUG
			Options::Get()->AddOptionBool( "ConsoleOutput", false );
			Options::Get()->AddOptionString( "LogFileName", "openzwave.log", false );
			Options::Get()->AddOptionInt( "PollInterval", 60000 ); // 60 seconds
			Options::Get()->AddOptionInt( "DriverMaxAttempts", 3 );
			Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
			Options::Get()->AddOptionBool( "ValidateValueChanges", true );
			Options::Get()->AddOptionBool( "Associate", true );
			Options::Get()->AddOptionBool( "SaveConfiguration", true );
			Options::Get()->AddOptionBool( "AppendLogFile", false );
			Options::Get()->Lock();
			
			Manager::Create();
		}

		// There should be a valid manager right now, fail if there isn't or if we're unable to use it.
		if (
			NULL == Manager::Get()
			|| ! Manager::Get()->AddWatcher( micasa_openzwave_notification_handler, this )
			|| ! Manager::Get()->AddDriver( this->m_settings->get( "port" ) )
		) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Unable to initialize OpenZWave manager." );
			this->setState( Hardware::State::FAILED );
			return;
		}

		this->m_port = this->m_settings->get( "port" );
		this->m_homeId = this->m_settings->get<unsigned int>( "home_id", 0 );

		g_managerWatchers++;
		lock.unlock();
		
#ifdef _WITH_LIBUDEV
		// If libudev is available we can detect if a device is added or removed from the system.
		g_controller->addSerialPortCallback( "openzwave_" + this->m_id, [this]( const std::string& serialPort_, const std::string& action_ ) {
			std::lock_guard<std::timed_mutex> lock( ZWave::g_managerMutex );

			// If the serial port we're currently using was removed, set the device state to disabled.
			if (
				serialPort_ == this->m_port
				&& action_ == "remove"
			) {
				Manager::Get()->RemoveDriver( serialPort_ );
				this->m_port.clear();
				
				this->setState( Hardware::State::DISCONNECTED );
				auto children = g_controller->getChildrenOfHardware( *this );
				for ( auto childrenIt = children.begin(); childrenIt != children.end(); childrenIt++ ) {
					(*childrenIt)->setState( Hardware::State::DISCONNECTED );
				}

				g_logger->log( Logger::LogLevel::WARNING, this, "Disconnected." );
			}

			// If a serial port was added (not necessarely ours) and we're currently disabled, see if it
			// is the device we should use.
			// TODO make this more rebust and detect what kind of device was inserted before feeding it
			// to the OpenZWave library?
			if (
				action_ == "add"
				&& (
					this->getState() == Hardware::State::DISCONNECTED
					|| this->getState() == Hardware::State::FAILED
				)
				&& Manager::Get()->AddDriver( serialPort_ )
			) {
				this->m_port = serialPort_;
			
				this->setState( Hardware::State::INIT );
				auto children = g_controller->getChildrenOfHardware( *this );
				for ( auto childrenIt = children.begin(); childrenIt != children.end(); childrenIt++ ) {
					(*childrenIt)->setState( Hardware::State::INIT );
				}
			
				g_logger->log( Logger::LogLevel::VERBOSE, this, "Device added." );
			}
		} );
#endif // _WITH_LIBUDEV

		Hardware::start();

		// Add the devices that will initiate controller actions. NOTE this has to be done *after* the
		// parent hardware instance is started to make sure previously created devices get picked up by the
		// declareDevice method.
		this->declareDevice<Switch>( "heal", "Network Heal", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE, Switch::resolveSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS, User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
		this->declareDevice<Switch>( "include", "Inclusion Mode", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE, Switch::resolveSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS, User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
		this->declareDevice<Switch>( "exclude", "Exclusion Mode", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE, Switch::resolveSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS, User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
	};

	void ZWave::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		std::unique_lock<std::timed_mutex> lock( ZWave::g_managerMutex );
		Manager::Get()->RemoveWatcher( micasa_openzwave_notification_handler, this );
		if ( ! this->m_port.empty() ) {
			Manager::Get()->RemoveDriver( this->m_port );
			this->m_port.clear();
		}
		g_managerWatchers--;
		if ( 0 == g_managerWatchers ) {
			Manager::Destroy();
			Options::Destroy();
		}
		lock.unlock();

#ifdef _WITH_LIBUDEV
		g_controller->removeSerialPortCallback( "openzwave_" + this->m_id );
#endif // _WITH_LIBUDEV

		Hardware::stop();
	};

	std::string ZWave::getLabel() const {
		return this->m_settings->get( "label", "Z-Wave" );
	};

	json ZWave::getJson( bool full_ ) const {
		json result = Hardware::getJson( full_ );
		result["port"] = this->m_settings->get( "port", "" );
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		return result;
	};

	json ZWave::getSettingsJson() const {
		json result = Hardware::getSettingsJson();
		json setting = {
			{ "name", "port" },
			{ "label", "Port" },
			{ "type", "string" },
			{ "class", this->m_settings->contains( "port" ) ? "advanced" : "normal" }
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

		result += setting;
		return result;
	};

	bool ZWave::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		if ( device_->getType() == Device::Type::SWITCH ) {
			std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
			if ( device->getValueOption() == Switch::Option::ACTIVATE ) {
			
				if (
					this->getState() != Hardware::State::READY
					|| ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) == false
				) {
					g_logger->log( Logger::LogLevel::ERROR, this, "Controller busy." );
					return false;
				}
			
				if ( device->getReference() == "heal" ) {
					
					Manager::Get()->HealNetwork( this->m_homeId, true );
					g_logger->log( Logger::LogLevel::NORMAL, this, "Network heal initiated." );
					this->wakeUpAfter( std::chrono::milliseconds( 1000 * 5 ) );

				} else if ( device->getReference() == "include" ) {

					if ( Manager::Get()->AddNode( this->m_homeId, false ) ) {
						g_logger->log( Logger::LogLevel::NORMAL, this, "Inclusion mode activated." );

						// Wake up the worker thread after xx microsecnds to cancel the in-/exclusion mode.
						this->m_performCancel = true;
						this->wakeUpAfter( std::chrono::milliseconds( 1000 * 60 * OPEN_ZWAVE_IN_EXCLUSION_MODE_DURATION_MINUTES ) );
					} else {
						g_logger->log( Logger::LogLevel::ERROR, this, "Unable to activate inclusion mode." );
					}

				} else if ( device->getReference() == "exclude" ) {

					if ( Manager::Get()->RemoveNode( this->m_homeId ) ) {
						g_logger->log( Logger::LogLevel::NORMAL, this, "Exclusion mode activated." );

						// Wake up the worker thread after xx microsecnds to cancel the in-/exclusion mode.
						this->m_performCancel = true;
						this->wakeUpAfter( std::chrono::milliseconds( 1000 * 60 * OPEN_ZWAVE_IN_EXCLUSION_MODE_DURATION_MINUTES ) );
					} else {
						g_logger->log( Logger::LogLevel::ERROR, this, "Unable to activate exclusion mode." );
					}
				}

				ZWave::g_managerMutex.unlock();
				return true;
			}
		}
		
		return false;
	};

	std::chrono::milliseconds ZWave::_work( const unsigned long int& iteration_ ) {
		// Because the action devices needed to be created after the hardware was started, they might not exist in
		// the first iteration.
		if ( iteration_ > 1 ) {
			if ( this->m_performCancel ) {
				if ( ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					Manager::Get()->CancelControllerCommand( this->m_homeId );
					
					std::static_pointer_cast<Switch>( this->getDevice( "include" ) )->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
					std::static_pointer_cast<Switch>( this->getDevice( "exclude" ) )->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
					
					this->m_performCancel = false;
				} else {
					return std::chrono::milliseconds( 1000 * 60 );
				}
			}
			auto device = std::static_pointer_cast<Switch>( this->getDevice( "heal" ) );
			if ( device->getValueOption() == Switch::Option::ACTIVATE ) {
				device->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
			}
		}
		return std::chrono::milliseconds( 1000 * 60 * 15 );
	};

	void ZWave::_handleNotification( const Notification* notification_ ) {
		if ( ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {

#ifdef _DEBUG
			g_logger->log( Logger::LogLevel::VERBOSE, this, notification_->GetAsString() );
#endif // _DEBUG

			unsigned int homeId = notification_->GetHomeId();
			unsigned int nodeId = notification_->GetNodeId();

			// Notifications for specific nodes are handled by the node hardware.
			std::stringstream reference;
			reference << homeId << "/" << (unsigned int)nodeId;
			auto node = std::static_pointer_cast<ZWaveNode>( g_controller->getHardware( reference.str() ) );
			if (
				node != nullptr
				&& node->isRunning()
			) {
				node->_handleNotification( notification_ );
			}

			// Handle certain controller- or driver notifications ourselves.
			switch( notification_->GetType() ) {

				case Notification::Type_DriverReady: {
					if (
						this->m_homeId == 0
						|| this->m_homeId == homeId
					) {
						this->m_homeId = homeId;
						this->m_settings->put( "port", this->m_port );
						this->m_settings->put( "home_id", homeId );
						g_logger->log( Logger::LogLevel::NORMAL, this, "Driver ready." );
					} else {
						Manager::Get()->RemoveDriver( this->m_port );
						this->m_port.clear();
						g_logger->log( Logger::LogLevel::ERROR, this, "Driver has wrong home id." );
					}
					break;
				}

				case Notification::Type_DriverFailed: {
					this->setState( Hardware::State::FAILED );
					if ( this->m_port == this->m_settings->get( "port" ) ) {
						g_logger->log( Logger::LogLevel::ERROR, this, "Driver failed." );
					}
					Manager::Get()->RemoveDriver( this->m_port );
					this->m_port.clear();
					break;
				}

				case Notification::Type_AwakeNodesQueried:
				case Notification::Type_AllNodesQueried:
				case Notification::Type_AllNodesQueriedSomeDead: {
					this->setState( Hardware::State::READY );
					
					// At this point we're going to instruct all nodes to report their configuration parameters.
					g_logger->logr( Logger::LogLevel::WARNING, this, "Requesting all node configuration parameters." );
					auto nodes = g_controller->getChildrenOfHardware( *this );
					for ( auto nodeIt = nodes.begin(); nodeIt != nodes.end(); nodeIt++ ) {
						auto node = std::static_pointer_cast<ZWaveNode>( *nodeIt );
						Manager::Get()->RequestAllConfigParams( node->m_homeId, node->m_nodeId );
					}
					break;
				}

				case Notification::Type_ControllerCommand: {
					switch( notification_->GetEvent() ) {
						case Driver::ControllerState_Cancel: {
							break;
						}
						case Driver::ControllerState_Error: {
							break;
						}
						case Driver::ControllerState_Completed: {
							break;
						}
						case Driver::ControllerState_Failed: {
							break;
						}
					}
					break;
				}

				case Notification::Type_NodeAdded: {
					if (
						homeId == this->m_homeId
						&& nodeId > 1 // skip the controller (most likely node 1)
					) {
						g_controller->declareHardware( Hardware::Type::ZWAVE_NODE, reference.str(), this->shared_from_this(), {
							{ "home_id", std::to_string( homeId ) },
							{ "node_id", std::to_string( nodeId ) }
						}, true /* auto start */ );
					}
				}

				case Notification::Type_NodeNaming: {
					if ( node == nullptr ) {
						std::string manufacturer = Manager::Get()->GetNodeManufacturerName( homeId, nodeId );
						std::string product = Manager::Get()->GetNodeProductName( homeId, nodeId );
						std::string nodeName = Manager::Get()->GetNodeName( homeId, nodeId );
						if ( ! manufacturer.empty() ) {
							this->m_settings->put( "label", manufacturer + " " + product );
						}
						if (
							! nodeName.empty()
							&& this->m_settings->get( "name", "" ).empty()
						) {
							this->m_settings->put( "name", nodeName );
						}
					}
					break;
				}
				
				case Notification::Type_NodeRemoved: {
					// The NodeRemove notification is called on exclusion of nodes aswell as application shutdown.
					// NOTE we're removing the watcher before the driver so this event should not be executed when
					// the application is shutdown.
					if ( node != nullptr ) {
						g_controller->removeHardware( node );
					}
					break;
				};

				default: {
					break;
				}
			}

			ZWave::g_managerMutex.unlock();

		} else {
			g_logger->logr( Logger::LogLevel::VERBOSE, this, "OpenZWave notification \"%s\" missed.", notification_->GetAsString().c_str() );
		}
	};

}; // namespace micasa
