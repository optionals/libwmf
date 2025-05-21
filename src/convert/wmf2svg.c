/* libwmf (convert/wmf2svg.c): library for wmf conversion
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


#ifdef HAVE_CONFIG_H
#include "wmfconfig.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "wmfdefs.h"

#include "libwmf/svg.h"

typedef struct
{	int    argc;
	char** argv;

	char** auto_files;
	char*  wmf_filename;
	char*  svg_filename;

	FILE*  out;
	gzFile outz;

	unsigned int svgz;
	unsigned int inline_images;

	wmf_svg_t options;

	unsigned int max_width;
	unsigned int max_height;

	unsigned long max_flags;

	char* svg_version; /* Added for --svg-version option */
} PlotData;

typedef struct _ImageContext ImageContext;

struct _ImageContext
{	int number;
	char* prefix;
};

char* image_name (void*);

#define WMF2SVG_MAXPECT (1 << 0)
#define WMF2SVG_MAXSIZE (1 << 1)

int  wmf2svg_draw (PlotData*);

void wmf2svg_init (PlotData*,int,char**);
void wmf2svg_help (PlotData*);
int  wmf2svg_args (PlotData*);
int  wmf2svg_auto (PlotData*);
int  wmf2svg_file (PlotData*);

int  explicit_wmf_error (char*,wmf_error_t);

