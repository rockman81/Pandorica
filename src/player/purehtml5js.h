/*
 * Copyright (c) year Marco Gulino <marco.gulino@gmail.com>
 *
 * This file is part of Pandorica: https://github.com/GuLinux/Pandorica
 *
 * Pandorica is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details (included the COPYING file).
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef PUREHTML5JS_H
#define PUREHTML5JS_H

#include "player/html5player.h"

class PureHTML5Js : public PlayerJavascript
{
public:
    virtual std::string resizeJs();
    virtual void onPlayerReady();
    PureHTML5Js(HTML5PlayerSetup html5PlayerSetup, Wt::WObject* parent);
    virtual std::string customPlayerHTML();
};

#endif // PUREHTML5JS_H
