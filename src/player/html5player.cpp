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

#include "html5player.h"
#include "mediaelementjs.h"
#include "purehtml5js.h"
#include "videojs.h"
#include "utils/utils.h"
#include "Wt-Commons/wt_helpers.h"

#include <Wt/WApplication>
#include <Wt/WAnchor>
#include <Wt/WTimer>
#include <Wt/WPushButton>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include "private/html5player_p.h"
#include "utils/d_ptr_implementation.h"
using namespace Wt;
using namespace std;
using namespace boost;
using namespace WtCommons;


HTML5Player::Private::Private(HTML5Player* q): q(q), ended(q, "playbackEnded"), playing(q, "playbackStarted"),
  playerReady(q, "playbackReady"), currentTime(q, "currentTime")
{
}


PlayerJavascript::PlayerJavascript(HTML5PlayerSetup html5PlayerSetup, WObject* parent): WObject(parent), html5PlayerSetup(html5PlayerSetup)
{
}

void PlayerJavascript::runJavascript(string js)
{
  string runJS = WString(JS(
    var mediaPlayer = document.getElementById('{1}');
    {2}
  )).arg( html5PlayerSetup.playerId() ).arg(js).toUTF8();
  html5PlayerSetup.doJavascript(runJS);
}


HTML5Player::HTML5Player(HTML5Player::SubType subType, WContainerWidget* parent)
  : WContainerWidget(parent), d(this)
{
  d->templateWidget = new WTemplate();
  d->templateWidget->setTemplateText(wtr("html5player.mediatag"), Wt::XHTMLUnsafeText);
  HTML5PlayerSetup playerSetup{
    [=]() {return d->playerId(); },
    [=]() {return d->templateWidget->id(); },
    [=]() {return d->templateWidget->jsRef(); },
    [=](const string &key) { d->templateWidget->bindEmpty(key); },
    [=](const string &key, WWidget *w) { d->templateWidget->bindWidget(key, w); },
    [=](const string &javascript) { d->templateWidget->doJavaScript(javascript); },
    [=]() { return d->sources[0].type.find("video/") == string::npos ? Audio : Video; },
    [=](const string &key) { return d->templateWidget->resolveWidget(key); },
    [=]() { return d->defaultTracks; },
  };
  switch(subType) {
    case PureHTML5:
      d->playerJavascript = new PureHTML5Js(playerSetup, this);
      break;
    case MediaElementJs:
      d->playerJavascript = new ::MediaElementJs(playerSetup, this);
      break;
    case VideoJs:
      d->playerJavascript = new ::VideoJs(playerSetup, this);
      break;
  }
  d->templateWidget->setMargin(WLength::Auto, Side::Left|Side::Right);
  d->templateWidget->addFunction("sources", [this](WTemplate *t, vector<WString> args, std::ostream &output) {
    for(Source source: d->sources) {
      output << wtr("player.source").arg(source.type).arg(source.src);
    }
    return true;
  });
  d->templateWidget->addFunction("track", [this](WTemplate *t, vector<WString> args, std::ostream &output) {
    WString trackType = args[0];
    vector<Track> tracksForType = d->tracks[trackType.toUTF8()];
    for(Track track: tracksForType) {
      output << wtr("player.track").arg(track.src).arg(track.lang).arg(track.label).arg(trackType)
        .arg(d->defaultTracks[trackType.toUTF8()] == track ? "default" : "");
    }
    return true;
  });
  d->templateWidget->bindString("player.id",  d->playerId());
  d->playing.connect([=](_n6){
    log("notice") << "HTML5Player: Playing from source file " << d->sources[0].src << " started";
    d->isPlaying = true;
  });
  d->ended.connect([=](_n6) {
    log("notice") << "HTML5Player: Playing from source file " << d->sources[0].src << " ended";
    d->isPlaying = false;
  });
  d->addListener("play", d->playing.createCall());
  d->addListener("ended", d->ended.createCall());
  d->playerReady.connect([=](_n6) {
    log("notice") << "Player ready!";
    d->playerJavascript->onPlayerReady();
  });
  d->templateWidget->bindEmpty("poster_option");

  WContainerWidget *templateContainer = new WContainerWidget();
  templateContainer->addWidget(d->templateWidget);

  d->resizeLinks = WW<WContainerWidget>().css("btn-group");

  d->templateWidget->setJavaScriptMember("videoResize", d->linkResizeJS());

  d->resizeSlot.setJavaScript((
    boost::format("function(o,e) { %s.videoResize(o.attributes['resizeTo'].value); }")
    % d->templateWidget->jsRef()
  ).str());

  for(auto link: vector<pair<string,string>>{ {"small", "30"}, {"medium", "60"}, {"large", "75"}, {"full", "100"} } ) {
    WInteractWidget *button = WW<WPushButton>(wtr(string{"player_resize_"} + link.first), d->resizeLinks).css("btn btn-sm")
      .setAttribute("resizeTo", link.second);
      button->setWidth(120);
      button->clicked().connect(d->resizeSlot);
  }
  templateContainer->setMargin(WLength::Auto, Side::Left | Side::Right);
  addWidget(WW<WContainerWidget>().css("visible-lg visible-md btn-toolbar").add(d->resizeLinks));
  addWidget(templateContainer);

  d->playerJavascript->runJavascript(d->playerReady.createCall());
}

