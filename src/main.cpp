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


#include <iostream>

#include "pandorica.h"
#include "session.h"
#include "Models/models.h"
#include "Wt-Commons/compositeresource.h"
#include <Wt-Commons/quitresource.h>
#include "settings.h"


#include <Wt/WServer>
#include <Wt/WFileResource>
#include <Wt/WIOService>
#include <boost/thread.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#ifdef IMAGE_USE_GRAPHICSMAGICK
#include <Magick++.h>
#endif

#include <fstream>
#include <csignal>
// #include "migrations/migrations.h"

#include <Wt-Commons/wt_helpers.h>
#ifdef HAVE_QT
#include <QApplication>
#include "qttrayicon.h"
#endif
#include <boost/optional.hpp>
#include "Wt-Commons/wobjectscope.h"
#include <boost/format.hpp>
using namespace Wt;
using namespace std;
namespace fs = boost::filesystem;
using namespace WtCommons;
namespace po = boost::program_options;

std::vector<WApplication*> instances;
WApplication *createApplication( const WEnvironment &env )
{
  auto app = new Pandorica( env );
  WObjectScope *removeApp = new WObjectScope( [ = ] {
    instances.erase( std::remove( begin(instances), end(instances), app ) );
  }, app );
  instances.push_back( app );
  return app;
}

extern "C" {
#include <libavcodec/avcodec.h>    // required headers
#include <libavformat/avformat.h>
}


#ifdef BOOST_OSTREAM_OPERATOR_OPTIONAL_FIX
// compilation fix on archlinux... why?
namespace boost {
  template<>
std::basic_ostream<char, std::char_traits<char> >& operator<< <char, std::char_traits<char>, std::string>(std::basic_ostream<char, std::char_traits<char> >&, boost::optional<std::string> const&) {
}
}
#endif



#ifdef WIN32_GUI_APP
stringstream pErrStream;
stringstream pOutStream;
QApplication *msgBoxApp = 0;
std::ostream &pErr() {
  return pErrStream;
}
std::ostream &pOut() {
  return pOutStream;
}
#include <QMessageBox>
class MessagesPrinter {
  public:
    ~MessagesPrinter();
};

MessagesPrinter::~MessagesPrinter() {
  char *argv[1] {"dummy"};
  int argc = 1;
  bool appWasEmpty = false;
  if(!msgBoxApp) {
    msgBoxApp = new QApplication(argc, argv);
    appWasEmpty = true;
  }
  if(!pErrStream.str().empty()) {
    QMessageBox::critical(0, "Pandorica: Error", QString::fromStdString(pErrStream.str()));
  }
  if(!pOutStream.str().empty()) {
    QMessageBox::information(0, "Pandorica", QString::fromStdString(pOutStream.str()));
  }
  if(appWasEmpty)
    delete msgBoxApp;
}

#else
std::ostream &pErr() {
  return cerr;
}
std::ostream &pOut() {
  return cout;
}
#endif

map<string, string> extensionsMimetypes
{
  { ".css", "text/css" },
  { ".less", "text/css" },
  { ".png", "image/png" },
  { ".jpg", "image/jpeg" },
  { ".jpeg", "image/jpeg" },
  { ".gif", "image/gif" },
  { ".js", "application/javascript" },
  { ".svg", "image/svg+xml" },
  { ".swf", "application/x-shockwave-flash" },
  { ".xap", "application/x-silverlight-app" },
};


struct CmdOptions
{
  int argc;
  char **argv;
  static CmdOptions from( vector<string> strings )
  {
    char **argv_temp = new char*[strings.size()];
    int current = 0;

    for( string argument : strings )
    {
      char *pc = new char[argument.size() + 1];
      strcpy( pc, argument.c_str() );
      argv_temp[current++] = pc;
    }

    return { boost::lexical_cast<int>( strings.size() ), argv_temp };
  }
};

template<class T> T optionValue( po::variables_map &options, string optionName )
{
  return boost::any_cast<T>( options[optionName].value() );
}

bool checkForWrongOptions( bool errorCondition, string errorMessage )
{
  if( errorCondition )
    pErr() << "Error!\t" << errorMessage << "\n\n";

  return errorCondition;
}

