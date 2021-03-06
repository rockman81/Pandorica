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


#ifndef PLAYLIST_PRIVATE
#define PLAYLIST_PRIVATE
#include <Wt/WSignal>
#include <Wt/WAnchor>
#include "media/media.h"
#include "playlist.h"

namespace Wt {
class WWidget;
class WContainerWidget;
}

  class QueueItem : public Wt::WContainerWidget, public PlaylistItem {
  public:
    QueueItem(Media media, std::list<QueueItem*> &queue, Wt::WContainerWidget *container, Session *session, WContainerWidget* parent = 0);
    Wt::Signal<PlaylistItem*> &play() { return playSignal; }
    bool isCurrent();
    void setCurrent() { setActive(true); }
    void unsetCurrent() { setActive(false); }
    void setActive(bool active);
  private:
    Wt::Signal<PlaylistItem*> playSignal;
    Wt::WImage *removeButton;
    Wt::WImage *upButton;
    Wt::WImage *downButton;
  };
  
  class Playlist::Private {
  public:
    enum Direction { Forwards, Backwards };
    Private(Playlist *playlist, Session *session);
    Session *session;
    std::list<QueueItem*> internalQueue;
    Wt::Signal<PlaylistItem*> playSignal;
    Wt::WContainerWidget *container;
    std::function<void(bool)> setPlaylistVisible;
    void playlistIncrement(Direction direction);
  private:
    Playlist *q;
  };
#endif
