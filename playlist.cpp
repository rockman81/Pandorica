#include "playlist.h"
#include "session.h"
#include <Wt/WApplication>
#include <Wt/WAnchor>
#include "Wt-Commons/wt_helpers.h"

using namespace Wt;
using namespace std;
using namespace boost;
namespace fs = boost::filesystem;


Playlist::Playlist(Session* session, WContainerWidget* parent): WContainerWidget(parent), session(session)
{
  setList(true);
  addStyleClass("nav nav-pills nav-stacked");
}

Playlist::~Playlist()
{

}


Signal< Media >& Playlist::next()
{
  return _next;
}

Media Playlist::first()
{
  return internalQueue.front().second;
}

void Playlist::nextItem(WWidget* itemToPlay)
{
  wApp->log("notice") << "itemToPlay==" << itemToPlay;
  if(internalQueue.empty()) return;

  auto itemToSkip = internalQueue.begin();
  while(itemToSkip->first != itemToPlay) {
    removeWidget(itemToSkip->first);
    delete itemToSkip->first;
    itemToSkip = internalQueue.erase(itemToSkip);
    if(!itemToPlay) break;
  }
  
  wApp->log("notice") << "outside the loop: internalQueue.size(): " << internalQueue.size();
  if(internalQueue.empty()) return;
  QueueItem next = *internalQueue.begin();
  _next.emit(next.second);
}

void Playlist::reset()
{
  clear();
  internalQueue.clear();
}



void Playlist::queue(Media media)
{
  WAnchor* playlistEntry = WW<WAnchor>("", media.title(session)).css("link-hand");
  playlistEntry->addWidget(new WBreak());
  playlistEntry->clicked().connect([this,media, playlistEntry](WMouseEvent&){ nextItem(playlistEntry); });
  addWidget(WW<WContainerWidget>().add(playlistEntry));
  internalQueue.push_back(QueueItem(playlistEntry, media));
}