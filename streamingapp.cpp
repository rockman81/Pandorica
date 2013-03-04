/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2013  Marco Gulino <email>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "streamingapp.h"
#include "player.h"
#include "wmediaplayerwrapper.h"
#include "videojs.h"
#include <Wt/WGridLayout>
#include <Wt/WHBoxLayout>
#include <Wt/WVBoxLayout>
#include <Wt/WMediaPlayer>
#include <Wt/WContainerWidget>
#include <Wt/WTreeView>
#include <Wt/WStandardItem>
#include <Wt/WStandardItemModel>
#include <Wt/WAnchor>
#include <Wt/WFileResource>
#include <Wt/WTimer>
#include <Wt/WEnvironment>
#include <Wt/WMenu>
#include <Wt/WSubMenuItem>
#include <Wt/WImage>
#include <Wt/WText>
#include <Wt/Utils>
#include <Wt/WStreamResource>
#include <Wt/WMessageBox>
#include <Wt/Auth/AuthWidget>
#include <boost/algorithm/string.hpp>
#include <functional>
#include <iostream>
#include <fstream>
#include "authorizeduser.h"

#include "playlist.h"
#include "session.h"

using namespace Wt;
using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

typedef std::function<void(boost::filesystem::path)> RunOnPath;
class StreamingAppPrivate {
public:
  void addTo ( WMenu *menu, filesystem::path p );
  WMediaPlayer::Encoding encodingFor ( filesystem::path p );
  WLink linkFor(filesystem::path p);
  bool isAllowed(filesystem::path p);
  WMenu *menu;
  string videosDir();
  Player *player = 0;
  string extensionFor(filesystem::path p);
  map<string, WMediaPlayer::Encoding> types;
  StreamingAppPrivate();
  map<WMenuItem*, boost::filesystem::path> menuItemsPaths;
  map<string, boost::filesystem::path> filesHashes;
  void menuItemClicked(WMenuItem *item);
  void play(filesystem::path path);
  void setIconTo(WMenuItem *item, string url);
  WContainerWidget *infoBox;
  void parseFileParameter();
  Playlist *playlist;
    WContainerWidget* playerContainerWidget;
  void listDirectoryAndRun(filesystem::path directoryPath, RunOnPath runAction);
  Session session;
private:
    void queue(filesystem::path path);
    void addSubtitlesFor(filesystem::path path);
};

StreamingAppPrivate::StreamingAppPrivate() {
  types.insert(pair<string, WMediaPlayer::Encoding>(".mp3", WMediaPlayer::MP3));
  types.insert(pair<string, WMediaPlayer::Encoding>(".m4a", WMediaPlayer::M4A));
  types.insert(pair<string, WMediaPlayer::Encoding>(".m4v", WMediaPlayer::M4V));
  types.insert(pair<string, WMediaPlayer::Encoding>(".mp4", WMediaPlayer::M4V));
  types.insert(pair<string, WMediaPlayer::Encoding>(".oga", WMediaPlayer::OGA));
  types.insert(pair<string, WMediaPlayer::Encoding>(".ogg", WMediaPlayer::OGA));
  types.insert(pair<string, WMediaPlayer::Encoding>(".ogv", WMediaPlayer::OGV));
  types.insert(pair<string, WMediaPlayer::Encoding>(".wav", WMediaPlayer::WAV));
  types.insert(pair<string, WMediaPlayer::Encoding>(".webm", WMediaPlayer::WEBMV));
  types.insert(pair<string, WMediaPlayer::Encoding>(".flv", WMediaPlayer::FLV));
  types.insert(pair<string, WMediaPlayer::Encoding>(".f4v", WMediaPlayer::FLV));
}


StreamingApp::StreamingApp ( const Wt::WEnvironment& environment) : WApplication(environment), d(new StreamingAppPrivate) {
  d->session.login().changed().connect(this, &StreamingApp::authEvent);
     Wt::Auth::AuthWidget *authWidget
      = new Wt::Auth::AuthWidget(Session::auth(), d->session.users(),
                 d->session.login());
    authWidget->model()->addPasswordAuth(&Session::passwordAuth());
    authWidget->model()->addOAuth(Session::oAuth());
    authWidget->setRegistrationEnabled(true);
    authWidget->processEnvironment();
    root()->addWidget(authWidget);
//   setupGui();
}

