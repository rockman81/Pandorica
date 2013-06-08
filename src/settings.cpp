#include "settings.h"
#include <Wt/WApplication>
#include <Wt/WEnvironment>
#include <Wt/WDateTime>
#include <Wt/WFileResource>
#include <Wt/Utils>
#include <Wt/WServer>

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include "player/html5player.h"
#include "player/wmediaplayerwrapper.h"
#include "private/settings_p.h"
#include "Models/setting.h"
#include "session.h"

using namespace std;
using namespace Wt;
using namespace boost;
using namespace StreamingPrivate;

namespace fs=boost::filesystem;

const std::string Settings::mediaAutoplay = "media_autoplay";
const std::string Settings::preferredPlayer = "player";
const std::string Settings::guiLanguage = "gui_language";

map<string,string> defaultValues {
  {Settings::mediaAutoplay, "autoplay_always"},
  {Settings::preferredPlayer, "html5"},
  {Settings::guiLanguage, "<browserdefault>"},
  };

Settings::Settings() : d(new SettingsPrivate(this)) {}
Settings::~Settings() { delete d; }

vector< string > Settings::mediasDirectories(Dbo::Session *session) const
{
  if(d->mediaDirectories.empty()) {
    Dbo::Transaction t(*session);
    d->mediaDirectories = Setting::values<string>("media_directories", t);
  }
  return d->mediaDirectories;
}

void Settings::addMediaDirectory(string directory, Dbo::Session* session)
{
  d->mediaDirectories.push_back(directory);
  Dbo::Transaction t(*session);
  Setting::write<string>("media_directories", d->mediaDirectories, t);
  t.commit();
}

void Settings::removeMediaDirectory(string directory, Dbo::Session* session)
{
  d->mediaDirectories.erase(remove_if(d->mediaDirectories.begin(), d->mediaDirectories.end(), [=](string d) { return d == directory; }), d->mediaDirectories.end());
  Dbo::Transaction t(*session);
  Setting::write<string>("media_directories", d->mediaDirectories, t);
  t.commit();
}



string Settings::relativePath(string mediaPath, Dbo::Session* session, bool removeTrailingSlash) const
{
  for(string mediaDirectory: mediasDirectories(session)) {
    if(mediaPath.find(mediaDirectory) == string::npos) continue;
    string relPath = boost::replace_all_copy(mediaPath, mediaDirectory, "");
    if(removeTrailingSlash && relPath[0] == '/') {
      boost::replace_first(relPath, "/", "");
    }
    return relPath;
  }
  return mediaPath;
}


string Settings::value(string cookieName)
{
  if(!d->sessionSettings[cookieName].empty())
    return d->sessionSettings[cookieName];
  
  const string *value = wApp->environment().getCookieValue(cookieName);
  if(!value) {
    wApp->log("notice") << "cookie " << cookieName << " not found; returning default";
    return defaultValues[cookieName];
  }
  wApp->log("notice") << "cookie " << cookieName << " found: " << *value;
  return *value;
}

string Settings::locale()
{
  string storedValue = value(guiLanguage);
  if(storedValue == "<browserdefault>") storedValue = wApp->environment().locale();
  return storedValue;
}


void Settings::setValue(string settingName, string value)
{
  wApp->setCookie(settingName, value, WDateTime::currentDateTime().addDays(365));
  d->sessionSettings[settingName] = value;
  if(settingName == guiLanguage) {
    wApp->setLocale(locale());
  }
}


Player* Settings::newPlayer()
{
  string playerSetting = value(Settings::preferredPlayer);
  if(playerSetting == "jplayer")
    return new WMediaPlayerWrapper();
  return new HTML5Player();
}

bool Settings::autoplay(const Media& media)
{
  string autoplay = value(Settings::mediaAutoplay);
  if(autoplay == "autoplay_always")
    return true;
  if(autoplay == "autoplay_audio_only")
    return media.mimetype().find("audio") != string::npos;
  if(autoplay == "autoplay_video_only")
    return media.mimetype().find("video") != string::npos;
  return false;
}



