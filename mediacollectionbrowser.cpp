#include "mediacollectionbrowser.h"
#include <Wt/WMenu>
#include <Wt/WAnchor>
#include <Wt/WText>
#include <Wt/WPopupMenu>
#include <Wt/WImage>
#include <Wt/WMemoryResource>
#include "Wt-Commons/wt_helpers.h"
#include "Wt-Commons/wt_helpers.h"
#include "settings.h"
#include "session.h"
#include "utils.h"
#include <boost/format.hpp>
#include <algorithm>
#include <Wt/Dbo/Transaction>
#include <Wt/WTime>
#include <Wt/WMessageBox>
#include <Wt/WLineEdit>
#include <Wt/WPushButton>
#include <Wt/WFileResource>
#include <Wt/WVBoxLayout>
#include <Wt/WHBoxLayout>
#include "private/mediacollectionbrowser_p.h"
#include "Models/models.h"
#include "MediaScanner/createthumbnails.h"
#include "ffmpegmedia.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <Wt/WPanel>
#include <Wt/WGroupBox>
#include <Wt/WTimer>

using namespace Wt;
using namespace std;
using namespace boost;
using namespace StreamingPrivate;
namespace fs = boost::filesystem;
using namespace WtCommons;

MediaCollectionBrowser::MediaCollectionBrowser(MediaCollection* collection, Settings* settings, Session* session, WContainerWidget* parent)
  : WContainerWidget(parent), d(new MediaCollectionBrowserPrivate(collection, settings, session, this))
{
  d->breadcrumb = WW<WContainerWidget>().css("breadcrumb");
  d->breadcrumb->setList(true);
  d->browser = WW<WContainerWidget>().css("thumbnails").setMargin(WLength::Auto, Left).setMargin(WLength::Auto, Right);
  WContainerWidget *mainContainer = new WContainerWidget;
  d->infoPanel = new InfoPanel(session, settings);

/*  
  mainContainer->setStyleClass("row-fluid");
  mainContainer->addWidget(d->infoPanel);
  d->infoPanel->addStyleClass("span2");
  mainContainer->addWidget(d->browser);
  d->browser->addStyleClass("span10");
*/
  auto layout = new WHBoxLayout();
  mainContainer->setLayout(layout);
  layout->addWidget(d->infoPanel);
  layout->setResizable(0, true);
  d->infoPanel->setWidth(450);
  layout->addWidget(WW<WContainerWidget>().css("mediabrowser").add(d->browser), 1);

  d->browser->setList(true);
  addWidget(d->breadcrumb);
  addWidget(mainContainer);
  d->currentPath = d->collection->rootPath();
  collection->scanned().connect(this, &MediaCollectionBrowser::reload);
  d->infoPanel->play().connect([=](Media media, _n5) { d->playSignal.emit(media); });
  d->infoPanel->queue().connect([=](Media media, _n5) { d->queueSignal.emit(media); });
  d->infoPanel->setTitle().connect(d, &MediaCollectionBrowserPrivate::setTitleFor);
  d->infoPanel->setPoster().connect(d, &MediaCollectionBrowserPrivate::setPosterFor);
  d->infoPanel->deletePoster().connect(d, &MediaCollectionBrowserPrivate::clearThumbnailsFor);
}

void MediaCollectionBrowser::reload()
{
  d->browse(d->currentPath);
}



InfoPanel::InfoPanel(Session *session, Settings *settings, Wt::WContainerWidget* parent): WContainerWidget(parent), session(session), settings(settings)
{
  setStyleClass("browser-info-panel");
  isAdmin = session->user()->isAdmin();
  reset();
}


void InfoPanel::reset()
{
  clear();
  addWidget(WW<WContainerWidget>().setContentAlignment(AlignCenter)
    .add(WW<WText>(wtr("infopanel.empty.title")).css("media-title"))
    .add(WW<WImage>(Settings::staticPath("/icons/site-logo-big.png")))
  );
  addWidget(new WBreak);
  addWidget(WW<WText>(wtr("infopanel.empty.message")));
}