void StreamingApp::authEvent()
{
  if(!d->session.login().loggedIn()) {
    return;
  }
  Auth::User user = d->session.login().user();
  WAnimation messageBoxAnimation(WAnimation::Fade, WAnimation::Linear, 1000);
  if(user.email().empty()) {
    WMessageBox::show("Login", "You need to verify your email address before logging in.\nPlease check your inbox.", Ok, messageBoxAnimation);
    return;
  }
  AuthorizedUserPtr authUser = d->session.find<AuthorizedUser>().where("email = ?").bind(user.email());
  if(!authUser) {
    WMessageBox::show("Login", "Your user is not authorized for this server.\nIf you think this is an error, contact me at marco.gulino (at) gmail.com", Ok, messageBoxAnimation);
    return;
  }
  wApp->log("notice") << "User logged in: role normalUser=" << (authUser->role() == AuthorizedUser::NormalUser)
    << ", admin user=" << (authUser->role() == AuthorizedUser::Admin);
  root()->clear();
  setupGui();
}


void StreamingApp::setupGui()
{
  WBoxLayout *layout = new WHBoxLayout();
  useStyleSheet("http://test.gulinux.net/css/videostreaming.css");
//   requireJQuery("http://myrent.gulinux.net/css/jquery-latest.js");
  useStyleSheet("http://test.gulinux.net/css/bootstrap/css/bootstrap.css");
  useStyleSheet("http://test.gulinux.net/css/bootstrap/css/bootstrap-responsive.css");
  require("http://test.gulinux.net/css/bootstrap/js/bootstrap.js");
  d->menu = new WMenu(Wt::Vertical);
  d->menu->itemSelected().connect(d, &StreamingAppPrivate::menuItemClicked);
  WContainerWidget *menuContainer = new WContainerWidget();
  WImage* reloadImg = new WImage("http://test.gulinux.net/css/reload.png");
  reloadImg->resize(24, 24);
  WAnchor* reloadLink = new WAnchor("javascript:false", reloadImg);
  reloadLink->setText("Reload");
  reloadLink->clicked().connect([this](WMouseEvent&){
    wApp->changeSessionId();
    wApp->redirect(wApp->bookmarkUrl());
  });
  useStyleSheet("http://vjs.zencdn.net/c/video-js.css");
  require("http://vjs.zencdn.net/c/video.js");
  menuContainer->addWidget(reloadLink);
  menuContainer->setOverflow(WContainerWidget::OverflowAuto);
  menuContainer->addWidget(d->menu);
  d->menu->setRenderAsList(true);
  d->menu->setStyleClass("nav nav-list");
  
  
  WVBoxLayout* leftPanel = new WVBoxLayout();
  leftPanel->addWidget(menuContainer);
  layout->addLayout(leftPanel);
//   layout->addWidget(menuContainer);
  leftPanel->setResizable(0, true, 450);
  
  WContainerWidget *playerContainer = new WContainerWidget();
  WBoxLayout *playerContainerLayout = new WVBoxLayout();
  d->playerContainerWidget = new WContainerWidget();
  d->playerContainerWidget->setContentAlignment(AlignCenter);
  playerContainerLayout->addWidget(d->playerContainerWidget);
  d->playlist = new Playlist();
  d->playlist->setList(true);
  leftPanel->addWidget(d->playlist, 1);
  playerContainerLayout->setResizable(0, true);
  playerContainer->setLayout(playerContainerLayout);
  d->infoBox = new WContainerWidget();
  d->playerContainerWidget->addWidget(d->infoBox);
  layout->addWidget(playerContainer);
  layout->setResizable(0, true, 400);
  root()->setLayout(layout);

  
  d->listDirectoryAndRun(fs::path(d->videosDir()), [this](fs::path path) {
    d->addTo(d->menu, path);
  });
  d->parseFileParameter();
  
  d->playlist->next().connect(d, &StreamingAppPrivate::play);
}


