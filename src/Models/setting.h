/***********************************************************************
Copyright (c) 2013 "Marco Gulino <marco.gulino@gmail.com>"

This file is part of Pandorica: https://github.com/GuLinux/Pandorica

Pandorica is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details (included the COPYING file).

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
***********************************************************************/




#ifndef SETTING_H
#define SETTING_H

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/Types>
#include <Wt/Dbo/ptr>

#include <string>
#include <mutex>
#include <boost/lexical_cast.hpp>

class Setting
{
  class Session : public Wt::Dbo::Session {
  public: 
    Session();
    static std::shared_ptr<std::unique_lock<std::mutex>> writeLock();
  private:
    std::unique_ptr<Wt::Dbo::SqlConnection> connection;
  };
public:
    Setting();
    ~Setting();
    enum KeyName {
      QuitPassword,
      PostgreSQL_Hostname,
      PostgreSQL_Port,
      PostgreSQL_Database,
      PostgreSQL_Application,
      PostgreSQL_Username,
      PostgreSQL_Password,
      GoogleBrowserDeveloperKey,
      DatabaseVersion,
      MediaDirectories,
      EmailVerificationMandatory,
      ThreadPoolThreads,
      AdminEmailName,
      AdminEmailAddress,
      AuthEmailName,
      AuthEmailAddress,
      GroupsACL,
    };

    template<class Action>
    void persist(Action& a) {
        Wt::Dbo::field(a, _key, "key");
        Wt::Dbo::field(a, _value, "value");
    }


    
    template<class Type>
    static void write(KeyName key, const Type &value) {
      Session session;
      Wt::Dbo::Transaction t(session);
      auto writeLock = session.writeLock();
      session.execute("DELETE FROM settings WHERE \"key\" = ?").bind(keyName(key));
      Setting *setting = new Setting;
      setting->_key = key;
      setting->_value = boost::lexical_cast<std::string>(value);
      session.add(setting);
    }
    
    template<typename Type, class Cont>
    static void write(KeyName key, const Cont &values) {
      Session session;
      Wt::Dbo::Transaction t(session);
      auto writeLock = session.writeLock();
      session.execute("DELETE FROM settings WHERE \"key\" = ?").bind(keyName(key));
      for(Type value: values) {
        Setting *setting = new Setting;
        setting->_key = key;
        setting->_value = boost::lexical_cast<std::string>(value);
        session.add(setting);
      }
    }
    
    template<class Type>
    static Type value(KeyName key, const Type &defaultValue = Type{} ) {
      Session session;
      Wt::Dbo::Transaction t(session);
      Wt::Dbo::ptr<Setting> setting = session.find<Setting>().where("\"key\" = ?").bind(keyName(key));
      if(!setting)
        return defaultValue;
      return boost::lexical_cast<Type>(setting->_value);
    }
    
    template<class Type>
    static std::vector<Type> values(KeyName key) {
      Session session;
      Wt::Dbo::Transaction t(session);
      std::vector<Type> collection;
      auto dboValues = session.find<Setting>().where("\"key\" = ?").bind(keyName(key)).resultList();
      std::transform(dboValues.begin(), dboValues.end(), std::back_inserter(collection),
                     [=](Wt::Dbo::ptr<Setting> setting) { return boost::lexical_cast<Type>(setting->_value); });
      return collection;
    }


    
private:
    std::string _key;
    std::string _value;
    static std::string keyName(KeyName key);
};


#endif // SETTING_H
