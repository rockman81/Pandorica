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

#ifndef MEDIADIRECTORYPRIVATE_H
#define MEDIADIRECTORYPRIVATE_H
#include <boost/filesystem.hpp>
#include "media/media.h"
#include "media/mediadirectory.h"
#include <map>

class MediaDirectory::Private
{
  public:
    Private( MediaDirectory *q, const boost::filesystem::path &path, const std::shared_ptr<MediaDirectory> &parent );
    boost::filesystem::path path;
    std::vector<Media> medias;
    std::map<boost::filesystem::path, std::shared_ptr<MediaDirectory>> subdirectories;
    void addTree(const Media &media, const boost::filesystem::path &directories);
    std::string label;
    std::shared_ptr<MediaDirectory> parent;
    bool contains(const std::vector<Media> mediaList, const Media &media) const;
  private:
    MediaDirectory *const q;
};


#endif // MEDIADIRECTORYPRIVATE_H