bool initServer( int argc, char **argv, WServer &server, po::variables_map &vm )
{
  string homeVariablename = 
#ifdef WIN32
"USERPROFILE"
#else
"HOME"
#endif
;
  char *homeDirectory = getenv(homeVariablename.c_str());
  bool have_home=homeDirectory;
  if(!homeDirectory) {
    pErr() << homeVariablename << " variable not found" << endl;
  }
  po::options_description pandorica_visible_options( "Pandorica Options" );
  po::options_description pandorica_general_options( "General" );
  pandorica_general_options.add_options()
  ( "http-address", po::value<string>()->default_value( "0.0.0.0" ), "http address for listening" )
  ( "http-port", po::value<int>()->default_value( 38000 ), "http port for listening" )
  ( "config,c", po::value<string>()->default_value( (boost::format( "%s/pandorica.xml" ) % SHARED_FILES_DIR ).str() ), "Pandorica XML Configuration file (wt_config.xml)" )
  ( "server-mode", po::value<string>()->default_value( "standalone" ), "Server run mode.\nAllowed modes are:\n\
  \t* standalone: Pandorica will run without an external http server.\n\
  \t* managed:    Pandorica will run inside an http server (apache, lighttp, etc). This means you have to take care of shared resource files." )
#ifdef HAVE_QT
  ( "disable-tray", "Disable system tray icon" )
#endif
  ( "help", "shows Pandorica options" )
  ( "help-full", "shows full options list, including Wt." )
  ;



  string configDirectory ="/etc/Pandorica";
  if(have_home) {
    configDirectory = string{homeDirectory} + "/.config/Pandorica";
  }
  try {
    fs::create_directories( configDirectory );
  }
    catch( std::exception &e ) {
  }

  po::options_description pandorica_db_options( "Database Options" );
  pandorica_db_options.add_options()
  ( "sqlite3-databases-path", po::value<string>()->default_value( configDirectory ), "Path where sqlite3 databases will be stored." )
  ( "dump-schema", po::value<string>(), "dumps the schema to a file (argument) and exits, useful to manually execute migrations (use '-' to write to stdout)." )
  ;
  po::options_description pandorica_managed_options( "Managed Mode Options" );
  pandorica_managed_options.add_options()
  ( "static-deploy-path", po::value<string>(), (string{"Path in your web server pointing to Pandorica static files directory ("} + Settings::sharedFilesDir("/static") + ")" ).c_str() )
  ;

  pandorica_visible_options.add( pandorica_general_options ).add( pandorica_db_options ).add( pandorica_managed_options );
  po::options_description pandorica_invisible_options;
  pandorica_invisible_options.add_options()
  ( "docroot", po::value<string>()->default_value( (boost::format( "%s;/resources" ) % WT_SHARED_FILES_DIR ).str() ) );
  po::options_description pandorica_all_options;
  pandorica_all_options.add( pandorica_visible_options ).add( pandorica_invisible_options );

  auto po_parsed = po::command_line_parser( argc, argv ).options( pandorica_all_options ).allow_unregistered().run();
  po::store( po_parsed, vm );
  vector<string> wt_options = po::collect_unrecognized( po_parsed.options, boost::program_options::include_positional );
  wt_options.insert( begin(wt_options), argv[0] );
  bool optionsAreWrong = false;

  optionsAreWrong |= checkForWrongOptions( optionValue<string>( vm, "server-mode" ) != "managed" && optionValue<string>( vm, "server-mode" ) != "standalone", "server-mode must be one of 'managed' or 'standalone'" );
  optionsAreWrong |= checkForWrongOptions( optionValue<string>( vm, "server-mode" ) == "managed" && !vm.count( "static-deploy-path" ), "You must set static-deploy-path in managed server mode." );

  if( vm.count( "help" ) || optionsAreWrong )
  {
    pOut() << pandorica_visible_options;
    return false;
  }

  if( vm.count( "help-full" ) )
  {
    pOut() << pandorica_visible_options << "\n\t\t***** Wt Options *****\n";
    wt_options.push_back( "--help" );
  }

  wt_options.push_back( "--http-address" );
  wt_options.push_back( optionValue<string>( vm, "http-address" ) );
  
  wt_options.push_back( "--config" );
  wt_options.push_back( optionValue<string>( vm, "config" ) );


  wt_options.push_back( "--docroot" );
  wt_options.push_back( optionValue<string>( vm, "docroot" ) );

  wt_options.push_back( "--http-port" );
  wt_options.push_back( boost::lexical_cast<string>( optionValue<int>( vm, "http-port" ) ) );
  pErr() << "wt options: ";

  for( string option : wt_options )
    pErr() << " " << option;

  pErr() << '\n';

  CmdOptions options = CmdOptions::from( wt_options );

  server.setServerConfiguration( options.argc, options.argv, WTHTTP_CONFIGURATION );
  return true;
}