int wmf2svg_draw (PlotData* pdata)
{	int status = 0;

	float wmf_width;
	float wmf_height;

	float ratio_wmf;
	float ratio_bounds;

	unsigned long flags;
	unsigned long max_flags;

	static char* Default_Description = "wmf2svg";

	ImageContext IC;

	wmf_error_t err;

	wmf_svg_t* ddata = 0;

	wmfAPI* API = 0;

	wmfAPI_Options api_options;

	flags = 0;

	flags |= WMF_OPT_FUNCTION;
	api_options.function = wmf_svg_function;

	flags |= WMF_OPT_ARGS;
	api_options.argc = pdata->argc;
	api_options.argv = pdata->argv;
#ifndef DEBUG
	flags |= WMF_OPT_IGNORE_NONFATAL;
#endif
	err = wmf_api_create (&API,flags,&api_options);
	status = explicit_wmf_error ("wmf_api_create",err);

	if (status)
	{	if (API) wmf_api_destroy (API);
		return (status);
	}

	err = wmf_file_open (API,pdata->wmf_filename);
	status = explicit_wmf_error ("wmf_file_open",err);

	if (status)
	{	wmf_api_destroy (API);
		return (status);
	}

	err = wmf_scan (API,0,&(pdata->options.bbox));
	status = explicit_wmf_error ("wmf_scan",err);

	if (status)
	{	wmf_api_destroy (API);
		return (status);
	}

/* Okay, got this far, everything seems cool.
 */
	ddata = WMF_SVG_GetData (API);
	ddata->version_string = pdata->svg_version; /* Pass SVG version to device data */

	if (pdata->out)
	{	ddata->out = wmf_stream_create (API,pdata->out);
	}
	else
	{	ddata->out = wmf_ztream_create (API,pdata->outz);
	}

	if (pdata->options.Description) ddata->Description = pdata->options.Description;
	else                            ddata->Description = Default_Description;

	// Assign the scanned bounding box to the device data.
	// pdata->options.bbox is populated by wmf_scan() earlier.
	ddata->bbox = pdata->options.bbox;

	// Calculate base dimensions for the SVG canvas from the bounding box scan.
	// These dimensions represent the actual extent of the WMF content.
	float base_width_from_bbox = ddata->bbox.BR.x - ddata->bbox.TL.x;
	float base_height_from_bbox = ddata->bbox.BR.y - ddata->bbox.TL.y;

	// Robust sanitization for width
	if (isnan(base_width_from_bbox) || isinf(base_width_from_bbox) || base_width_from_bbox <= 0.0f) {
		base_width_from_bbox = 1.0f;
	}

	// Robust sanitization for height
	if (isnan(base_height_from_bbox) || isinf(base_height_from_bbox) || base_height_from_bbox <= 0.0f) {
		base_height_from_bbox = 1.0f;
	}
	
	// The wmf_size() function is NOT used here to determine base_width/base_height
	// as we want the SVG canvas to be based on the actual content extents (bbox).
	// float wmf_api_width, wmf_api_height;
	// wmf_size (API, &wmf_api_width, &wmf_api_height); // This might provide different values

	max_flags = pdata->max_flags; // User-specified scaling flags from command-line

	// Determine if scaling is needed based on max_width/max_height command-line options.
	// The comparison is against the content's actual span (base_width_from_bbox, base_height_from_bbox).
	if ((base_width_from_bbox  > (float) pdata->max_width )
	 || (base_height_from_bbox > (float) pdata->max_height))
	{	
		// If content exceeds specified max dimensions and no specific scaling flag is set,
		// default to preserving aspect ratio (WMF2SVG_MAXPECT).
		if (max_flags == 0) max_flags = WMF2SVG_MAXPECT;
	}

	if (max_flags == WMF2SVG_MAXPECT) /* Scale the image to fit within max_width/max_height, preserving aspect ratio */
	{	
		float aspect_ratio_content = base_height_from_bbox / base_width_from_bbox;
		// Calculate the aspect ratio of the bounding box defined by max_width and max_height.
		// Ensure pdata->max_width is not zero to prevent division by zero.
		float aspect_ratio_bounds = (pdata->max_width > 0) ? ((float) pdata->max_height / (float) pdata->max_width) : 0;

		if (pdata->max_width == 0 || pdata->max_height == 0 || aspect_ratio_bounds == 0) { // Avoid division by zero if max_width/height is 0
			ddata->width = (unsigned int) ceil((double)base_width_from_bbox);
			ddata->height = (unsigned int) ceil((double)base_height_from_bbox);
		} else if (aspect_ratio_content > aspect_ratio_bounds) // Content is "taller" or "thinner" than the bounds' aspect ratio
		{	
			ddata->height = pdata->max_height;
			ddata->width  = (unsigned int) ceil((float) ddata->height / aspect_ratio_content);
		}
		else // Content is "wider" or "shorter" than the bounds' aspect ratio
		{	
			ddata->width  = pdata->max_width;
			ddata->height = (unsigned int) ceil((float) ddata->width  * aspect_ratio_content);
		}
	}
	else if (max_flags == WMF2SVG_MAXSIZE) /* Scale image to max_width and max_height, potentially distorting aspect ratio */
	{	
		ddata->width  = pdata->max_width;
		ddata->height = pdata->max_height;
	}
	else // No scaling flags (--maxpect or --maxsize), or image dimensions are within specified max_width/max_height
	{	
		// Set SVG width/height attributes directly from the content's bounding box span.
		ddata->width  = (unsigned int) ceil ((double) base_width_from_bbox );
		ddata->height = (unsigned int) ceil ((double) base_height_from_bbox );
	}

	if (pdata->inline_images)
	{	ddata->flags |= WMF_SVG_INLINE_IMAGES;
	}
	else
	{	IC.number = 0;
		IC.prefix = (char*) malloc (strlen (pdata->wmf_filename) + 1);
		if (IC.prefix)
		{	strcpy (IC.prefix,pdata->wmf_filename);
			if (wmf_strstr (pdata->wmf_filename,".wmf"))
			{	IC.prefix[strlen (pdata->wmf_filename)-4] = 0;
			}
			ddata->image.context = (void*) (&IC);
			ddata->image.name = image_name;
		}
	}

	if (status == 0)
	{	err = wmf_play (API,0,&(pdata->options.bbox));
		status = explicit_wmf_error ("wmf_play",err);
	}

	wmf_api_destroy (API);

	return (status);
}

void wmf2svg_init (PlotData* pdata,int argc,char** argv)
{	pdata->argc = argc;
	pdata->argv = argv;

	pdata->auto_files = 0;
	pdata->wmf_filename = 0;
	pdata->svg_filename = 0;

	pdata->out  = 0;
	pdata->outz = 0;

	pdata->svgz = 0;

	pdata->inline_images = 0;

	pdata->options.Description = 0;

	pdata->options.width  = 0;
	pdata->options.height = 0;

	pdata->options.flags = 0;

	pdata->max_width  = 768;
	pdata->max_height = 512;

	pdata->max_flags = 0;

	pdata->svg_version = "1.1"; /* Default SVG version */
}

void wmf2svg_help (PlotData* pdata)
{	(void)pdata;
	fputs ("\
Usage: wmf2svg [OPTION]... [-o <file.svg>] <file.wmf>\n\
  or:  wmf2svg [OPTION]... --auto <file1.wmf> [<file2.wmf> ...]\n\
Convert metafile image to W3C's scaleable vector graphic (SVG) format.\n\
\n\
  -z              compressed output (.svgz)\n\
  --inline        include images in the svg code\n\
  --desc=<str>    description tag\n\
  --maxwidth=<w>  where <w> is maximum width image may have.\n\
  --maxheight=<h> where <h> is maximum height image may have.\n\
  --maxpect       scale image to maximum size keeping aspect.\n\
  --maxsize       scale image to maximum size.\n\
  --svg-version=<ver> use SVG version <ver> (\"1.0\" or \"1.1\", default: \"1.1\").\n\
  --version       display version info and exit.\n\
  --help          display this help and exit.\n\
  --wmf-help      display wmf-related help and exit.\n\
\n\
Report bugs to <http://www.wvware.com/>.\n",stdout);
}

