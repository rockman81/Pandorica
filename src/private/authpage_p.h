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



#ifndef AUTHPAGEPRIVATE_H
#define AUTHPAGEPRIVATE_H
#include <Wt/Auth/AuthWidget>
#include "authpage.h"

class Session;
class AuthPage;
class AuthWidgetCustom;
class AuthPage::Private
{
public:
    Private(Session *session, AuthPage* q);
    Wt::WContainerWidget* messagesContainer;
    Wt::Signal<> loggedIn;
    Wt::Signal<> loggedOut;
    Session *session;
    bool mailSent = false;
    AuthWidgetCustom* authWidget;
    void authEvent();
private:
    class AuthPage* const q;
    bool seedIfNoAdmins(Wt::Dbo::Transaction& transaction, Wt::Auth::User& user);
};

class AuthWidgetCustom : public Wt::Auth::AuthWidget {
public:
    AuthWidgetCustom(const Wt::Auth::AuthService& baseAuth, Wt::Auth::AbstractUserDatabase& users, Wt::Auth::Login& login, Wt::WContainerWidget* parent = 0);
protected:
    virtual Wt::Auth::RegistrationModel* createRegistrationModel();
    virtual void createOAuthLoginView();
};


class Message : public Wt::WTemplate {
public:
    Message(Wt::WString text, Wt::WContainerWidget* parent = 0);
};




#endif // STREAMINGPRIVATE::AUTHPAGEPRIVATE_H