void InfoPanel::info(Media media)
{
  clear();
  WContainerWidget *header = WW<WContainerWidget>().setContentAlignment(AlignCenter);
  Dbo::Transaction t(*session);
  WString title = media.title(t);
  header->addWidget(WW<WText>(title).css("media-title"));
  Dbo::ptr<MediaAttachment> previewAttachment = media.preview(t, Media::PreviewPlayer);
  if(previewAttachment) {
    WLink previewLink = previewAttachment->link(previewAttachment, header);
    WLink fullImage = previewLink;
    
    Dbo::ptr<MediaAttachment> fullImageAttachment = media.preview(t, Media::PreviewFull);
    if(fullImageAttachment)
      fullImage = fullImageAttachment->link(fullImageAttachment, header);
    
    WAnchor *fullImageLink = new WAnchor{fullImage, new WImage{previewLink, title}};
    fullImageLink->setTarget(AnchorTarget::TargetNewWindow);
    header->addWidget(fullImageLink);
  } else {
    auto iconType = (media.mimetype().find("video") == string::npos) ? Settings::AudioFile : Settings::VideoFile;
    WImage *icon = new WImage{ settings->icon(iconType) };
    header->addWidget(icon);
  }
  auto mediaInfoPanel = createPanel("mediabrowser.information");
  
  mediaInfoPanel.second->addWidget(labelValueBox("mediabrowser.filename", media.filename()) );
  mediaInfoPanel.second->addWidget(labelValueBox("mediabrowser.filesize", Utils::formatFileSize(fs::file_size(media.path())) ) );
  
  MediaPropertiesPtr mediaProperties = media.properties(t);
  if(mediaProperties) {
    mediaInfoPanel.second->addWidget(labelValueBox("mediabrowser.medialength", WTime(0,0,0).addSecs(mediaProperties->duration()).toString()) );
    if(media.mimetype().find("video") != string::npos && mediaProperties->width() > 0 && mediaProperties->height() > 0)
      mediaInfoPanel.second->addWidget(labelValueBox("mediabrowser.resolution", WString("{1}x{2}").arg(mediaProperties->width()).arg(mediaProperties->height()) ) );
  }
  Ratings rating = MediaRating::ratingFor(media, t);
  if(rating.users > 0) {
    WContainerWidget *avgRatingWidget = new WContainerWidget;
    for(int i=1; i<=5; i++) {
      avgRatingWidget->addWidget(WW<WImage>(Settings::staticPath("/icons/rating_small.png")).css(rating.ratingAverage <i ? "rating-unrated" : ""));
    }
    mediaInfoPanel.second->addWidget(labelValueBox("mediabrowser.rating", avgRatingWidget));
  }
  
  auto actions = createPanel("mediabrowser.actions");
  actions.second->addWidget(WW<WPushButton>(wtr("mediabrowser.play")).css("btn btn-block btn-small btn-primary").onClick([=](WMouseEvent){ _play.emit(media);} ));
  actions.second->addWidget(WW<WPushButton>(wtr("mediabrowser.queue")).css("btn btn-block btn-small").onClick([=](WMouseEvent){ _queue.emit(media);} ));
  actions.second->addWidget(WW<WPushButton>(wtr("mediabrowser.share")).css("btn btn-block btn-small").onClick([=](WMouseEvent){
    Wt::Dbo::Transaction t(*session);
    auto shareMessageBox = new WMessageBox(wtr("mediabrowser.share"), wtr("mediabrowser.share.dialog").arg(media.title(t)).arg(settings->shareLink(media.uid()).url()), NoIcon, Ok);
    shareMessageBox->button(Ok)->clicked().connect([=](WMouseEvent) { shareMessageBox->accept(); });
    shareMessageBox->show();
  } ));
  addWidget(header);
  addWidget(mediaInfoPanel.first);
  addWidget(actions.first);
  
  if(isAdmin) {
    auto adminActions = createPanel("mediabrowser.admin.actions");
    adminActions.first->setCollapsed(true);
    adminActions.second->addWidget(WW<WPushButton>(wtr("mediabrowser.admin.settitle")).css("btn btn-block btn-small btn-primary").onClick([=](WMouseEvent){ setTitle().emit(media);} ));
    adminActions.second->addWidget(WW<WPushButton>(wtr("mediabrowser.admin.setposter")).css("btn btn-block btn-small btn-primary").onClick([=](WMouseEvent){ setPoster().emit(media);} ));
    adminActions.second->addWidget(WW<WPushButton>(wtr("mediabrowser.admin.deletepreview")).css("btn btn-block btn-small btn-danger").onClick([=](WMouseEvent){ deletePoster().emit(media);} ));
    addWidget(adminActions.first);
  }
}


pair<WPanel*,WContainerWidget*> InfoPanel::createPanel(string titleKey)
{
  WPanel *panel = new WPanel();
  panel->setTitle(wtr(titleKey));
  panel->setCollapsible(true);
  panel->setMargin(10, Wt::Side::Top);
  panel->setAnimation({WAnimation::SlideInFromTop, WAnimation::EaseOut, 500});
  panel->titleBarWidget()->clicked().connect([=](WMouseEvent){ panel->setCollapsed(!panel->isCollapsed());});
  WContainerWidget *container = new WContainerWidget();
  panel->setCentralWidget(container);
  return {panel, container};
}