WLink Settings::linkFor(filesystem::path p, Dbo::Session* session)
{
  Dbo::Transaction t(*session);
  map<string,string> mediaDirectoriesDeployPaths;
  DeployType deployType = (DeployType) Setting::value<int>(Setting::deployType(), t, Internal);
  for(string directory: mediasDirectories(session)) {
    mediaDirectoriesDeployPaths[directory] = Setting::value(Setting::deployPath(directory), t, string{});
  }
  string secureDownloadPassword = Setting::value(Setting::secureDownloadPassword(), t, string{});
  bool missingSecureLinkPassword = (deployType == NginxSecureLink || deployType == LighttpdSecureDownload) && secureDownloadPassword.empty();
  
  if(missingSecureLinkPassword ) {
    log("notice") << "Deploy type is set to secureLink/secureDownload but no password is present, fallback to Internal";
    deployType = Internal;
  }
  if(deployType != Internal) {
    for(auto deployDir: mediaDirectoriesDeployPaths) {
      if(p.string().find(deployDir.first) != string::npos) {
        switch(deployType) {
          case LighttpdSecureDownload:
            return d->lightySecDownloadLinkFor(deployDir.second, deployDir.first, secureDownloadPassword, p);
          case NginxSecureLink:
            return d->nginxSecLinkFor(deployDir.second, deployDir.first, secureDownloadPassword, p);
          default:
            string relpath = p.string();
            boost::replace_all(relpath, deployDir.first + '/', deployDir.second); // TODO: consistency check
            return relpath;
        }
      }
    }
  }
  
   WLink link{new WFileResource(p.string(), wApp)};
   wApp->log("notice") << "Generated url: " << link.url();
   return link;
}

WLink Settings::shareLink(string mediaId)
{
  return {wApp->makeAbsoluteUrl(wApp->bookmarkUrl("/") + string("?media=") + mediaId)};
}


Wt::WLink SettingsPrivate::lightySecDownloadLinkFor(string secDownloadPrefix, string secDownloadRoot, string secureDownloadPassword, filesystem::path p)
{
    string filePath = p.string();
    boost::replace_all(filePath, secDownloadRoot, "");
    string hexTime = (boost::format("%1$x") %WDateTime::currentDateTime().toTime_t()) .str();
    string token = Utils::hexEncode(Utils::md5(secureDownloadPassword + filePath + hexTime));
    string secDownloadUrl = secDownloadPrefix + token + "/" + hexTime + filePath;
    wApp->log("notice") << "****** secDownload: filename= " << filePath;
    wApp->log("notice") << "****** secDownload: url= " << secDownloadUrl;
    return secDownloadUrl;
}

Wt::WLink SettingsPrivate::nginxSecLinkFor(string secDownloadPrefix, string secLinkRoot, string secureDownloadPassword, filesystem::path p)
{
    string filePath = p.string();
    boost::replace_all(filePath, secLinkRoot + '/', ""); // TODO: consistency check
    long expireTime = WDateTime::currentDateTime().addSecs(20000).toTime_t();
    string token = Utils::base64Encode(Utils::md5( (boost::format("%s%s%d") % secureDownloadPassword % filePath % expireTime).str() ), false);
    token = boost::replace_all_copy(token, "=", "");
    token = boost::replace_all_copy(token, "+", "-");
    token = boost::replace_all_copy(token, "/", "_");
    string secDownloadUrl = (boost::format("%s%s?st=%s&e=%d") % secDownloadPrefix % filePath % token % expireTime).str();
    wApp->log("notice") << "****** secDownload: filename= " << filePath;
    wApp->log("notice") << "****** secDownload: url= " << secDownloadUrl;
    return secDownloadUrl;
}

map<Settings::Icons,string> iconsMap {
  {Settings::FolderBig, "%s/icons/filesystem/directory-big.png"},
  {Settings::FolderSmall, "%s/icons/filesystem/directory-small.png"},
  {Settings::VideoFile, "%s/icons/filesystem/video.png"},
  {Settings::AudioFile, "%s/icons/filesystem/audio.png"}
};

string Settings::icon(Settings::Icons icon)
{
  string staticDeployPath{Settings::staticDeployPath()};
  return (boost::format(iconsMap[icon]) % staticDeployPath).str();
}

string staticFilesDeployPath{"/static"};
string Settings::staticDeployPath()
{
  return staticFilesDeployPath;
}

string Settings::staticPath(const string& relativeUrl)
{
  return staticDeployPath() + relativeUrl;
}

string sqlite3DatabasePath_;
void Settings::init(program_options::variables_map commandLineOptions)
{
  if( boost::any_cast<string>(commandLineOptions["server-mode"].value()) == "managed")
    staticFilesDeployPath = boost::any_cast<string>(commandLineOptions["static-deploy-path"].value()) ;
  sqlite3DatabasePath_ = boost::any_cast<string>(commandLineOptions["sqlite3-database-path"].value());
}

string Settings::sqlite3DatabasePath()
{
  return sqlite3DatabasePath_;
}

bool Settings::emailVerificationMandatory()
{
  string emailVerificationMandatorySetting{"false"};
  if(!WServer::instance()->readConfigurationProperty("email-verification-mandatory", emailVerificationMandatorySetting))
    return false;
  return emailVerificationMandatorySetting == "true";
}




