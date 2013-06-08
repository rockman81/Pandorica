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

#include "MediaScanner/mediascannerdialog.h"
#include "private/mediascannerdialog_p.h"
#include "scanmediainfostep.h"
#include "createthumbnails.h"
#include "savesubtitlestodatabase.h"
#include <session.h>
#include <mediacollection.h>
#include "Wt-Commons/wt_helpers.h"
#include <settings.h>
#include <ffmpegmedia.h>
#include <Wt/WPushButton>
#include <Wt/WStackedWidget>
#include <Wt/WTimer>
#include <Wt/WProgressBar>
#include <Wt/Dbo/backend/Postgres>
#include <Wt/WGroupBox>
#include <thread>
#include <boost/thread.hpp>

using namespace Wt;
using namespace std;
using namespace StreamingPrivate;
using namespace WtCommons;

MediaScannerDialogPrivate::MediaScannerDialogPrivate(MediaScannerDialog* q, MediaCollection *mediaCollection, Session *session, Settings* settings)
  : q(q), mediaCollection(mediaCollection), session(session), settings(settings)
{
}
MediaScannerDialogPrivate::~MediaScannerDialogPrivate()
{
}

MediaScannerDialog::MediaScannerDialog(Session *session, Settings* settings, MediaCollection* mediaCollection, WObject* parent)
    : d(new MediaScannerDialogPrivate(this, mediaCollection, session, settings))
{
  resize(700, 650);
  setWindowTitle(wtr("mediascanner.title"));
  setClosable(false);
  setResizable(true);
  d->progressBar = new WProgressBar;
  d->progressBar->addStyleClass("pull-left");
  footer()->addWidget(d->progressBar);
  footer()->addWidget(d->buttonCancel = WW<WPushButton>(wtr("button.cancel")).css("btn btn-danger").onClick([=](WMouseEvent) {
    d->canceled = true;
    reject();
  }));
  footer()->addWidget(d->buttonSkip = WW<WPushButton>(wtr("button.skip")).css("btn btn-warning").onClick([=](WMouseEvent) {
    d->skipped = true;
  }));
  footer()->addWidget(d->buttonNext = WW<WPushButton>(wtr("button.next")).css("btn btn-primary").setEnabled(false).onClick([=](WMouseEvent) { d->canContinue = true; }));
  footer()->addWidget(d->buttonClose = WW<WPushButton>(wtr("button.close")).css("btn btn-success").onClick([=](WMouseEvent) {
    accept();
  }).setEnabled(false));
  contents()->addWidget(WW<WContainerWidget>()
    .add(d->progressBarTitle = WW<WText>().css("mediascannerdialog-filename"))
    .padding(6)
    .setContentAlignment(AlignCenter)
  );
  finished().connect([=](DialogCode code, _n5) {
    scanFinished().emit();
  });
  d->steps = {
    new ScanMediaInfoStep{wApp, this},
    new SaveSubtitlesToDatabase{wApp, this},
    new CreateThumbnails{wApp, settings, this},
  };
  
  WContainerWidget* stepsContainer = new WContainerWidget;
  contents()->addWidget(stepsContainer);

  for(auto step: d->steps) {
    auto groupBox = new WGroupBox(wtr(string{"stepname."} + step->stepName() ));
    groupBox->setStyleClass("step-groupbox");
    auto container = new WContainerWidget;
    groupBox->addWidget(container);
    d->stepsContents[step] = {groupBox, container};
    stepsContainer->addWidget(groupBox);
    groupBox->setPadding(5);
  }
}

MediaScannerDialog::~MediaScannerDialog()
{
  delete d;
}

Signal<>& MediaScannerDialog::scanFinished()
{
  return d->scanFinished;
}


void MediaScannerDialog::run()
{
  show();
  d->progressBar->setMaximum(d->mediaCollection->collection().size());
  UpdateGuiProgress updateProgressBar = [=] (int progress, string text) {
    d->progressBar->setValue(progress);
    d->progressBarTitle->setText(text);
    for(auto stepContainers : d->stepsContents)
      stepContainers.second.content->clear();
    wApp->triggerUpdate();
  };
  OnScanFinish enableCloseButton = [=] {
    d->buttonClose->enable();
    d->buttonCancel->disable();
    d->progressBarTitle->setText("");
    for(auto stepContainers : d->stepsContents)
      stepContainers.second.content->clear();
    wApp->triggerUpdate();
  };
  boost::thread t( boost::bind(&MediaScannerDialogPrivate::scanMedias, d, wApp, updateProgressBar, enableCloseButton ) );
}

void MediaScannerDialogPrivate::scanMedias(Wt::WApplication* app, UpdateGuiProgress updateGuiProgress, OnScanFinish onScanFinish)
{
  canceled = false;
  uint current = 0;
  Session session;
  Dbo::Transaction transaction(session);
  mediaCollection->rescan(transaction);
  
  for(auto mediaPair: mediaCollection->collection()) {
    if(canceled)
      return;
    Media media = mediaPair.second;
    current++;
    guiRun(app, [=] {
      buttonNext->disable();
      buttonSkip->disable();
      for(auto stepContent: stepsContents)
        stepContent.second.groupBox->hide();
      updateGuiProgress(current, media.filename());
    });
    runStepsFor(media, app, session);
  }
  this_thread::sleep_for(chrono::milliseconds{10});
  guiRun(app, [=] { onScanFinish(); });
}


void MediaScannerDialogPrivate::runStepsFor(Media media, WApplication* app, Session& session)
{
  canContinue = false;
  skipped = false;
  FFMPEGMedia ffmpegMedia{media};
  Dbo::Transaction t(session);
  for(MediaScannerStep *step: steps) {
    step->run(&ffmpegMedia, media, stepsContents[step].content, &t);
  }
  while(!canContinue && !canceled && !skipped) {
    bool stepsAreSkipped = true;
    bool stepsAreFinished = true;

    
    for(MediaScannerStep *step: steps) {
      auto stepResult = step->result();
      if(stepResult != MediaScannerStep::Skip)
        guiRun(app, [=] { stepsContents[step].groupBox->show(); });
      stepsAreSkipped &= stepResult == MediaScannerStep::Skip;
      stepsAreFinished &= stepResult == MediaScannerStep::Skip || stepResult == MediaScannerStep::Done;
      
      if(stepResult == MediaScannerStep::Redo)
        step->run(&ffmpegMedia, media, stepsContents[step].content, &t);
    }
    canContinue |= stepsAreSkipped;
    guiRun(app, [=] {
      buttonSkip->enable();
      buttonNext->setEnabled(stepsAreFinished && ! stepsAreSkipped);
      wApp->triggerUpdate();
    });
    if(!canContinue && !canceled && !skipped)
      this_thread::sleep_for(chrono::milliseconds{50});
  }
  if(canceled)
    return;
  if(!skipped) {
    for(MediaScannerStep *step: steps) {
      step->save(&t);
    }
    t.commit();
  }
}