Wt::WContainerWidget *InfoPanel::labelValueBox(string label, Wt::WString value)
{
  return labelValueBox(label, new WText{value});
}


WContainerWidget* InfoPanel::labelValueBox(string label, WWidget* widget)
{
  WContainerWidget *box = WW<WContainerWidget>().css("row-fluid");
  box->addWidget(WW<WContainerWidget>().css("span3").add(WW<WText>(wtr(label)).css("label label-info")));
  box->addWidget(WW<WContainerWidget>().css("span9 media-info-value").setContentAlignment(AlignRight).add(widget));
  return box;
}



void MediaCollectionBrowserPrivate::browse(filesystem::path currentPath)
{
  this->currentPath = currentPath;
  infoPanel->reset();
  browser->clear();
  rebuildBreadcrumb();
  auto belongsToCurrent = [=](fs::path p){
    return p.parent_path() == this->currentPath;
  };
  
  set<fs::path> directories;
  vector<Media> medias;
  
  for(MediaEntry m: collection->collection()) {
    if(belongsToCurrent(m.second.path()))
      medias.push_back(m.second);
    if(m.second.filename().empty() || m.second.fullPath().empty()) continue;
    
    fs::path directory = m.second.parentDirectory();
    while(directory != collection->rootPath() && !belongsToCurrent(directory) && directory != filesystem::path("/")) {
      directory = directory.parent_path();
    }
    if(directory != currentPath && belongsToCurrent(directory) && !directories.count(directory))
      directories.insert(directory);
  }
  
  std::sort(medias.begin(), medias.end(), [](const Media &a, const Media &b)->bool{ return (a.filename() <b.filename()); } );
  
  for(fs::path directory: directories) addDirectory(directory);
  for(Media media: medias) addMedia(media);
}

void MediaCollectionBrowserPrivate::addDirectory(filesystem::path directory)
{
  auto onClick = [=](WMouseEvent){
    browse(directory);
  };
  addIcon(directory.filename().string(), [](WObject*){ return Settings::icon(Settings::FolderBig); }, onClick);
}


void MediaCollectionBrowserPrivate::addMedia(Media &media)
{
  wApp->log("notice") << "adding media " << media.path();
  Dbo::Transaction t(*session);
  
  auto onClick = [=](WMouseEvent e){ infoPanel->info(media); };
  
  GetIconF icon = [](WObject *){ return Settings::icon(Settings::VideoFile); };
  if(media.mimetype().find("audio") != string::npos)
    icon = [](WObject *){ return Settings::icon(Settings::AudioFile); };
  
  Dbo::ptr<MediaAttachment> preview = media.preview(t, Media::PreviewThumb);
  if(preview)
    icon = [=](WObject *parent) { return preview->link(preview, parent).url(); };
  
  addIcon(media.title(t), icon, onClick);
}

void MediaCollectionBrowserPrivate::clearThumbnailsFor(Media media)
{
  string cacheDir;
  wApp->readConfigurationProperty("thumbnails_cache_dir", cacheDir);
  if(!cacheDir.empty() && boost::filesystem::is_directory(cacheDir)) {
    boost::filesystem::path cacheFile{cacheDir + "/" + media.uid() + "_thumb.png"};
    boost::filesystem::remove(cacheFile);
  }
  Dbo::Transaction t(*session);
  session->execute("DELETE FROM media_attachment WHERE media_id=? and type = 'preview';").bind(media.uid());
  t.commit();
  q->reload();
}


void MediaCollectionBrowserPrivate::setPosterFor(Media media)
{
  // TODO: very messy... FFMPEGMedia is of course deleted, need to create a copy...
  FFMPEGMedia *ffmpegMedia = new FFMPEGMedia{media};
  WDialog *dialog = new WDialog(wtr("mediabrowser.admin.setposter"));
  auto createThumbs = new CreateThumbnails{wApp, settings, dialog};
  dialog->footer()->addWidget(WW<WPushButton>(wtr("button.cancel")).css("btn btn-danger").onClick([=](WMouseEvent) {
    dialog->reject();
    delete ffmpegMedia;
  }));
  dialog->footer()->addWidget(WW<WPushButton>(wtr("button.ok")).css("btn btn-success").onClick([=](WMouseEvent) {
    Dbo::Transaction t(*session);
    createThumbs->save(&t);
    t.commit();
    string cacheDir;
    wApp->readConfigurationProperty("thumbnails_cache_dir", cacheDir);
    if(!cacheDir.empty() && boost::filesystem::is_directory(cacheDir)) {
      boost::filesystem::path cacheFile{cacheDir + "/" + media.uid() + "_thumb.png"};
      boost::filesystem::remove(cacheFile);
    }
    dialog->accept();
    q->reload();
    delete ffmpegMedia;
  }));
  dialog->show();
  dialog->resize(500, 500);
  auto runStep = [=] {
    Dbo::Transaction t(*session);
    createThumbs->run(ffmpegMedia, media, dialog->contents(), &t, MediaScannerStep::OverwriteIfExisting );
  };
  createThumbs->redo().connect([=](_n6) {
    runStep();
  });
  runStep();
}