void HTML5Player::Private::setZoomScroll() {
  string jsScrollZoom = (boost::format(JS(function(o,e) {
      if( $(window).width() < %d ) return;
      var playerWidget = %s;
      var currentSize = parseInt(playerWidget.currentSizeInPercent);
      if(e.wheelDelta > 0) playerWidget.videoResize(currentSize+2);
      if(e.wheelDelta < 0) playerWidget.videoResize(currentSize-2);
    }))
    % MINIMUM_DESKTOP_SIZE
    % templateWidget->jsRef()).str();
  scrollSlot.setJavaScript(jsScrollZoom);
  templateWidget->mouseWheel().preventDefaultAction();
  templateWidget->mouseWheel().preventPropagation();
  templateWidget->mouseWheel().connect(scrollSlot);
}

string HTML5Player::Private::linkResizeJS() const
{
  return (boost::format(JS(
    function(newSize) {
      if(newSize>100) newSize = 100;
      if(newSize<5) newSize = 5;
      var playerId = '#%s';
      var myself = %s;
      var myId = '#%s';
      $(myId).width('' + newSize + '%%');
      myself.currentSizeInPercent=newSize;
      %s
    }
  ))
  % playerId()
  % templateWidget->jsRef()
  % templateWidget->id()
  % playerJavascript->resizeJs()
  ).str();
}


HTML5Player::~HTML5Player()
{
  d->playerJavascript->runJavascript("mediaPlayer.src(""); mediaPlayer.erase();");
}

void HTML5Player::setPoster(const WLink& poster)
{
  d->templateWidget->bindString("poster_option", string("poster='") + poster.url() + "'");
}



void HTML5Player::play()
{
  d->playerJavascript->runJavascript("mediaPlayer.play();");
}

void HTML5Player::stop()
{
  d->playerJavascript->runJavascript("mediaPlayer.stop();");
}
void HTML5Player::pause()
{
  d->playerJavascript->runJavascript("mediaPlayer.pause();");
}

Wt::JSignal<>& HTML5Player::ended()
{
  return d->ended;
}

Wt::WWidget* HTML5Player::widget()
{
  return this;
}

bool HTML5Player::playing()
{
  return d->isPlaying;
}

void HTML5Player::addSubtitles(const Track& track)
{
  d->tracks["subtitles"].push_back(track);
  if(!d->defaultTracks["subtitles"].isValid())
    d->defaultTracks["subtitles"] = track;
}


void HTML5Player::addSource(const Source& source)
{
  if(source.type.find("video/") != string::npos) {
    d->templateWidget->bindString("media.tagtype",  "video" );
    d->setZoomScroll();
  } else {
    d->templateWidget->bindString("media.tagtype", "audio" );
    d->resizeLinks->setHidden(true);
  }
  d->sources.push_back(source);
  d->templateWidget->bindString("html5_custom_tags", d->playerJavascript->customPlayerHTML());
}

void HTML5Player::setAutoplay(bool autoplay)
{
  d->templateWidget->setCondition("autoplay", autoplay);
}


void HTML5Player::Private::playerReadySlot()
{
}

void HTML5Player::Private::addListener(string eventName, string function)
{
  string js = WString(JS(
    mediaPlayer.addEventListener('{1}', function() { {2} });
  )).arg(eventName).arg(function).toUTF8();
  playerJavascript->runJavascript(js);
}


void HTML5Player::refresh()
{
  d->playerJavascript->runJavascript(string("$('video,audio').mediaelementplayer();"));
}


string HTML5Player::Private::playerId() const
{
  return string("player_id") + templateWidget->id();
}



