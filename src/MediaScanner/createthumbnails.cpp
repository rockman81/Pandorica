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



#include "createthumbnails.h"
#include "private/createthumbnails_p.h"
#include "googlepicker.h"

#include "ffmpegmedia.h"
#include "session.h"
#include <Wt/WProgressBar>
#include <Wt/WText>
#include <Wt/WPushButton>
#include <Wt/WTime>
#include <Wt/WMemoryResource>
#include <Wt/WImage>
#include "Wt-Commons/wt_helpers.h"
#include "media/mediacollection.h"
#include <player/html5player.h>
#include <settings.h>
#include <boost/format.hpp>

#include <chrono>
#include <random>
#include <thread>
#include <Wt/WFileUpload>
#include <Wt/WCheckBox>
#include <Wt/WOverlayLoadingIndicator>
#include <Wt/Http/Client>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include "Models/models.h"

#include "utils/d_ptr_implementation.h"
#include <utils/utils.h>
#include <utils/image.h>
#include <boost/thread.hpp>
#include <mutex>

using namespace Wt;
using namespace std;
using namespace std::chrono;
using namespace WtCommons;



time_point<high_resolution_clock> serverStartTimeForRandomSeeding {high_resolution_clock::now()};
mt19937_64 randomEngine {( uint64_t ) serverStartTimeForRandomSeeding.time_since_epoch().count()};

CreateThumbnails::Private::Private( WApplication *app, Settings *settings, CreateThumbnails *q )
  : app( app ), settings( settings ), q( q )
{
}

CreateThumbnails::CreateThumbnails(const std::shared_ptr< MediaScannerSemaphore >& semaphore, WApplication* app, Settings* settings)
  : MediaScannerStep(semaphore), d( app, settings, this )
{

}

CreateThumbnails::~CreateThumbnails()
{
}


ImageUploader::ImageUploader( WContainerWidget *parent )
  : WContainerWidget( parent )
{
  reset();
}

void ImageUploader::reset()
{
  clear();
  WContainerWidget *hidden = new WContainerWidget();
  linkContainer = WW<WContainerWidget>().setContentAlignment( AlignCenter );
  WAnchor *uploadLink = new WAnchor( "", wtr( "mediascannerdialog.thumbnail.upload.label" ) );
  uploadLink->clicked().connect( [ = ]( WMouseEvent )
  {
    linkContainer->hide();
    addWidget( hidden );
  } );
  linkContainer->addWidget( uploadLink );
  addWidget( linkContainer );
  upload = new WFileUpload();
  upload->setProgressBar( new WProgressBar() );
  hidden->addWidget( upload );
  upload->uploaded().connect( this, &ImageUploader::uploaded );
  upload->changed().connect( [ = ]( _n1 )
  {
    if( upload->canUpload() )
    {
      upload->disable();
      upload->upload();
    }
  } );
  upload->fileTooLarge().connect( [ = ]( int64_t, _n5 )
  {
    reset();
    linkContainer->addWidget( WW<WContainerWidget>().add( new WText {wtr( "mediascannerdialog.thumbnail.upload.toobig" )} ).css( "alert" ) );
  } );
  setStyleClass( "form-inline" );
}


void ImageUploader::uploaded()
{
  try
  {
    log( "notice" ) << "uploaded file to " << upload->spoolFileName();
    Image image(upload->spoolFileName());
    _previewImage.emit( image );
    reset();
  }
  catch
    ( std::exception &e )
  {
    log( "error" ) << "Error decoding image: " << e.what();
    reset();
    linkContainer->addWidget( WW<WContainerWidget>().add( new WText {wtr( "mediascannerdialog.thumbnail.upload.error" )} ).css( "alert alert-danger" ) );
  }
}


void CreateThumbnails::run( FFMPEGMedia* ffmpegMedia, Media media, Dbo::Transaction& transaction, MediaScannerStep::ExistingFlags onExisting )
{
  d->app->log("notice") << __PRETTY_FUNCTION__;
  if(d->googlePicker)
    d->googlePicker->searchString(media.title(transaction));
  unique_lock<MediaScannerSemaphore> lock(semaphore);

  if( onExisting == SkipIfExisting && transaction.session().query<int>( "SELECT COUNT(id) FROM media_attachment WHERE media_id = ? AND type = 'preview'" ).bind( media.uid() ) > 0 )
  {
    return;
  }
  semaphore.needsSaving(true);
  d->currentMedia = media;
  d->currentFFMPEGMedia = ffmpegMedia;

//   if( !ffmpegMedia->isVideo() )
//   {
//     return;
//   }

  d->createThumbnailFromMedia(lock);
}


