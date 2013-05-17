#include "mediacollection.h"
#include "session.h"
#include <Wt/Utils>
#include <Wt/WApplication>
#include <Wt/WServer>
#include <thread>
#include "private/mediacollection_p.h"

using namespace Wt;
using namespace std;
using namespace boost;
using namespace Wt::Utils;
using namespace StreamingPrivate;

namespace fs = boost::filesystem;

MediaCollection::MediaCollection(string basePath, Session* session, WApplication* parent)
    : WObject(parent), d(new MediaCollectionPrivate(basePath, session, parent))
{
}

void MediaCollection::rescan()
{
    Dbo::Transaction t(*d->session);
    d->allowedPaths = d->session->user()->allowedPaths();
    d->collection.clear();
    d->listDirectory(d->basePath);
    // TODO: muovere su nuovo thread, al momento pare non andare (eccezione postgres)
//   WServer::instance()->post(d->app->sessionId(), [=] {
    d->scanned.emit();
//     wApp->triggerUpdate();
//   });
}

bool MediaCollectionPrivate::isAllowed(filesystem::path path)
{
    for(string p: allowedPaths) {
        if(path.string().find(p) != string::npos)
            return true;
    }
    return false;
}


void MediaCollectionPrivate::listDirectory(filesystem::path path)
{
    vector<fs::directory_entry> v;
    try {
      copy(fs::recursive_directory_iterator(path, fs::symlink_option::recurse), fs::recursive_directory_iterator(), back_inserter(v));
        sort(v.begin(), v.end());
        for(fs::directory_entry entry: v) {
          fs::path path = entry.path();
          Media media {path};
          if(fs::is_regular_file(path) && media.mimetype() != "UNSUPPORTED" && isAllowed(path)) {
            collection[media.uid()] = media;
          }
        }
    } catch(std::exception &e) {
        log("error") << "Error trying to add path " << path << ": " << e.what();
    }
}

map< string, Media > MediaCollection::collection() const
{
    return d->collection;
}

Media MediaCollection::media(string uid) const
{
    return d->collection[uid];
}

Signal<>& MediaCollection::scanned()
{
    return d->scanned;
}


MediaCollection::~MediaCollection()
{
    delete d;
}

filesystem::path MediaCollection::rootPath() const
{
    return d->basePath;
}
