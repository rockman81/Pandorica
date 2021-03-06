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


#ifndef MEDIACOLLECTION_H
#define MEDIACOLLECTION_H

#include <Wt/WObject>
#include <Wt/WSignal>
#include <boost/filesystem.hpp>
#include "media.h"
#include "utils/d_ptr.h"
#include "media/mediadirectory.h"

class Settings;
namespace Wt {
namespace Dbo {
class Transaction;
}
}

class Session;
class User;
class MediaCollection : public Wt::WObject
{
public:
  MediaCollection(Settings* settings, Session* session, Wt::WApplication* parent = 0);
    virtual ~MediaCollection();
    void rescan(const std::function<void()> &onFinish);
    std::map<std::string,Media> collection() const;
    Media media(std::string uid) const;
    void setUser(const Wt::Dbo::ptr<User> &user);
    Wt::Dbo::ptr<User> viewingAs() const;
    std::vector<Media> sortedMediasList() const;
    bool isAllowed(const boost::filesystem::path &path) const;
    std::vector<std::shared_ptr<MediaDirectory>> rootDirectories() const;
    std::shared_ptr<MediaDirectory> find(const std::string &directoryPath) const;
    Wt::Signal<> &scanning() const;
    Wt::Signal<> &scanned() const;
private:
  D_PTR;
};

#endif // MEDIACOLLECTION_H
