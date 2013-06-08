// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2009 Emweb bvba, Kessel-Lo, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef SESSION_H_
#define SESSION_H_

#include <Wt/Auth/Login>

#include <Wt/Dbo/Session>
#include <Wt/Dbo/ptr>
#include <Wt/Dbo/SqlConnection>
#include <Wt/Dbo/backend/Sqlite3>
#include <Wt/Auth/PasswordService>
#include <Wt/Auth/AuthService>
#include <Models/user.h>



namespace dbo = Wt::Dbo;

typedef Wt::Auth::Dbo::UserDatabase<AuthInfo> UserDatabase;

namespace StreamingPrivate {
  class SessionPrivate;
}
class Session : public dbo::Session
{
public:
  static void configureAuth();

  Session(bool full = false);
  ~Session();

  dbo::ptr<User> user();

  Wt::Auth::AbstractUserDatabase& users();
  Wt::Auth::Login& login();

  static const Wt::Auth::AuthService& auth();
  static const Wt::Auth::PasswordService& passwordAuth();
  static const std::vector<const Wt::Auth::OAuthService *>& oAuth();
private:
  StreamingPrivate::SessionPrivate * const d;
};

#endif // SESSION_H_