void StreamingAppPrivate::listDirectoryAndRun(filesystem::path directoryPath, RunOnPath runAction)
{
  vector<filesystem::path> v;
  copy(filesystem::directory_iterator(directoryPath), filesystem::directory_iterator(), back_inserter(v));
  sort(v.begin(), v.end());
  for(filesystem::path path: v) {
    runAction(path);
  }
}


void StreamingAppPrivate::parseFileParameter() {
  if(wApp->environment().getParameter("file")) {
    log("notice") << "Got parameter file: " << *wApp->environment().getParameter("file");
    WTimer::singleShot(1000, [this](WMouseEvent&) {
      string fileHash = * wApp->environment().getParameter("file");
      queue(filesystem::path( filesHashes[fileHash]));
    });
  }    
}


void StreamingApp::refresh() {
  Wt::WApplication::refresh();
  d->parseFileParameter();
}



WMediaPlayer::Encoding StreamingAppPrivate::encodingFor ( filesystem::path p ) {
  return types[extensionFor(p)];
}

class StreamFileResource : public WStreamResource {
public:
  StreamFileResource(string fname, WObject *parent =0) : WStreamResource(parent), fileName_(fname) {}
  void handleRequest(const Http::Request& request, Http::Response& response) {
    ifstream r(fileName_.c_str(), ios::in | ios::binary);
    handleRequestPiecewise(request, response, r);
  }
private:
  string fileName_;
};

WLink StreamingAppPrivate::linkFor ( filesystem::path p ) {
//   return WLink(new StreamFileResource(p.string(), wApp));
  
  string videosDeployDir;
  if(wApp->readConfigurationProperty("videos-deploy-dir", videosDeployDir )) {
    string relpath = p.string();
    boost::replace_all(relpath, videosDir(), videosDeployDir);
    return WLink(relpath);
  }

   WLink link = WLink(new StreamFileResource(p.string(), wApp));
   wApp->log("notice") << "Generated url: " << link.url();
   return link;
}

string StreamingAppPrivate::extensionFor ( filesystem::path p ) {
  string extension = p.extension().string();
  boost::algorithm::to_lower(extension);
  wApp->log("notice") << "extension for " << p << ": " << extension;
  return extension;
}


bool StreamingAppPrivate::isAllowed ( filesystem::path p ) {
  return types.count(extensionFor(p)) || fs::is_directory(p);
}


string StreamingAppPrivate::videosDir() {
  string videosDir = string(getenv("HOME")) + "/Videos";
  wApp->readConfigurationProperty("videos-dir", videosDir);
  return videosDir;
}



class IconMenuItem : public WSubMenuItem {
public:
  IconMenuItem(string text) : WSubMenuItem(text, 0) {
    this->text = text;
  }
  virtual WWidget* createItemWidget();
  string text;
};

WWidget* IconMenuItem::createItemWidget() {
  WAnchor *anchor = new WAnchor();
  anchor->setText(text);
  return anchor;
}


void StreamingAppPrivate::addTo ( WMenu* menu, filesystem::path p ) {
  if(!isAllowed(p)) return;
  WSubMenuItem *menuItem = new WSubMenuItem(p.filename().string(), 0);
  menuItem->setPathComponent(p.string());
  WMenu *subMenu = new WMenu(Wt::Vertical);
  subMenu->itemSelected().connect(this, &StreamingAppPrivate::menuItemClicked);
  subMenu->setRenderAsList(true);
  subMenu->setStyleClass("nav nav-list");
  menuItem->setSubMenu(subMenu);
  subMenu->hide();
  menu->addItem(menuItem);
  if(fs::is_directory(p)) {
    setIconTo(menuItem, "http://test.gulinux.net/css/folder.png");
    menu->itemSelected().connect([menuItem, subMenu](WMenuItem* selItem, NoClass, NoClass, NoClass, NoClass, NoClass) {
      if(selItem == menuItem) {
	if(subMenu->isVisible())
	  subMenu->animateHide(WAnimation(WAnimation::SlideInFromBottom));
	else
	  subMenu->animateShow(WAnimation(WAnimation::SlideInFromTop));
      }
    });
    listDirectoryAndRun(p, [subMenu,this](fs::path p){
      addTo(subMenu, p);
    });
  } else {
    filesHashes[Utils::hexEncode(Utils::md5(p.string()))] = p;
    setIconTo(menuItem, "http://test.gulinux.net/css/video.png");
    menuItemsPaths[menuItem] = p;
  }
}