void CreateThumbnails::setupGui( WContainerWidget *container )
{
  
  auto image_loaded = [ = ]( ImageBlob blob )
  {
    unique_lock<MediaScannerSemaphore> lock(semaphore);
    delete d->thumbnail;
    d->fullImage = blob;
    d->thumbnail = new WMemoryResource {"image/png", d->resize( blob, IMAGE_SIZE_PREVIEW ), container};
    d->previewImage->setImageLink( d->thumbnail );
    d->previewImage->show();
    semaphore.needsSaving(true);
  };
  container->addWidget(WW<WText>( wtr( "mediascannerdialog.thumbnaillabel" ) ).css( "small-text" ).setInline( false ));
  ImageUploader *imageUploader = new ImageUploader;
  container->addWidget(d->googlePicker = new GooglePicker("Search on Google"));
  auto httpClient = new Http::Client(d->googlePicker);
  httpClient->setMaximumResponseSize(1024*1024*15);
  httpClient->done().connect([=](const boost::system::error_code &err, const Http::Message &message, _n4){
      if(err.value() != boost::system::errc::success) {
	wApp->log("warning") << "unable to download: " << message.status() << ", " << err.message();
	return;
      }
      ImageBlob blob;
      auto body = message.body();
      std::copy(body.begin(), body.end(), back_inserter(blob));
      image_loaded(blob);
  });
  d->googlePicker->imageChosen().connect([=](const Wt::WString &url, _n5) {
    wApp->log("notice") << "picked url: " << url;
    httpClient->get(url.toUTF8());
  });
  container->addWidget( imageUploader );
  d->thumbnail = nullptr;
  d->redoSignalWasConnected = false;
  WCheckBox *skipImageCheckbox = new WCheckBox( wtr( "create_thumbnails_no_image_checkbox" ), container );
  skipImageCheckbox->changed().connect( [ = ]( _n1 )
  {
    // TODO: if we select "No Image", the thumbnail is not saved.
    // This means that on next run, it will be asked again.
    // Should should try and save some kind of flag instead?
    d->previewImage->setHidden(skipImageCheckbox->isChecked());
    semaphore.needsSaving(!skipImageCheckbox->isChecked());
//     setResult( skipImageCheckbox->isChecked() ? MediaScannerStep::Skip : (d->thumbnail ? MediaScannerStep::Done : MediaScannerStep::Waiting) );
  } );
  WContainerWidget *imageContainer = new WContainerWidget;
  imageContainer->setContentAlignment( AlignCenter );
  imageContainer->addWidget( d->previewImage = WW< WImage >().css("img-rounded img-responsive").setHidden( true ) );
  container->addWidget( imageContainer );
  imageUploader->previewImage().connect( [=](const ImageBlob &b, _n5){image_loaded(b); });
}


void CreateThumbnails::Private::createThumbnailFromMedia(const unique_lock< MediaScannerSemaphore > &semaphoreLock)
{
  auto setLoadingIcon = [=]{
    previewImage->setImageLink(Settings::staticPath("/icons/loader-large.gif"));
    app->triggerUpdate();
  };
  guiRun(app, setLoadingIcon);
  unique_lock<FFMPEGMedia> lockFFMPeg( *currentFFMPEGMedia );
  delete thumbnail;
  thumbnail = 0;
  findRandomPosition();
  
  int fullSize = max( currentFFMPEGMedia->resolution().first, currentFFMPEGMedia->resolution().second );
  try {
    thumbnailFor( fullSize, 10 );
  } catch(std::exception &e) {
    app->log("notice") << "Error creating thumbnail: " << e.what();
    guiRun(app, [=]{ previewImage->hide(); app->triggerUpdate(); });
    
    fullImage = ImageBlob();
    cerr << "fullImage length: " << fullImage.size() << endl;
    while(fullImage.size() == 0 && q->semaphore.needsSaving()) {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
    }
    return;
  }

  thumbnail = new WMemoryResource {"image/png", resize( fullImage, IMAGE_SIZE_PREVIEW ), previewImage};
  guiRun( app, [ = ]
  {
    previewImage->addStyleClass( "link-hand" );
    previewImage->setImageLink( thumbnail );
    previewImage->show();

    if( !redoSignalWasConnected ) {
      previewImage->clicked().connect( [=]( WMouseEvent ) {
	boost::thread reload([=]{
	  unique_lock<MediaScannerSemaphore> lock(q->semaphore);
	  createThumbnailFromMedia(lock);
	});
      });
      redoSignalWasConnected = true;
    }
    app->triggerUpdate();
  } );
}


