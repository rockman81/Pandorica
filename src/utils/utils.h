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




#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <Wt/WString>
#include <Wt/WLength>
#include <algorithm>
#include "utils/d_ptr.h"

namespace Wt {
class WInteractWidget;
}
class Utils
{
public:
    Utils();
    ~Utils();
    static std::string titleHintFromFilename(std::string filename);
    static void mailForUnauthorizedUser(std::string email, Wt::WString identity);
    static void mailForNewAdmin(std::string email, Wt::WString identity);
    static std::string formatFileSize(long size);
    static Wt::WInteractWidget *help(std::string titleKey, std::string contentKey, std::string side, Wt::WLength size = Wt::WLength::Auto);
    
    template<typename Out, typename In, typename TransformF>
    static Out transform(In in, Out out, TransformF f) {
      std::transform(in.begin(), in.end(), std::back_insert_iterator<Out>(out), f);
      return out;
    }
private:
  D_PTR;
};

#endif // UTILS_H