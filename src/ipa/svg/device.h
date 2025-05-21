/* libwmf ("ipa/svg/device.h"): library for wmf conversion
   Copyright (C) 2000 - various; see CREDITS, ChangeLog, and sources

   The libwmf Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The libwmf Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the libwmf Library; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */


/* This is called by wmf_play() the *first* time the meta file is played
 */
static void wmf_svg_device_open (wmfAPI* API)
{	/* wmf_svg_t* ddata = WMF_SVG_GetData (API); */

	WMF_DEBUG (API,"~~~~~~~~wmf_[svg_]device_open");

	
}

/* This is called by wmf_api_destroy()
 */
static void wmf_svg_device_close (wmfAPI* API)
{	/* wmf_svg_t* ddata = WMF_SVG_GetData (API); */

	WMF_DEBUG (API,"~~~~~~~~wmf_[svg_]device_close");

	
}

/* This is called from the beginning of each play for initial page setup
 */
static void wmf_svg_device_begin (wmfAPI* API)
{
    wmf_svg_t* ddata = WMF_SVG_GetData (API);
    wmfStream* out = ddata->out;

    WMF_DEBUG (API,"~~~~~~~~wmf_[svg_]device_begin");

    if (out == 0) return;

    if ((out->reset (out->context)) && ((API->flags & WMF_OPT_IGNORE_NONFATAL) == 0))
    {
        WMF_ERROR (API,"unable to reset output stream!");
        API->err = wmf_E_DeviceError;
        return;
    }

    // Fallback for ddata->width and ddata->height if not set by wmf2svg_draw
    // This part should ideally rely on positive bbox spans from wmf2svg_draw's logic
    if ((ddata->width == 0) || (ddata->height == 0))
    {
        float fallback_width = ddata->bbox.BR.x - ddata->bbox.TL.x;
        float fallback_height = ddata->bbox.BR.y - ddata->bbox.TL.y;

        if (fallback_width <= 0.0f) fallback_width = 1.0f;
        if (fallback_height <= 0.0f) fallback_height = 1.0f;
        
        if (ddata->width == 0) ddata->width  = (unsigned int) ceil (fallback_width);
        if (ddata->height == 0) ddata->height = (unsigned int) ceil (fallback_height);
    }
    
    // Ensure ddata->width and ddata->height are at least 1 if they somehow ended up zero
    if (ddata->width == 0) ddata->width = 1;
    if (ddata->height == 0) ddata->height = 1;

    // Robustly calculate viewBox dimensions
    float viewbox_x = ddata->bbox.TL.x;
    float viewbox_y = ddata->bbox.TL.y;
    float viewbox_width = ddata->bbox.BR.x - viewbox_x; 
    float viewbox_height = ddata->bbox.BR.y - viewbox_y;

    if (viewbox_width <= 0.0f) {
        viewbox_width = 1.0f;
    }
    if (viewbox_height <= 0.0f) {
        viewbox_height = 1.0f;
    }

    // Determine SVG version string for the output
    const char* svg_version_str = "1.1"; // Default
    const char* svg_doctype_public = "-//W3C//DTD SVG 1.1//EN";
    const char* svg_doctype_system = "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd";

    if (ddata->version_string && strcmp(ddata->version_string, "1.0") == 0) {
        svg_version_str = "1.0";
        svg_doctype_public = "-//W3C//DTD SVG 20001102//EN"; // Older SVG 1.0 DTD
        svg_doctype_system = "http://www.w3.org/TR/2000/CR-SVG-20001102/DTD/svg-20001102.dtd";
    }

    wmf_stream_printf (API,out,"<?xml version=\"1.0\" standalone=\"no\"?>\n");
    wmf_stream_printf (API,out,"<!DOCTYPE svg PUBLIC \"%s\"\n", svg_doctype_public);
    wmf_stream_printf (API,out,"\"%s\">\n", svg_doctype_system);

    wmf_stream_printf (API,out,"<svg width=\"%u\" height=\"%u\" viewBox=\"%f %f %f %f\" version=\"%s\"\n",
                       ddata->width, ddata->height,
                       viewbox_x, viewbox_y, viewbox_width, viewbox_height,
                       svg_version_str);
    wmf_stream_printf (API,out,"\txmlns:sodipodi=\"http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd\">\n");

    if (ddata->Description)
    {
        wmf_stream_printf (API,out,"<desc>%s</desc>\n",ddata->Description);
    }
}

/* This is called from the end of each play for page termination
 */
static void wmf_svg_device_end (wmfAPI* API)
{	wmf_svg_t* ddata = WMF_SVG_GetData (API);

	wmfStream* out = ddata->out;

	WMF_DEBUG (API,"~~~~~~~~wmf_[svg_]device_end");

	if (out == 0) return;

	wmf_stream_printf (API,out,"</svg>\n");
}

static svgPoint svg_translate (wmfAPI* API,wmfD_Coord d_pt)
{	wmf_svg_t* ddata = WMF_SVG_GetData (API);

	svgPoint g_pt;

	/* With viewBox, coordinates are in WMF's native system. */
	/* The viewBox itself handles mapping to the output width/height. */
	g_pt.x = (float) d_pt.x;
	g_pt.y = (float) d_pt.y;

	return (g_pt);
}

static float svg_width (wmfAPI* API,float wmf_width)
{	/* wmf_svg_t* ddata = WMF_SVG_GetData (API); */ /* No longer needed */

	/* With viewBox, widths are in WMF's native coordinate system. */
	return wmf_width;
}

static float svg_height (wmfAPI* API,float wmf_height)
{	/* wmf_svg_t* ddata = WMF_SVG_GetData (API); */ /* No longer needed */

	/* With viewBox, heights are in WMF's native coordinate system. */
	return wmf_height;
}