ImageBlob CreateThumbnails::Private::resize( const ImageBlob &data, uint32_t size, uint32_t quality)
{
  Image image(data);
  return image.resize(size, quality);
}

void CreateThumbnails::Private::findRandomPosition()
{
  auto randomNumber = randomEngine();

  if( currentFFMPEGMedia->durationInSeconds() < 100 )
  {
    int percent = randomNumber % 100;

    if( percent < 10 )
      percent += 10;

    if( percent > 80 )
      percent -= 20;

    currentPosition = {percent};
    return;
  }

  int percent {0};
  int position {0};

  while( percent < 10 || percent > 80 )
  {
    randomNumber = randomEngine();
    position = randomNumber % currentFFMPEGMedia->durationInSeconds();
    percent = position * 100.0 / currentFFMPEGMedia->durationInSeconds();
  }

  currentPosition = ThumbnailPosition::from( position );
}

ThumbnailPosition ThumbnailPosition::from( int timeInSeconds )
{
  int remaining = 0;
  int hours = timeInSeconds / 3600;
  timeInSeconds %= 3600;
  int minutes = timeInSeconds / 60;
  timeInSeconds %= 60;
  string currentTimeStr = ( boost::format( "%.2d:%.2d:%.2d" ) % hours % minutes % timeInSeconds ).str();
  return { -1, currentTimeStr};
}


void CreateThumbnails::save( Dbo::Transaction& transaction )
{
  if(d->fullImage.size() == 0) return;
  log( "notice" ) << "Deleting old data from media_attachment for media_id " << d->currentMedia.uid();
  transaction.session().execute( "DELETE FROM media_attachment WHERE media_id = ? AND type = 'preview'" ).bind( d->currentMedia.uid() );
  MediaAttachment *fullAttachment = new MediaAttachment {"preview", "full", "", d->currentMedia.uid(), "image/png", d->fullImage };
  MediaAttachment *thumbnailAttachment = new MediaAttachment {"preview", "thumbnail", "", d->currentMedia.uid(), "image/png", d->resize( d->fullImage, IMAGE_SIZE_THUMB, 60 ) };
  MediaAttachment *playerAttachment = new MediaAttachment {"preview", "player", "", d->currentMedia.uid(), "image/png", d->resize( d->fullImage, IMAGE_SIZE_PLAYER ) };
  transaction.session().add( fullAttachment );
  transaction.session().add( thumbnailAttachment );
  transaction.session().add( playerAttachment );
  log( "notice" ) << "Images saved";
  d->currentMedia = Media::invalid();
  d->currentFFMPEGMedia = 0;
}

#include "videothumbnailer.h"
#include <filmstripfilter.h>
using namespace ffmpegthumbnailer;
void CreateThumbnails::Private::thumbnailFor( int size, int quality )
{
  VideoThumbnailer videoThumbnailer( size, false, true, quality, true );
  FilmStripFilter filmStripFilter;
  if( currentMedia.mimetype().find( "video" ) != string::npos )
    videoThumbnailer.addFilter( &filmStripFilter );
  if( currentPosition.percent > 0 )
    videoThumbnailer.setSeekPercentage( currentPosition.percent );
  else
    videoThumbnailer.setSeekTime( currentPosition.timing );

  videoThumbnailer.generateThumbnail( currentMedia.fullPath(), ThumbnailerImageType::Png, fullImage );
}

