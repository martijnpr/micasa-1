#pragma once

#include <chrono>
#include <mutex>
#include <vector>
#include <iostream>
#include <list>

#include "Hardware.h"
#include "Settings.h"
#include "Worker.h"

#include "device/Counter.h"
#include "device/Level.h"
#include "device/Switch.h"
#include "device/Text.h"

#include "json.hpp"

#ifdef _WITH_LIBUDEV
	#include <libudev.h>
#endif // _WITH_LIBUDEV

extern "C" {
	#include "v7.h"
	
	v7_err micasa_v7_update_device( struct v7* js_, v7_val_t* res_ );
	v7_err micasa_v7_get_device( struct v7* js_, v7_val_t* res_ );
	v7_err micasa_v7_include( struct v7* js_, v7_val_t* res_ );
} // extern "C"

namespace micasa {

	class Controller final : public Worker, public std::enable_shared_from_this<Controller> {

		friend std::ostream& operator<<( std::ostream& out_, const Controller* ) { out_ << "Controller"; return out_; }
		friend v7_err (::micasa_v7_update_device)( struct v7* js_, v7_val_t* res_ );
		friend v7_err (::micasa_v7_get_device)( struct v7* js_, v7_val_t* res_ );
		friend v7_err (::micasa_v7_include)( struct v7* js_, v7_val_t* res_ );

#ifdef _WITH_LIBUDEV
		typedef std::function<void( const std::string& serialPort_, const std::string& action_ )> t_serialPortCallback;
#endif // _WITH_LIBUDEV

	public:
		typedef std::chrono::time_point<std::chrono::system_clock> t_scheduled;
		
		struct Task {
			std::shared_ptr<Device> device;
			Device::UpdateSource source;
			t_scheduled scheduled;
			
			Text::t_value textValue;
			Switch::t_value switchValue;
			Level::t_value levelValue;
			Counter::t_value counterValue;
			
			template<class D> void setValue( const typename D::t_value& value_ );
		}; // struct Task
		
		struct TaskOptions {
			double forSec;
			double afterSec;
			int repeat;
			double repeatSec;
			bool clear;
			bool recur;
		}; // struct TaskOptions

		Controller();
		~Controller();

		void start();
		void stop();
		
		std::shared_ptr<Hardware> getHardware( const std::string& reference_ ) const;
		std::shared_ptr<Hardware> getHardwareById( const unsigned int& id_ ) const;
		std::vector<std::shared_ptr<Hardware> > getChildrenOfHardware( const Hardware& hardware_ ) const;
		std::vector<std::shared_ptr<Hardware> > getAllHardware() const;
		std::shared_ptr<Hardware> declareHardware( const Hardware::Type type_, const std::string reference_, const std::vector<Setting>& settings_, const bool& start_ = false );
		std::shared_ptr<Hardware> declareHardware( const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_, const std::vector<Setting>& settings_, const bool& start_ = false );
		void removeHardware( const std::shared_ptr<Hardware> hardware_ );

		std::shared_ptr<Device> getDevice( const std::string& reference_ ) const;
		std::shared_ptr<Device> getDeviceById( const unsigned int& id_ ) const;
		std::shared_ptr<Device> getDeviceByName( const std::string& name_ ) const;
		std::shared_ptr<Device> getDeviceByLabel( const std::string& label_ ) const;
		std::vector<std::shared_ptr<Device> > getAllDevices() const;

		template<class D> void newEvent( const D& device_, const Device::UpdateSource& source_ );

#ifdef _WITH_LIBUDEV
		void addSerialPortCallback( const std::string& name_, const t_serialPortCallback& callback_ );
		void removeSerialPortCallback( const std::string& name_ );
#endif // _WITH_LIBUDEV

	private:
		std::vector<std::shared_ptr<Hardware> > m_hardware;
		mutable std::mutex m_hardwareMutex;
		std::list<std::shared_ptr<Task> > m_taskQueue;
		mutable std::mutex m_taskQueueMutex;
		v7* m_v7_js;
		mutable std::mutex m_jsMutex;
		
#ifdef _WITH_LIBUDEV
		std::map<std::string, t_serialPortCallback> m_serialPortCallbacks;
		mutable std::mutex m_serialPortCallbacksMutex;
		udev* m_udev;
		udev_monitor* m_udevMonitor;
		std::thread m_udevMonitorThread;
#endif // _WITH_LIBUDEV
		
		std::chrono::milliseconds _work( const unsigned long int& iteration_ );
		void _runScripts( const std::string& key_, const nlohmann::json& data_, const std::vector<std::map<std::string, std::string> >& scripts_ );
		void _runTimers();
		template<class D> bool _updateDeviceFromScript( const std::shared_ptr<D> device_, const typename D::t_value& value_, const std::string& options_ = "" );
		bool _includeFromScript( const std::string& name_, std::string& script_ );
		void _scheduleTask( const std::shared_ptr<Task> task_ );
		void _clearTaskQueue( const std::shared_ptr<Device> device_ );
		TaskOptions _parseTaskOptions( const std::string& options_ ) const;
		
	}; // class Controller

}; // namespace micasa
