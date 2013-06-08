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

#include "createthumbnails.h"
#include "private/createthumbnails_p.h"

#include "ffmpegmedia.h"
#include "session.h"
#include <Wt/WProgressBar>
#include <Wt/WText>
#include <Wt/WPushButton>
#include <Wt/WTime>
#include <Wt/WMemoryResource>
#include <Wt/WImage>
#include "Wt-Commons/wt_helpers.h"
#include <mediacollection.h>
#include <player/html5player.h>
#include <settings.h>
#include <boost/thread.hpp>
#include <boost/format.hpp>

#include <libffmpegthumbnailer/videothumbnailer.h>
#include <libffmpegthumbnailer/videothumbnailerc.h>
#include <libffmpegthumbnailer/imagetypes.h>
#include <chrono>
#include <random>
#include <thread>
#include <Wt/WFileUpload>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>

#include "Models/models.h"

#include <Magick++/Image.h>
#include <Magick++/Geometry.h>

using namespace Wt;
using namespace std;
using namespace std::chrono;
using namespace ffmpegthumbnailer;
using namespace Magick;
using namespace WtCommons;

using namespace StreamingPrivate;

vector<uint8_t> vectorFrom(Blob blob) {
  return { (char*) blob.data(), (char*) blob.data() + blob.length()};
}

time_point<high_resolution_clock> serverStartTimeForRandomSeeding{high_resolution_clock::now()};
mt19937_64 randomEngine{(uint64_t) serverStartTimeForRandomSeeding.time_since_epoch().count()};

CreateThumbnailsPrivate::CreateThumbnailsPrivate(WApplication* app, Settings* settings, CreateThumbnails* q)
  : app(app), settings(settings), q(q)
{
}
CreateThumbnailsPrivate::~CreateThumbnailsPrivate()
{
}

CreateThumbnails::CreateThumbnails(WApplication* app, Settings* settings, WObject* parent)
  : WObject(parent), d(new CreateThumbnailsPrivate{app, settings, this})
{

}

CreateThumbnails::~CreateThumbnails()
{
    delete d;
}


ImageUploader::ImageUploader(WContainerWidget* parent)
  : WContainerWidget(parent)
{
  reset();
}

void ImageUploader::reset() {
  clear();
  WContainerWidget *hidden = new WContainerWidget();
  linkContainer = WW<WContainerWidget>().setContentAlignment(AlignCenter);
  WAnchor *uploadLink = new WAnchor("", wtr("mediascannerdialog.thumbnail.upload.label"));
  uploadLink->clicked().connect([=](WMouseEvent) {
    linkContainer->hide();
    addWidget(hidden);
  });
  linkContainer->addWidget(uploadLink);
  addWidget(linkContainer);
  upload = new WFileUpload();
  upload->setProgressBar(new WProgressBar());
  hidden->addWidget(upload);
  upload->uploaded().connect(this, &ImageUploader::uploaded);
  upload->changed().connect([=](_n1){
    if(upload->canUpload()) {
      upload->disable();
      upload->upload();
    }
  });
  upload->fileTooLarge().connect([=](int64_t, _n5) {
    reset();
    linkContainer->addWidget(WW<WContainerWidget>().add(new WText{wtr("mediascannerdialog.thumbnail.upload.toobig")}).css("alert"));
  });
  setStyleClass("form-inline");
}


void ImageUploader::uploaded() {
  try {
    log("notice") << "uploaded file to " << upload->spoolFileName();
    Magick::Image fullImage(upload->spoolFileName());
    Blob blob;
    fullImage.quality(100);
    fullImage.write(&blob);
    _previewImage.emit(blob);
    reset();
  } catch(std::exception &e) {
    log("error") << "Error decoding image with imagemagick: " << e.what();
    reset();
    linkContainer->addWidget(WW<WContainerWidget>().add(new WText{wtr("mediascannerdialog.thumbnail.upload.error")}).css("alert alert-error"));
  }
}


void CreateThumbnails::run(FFMPEGMedia* ffmpegMedia, Media media, WContainerWidget* container, Dbo::Transaction* transaction, MediaScannerStep::ExistingFlags onExisting)
{
  d->result = Waiting;
  if(onExisting == SkipIfExisting && transaction->session().query<int>("SELECT COUNT(id) FROM media_attachment WHERE media_id = ? AND type = 'preview'").bind(media.uid()) > 0) {
    d->result = Skip;
    return;
  }

  d->currentMedia = media;
  d->currentFFMPEGMedia = ffmpegMedia;
  if(!ffmpegMedia->isVideo()) {
    d->addImageChooser(container);
    return;
  }
  d->currentPosition = d->randomPosition(ffmpegMedia);
  d->chooseRandomFrame(container);
  d->result = Done;
}

void CreateThumbnailsPrivate::addImageChooser(Wt::WContainerWidget* container)
{
  guiRun(app, [=]{
    container->clear();
    ImageUploader *imageUploader = new ImageUploader;
    
    container->addWidget(imageUploader);
    WContainerWidget *imageContainer = new WContainerWidget;
    imageContainer->setContentAlignment(AlignCenter);
    container->addWidget(imageContainer);
    imageUploader->previewImage().connect([=](Blob blob, _n5) {
      delete thumbnail;
      imageContainer->clear();
      fullImage = blob;
      thumbnail = new WMemoryResource{"image/png", vectorFrom(resize(blob, IMAGE_SIZE_PREVIEW)), container};
      imageContainer->addWidget(new WImage{thumbnail});
      result = MediaScannerStep::Done;
    });
    wApp->triggerUpdate();
  });
    
}


