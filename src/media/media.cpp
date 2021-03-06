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



#include "media.h"
#include <Wt/Utils>
#include <map>
#include "settings.h"
#include "Wt/WApplication"
#include "session.h"
#include "settings.h"
#include <boost/filesystem.hpp>
#include "Models/models.h"
#include <utils/image.h>
#include <threadpool.h>

#define IMAGE_SIZE_PREVIEW 550
#define IMAGE_SIZE_THUMB 260
#define IMAGE_SIZE_PLAYER 640

using namespace Wt;
using namespace std;
using namespace boost;
using namespace Wt::Utils;
namespace fs = boost::filesystem;


map<string,string> supportedMimetypes{
  {".mp4", "video/mp4"}, {".m4v", "video/mp4"}, {".mov", "video/mp4"} /* TODO: check */, {".ogv", "video/ogg"}, {".webm", "video/webm"}, {".flv", "video/x-flv"},
  {".ogg", "audio/ogg"}, {".mp3", "audio/mpeg"}
};

Media::Media(const filesystem::path& path) : m_path{path}, m_uid{hexEncode(md5(path.string()))} {
}

Media::Media() : Media(filesystem::path{})  {
}


Media::~Media()
{
}
string Media::fullPath() const
{
  return m_path.string();
}
string Media::extension() const
{
  return m_path.extension().string();
}
string Media::filename() const
{
  return m_path.filename().string();
}

WString Media::title(Dbo::Transaction& transaction) const
{
  MediaPropertiesPtr properties = transaction.session().find<MediaProperties>().where("media_id = ?").bind(uid());
  if(!properties || properties->title().empty())
    return WString::fromUTF8(filename());
  return WString::fromUTF8(properties->title());
}

string Media::mimetype() const
{
  if(supportedMimetypes.count(extension()) >0)
    return supportedMimetypes[extension()];
  return "UNSUPPORTED";
}
filesystem::path Media::path() const
{
  return m_path;
}
filesystem::path Media::parentDirectory() const
{
  return m_path.parent_path();
}

string Media::uid() const
{
  return m_uid;
}


bool Media::valid() const
{
  return !uid().empty() && !path().empty() && ! path().string().empty() && mimetype() != "UNSUPPORTED";
}

Media Media::invalid()
{
  return {};
}


Dbo::ptr< MediaAttachment > Media::preview(Dbo::Transaction& transaction, Media::PreviewSize size ) const
{
  string previewSize;
  switch(size) {
    case Media::PreviewFull:
      previewSize="full"; break;
    case Media::PreviewPlayer:
      previewSize="player"; break;
    case Media::PreviewThumb:
      previewSize="thumbnail"; break;
  }
  return transaction.session().find<MediaAttachment>().where("media_id = ? AND type = 'preview' AND name = ?")
    .bind(uid()).bind(previewSize);
}

void Media::setImage(const std::shared_ptr<Image>& image, Dbo::Transaction& transaction) const
{
  transaction.session().execute( "DELETE FROM media_attachment WHERE media_id = ? AND type = 'preview'" ).bind( uid() );
  MediaAttachment *fullAttachment = MediaAttachment::image("full", uid(), *image );
  MediaAttachment *thumbnailAttachment = MediaAttachment::image("thumbnail", uid(), *image->scaled(IMAGE_SIZE_THUMB, 60));
  MediaAttachment *playerAttachment = MediaAttachment::image("player", uid(), *image->scaled(IMAGE_SIZE_PLAYER) );
  transaction.session().add( fullAttachment );
  transaction.session().add( thumbnailAttachment );
  transaction.session().add( playerAttachment );
}



WDateTime Media::creationTime(Wt::Dbo::Transaction &transaction) const
{
  return WDateTime::fromPosixTime(posixCreationTime(transaction));
}

boost::posix_time::ptime Media::posixCreationTime(Wt::Dbo::Transaction &transaction) const
{
  auto mediaProperties = this->properties(transaction);
  if(!mediaProperties) {
    return boost::posix_time::from_time_t(boost::filesystem::last_write_time(m_path));
  }
  return mediaProperties->posixCreationTime();
}



Dbo::collection<MediaAttachmentPtr> Media::subtitles(Dbo::Transaction& transaction) const
{
  return transaction.session().find<MediaAttachment>().where("media_id = ? AND type = 'subtitles'").bind(uid()).resultList();
}

long Media::subtitles_count(Dbo::Transaction& transaction) const
{
  return transaction.session().query<long>("select count(*) from media_attachment").where("media_id = ? AND type = 'subtitles'").bind(uid()).resultValue();
}


MediaPropertiesPtr Media::properties(Dbo::Transaction& transaction) const
{
  return transaction.session().find<MediaProperties>().where("media_id = ?").bind(uid()).resultValue();
}


bool Media::operator==( const Media &other ) const
{
  return other.uid() == uid();
}

ostream &operator<<( ostream &os, const Media &m )
{
  os << "Media[" << m.uid() << "]{" << m.fullPath() << ", " << m.mimetype() << "}";
  return os;
}