int wmf2svg_args (PlotData* pdata)
{	int status = 0;
	int arg = 0;

	int    argc = pdata->argc;
	char** argv = pdata->argv;

	while ((++arg) < argc)
	{	if (strcmp (argv[arg],"--help") == 0)
		{	wmf2svg_help (pdata);
			status = argc; /* i.e., not an error but don't continue */
			break;
		}

		if (strcmp (argv[arg],"--wmf-help") == 0)
		{	fputs (wmf_help (),stdout);
			status = argc; /* i.e., not an error but don't continue */
			break;
		}

		if (strcmp (argv[arg],"--version") == 0)
		{	fprintf (stdout,"%s: version %s\n",PACKAGE,VERSION);
			status = argc; /* i.e., not an error but don't continue */
			break;
		}

		if (strcmp (argv[arg],"-z") == 0)
		{	pdata->svgz = 1;
			continue;
		}

		if (strncmp (argv[arg],"--svg-version=",14) == 0)
		{	char* version_arg = argv[arg]+14;
			if (strcmp(version_arg, "1.0") == 0 || strcmp(version_arg, "1.1") == 0)
			{	pdata->svg_version = version_arg;
			}
			else
			{	fprintf (stderr,"usage: --svg-version=<ver>, where <ver> is \"1.0\" or \"1.1\".\n");
				status = arg;
				break;
			}
			continue;
		}

		if (strcmp (argv[arg],"--inline") == 0)
		{	pdata->inline_images = 1;
			continue;
		}

		if (strncmp (argv[arg],"--desc=",7) == 0)
		{	pdata->options.Description = argv[arg] + 7;
			continue;
		}

		if (strncmp (argv[arg],"--maxwidth=",11) == 0)
		{	if (sscanf (argv[arg]+11,"%u",&(pdata->max_width)) != 1)
			{	fputs ("usage: --maxwidth=<width>, where <width> is +ve integer.\n",stderr);
				status = arg;
				break;
			}
			continue;
		}
		if (strncmp (argv[arg],"--maxheight=",12) == 0)
		{	if (sscanf (argv[arg]+12,"%u",&(pdata->max_height)) != 1)
			{	fputs ("usage: --maxheight=<height>, where <height> is +ve integer.\n",stderr);
				status = arg;
				break;
			}
			continue;
		}

		if (strcmp (argv[arg],"--maxpect") == 0)
		{	pdata->max_flags = WMF2SVG_MAXPECT;
			continue;
		}
		if (strcmp (argv[arg],"--maxsize") == 0)
		{	pdata->max_flags = WMF2SVG_MAXSIZE;
			continue;
		}

		if (strcmp (argv[arg],"--auto") == 0)
		{	pdata->auto_files = argv + arg + 1;
			break;
		}
		if (strcmp (argv[arg],"-o") == 0)
		{	if ((++arg) < argc)
			{	pdata->svg_filename = argv[arg];
				continue;
			}
			fprintf (stderr,"usage: `wmf2svg -o <file.svg> <file.wmf>'.\n");
			fprintf (stderr,"Try `%s --help' for more information.\n",argv[0]);
			status = arg;
			break;
		}
		if (strncmp (argv[arg],"--wmf-",6) == 0)
		{	continue;
		}
		if (argv[arg][0] != '-')
		{	pdata->wmf_filename = argv[arg];
			continue;
		}

		fprintf (stderr,"option `%s' not recognized.\n",argv[arg]);
		fprintf (stderr,"Try `%s --help' for more information.\n",argv[0]);
		status = arg;
		break;
	}

	if (status == 0)
	{	if ((pdata->auto_files == 0) && (pdata->wmf_filename == 0))
		{	fprintf (stderr,"No input file specified!\n");
			fprintf (stderr,"Try `%s --help' for more information.\n",argv[0]);
			status = argc;
		}
	}

	return (status);
}