void StreamingAppPrivate::setIconTo ( WMenuItem* item, string url ) {
    WContainerWidget *cont = dynamic_cast<WContainerWidget*>(item->itemWidget());
    WImage *icon = new WImage(url);
    icon->setStyleClass("menu_item_ico");
    cont->insertWidget(0, icon);
}

void StreamingAppPrivate::menuItemClicked ( WMenuItem* item ) {
  filesystem::path path = menuItemsPaths[item];
  queue(path);
}

void StreamingAppPrivate::queue(filesystem::path path)
{
  if(path.empty()) return;
  playlist->queue(path);
  if(!player || !player->playing()) {
    WTimer::singleShot(500, [this](WMouseEvent) {
      play(playlist->first());
    });
  }
}


void StreamingAppPrivate::play ( filesystem::path path ) {
  log("notice") << "Playing file " << path;
  if(player) {
    if(!fs::is_regular_file( path ) || ! isAllowed( path )) return;
    player->stop();
    // player->clearSources();
    delete player;
  }
  WMediaPlayer::Encoding encoding = encodingFor( path );
  if(encoding == WMediaPlayer::WEBMV || encoding == WMediaPlayer::OGV || encoding == WMediaPlayer::M4V) {
    player = new VideoJS();
  } else {
    player = new WMediaPlayerWrapper();
  }
  player->ended().connect([this](NoClass,NoClass,NoClass,NoClass,NoClass,NoClass){
    playlist->nextItem();                                                                                                                                                                                                                                                   
  });
  player->addSource(encoding, linkFor( path ));
  addSubtitlesFor(path);
  playerContainerWidget->insertWidget(0, player->widget());
  infoBox->clear();
  infoBox->addWidget(new WText(string("File: ") + path.filename().string()));
  WLink shareLink(wApp->bookmarkUrl("/") + string("?file=") + Utils::hexEncode(Utils::md5(path.string())));
  infoBox->addWidget(new WBreak() );
  infoBox->addWidget(new WAnchor(shareLink, "Link per la condivisione"));
  wApp->setTitle( path.filename().string());
  log("notice") << "using url " << linkFor( path ).url();
  WTimer::singleShot(1000, [this](WMouseEvent){
    player->play();
  });  
}

void StreamingAppPrivate::addSubtitlesFor(filesystem::path path)
{
  wApp->log("notice") << "Adding subtitles for " << path;
  fs::path subsdir(path.parent_path().string() + "/.subs");
  wApp->log("notice") << "subs path: " << subsdir;
  if(!fs::exists(subsdir)) {
    wApp->log("notice") << "subs path: " << subsdir << " not existing, exiting";
    return;
  }
  vector<filesystem::path> v;
  copy(filesystem::directory_iterator(subsdir), filesystem::directory_iterator(), back_inserter(v));
  sort(v.begin(), v.end());
  for(filesystem::path langPath: v) {
    string lang = langPath.filename().string();
    string name = "Subtitles (unknown language)";
    if(lang == "it")
      name = "Italiano";
    if(lang == "en")
      name = "English";
    wApp->log("notice") << "subs lang path: " << langPath;
    fs::path mySub = fs::path(langPath.string() + "/" + path.filename().string() + ".vtt");
    if(fs::exists(mySub)) {
      wApp->log("notice") << "sub found: lang= " << lang << ", path= " << mySub;
      player->addSubtitles(linkFor(mySub), name, lang);
    }
  }
}


StreamingApp::~StreamingApp() {
  delete d;
}

