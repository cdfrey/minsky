/*
  @copyright Steve Keen 2018
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef RAVELWRAP_H
#define RAVELWRAP_H

#include "operation.h"

namespace minsky 
{
  class RavelWrap: public DataOp
  {
    void* ravel=nullptr;
    void* dataCube=nullptr;
    void (*ravel_delete)(void* ravel)=nullptr;
    void (*ravel_render)(void* ravel, cairo_t* cairo)=nullptr;
    void (*ravel_onMouseDown)(void* ravel, double x, double y)=nullptr;
    void (*ravel_onMouseUp)(void* ravel, double x, double y)=nullptr;
    bool (*ravel_onMouseMotion)(void* ravel, double x, double y)=nullptr;
    bool (*ravel_onMouseOver)(void* ravel, double x, double y)=nullptr;
    void (*ravel_onMouseLeave)(void* ravel)=nullptr;
    void (*ravel_rescale)(void* ravel, double radius);
    void noRavelSetup();
  public:
    RavelWrap();
    ~RavelWrap();
    void draw(cairo_t* cairo) const override;
  };
}

#ifdef CLASSDESC
#pragma omit pack minsky::RavelWrap
#pragma omit unpack minsky::RavelWrap
#endif

namespace classdesc_access
{
  template <> struct access_pack<minsky::RavelWrap>: 
    public access_pack<minsky::DataOp> {};
  template <> struct access_unpack<minsky::RavelWrap>: 
    public access_unpack<minsky::DataOp> {};
}
#include "ravelWrap.cd"

#endif