void CreateThumbnailsPrivate::chooseRandomFrame(WContainerWidget* container)
{
  delete thumbnail;
  int fullSize = max(currentFFMPEGMedia->resolution().first, currentFFMPEGMedia->resolution().second);
  thumbnailFor(fullSize, 10);

  thumbnail = new WMemoryResource{"image/png", vectorFrom(resize(fullImage, IMAGE_SIZE_PREVIEW)), container};
  guiRun(app, [=]{
    container->clear();
    ImageUploader *imageUploader = new ImageUploader;

    container->addWidget(imageUploader);
    WImage *imagePreview = WW<WImage>(thumbnail).css("link-hand").onClick([=](WMouseEvent) {
      result = MediaScannerStep::Redo;
      redo.emit();
    });
    container->addWidget(WW<WContainerWidget>()
      .add(WW<WText>(wtr("mediascannerdialog.thumbnaillabel")).css("small-text").setInline(false))
      .add(imagePreview)
      .setContentAlignment(AlignCenter));
    imageUploader->previewImage().connect([=](Blob blob, _n5) {
      delete thumbnail;
      fullImage = blob;
      thumbnail = new WMemoryResource{"image/png", vectorFrom(resize(blob, IMAGE_SIZE_PREVIEW)), container};
      imagePreview->setImageLink(thumbnail);
    });
    wApp->triggerUpdate();
  });
}

Magick::Blob StreamingPrivate::CreateThumbnailsPrivate::resize(Blob blob, int size, uint quality)
{
  Magick::Image image{blob};
  Blob output;
  image.sample({size, size});
  image.quality(quality);
  image.write(&output);
  return output;
}


Signal<>& CreateThumbnails::redo()
{
  return d->redo;
}



ThumbnailPosition CreateThumbnailsPrivate::randomPosition(FFMPEGMedia* ffmpegMedia)
{
  auto randomNumber = randomEngine();
  
  if(ffmpegMedia->durationInSeconds() < 100 ) {
    int percent = randomNumber % 100;
    if(percent < 10) percent += 10;
    if(percent > 80) percent -= 20;
    return {percent};
  }
  int percent{0};
  int position{0};
  while(percent < 10 || percent > 80) {
    randomNumber = randomEngine();
    position = randomNumber % ffmpegMedia->durationInSeconds();
    percent = position * 100.0 / ffmpegMedia->durationInSeconds();
  }
  return ThumbnailPosition::from(position);;
}

ThumbnailPosition ThumbnailPosition::from(int timeInSeconds)
{
  int remaining = 0;
  int hours = timeInSeconds / 3600;
  timeInSeconds %= 3600;
  int minutes = timeInSeconds / 60;
  timeInSeconds %= 60;
  string currentTimeStr = (boost::format("%.2d:%.2d:%.2d") %hours %minutes %timeInSeconds).str();
  return {-1, currentTimeStr};
}


MediaScannerStep::StepResult CreateThumbnails::result()
{
  return d->result;
}

void CreateThumbnails::save(Dbo::Transaction* transaction)
{
  log("notice") << "Result: " << d->result << " (done= " << Done << ", Redo = " << Redo << ")";
  if(d->result != Done)
    return;
  log("notice") << "Deleting old data from media_attachment for media_id " << d->currentMedia.uid();
  transaction->session().execute("DELETE FROM media_attachment WHERE media_id = ? AND type = 'preview'").bind(d->currentMedia.uid());
  MediaAttachment *fullAttachment = new MediaAttachment{"preview", "full", "", d->currentMedia.uid(), "image/png", vectorFrom(d->fullImage) };
  MediaAttachment *thumbnailAttachment = new MediaAttachment{"preview", "thumbnail", "", d->currentMedia.uid(), "image/png", vectorFrom(d->resize(d->fullImage, IMAGE_SIZE_THUMB, 60)) };
  MediaAttachment *playerAttachment = new MediaAttachment{"preview", "player", "", d->currentMedia.uid(), "image/png", vectorFrom(d->resize(d->fullImage, IMAGE_SIZE_PLAYER)) };
  transaction->session().add(fullAttachment);
  transaction->session().add(thumbnailAttachment);
  transaction->session().add(playerAttachment);
  log("notice") << "Images saved";
  d->currentMedia = Media::invalid();
  d->currentFFMPEGMedia = 0;
  d->result = Waiting;
}


void CreateThumbnailsPrivate::thumbnailFor(int size, int quality)
{
  video_thumbnailer *thumbnailer = video_thumbnailer_create();
  thumbnailer->overlay_film_strip = currentMedia.mimetype().find("video") == string::npos ? 0 : 1;
  
  if(currentPosition.percent>0)
    thumbnailer->seek_percentage = currentPosition.percent;
  else
    thumbnailer->seek_time = (char*)currentPosition.timing.c_str();;
  
  thumbnailer->thumbnail_image_type = Png;
  
  image_data *imageData = video_thumbnailer_create_image_data();
  thumbnailer->thumbnail_image_quality = quality;
  thumbnailer->thumbnail_size = size;
  video_thumbnailer_generate_thumbnail_to_buffer(thumbnailer, currentMedia.fullPath().c_str(), imageData);
  fullImage = {imageData->image_data_ptr, imageData->image_data_size};
//   vector<uint8_t> data{imageData->image_data_ptr, imageData->image_data_ptr + imageData->image_data_size};
  video_thumbnailer_destroy_image_data(imageData);
  video_thumbnailer_destroy(thumbnailer);
}