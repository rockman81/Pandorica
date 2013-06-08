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





#ifndef LOGGEDUSERSDIALOG_H
#define LOGGEDUSERSDIALOG_H

#include <Wt/WDialog>

class Settings;
class Session;
class LoggedUsersDialog : public Wt::WDialog
{

public:
    LoggedUsersDialog(Session *session, Settings *settings, bool showAll = false);
    virtual ~LoggedUsersDialog();
private:
  Session *session;
};

#endif // LOGGEDUSERSDIALOG_H
