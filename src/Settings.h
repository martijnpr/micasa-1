#pragma once

#include <string>
#include <map>
#include <mutex>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "json.hpp"

namespace micasa {

	class SettingValue: public std::string {
		
	public:
		SettingValue( const unsigned int& value_ );
		SettingValue( const int& value_ );
		SettingValue( const double& value_ );
		SettingValue( const bool& value_ );
		SettingValue( const std::string& value_ );
		SettingValue( const char* value_ );
	}; // class SettingValue

	typedef std::pair<std::string, SettingValue> Setting;

	template<class T> class SettingsHelper {
		
	public:
		SettingsHelper( const T& target_ );
		
		void commit();

	protected:
		void _populateOnce() const;

		const T& m_target;
		// NOTE is marked mutable to allow the populate method to be called as late as possible.
		mutable std::map<std::string, std::string> m_settings;
		mutable bool m_populated = false;
		std::vector<std::string> m_dirty;
		mutable std::mutex m_settingsMutex;
	
	}; // class SettingsHelper

	// For generic settings we're specializing the SettingsHelper class for type void.
	template<> class SettingsHelper<void> {

	public:
		SettingsHelper();

		void commit();

	protected:
		void _populateOnce() const;

		// NOTE is marked mutable to allow the populate method to be called as late as possible.
		mutable std::map<std::string, std::string> m_settings;
		mutable bool m_populated = false;
		std::vector<std::string> m_dirty;
		mutable std::mutex m_settingsMutex;

	}; // class SettingsHelper<void>

	// The actual Settings declaration follows below. These are the functions that do not require
	// a specialization of any kind.
	template<class T = void> class Settings: public SettingsHelper<T> {
		
	public:
		using SettingsHelper<T>::SettingsHelper;
		~Settings();
		// NOTE destructors are 'inherited' (quotes because that's not exactly what happens, but the effect is the same :))

		void insert( const std::vector<Setting>& settings_ );
		bool contains( const std::initializer_list<std::string>& settings_ ) const;
		bool contains( const std::string& key_ ) const;
		void remove( const std::string& key_ );
		unsigned int count() const;
		bool isDirty() const;
		
		std::string get( const std::string& key_ ) const;
		template<typename V> V get( const std::string& key_ ) const {
			// Unfortunately, to be able to use all types, the implementation needs to be in the header.
			std::lock_guard<std::mutex> lock( this->m_settingsMutex );
			this->_populateOnce();
			V value;
			std::istringstream( this->m_settings.at( key_ ) ) >> std::boolalpha >> std::fixed >> std::setprecision( 3 ) >> value;
			return value;
		};
		
		std::string get( const std::string& key_, const std::string& default_ ) const;
		template<typename V> V get( const std::string& key_, const V& default_ ) const {
			// Unfortunately, to be able to use all types, the implementation needs to be in the header.
			std::lock_guard<std::mutex> lock( this->m_settingsMutex );
			this->_populateOnce();
			try {
				V value;
				std::istringstream( this->m_settings.at( key_ ) ) >> std::boolalpha >> std::fixed >> std::setprecision( 3 ) >> value;
				return value;
			} catch( std::out_of_range exception_ ) {
				return default_;
			}
		};
		
		void put( const std::string& key_, const SettingValue& value_ );
		
		std::map<std::string, std::string> getAll() const;
		std::map<std::string, std::string> getAll( const std::string& prefix_ ) const;

		// The method below is a specialized method for validating input coming from the web client against a set of
		// setting defines that are also presented to the client.
		bool verifiedPut( const nlohmann::json& configuration_, const nlohmann::json& input_, std::string& error_ );

	}; // class Settings

}; // namespace micasa