void MediaCollectionBrowserPrivate::setTitleFor(Media media)
{
  Dbo::Transaction t(*session);
  MediaPropertiesPtr properties = media.properties(t);
  if(!properties) {
    t.rollback();
    WMessageBox::show(wtr("mediabrowser.admin.settitle.missingproperties.caption"), wtr("mediabrowser.admin.settitle.missingproperties.body"), StandardButton::Ok);
    return;
  }
  WDialog *setTitleDialog = new WDialog(wtr("mediabrowser.admin.settitle"));
  setTitleDialog->contents()->addStyleClass("form-inline");
  WLineEdit *titleEdit = new WLineEdit(properties->title().empty() ? media.filename() : properties->title());
  WPushButton* okButton = WW<WPushButton>("Ok").onClick([=](WMouseEvent) { setTitleDialog->accept(); } ).css("btn");
  auto editIsEnabled = [=] {
    return !titleEdit->text().empty() && titleEdit->text().toUTF8() != media.filename() && titleEdit->text().toUTF8() != properties->title();
  };
  okButton->setEnabled(editIsEnabled());
  
  titleEdit->keyWentUp().connect([=](WKeyEvent key){
    if(key.key() == Wt::Key_Enter && editIsEnabled() )
      setTitleDialog->accept();
    okButton->setEnabled(editIsEnabled());
  });
  setTitleDialog->contents()->addWidget(new WText{wtr("set.title.filename.hint")});
  string titleHint = Utils::titleHintFromFilename(media.filename());
  setTitleDialog->contents()->addWidget(WW<WAnchor>("", titleHint).css("link-hand").onClick([=](WMouseEvent){
    titleEdit->setText(titleHint);
    okButton->setEnabled(editIsEnabled());
  }));
  setTitleDialog->contents()->addWidget(new WBreak);
  setTitleDialog->contents()->addWidget(titleEdit);
  setTitleDialog->contents()->addWidget(okButton);
  setTitleDialog->contents()->setPadding(10);
  setTitleDialog->setClosable(true);
  titleEdit->setWidth(500);
  setTitleDialog->finished().connect([=](WDialog::DialogCode code, _n5){
    if(code != WDialog::Accepted)
      return;
    Dbo::Transaction t(*session);
    properties.modify()->setTitle(titleEdit->text().toUTF8());
    properties.flush();
    t.commit();
    q->reload();
  });
  setTitleDialog->show();
}


WContainerWidget* MediaCollectionBrowserPrivate::addIcon(WString filename, GetIconF icon, MouseEventListener onClick)
{
    WContainerWidget *item = WW<WContainerWidget>().css("span3 media-icon-container");
    item->setContentAlignment(AlignmentFlag::AlignCenter);
    WAnchor *link = WW<WAnchor>("").css("thumbnail filesystem-item link-hand");
    link->setImage(new WImage(icon(item) ));
    link->addWidget(WW<WText>(filename).css("filesystem-item-label"));
    item->addWidget(link);
    link->clicked().connect(onClick);

    browser->addWidget(item);
    return item;
}




void MediaCollectionBrowserPrivate::rebuildBreadcrumb()
{
  breadcrumb->clear();
  breadcrumb->addWidget(WW<WPushButton>(wtr("mediacollection.reload")).css("btn btn-small").onClick([=](WMouseEvent) {
    Dbo::Transaction t(*session);
    collection->rescan(t);
    browse(collection->rootPath());
  }));
  list<fs::path> paths;
  fs::path p = currentPath;
  while(p != collection->rootPath()) {
    paths.push_front(p);
    p = p.parent_path();
  }
  paths.push_front(p);
  
  for(fs::path p: paths) {
    WContainerWidget *item = new WContainerWidget;
    if(breadcrumb->count())
      item->addWidget(WW<WText>("/").css("divider"));
    item->addWidget( WW<WAnchor>("", p.filename().string()).css("link-hand").onClick([=](WMouseEvent){
      browse(p);
    }) );
    breadcrumb->addWidget(item);
  }
}

Signal< Media >& MediaCollectionBrowser::play()
{
  return d->playSignal;
}

Signal< Media >& MediaCollectionBrowser::queue()
{
  return d->queueSignal;
}



MediaCollectionBrowser::~MediaCollectionBrowser()
{

}

