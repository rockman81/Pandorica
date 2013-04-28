/*
 * Copyright 2013 Marco Gulino <email>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include "session.h"
#include "mediacollectionmanagerdialog.h"
#include <Wt/WText>
#include <Wt/WApplication>
#include <Wt/WProgressBar>
#include "Wt-Commons/wt_helpers.h"
#include "mediacollection.h"
#include "ffmpegmedia.h"
#include "mediaattachment.h"
#include "session.h"
#include "sessioninfo.h"
#include "sessiondetails.h"
#include "comment.h"
#include <boost/thread.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <Wt/WIOService>
#include <Wt/WTimer>
#include <Wt/WLineEdit>
#include <Wt/WPushButton>
#include <Wt/WLabel>
#include <thread>
#include <boost/regex.hpp>

using namespace std;
using namespace Wt;

class EditMediaTitle : public WContainerWidget {
public:
  WPushButton *okButton;
  WLineEdit *editTitle;
  EditMediaTitle(WContainerWidget *parent = 0) : WContainerWidget(parent) {
    setPadding(10);
    WLabel *label = new WLabel("Title");
    addWidget(editTitle = WW<WLineEdit>().css("input-xxlarge"));
    label->setBuddy(editTitle);
    addWidget(okButton = WW<WPushButton>("OK").css("btn btn-primary"));
    setStyleClass("form-inline");
  };
};


MediaCollectionManagerDialog::MediaCollectionManagerDialog(Session *session, MediaCollection *mediaCollection, Wt::WObject* parent)
  : WDialog(parent), session(session), mediaCollection(mediaCollection)
{
  setWindowTitle("Scanner");
  setClosable(false);
}

MediaCollectionManagerDialog::~MediaCollectionManagerDialog()
{
}


void MediaCollectionManagerDialog::run()
{
  show();
  WProgressBar *progressBar = new WProgressBar();
  WText *text = new WText;
  contents()->addWidget(text);
  contents()->addWidget(new WBreak);
  contents()->addWidget(progressBar);
  customContent = new WContainerWidget();
  contents()->addWidget(customContent);
  auto startScanning = [=] (WMouseEvent) {
    progressBar->setMaximum(mediaCollection->collection().size());
    boost::thread t(boost::bind(&MediaCollectionManagerDialog::scanMediaProperties, this, wApp, [=](int progress, string file) {
      customContent->clear();
      progressBar->setValue(progress);
      text->setText(file);
    }));
  };
  WTimer::singleShot(100, startScanning);
}

list<pair<string,string>> filenameToTileHints {
  // Extensions
  {"\\.mkv", ""},
  {"\\.mp4", ""},
  {"\\.m4v", ""},
  {"\\.ogg", ""},
  {"\\.mp3", ""},
  {"\\.flv", ""},
  {"\\.webm",""},
  // Other characters
  {"\\.", " "}, {"_", " "}, {"-", " "},
};

string titleHint(string filename) {
  for(auto hint: filenameToTileHints) {
    filename = boost::regex_replace(filename, boost::regex{hint.first}, hint.second);
  }
  while(filename.find("  ") != string::npos)
    boost::replace_all(filename, "  ", " ");
  boost::algorithm::trim(filename);
  return filename;
}

#define guiRun(f) WServer::instance()->post(app->sessionId(), f)
void MediaCollectionManagerDialog::scanMediaProperties(WApplication* app, UpdateGuiProgress updateGuiProgress)
{
  int current{0};
  for(auto media: mediaCollection->collection()) {
    this->title = "";
    this->secsRemaining = 10;
    Dbo::Transaction t{*session};
    guiRun([=] { customContent->clear(); });
    
    current++;
    guiRun(boost::bind(updateGuiProgress, current, media.second.filename()));
    MediaPropertiesPtr mediaPropertiesPtr = session->find<MediaProperties>().where("media_id = ?").bind(media.first);
    if(mediaPropertiesPtr)
      continue;
    
    FFMPEGMedia ffmpegMedia{media.second};
    string title = ffmpegMedia.metadata("title").empty() ? titleHint(media.second.filename()) : ffmpegMedia.metadata("title");
    guiRun([=] {
      editTitleWidgets(title);
    });
    
    
    while(secsRemaining != 0) {
      boost::this_thread::sleep(boost::posix_time::millisec(50));
    }
    pair<int, int> resolution = ffmpegMedia.resolution();
    auto mediaProperties = new MediaProperties{media.first, this->title, media.second.fullPath(), ffmpegMedia.durationInSeconds(), boost::filesystem::file_size(media.second.path()), resolution.first, resolution.second};
    session->add(mediaProperties);
    t.commit();
  }
  guiRun([=] { setClosable(true);});
}

void MediaCollectionManagerDialog::editTitleWidgets(string suggestedTitle)
{
  customContent->clear();
  customContent->addWidget(editMediaTitle = new EditMediaTitle());
  editMediaTitle->editTitle->setText(suggestedTitle);
  
  
  WTimer *timer = new WTimer(editMediaTitle);
  timer->setInterval(1000);
  auto okClicked = [=] {
    timer->stop();
    title = editMediaTitle->editTitle->text().toUTF8();
    secsRemaining = 0;
    editMediaTitle->okButton->disable();
  };
  timer->timeout().connect([=](WMouseEvent) {
    secsRemaining--;
    if(secsRemaining > 0)
      editMediaTitle->okButton->setText(WString("OK ({1})").arg(secsRemaining));
    else
      okClicked();
  });
  editMediaTitle->okButton->clicked().connect([=](WMouseEvent) {
    okClicked();
  });
  
  auto suspendTimer = [=] {
    timer->stop();
    editMediaTitle->okButton->setText("OK");
    secsRemaining = -1;
  };
  
  editMediaTitle->editTitle->clicked().connect([=](WMouseEvent) { suspendTimer(); });
  editMediaTitle->editTitle->focussed().connect([=](_n1) { suspendTimer(); });
  editMediaTitle->editTitle->keyWentUp().connect([=](WKeyEvent e) { 
    if(e.key() == Wt::Key_Enter)
      okClicked();
    else
      suspendTimer();
  });
  
  timer->start();
}


