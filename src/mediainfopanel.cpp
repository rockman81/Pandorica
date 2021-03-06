/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2013  Marco Gulino <email>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "mediainfopanel.h"
#include "private/mediainfopanel_p.h"
#include "utils/d_ptr_implementation.h"
#include "utils/utils.h"
#include "media/media.h"
#include "Models/models.h"
#include <Wt/Dbo/Transaction>
#include "session.h"
#include "Wt-Commons/wt_helpers.h"
#include "settings.h"
#include <Wt/WPushButton>
#include <Wt/WText>
#include <Wt/WImage>
#include <Wt/WTable>
#include <Wt/WTime>
#include <Wt/WMessageBox>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

using namespace std;
using namespace Wt;
using namespace WtCommons;

MediaInfoPanel::Private::Private( MediaInfoPanel *q, Session *session, Settings *settings ) : q( q ), session(session), settings(settings)
{
}
MediaInfoPanel::Private::~Private()
{
}

MediaInfoPanel::MediaInfoPanel( Session *session, Settings *settings, Wt::WContainerWidget *parent )
: WContainerWidget( parent ), d(this, session, settings)
{
  setStyleClass( "browser-info-panel" );
  Dbo::Transaction t( *session );
  reset();
}

MediaInfoPanel::~MediaInfoPanel()
{
}

void MediaInfoPanel::reset()
{
  clear();
  addWidget( WW<WContainerWidget>().setContentAlignment( AlignCenter )
             .add( WW<WText>( wtr( "infopanel.empty.title" ) ).css( "media-title" ) )
             .add( WW<WImage>( Settings::staticPath( "/icons/site-logo-big.png" ) ) )
           );
  addWidget( new WBreak );
  addWidget( WW<WText>( wtr( "infopanel.empty.message" ) ) );

  addWidget( d->addFolderActions() );
  wasResetted().emit();
}

WPanel *MediaInfoPanel::Private::addFolderActions()
{
  auto folderActions = createPanel( "mediabrowser.folderActions" );
  folderActions.second->addWidget( WW<WPushButton>( wtr( "mediabrowser.folderActions.playFolder" ) ).css( "btn btn-block btn-sm btn-primary" ).onClick( [ = ]( WMouseEvent )
  {
    playFolder.emit();
  } ) );
  folderActions.second->addWidget( WW<WPushButton>( wtr( "mediabrowser.folderActions.playFolderRecursive" ) ).css( "btn btn-block btn-sm" ).onClick( [ = ]( WMouseEvent )
  {
    playFolderRecursive.emit();
  } ) );
  return folderActions.first;
}