bool addStaticResources( WServer &server )
{
  CompositeResource *staticResources = new CompositeResource();
  string staticDirectory = Settings::sharedFilesDir("/static");
  if( !fs::is_directory( staticDirectory ) )
  {
    pErr() << "Error! Shared files directory " << staticDirectory << " doesn't exist. Perhaps you didn't install Pandorica correctly?\n";
    return false;
  }

  fs::recursive_directory_iterator it( staticDirectory, fs::symlink_option::recurse );

  while( it != fs::recursive_directory_iterator() )
  {
    if( fs::is_regular( *it ) )
    {
      string filePath {it->path().string()};
      string fileUrl {filePath};
      boost::replace_first( fileUrl, staticDirectory, "" );
      boost::replace_all(fileUrl, "\\", "/");
      auto resource = new WFileResource {filePath, staticResources};

      if( !extensionsMimetypes[it->path().extension().string()].empty() )
        resource->setMimeType( extensionsMimetypes[it->path().extension().string()] );

      staticResources->add( fileUrl, resource );
    }

    it++;
  }

  server.addResource( staticResources, Settings::staticDeployPath() );
  auto oauth_icons_path = boost::filesystem::path(SHARED_FILES_DIR) / "static" / "icons" / "oauth";
  server.addResource(new WFileResource("image/png", (oauth_icons_path / "oauth-google.png").string()), "/css/oauth-google.png");
  server.addResource(new WFileResource("image/png", (oauth_icons_path / "oauth-facebook.png").string()), "/css/oauth-facebook.png");
  return true;
}


int main( int argc, char **argv, char **envp )
{
#ifdef WIN32_GUI_APP
  MessagesPrinter messagesPrinter;
#endif
  try
  {
    av_register_all();
    avcodec_register_all();
    av_register_all();
    avformat_network_init();

#ifdef IMAGE_USE_GRAPHICSMAGICK
    Magick::InitializeMagick( *argv );
#endif
    WServer server( argv[0] );
    po::variables_map vm;

    if( !initServer( argc, argv, server, vm ) )
      return 1;

    // WIOService ioService;
    //ioService.setThreadCount(10);
    //server.setIOService(ioService);
    //ioService.start();
    Settings::init( vm );
    server.addEntryPoint( Application, createApplication );
    string quitPassword = Setting::value<string>(Setting::QuitPassword);
    if( vm.count( "dump-schema" ) )
    {
      Session session( false );
      string schemaPath {boost::any_cast<string>( vm["dump-schema"].value() )};
      if(schemaPath == "-") {
        pOut() << session.tableCreationSql();
      } else {
        ofstream schema;
        schema.open( schemaPath );
        schema << session.tableCreationSql();
        schema.close();
        pOut() << "Schema correctly wrote to " << schemaPath << '\n';
      }
      return 0;
    }

    auto checkForActiveSessions = [ = ]()
    {
      return instances.size() == 0;
    };

    server.addResource( ( new QuitResource {quitPassword, checkForActiveSessions} ), "/graceful-quit" );
    server.addResource( ( new QuitResource {quitPassword, checkForActiveSessions} )->setRestart( argc, argv, envp ), "/graceful-restart" );
    server.addResource( ( new QuitResource {quitPassword} ), "/force-quit" );
    server.addResource( ( new QuitResource {quitPassword} )->setRestart( argc, argv, envp ), "/force-restart" );

    if( optionValue<string>( vm, "server-mode" ) == "standalone" && ! addStaticResources( server ) )
    {
      return 1;
    }

    boost::thread t( [&server]
    {
      boost::this_thread::sleep_for( boost::chrono::milliseconds{30000} );

      while( server.isRunning() )
      {
        server.expireSessions()
        ;
        boost::this_thread::sleep_for( boost::chrono::milliseconds {30000} );
      }
    } );

    Session::configureAuth();

#ifdef HAVE_QT
#ifndef WIN32
    string display = getenv( "DISPLAY" ) ? string {getenv( "DISPLAY" )} :
                     string {};
    bool haveDisplay = !display.empty();
    if( !haveDisplay )
      pErr() << "Found no X11 display available, running without Qt Tray Icon\n";
#else
    bool haveDisplay = true;
#endif
    if( ! vm.count( "disable-tray" ) && haveDisplay )
    {
      QApplication app( argc, argv );
#ifdef WIN32_GUI_APP
      msgBoxApp = &app;
#endif
      QtTrayIcon icon( server );
      app.exec();
      return 0;
    }

#endif
    if( server.start() )
    {
      WServer::waitForShutdown();
      server.stop();
    }
  }
  catch
    ( WServer::Exception &e )
  {
    pErr() << e.what() << endl;
  }
  catch
    ( std::exception &e )
  {
    pErr() << "exception: " << e.what() << endl;
  }
}



