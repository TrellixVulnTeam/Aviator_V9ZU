
//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.3
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
//
// conv_dash
//
//----------------------------------------------------------------------------
#ifndef AGG_CONV_DASH_INCLUDED
#define AGG_CONV_DASH_INCLUDED
#include "agg_basics.h"
#include "agg_vcgen_dash.h"
#include "agg_conv_adaptor_vcgen.h"
namespace agg
{
template<class VertexSource, class Markers = null_markers>
struct conv_dash : public conv_adaptor_vcgen<VertexSource, vcgen_dash, Markers> {
    typedef Markers marker_type;
    typedef conv_adaptor_vcgen<VertexSource, vcgen_dash, Markers> base_type;
    conv_dash(VertexSource& vs) :
        conv_adaptor_vcgen<VertexSource, vcgen_dash, Markers>(vs)
    {
    }
    void remove_all_dashes()
    {
        base_type::generator().remove_all_dashes();
    }
    void add_dash(FX_FLOAT dash_len, FX_FLOAT gap_len)
    {
        base_type::generator().add_dash(dash_len, gap_len);
    }
    void dash_start(FX_FLOAT ds)
    {
        base_type::generator().dash_start(ds);
    }
    void shorten(FX_FLOAT s)
    {
        base_type::generator().shorten(s);
    }
    double shorten() const
    {
        return base_type::generator().shorten();
    }
private:
    conv_dash(const conv_dash<VertexSource, Markers>&);
    const conv_dash<VertexSource, Markers>&
    operator = (const conv_dash<VertexSource, Markers>&);
};
}
#endif