int wmf2svg_auto (PlotData* pdata)
{	int status = 0;

	while (1)
	{	pdata->wmf_filename = (*(pdata->auto_files));

		if (pdata->wmf_filename == 0) break;

		if (strcmp (pdata->wmf_filename+strlen(pdata->wmf_filename)-4,".wmf"))
		{	fprintf (stderr,"%s: expected suffix `.wmf'. ",pdata->wmf_filename);
			fprintf (stderr,"skipping...\n");
			status++;
		}
		else if ((pdata->svg_filename = malloc (strlen(pdata->wmf_filename)+2)) == 0)
		{	fprintf (stderr,"mem_alloc_err: skipping %s...\n",pdata->wmf_filename);
			status++;
		}
		else
		{	strcpy (pdata->svg_filename,pdata->wmf_filename);
			if (pdata->svgz)
			{	strcpy (pdata->svg_filename+strlen(pdata->svg_filename)-3,"svgz");
			}
			else
			{	strcpy (pdata->svg_filename+strlen(pdata->svg_filename)-3,"svg");
			}
			if (wmf2svg_file (pdata)) status++;
			free (pdata->svg_filename);
		}
		
		pdata->auto_files++;
	}

	return (status);
}

int wmf2svg_file (PlotData* pdata)
{	int status = 0;

	if (pdata->svgz)
	{	if (pdata->svg_filename == 0)
		{	fprintf (stderr,"unable to write compressed to stdout.\n");
			status = 1;
			return (status);
		}
		if ((pdata->outz = gzopen (pdata->svg_filename,"wb9")) == 0)
		{	fprintf (stderr,"unable to write to `%s'. ",pdata->svg_filename);
			fprintf (stderr,"skipping...\n");
			status = 1;
			return (status);
		}

		status = wmf2svg_draw (pdata);

		gzclose (pdata->outz);

		return (status);
	}

	pdata->out = stdout;
	if (pdata->svg_filename)
	{	if ((pdata->out = fopen (pdata->svg_filename,"w")) == 0)
		{	fprintf (stderr,"unable to write to `%s'. ",pdata->svg_filename);
			fprintf (stderr,"skipping...\n");
			status = 1;
			return (status);
		}
	}

	status = wmf2svg_draw (pdata);

	if (pdata->out != stdout) fclose (pdata->out);

	return (status);
}

int main (int argc,char** argv)
{	int status = 0;

	PlotData PData;

	wmf2svg_init (&PData,argc,argv);

	status = wmf2svg_args (&PData);

	if (status) return (status);

	if (PData.auto_files) status = wmf2svg_auto (&PData);
	else                  status = wmf2svg_file (&PData);

	return (status);
}

int explicit_wmf_error (char* str,wmf_error_t err)
{	(void)str;
	int status = 0;

	switch (err)
	{
	case wmf_E_None:
#ifdef DEBUG
		fprintf (stderr,"%s returned with wmf_E_None.\n",str);
#endif
		status = 0;
	break;

	case wmf_E_InsMem:
#ifdef DEBUG
		fprintf (stderr,"%s returned with wmf_E_InsMem.\n",str);
#endif
		status = 1;
	break;

	case wmf_E_BadFile:
#ifdef DEBUG
		fprintf (stderr,"%s returned with wmf_E_BadFile.\n",str);
#endif
		status = 1;
	break;

	case wmf_E_BadFormat:
#ifdef DEBUG
		fprintf (stderr,"%s returned with wmf_E_BadFormat.\n",str);
#endif
		status = 1;
	break;

	case wmf_E_EOF:
#ifdef DEBUG
		fprintf (stderr,"%s returned with wmf_E_EOF.\n",str);
#endif
		status = 1;
	break;

	case wmf_E_DeviceError:
#ifdef DEBUG
		fprintf (stderr,"%s returned with wmf_E_DeviceError.\n",str);
#endif
		status = 1;
	break;

	case wmf_E_Glitch:
#ifdef DEBUG
		fprintf (stderr,"%s returned with wmf_E_Glitch.\n",str);
#endif
		status = 1;
	break;

	case wmf_E_Assert:
#ifdef DEBUG
		fprintf (stderr,"%s returned with wmf_E_Assert.\n",str);
#endif
		status = 1;
	break;

	default:
#ifdef DEBUG
		fprintf (stderr,"%s returned unexpected value.\n",str);
#endif
		status = 1;
	break;
	}

	return (status);
}

char* image_name (void* context)
{	ImageContext* IC = (ImageContext*) context;

	int length;

	char* name = 0;

	length = strlen (IC->prefix) + 16;

	name = malloc (length);

	if (name == 0) return (0);

	IC->number++;

	sprintf (name,"%s-%d.png",IC->prefix,IC->number);

	return (name);
}
