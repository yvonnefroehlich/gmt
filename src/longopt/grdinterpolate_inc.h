/*--------------------------------------------------------------------
 *
 *	Copyright (c) 1991-2025 by the GMT Team (https://www.generic-mapping-tools.org/team.html)
 *	See LICENSE.TXT file for copying and redistribution conditions.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU Lesser General Public License as published by
 *	the Free Software Foundation; version 3 or any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU Lesser General Public License for more details.
 *
 *	Contact info: www.generic-mapping-tools.org
 *--------------------------------------------------------------------*/

#ifndef GRDINTERPOLATE_INC_H
#define GRDINTERPOLATE_INC_H

/* Translation table from long to short module options, directives and modifiers */

static struct GMT_KEYWORD_DICTIONARY module_kw[] = { /* Local options for this module */
	/* separator, short_option, long_option,
		  short_directives,    long_directives,
		  short_modifiers,     long_modifiers,
		  transproc_mask */
	{ 0, 'D', "netcdf|netCDF|ncheader",
	          "",                      "",
	          "x,y,z,c,d,s,o,n,t,r,v", "xname,yname,zname,cpt|cmap,dname,scale,offset,invalid,title,remark,varname",
		  GMT_TP_STANDARD },
	{ 0, 'E', "profile|crosssection",
	          "",                      "",
	          "a,g,i,l,n,o,p,r,x",     "azimuth,degrees,increment,length,npoints,origin,parallel,radius,rhumb",
		  GMT_TP_STANDARD },
	{ 0, 'F', "interptype",
	          "l,a,c,n",               "linear,akima,cubic,none",
	          "d",                     "derivative",
		  GMT_TP_STANDARD },
	GMT_G_OUTGRID_KW,
	{ 0, 'S', "pointseries",
	          "",                      "",
	          "h",                     "header",
		  GMT_TP_STANDARD },
	{ 0, 'T', "inc|range",
	          "",                      "",
	          "i,n",                   "inverse,numcoords",
		  GMT_TP_STANDARD },
	{ 0, 'Z', "levels",
	          "",                      "",
	          "i,n",                   "inverse,numcoords",
		  GMT_TP_STANDARD },
	{ 0, '\0', "", "", "", "", "", 0 }  /* End of list marked with empty option and strings */
};

#endif  /* !GRDINTERPOLATE_INC_H */