void MediaInfoPanel::info( const Media &media )
{
  clear();
  WContainerWidget *header = WW<WContainerWidget>().setContentAlignment( AlignCenter );
  Dbo::Transaction t( *d->session );
  WString title = media.title( t );
  header->addWidget( WW<WText>( title ).css( "media-title" ) );
  Dbo::ptr<MediaAttachment> previewAttachment = media.preview( t, Media::PreviewPlayer );

  if( previewAttachment )
  {
    WLink previewLink = previewAttachment->link( previewAttachment, t, header );
    WLink fullImage = previewLink;

    Dbo::ptr<MediaAttachment> fullImageAttachment = media.preview( t, Media::PreviewFull );

    if( fullImageAttachment )
      fullImage = fullImageAttachment->link( fullImageAttachment, t, header );

    WAnchor *fullImageLink = new WAnchor {fullImage, WW<WImage>(previewLink, title).css("img-responsive img-rounded")};
    fullImageLink->setTarget( Wt::AnchorTarget::TargetNewWindow );
    header->addWidget( fullImageLink );
  }
  else
  {
    auto iconType = ( media.mimetype().find( "video" ) == string::npos ) ? Settings::AudioFile : Settings::VideoFile;
    WImage *icon = new WImage { d->settings->icon( iconType ) };
    header->addWidget( icon );
  }

  auto mediaMediaInfoPanel = d->createPanel( "mediabrowser.information" );
  WTable *table = new WTable( mediaMediaInfoPanel.second );
  table->setWidth( WLength( 100, WLength::Percentage ) );
  d->labelValueBox( "mediabrowser.filename", WString::fromUTF8(media.filename()), table );
  d->labelValueBox( "mediabrowser.filesize", Utils::formatFileSize( fs::file_size( media.path() ) ), table );

  MediaPropertiesPtr mediaProperties = media.properties( t );

  if( mediaProperties )
  {
    d->labelValueBox( "mediabrowser.creation_time", mediaProperties->creationTime().toString(), table );
    d->labelValueBox( "mediabrowser.medialength", WTime( 0, 0, 0 ).addSecs( mediaProperties->duration() ).toString(), table );

    if( media.mimetype().find( "video" ) != string::npos && mediaProperties->width() > 0 && mediaProperties->height() > 0 )
      d->labelValueBox( "mediabrowser.resolution", WString( "{1}x{2}" ).arg( mediaProperties->width() ).arg( mediaProperties->height() ), table );
  }

  Ratings rating = MediaRating::ratingFor( media, t );

  if( rating.users > 0 )
  {
    WContainerWidget *avgRatingWidget = new WContainerWidget;

    for( int i = 1; i <= 5; i++ )
    {
      avgRatingWidget->addWidget( WW<WImage>( Settings::staticPath( "/icons/rating_small.png" ) ).css( rating.ratingAverage < i ? "rating-unrated" : "" ) );
    }

    d->labelValueBox( "mediabrowser.rating", avgRatingWidget, table );
  }

  auto actions = d->createPanel( "mediabrowser.actions" );
  actions.second->addWidget( WW<WPushButton>( wtr( "mediabrowser.play" ) ).css( "btn btn-block btn-sm btn-primary" ).onClick( [ = ]( WMouseEvent )
  {
    d->play.emit( media );
  } ) );
  actions.second->addWidget( WW<WPushButton>( wtr( "mediabrowser.queue" ) ).css( "btn btn-block btn-sm" ).onClick( [ = ]( WMouseEvent )
  {
    d->queue.emit( media );
  } ) );
  actions.second->addWidget( WW<WPushButton>( wtr( "mediabrowser.share" ) ).css( "btn btn-block btn-sm" ).onClick( [ = ]( WMouseEvent )
  {
    Wt::Dbo::Transaction t( *d->session );
    WW<WMessageBox>(wtr( "mediabrowser.share" ),
                    wtr( "mediabrowser.share.dialog" )
                      .arg( media.title( t ) )
                      .arg( d->settings->shareLink( media.uid() ).url() ),
                    Wt::Information, Ok)
    .button(Ok, [=](WMessageBox *msgBox){ msgBox->accept(); }).get()->show();
  } ) );
  addWidget( WW<WPushButton>( wtr( "button.close.info" ) ).css( "btn btn-primary btn-block hidden-lg hidden-md" )
             .onClick( [ = ]( WMouseEvent )
  {
    wasResetted().emit();
  } ) );
  actions.second->addWidget( WW<WPushButton>( wtr( "player.downloadlink" ) ).css( "btn btn-block btn-sm btn-success" ).onClick( [ = ]( WMouseEvent )
  {
    WDialog *downloadDialog = new WDialog(wtr("player.downloadlink"));
    downloadDialog->contents()->addWidget(new WText{wtr("player.downloadlink.message").arg(d->settings->linkFor( media.path(), media.mimetype() , d->session).url()), XHTMLUnsafeText});
    downloadDialog->footer()->addWidget(WW<WPushButton>(wtr("button.ok")).css("btn btn-primary").onClick([=](WMouseEvent){ downloadDialog->accept(); }));
    downloadDialog->show();
  } ) );
  addWidget( WW<WPushButton>( wtr( "button.close.info" ) ).css( "btn btn-primary btn-block hidden-lg hidden-md" )
             .onClick( [ = ]( WMouseEvent )
  {
    wasResetted().emit();
  } ) );
  addWidget( header );
  addWidget( mediaMediaInfoPanel.first );
  addWidget( actions.first );
  addWidget(d->addFolderActions());
  gotInfo().emit();
}


pair<WPanel *, WContainerWidget *> MediaInfoPanel::Private::createPanel( string titleKey )
{
  WPanel *panel = new WPanel();
  panel->setTitle( wtr( titleKey ) );
  panel->setCollapsible( true );
  panel->setMargin( 10, Wt::Side::Top );
  settings->setAnimation( Settings::PanelAnimation, panel );
  setHeaderCollapsible( panel );
  WContainerWidget *container = new WContainerWidget();
  panel->setCentralWidget( container );
  return {panel, container};
}

void MediaInfoPanel::Private::labelValueBox( string label, Wt::WString value, WTable *container )
{
  return labelValueBox( label, new WText {value}, container );
}

void MediaInfoPanel::Private::labelValueBox( string label, WWidget *widget, WTable *container )
{
  int currentRow = container->rowCount();
  container->elementAt( currentRow, 0 )->addWidget( WW<WText>( wtr( label ) ).css( "label label-info" ) );
  container->elementAt( currentRow, 1 )->addWidget( WW<WContainerWidget>().css( "media-info-value" ).setContentAlignment( AlignRight ).add( widget ) );
}

Signal< NoClass > &MediaInfoPanel::gotInfo() const
{
  return d->gotInfo;
}
Signal< Media > &MediaInfoPanel::play() const
{
  return d->play;
}
Signal< NoClass > &MediaInfoPanel::playFolder() const
{
  return d->playFolder;
}
Signal< NoClass > &MediaInfoPanel::playFolderRecursive() const
{
  return d->playFolderRecursive;
}
Signal< Media > &MediaInfoPanel::queue() const
{
  return d->queue;
}
Signal< NoClass > &MediaInfoPanel::wasResetted() const
{
  return d->wasResetted;
}









