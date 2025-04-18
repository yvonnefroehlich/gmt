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
/*
 * Author:	Paul Wessel
 * Date:	1-JAN-2010
 * Version:	6 API
 *
 * Brief synopsis: grdinfo reads one or more grid/cube file and [optionally] prints
 * out various statistics like mean/standard deviation and median/scale.
 *
 */

#include "gmt_dev.h"
#include "longopt/grdinfo_inc.h"

#define THIS_MODULE_CLASSIC_NAME	"grdinfo"
#define THIS_MODULE_MODERN_NAME	"grdinfo"
#define THIS_MODULE_LIB		"core"
#define THIS_MODULE_PURPOSE	"Extract information from 2-D grids or 3-D cubes"
#define THIS_MODULE_KEYS	"<?{+,>D}"
#define THIS_MODULE_NEEDS	""
#define THIS_MODULE_OPTIONS "->RVfho"

/* Control structure for grdinfo */

enum Opt_I_modes {
	GRDINFO_GIVE_INCREMENTS = 0,
	GRDINFO_GIVE_REG_ORIG,
	GRDINFO_GIVE_REG_OBLIQUE,
	GRDINFO_GIVE_REG_IMG,
	GRDINFO_GIVE_REG_ROUNDED,
	GRDINFO_GIVE_BOUNDBOX};

enum Opt_C_modes {
	GRDINFO_TRADITIONAL	= 0,
	GRDINFO_NUMERICAL	= 1,
	GRDINFO_TRAILING	= 2};

enum Opt_L_modes {
	GRDINFO_RANGE	= 0,
	GRDINFO_MEAN	= 1,
	GRDINFO_MEDIAN	= 2,
	GRDINFO_MODE	= 4};

enum Opt_M_modes {
	GRDINFO_FORCE_REPORT	= 1,
	GRDINFO_FORCE			= 2,
	GRDINFO_CONDITIONAL		= 3};

struct GRDINFO_CTRL {
	unsigned int n_files;	/* How many grids given */
	bool is_cube;
	struct GRDINFO_C {	/* -C[n|t] */
		bool active;
		unsigned int mode;
	} C;
	struct GRDINFO_D {	/* -D[dx[/dy]][+i] */
		bool active;
		unsigned int mode;
		double inc[2];
	} D;
	struct GRDINFO_E {	/* -E[x|y][+u|l ]*/
		bool active;
		unsigned int mode;	/* GMT_X for cols, GMT_Y for rows */
		int val;	/* -1 for finding the minimum (+l); +1 for the maximum (+u) [Default] */
		int type;	/* -1: Only negative values tested, 0: all values [Default], +1: Only positive values tested */
	} E;
	struct GRDINFO_F {	/* -F */
		bool active;
	} F;
	struct GRDINFO_G {	/* -G */
		bool active;
	} G;
	struct GRDINFO_I {	/* -Ir|b|i|dx[/dy] */
		bool active;
		unsigned int status;
		double inc[2];
	} I;
	struct GRDINFO_M {	/* -M[c|f] */
		bool active;
		unsigned int mode;	/* 1 is force, 2 is conditional */
	} M;
	struct GRDINFO_L {	/* -L[0|1|2|p] */
		bool active;
		unsigned int norm;
	} L;
	struct GRDINFO_T {	/* -T[s]<dv>  -T[<dv>][+s][+a[<alpha>]] */
		bool active;
		unsigned int mode;
		double inc;
		double alpha[2];	/* XLO is lower and XHI is upper tail */
	} T;
};

static void *New_Ctrl (struct GMT_CTRL *GMT) {	/* Allocate and initialize a new control structure */
	struct GRDINFO_CTRL *C;

	C = gmt_M_memory (GMT, NULL, 1, struct GRDINFO_CTRL);

	/* Initialize values whose defaults are not 0/false/NULL */
	C->E.val = +1;	/* Default is to look for maxima */
	C->T.alpha[XLO] = C->T.alpha[XHI] = 1.0;	/* 1 % alpha trim for both tails is default if not specified */
	return (C);
}

static void Free_Ctrl (struct GMT_CTRL *GMT, struct GRDINFO_CTRL *C) {	/* Deallocate control structure */
	if (!C) return;
	gmt_M_free (GMT, C);
}

static int usage (struct GMTAPI_CTRL *API, int level) {
	const char *name = gmt_show_name_and_purpose (API, THIS_MODULE_LIB, THIS_MODULE_CLASSIC_NAME, THIS_MODULE_PURPOSE);
	if (level == GMT_MODULE_PURPOSE) return (GMT_NOERROR);
	GMT_Usage (API, 0, "usage: %s %s [-C[n|t]] [-D[<offx>[/<offy>]][+i]] [-E[x|y][+l|L|u|U]] [-F] [-G] [-I[<dx>[/<dy>]|b|i|o|r]] [-L[a|0|1|2|p]] "
		"[-M[c|f]] [%s] [-T[<dv>][+a[<alpha>]][+s]] [%s] [%s] [%s] [%s] [%s]\n", name, GMT_INGRID, GMT_Rgeo_OPT, GMT_V_OPT, GMT_f_OPT, GMT_ho_OPT, GMT_o_OPT, GMT_PAR_OPT);

	if (level == GMT_SYNOPSIS) return (GMT_MODULE_SYNOPSIS);

	GMT_Message (API, GMT_TIME_NONE, "  REQUIRED ARGUMENTS:\n");
	gmt_ingrid_syntax (API, 0, "Name of one or more grid or cube files");
	GMT_Usage (API, 1, "\nNote: 3-D cubes are not compatible with -D, -E, -F and -Ib.");
	GMT_Message (API, GMT_TIME_NONE, "\n  OPTIONAL ARGUMENTS:\n");
	GMT_Usage (API, 1, "\n-C[n|t]");
	GMT_Usage (API, -2, "Report information in fields on a single line using the format "
		"<file w e s n {b t} v0 v1 dx dy {dz} n_columns n_rows {n_layers} [x0 y0 {z0} x1 y1 {z1}] [med L1scale] [mean std rms] [n_nan] [mode LMSscale] registration type>, "
		"where -M adds [x0 y0 x1 y1] and [n_nan], -L1 adds [median L1scale], -L2 adds [mean std rms], "
		"and -Lp adds [mode LMSscale]). Ends with registration (0=gridline, 1=pixel) and type (0=Cartesian, 1=geographic). Optional directives:");
	GMT_Usage (API, 3, "t: Place <file> at the end of the output record.");
	GMT_Usage (API, 3, "n: Write only numerical columns.");
	GMT_Usage (API, -2, "The items in {} are only output when used with 3-D data cubes.");
	GMT_Usage (API, 1, "\n-D[<offx>[/<offy>]][+i]");
	GMT_Usage (API, -2, "Report tile regions using tile size set in -I. Optionally, extend each tile region by <offx>/<offy> to add overlap. "
		"Append +i to only report tiles if the subregion contains data (limited to one input grid). "
		"If no grid is given then -R must be given and we tile based on the given region. "
		"Use -Ct to append the region string as trailing text to the numerical columns.");
	GMT_Usage (API, 1, "\n-E[x|y][+l|L|u|U]");
	GMT_Usage (API, -2, "Report extreme values per column (append x) or row (append y) [x]. Only one input grid is accepted:");
	GMT_Usage (API, 3, "+l Report minima.");
	GMT_Usage (API, 3, "+L Same as +l but only consider positive values.");
	GMT_Usage (API, 3, "+u Report maxima [Default].");
	GMT_Usage (API, 3, "+U Same as +u but only consider negative values.");
	GMT_Usage (API, 1, "\n-F Report domain in world mapping format [Default is generic].");
	GMT_Usage (API, 1, "\n-G Force possible download of all tiles for a remote <grid> if given as input [no report for tiled grids].");
	GMT_Usage (API, 1, "\n-I[<dx>[/<dy>]|b|i|o|r]");
	GMT_Usage (API, -2, "Return various results depending on directives:");
	GMT_Usage (API, 3, "b: Return the grid's bounding box polygon at node resolution.");
	GMT_Usage (API, 3, "i: The original img2grd -R string is issued, if available. "
		"If the grid is not an img grid then the regular -R string is issued.");
	GMT_Usage (API, 3, "o: The grid's -R string is issued but in oblique format (-RLLX/LLY/URX/URY+r).");
	GMT_Usage (API, 3, "r: The grid's -R string is issued in regular -Rw/e/s/n format.");
	GMT_Usage (API, -2, "Otherwise, return textstring -Rw/e/s/n{/b/t} to nearest multiple of dx/dy{/dz}. "
		"If -C is set then rounding will occur but no -R string is issued. "
		"If no argument is given then the -I<xinc>/<yinc>{/<zinc>} string is issued.");
	GMT_Usage (API, 1, "\n-L[a|0|1|2|p]");
	GMT_Usage (API, -2, "Set report mode, append directive:");
	GMT_Usage (API, 3, "0: Report range of data by actually reading them (not from header).");
	GMT_Usage (API, 3, "1: Report median and L1 scale (MAD w.r.t. median) of data set.");
	GMT_Usage (API, 3, "2: Report mean, standard deviation, and rms of data set [Default].");
	GMT_Usage (API, 3, "p: Report mode (lms) and LMS-scale (MAD w.r.t. mode) of data set.");
	GMT_Usage (API, 3, "a: All of the above.");
	GMT_Usage (API, -2, "Note: If grid is geographic then we report area-weighted statistics.");
	GMT_Usage (API, 1, "\n-M[c|f]");
	GMT_Usage (API, -2, "\nSearch for the global data min and max locations (x0,y0{,z0}) and (x1,y1{,z1}) [Default]."
		" Append c to only determine data min/max range if missing from the header, or f to force that search to override the header range.");
	GMT_Option (API, "R");
	GMT_Usage (API, 1, "\n-T[<dv>][+a[<alpha>]][+s]");
	GMT_Usage (API, -2, "Print global -Tvmin/vmax[/dv] (in rounded multiples of <dv>, if given). Optional modifiers:");
	GMT_Usage (API, 3, "+a Trim grid range by excluding the <alpha>/2 from each tail [2 %%]. "
		"Note: +a is limited to a single grid.  Give <alpha> in percent. "
		"Give <alpha> as <alphaL>/<alphaR> to set unequal tail areas.");
	GMT_Usage (API, 3, "+s Force a symmetrical range about zero.");
	GMT_Option (API, "V,f,h,o,.");

	return (GMT_MODULE_USAGE);
}

static int parse (struct GMT_CTRL *GMT, struct GRDINFO_CTRL *Ctrl, struct GMT_OPTION *options) {

	/* This parses the options provided to grdcut and sets parameters in CTRL.
	 * Any GMT common options will override values set previously by other commands.
	 * It also replaces any file names specified as input or output with the data ID
	 * returned when registering these sources/destinations with the API.
	 */

	bool no_file_OK, num_report;
	unsigned int n_errors = 0, nc = 0, ng = 0;
	char text[GMT_LEN32] = {""}, *c = NULL;
	static char *M[2] = {"minimum", "maximum"}, *V[3] = {"negative", "all", "positive"}, *T[2] = {"column", "row"};
	struct GMT_OPTION *opt = NULL;
	struct GMTAPI_CTRL *API = GMT->parent;

	/* First precheck if we are dealing with cubes or grids - no mixing is allowed */

	for (opt = options; opt; opt = opt->next) {	/* Loop over arguments, skip options */

		if (opt->option != '<') continue;	/* We are only processing filenames here */

		if (gmt_M_memfile_is_grid (opt->arg))	/* Can just check file name */
			ng++;
		else if (gmt_M_memfile_is_cube (opt->arg))	/* Can just check file name */
			nc++;
		else if (gmt_nc_is_cube (API, opt->arg))	/* Determine if this file is a netCDF cube or not */
			nc++;
		else	/* Thus a grid */
			ng++;
	}

	if (nc && ng) {
		GMT_Report (API, GMT_MSG_ERROR, "Cannot give a mixed list of data cubes and data grids\n");
		n_errors++;
		goto time_to_return;
	}
	Ctrl->is_cube = (nc > 0);

	for (opt = options; opt; opt = opt->next) {	/* Process all the options given */

		switch (opt->option) {
			/* Common parameters */

			case '<':	/* Input files */
				if (GMT_Get_FilePath (API, GMT_IS_GRID, GMT_IN, GMT_FILE_REMOTE, &(opt->arg)))
					n_errors++;
				else
					Ctrl->n_files++;
				break;

			/* Processes program-specific parameters */

			case 'C':	/* Column format */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->C.active);
				if (opt->arg[0] == 'n')
					Ctrl->C.mode = GRDINFO_NUMERICAL;
				else if (opt->arg[0] == 't')
					Ctrl->C.mode = GRDINFO_TRAILING;
				if (GMT->parent->external && Ctrl->C.mode == GRDINFO_TRADITIONAL) Ctrl->C.mode = GRDINFO_NUMERICAL;
				break;
			case 'D':	/* Tiling output w/ optional overlap */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->D.active);
				if (opt->arg[0]) {
					if ((c = strstr (opt->arg, "+i"))) {
						c[0] = '\0';	/* Temporarily chop off modifier */
						Ctrl->D.mode = 1;
					}
					if (opt->arg[0] && gmt_getinc (GMT, opt->arg, Ctrl->D.inc)) {
						gmt_inc_syntax (GMT, 'D', 1);
						n_errors++;
					}
				}
				break;
			case 'E':	/* Report Extrema per row/col */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->E.active);
				switch (opt->arg[0]) {
					case 'x': case '\0': case '+':	/* Handles -E, -Ex, -E+l */
						Ctrl->E.mode = GMT_X; break;
					case 'y':
						Ctrl->E.mode = GMT_Y;
						break;
					default:
						GMT_Report (API, GMT_MSG_ERROR, "Option -E: Expected -Ex or -Ey\n");
						n_errors++;
						break;
				}
				if ((c = strchr (opt->arg, '+'))) {	/* Gave a modifier */
					switch (c[1]) {
						case 'l': Ctrl->E.val = -1; break;
						case 'L': Ctrl->E.val = -1; Ctrl->E.type = +1; break;
						case 'u': Ctrl->E.val = +1; break;
						case 'U': Ctrl->E.val = +1; Ctrl->E.type = -1; break;
						default:
							GMT_Report (API, GMT_MSG_ERROR, "Option -E: Expected modifiers +l|L|u|U, not +%c\n", c[1]);
							n_errors++;
							break;
					}
				}
				GMT_Report (API, GMT_MSG_INFORMATION, "Will look for the %s value among %s values along each %s\n", M[(Ctrl->E.val+1)/2], V[Ctrl->E.type+1], T[Ctrl->E.mode]);
				break;
			case 'F':	/* World mapping format */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->F.active);
				n_errors += gmt_get_no_argument (GMT, opt->arg, opt->option, 0);
				break;
			case 'G':	/* Force download of tiled grids if information is requested */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->G.active);
				n_errors += gmt_get_no_argument (GMT, opt->arg, opt->option, 0);
				break;
			case 'I':	/* Increment rounding */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->I.active);
				if (!opt->arg[0])	/* No args given, we want to output the -I string */
					Ctrl->I.status = GRDINFO_GIVE_INCREMENTS;
				else if (opt->arg[0] == 'b' && opt->arg[1] == '\0')	/* -Ib means return grid perimeter as bounding box */
					Ctrl->I.status = GRDINFO_GIVE_BOUNDBOX;
				else if (opt->arg[0] == 'i' && opt->arg[1] == '\0')	/* -Ii means return -R string from original img2grd */
					Ctrl->I.status = GRDINFO_GIVE_REG_IMG;
				else if ((opt->arg[0] == 'r' || opt->arg[0] == '-') && opt->arg[1] == '\0')	/* -Ir: we want to output the actual -R string */
					Ctrl->I.status = GRDINFO_GIVE_REG_ORIG;
				else if (opt->arg[0] == 'o' && opt->arg[1] == '\0')	/* -Io means return -R string in oblique format (+r) */
					Ctrl->I.status = GRDINFO_GIVE_REG_OBLIQUE;
				else {	/* Report -R to nearest given multiple increment */
					Ctrl->I.status = GRDINFO_GIVE_REG_ROUNDED;
					if (gmt_getinc (GMT, opt->arg, Ctrl->I.inc)) {
						gmt_inc_syntax (GMT, 'I', 1);
						n_errors++;
					}
				}
				break;
			case 'L':	/* Selects norm (repeatable) */
				Ctrl->L.active = true;
				switch (opt->arg[0]) {
					case '\0': case '2':
						Ctrl->L.norm |= GRDINFO_MEAN; break;
					case '1':
						Ctrl->L.norm |= GRDINFO_MEDIAN; break;
					case 'p':
						Ctrl->L.norm |= GRDINFO_MODE; break;
					case 'a':	/* Select all three */
						Ctrl->L.norm |= (GRDINFO_MEAN+GRDINFO_MEDIAN+GRDINFO_MODE); break;
				}
				break;
			case 'M':	/* Global extrema and|or update missing header data range */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->M.active);
				switch (opt->arg[0]) {
					case 'c':	Ctrl->M.mode = GRDINFO_CONDITIONAL;	break;
					case 'f':	Ctrl->M.mode = GRDINFO_FORCE;		break;
					case '\0':	Ctrl->M.mode = GRDINFO_FORCE_REPORT;	break;
					default:
						GMT_Report (API, GMT_MSG_ERROR, "Option -M: Unrecognized modifier %s\n", opt->arg);
						n_errors++;
						break;
				}
				break;
			case 'Q':	/* Expect cubes is no longer an option as we detect it automatically */
				break;
			case 'T':	/* CPT range */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->T.active);
				if (opt->arg[0] == 's' && gmt_M_compat_check (GMT, 5)) {	/* Old-style format, cast in new syntax */
					GMT_Report (API, GMT_MSG_COMPAT, "-Ts option is deprecated; please use -T[<dv>][+s][+a[<alpha>]] next time.\n");
					sprintf (text, "%s+s", &opt->arg[1]);
				}
				else
					strncpy (text, opt->arg, GMT_LEN32-1);
				if (gmt_validate_modifiers (GMT, text, opt->option, "as", GMT_MSG_ERROR)) {
					GMT_Report (API, GMT_MSG_COMPAT, "Option -T: Syntax is -T[<dv>][+s][+a[<alpha>]] next time.\n");
					n_errors++;
				}
				else {
					char string[GMT_LEN32] = {""};
					if (text[0] && text[0] != '+')
						Ctrl->T.inc = atof (text);
					if (gmt_get_modifier (text, 's', string))	/* Want symmetrical range about 0, i.e., -3500/3500[/500] */
						Ctrl->T.mode |= 1;
					if (gmt_get_modifier (text, 'a', string)) {	/* Want alpha-trimmed range before determining limits */
						Ctrl->T.mode |= 2;
						if (string[0]) {	/* Gave a specific tail size or sizes */
							double *p = Ctrl->T.alpha;
							int n = GMT_Get_Values (API, string, p, 2);
							if (n == 1) /* Split the total equally */
								Ctrl->T.alpha[XLO] /= 2.0, Ctrl->T.alpha[XHI] = Ctrl->T.alpha[XLO];
							if (Ctrl->T.alpha[XLO] < 0.0 || Ctrl->T.alpha[XHI] < 0.0 || Ctrl->T.alpha[XLO] > 100.0 || Ctrl->T.alpha[XHI] > 100.0) {
								GMT_Report (API, GMT_MSG_COMPAT, "Option -T: Alpha values must be in the 0-100%% range.\n");
								n_errors++;
							}
						}
					}
				}
				break;

			default:	/* Report bad options */
				n_errors += gmt_default_option_error (GMT, opt);
				break;
		}
	}

	num_report = (Ctrl->C.active && (Ctrl->D.active || Ctrl->C.mode != GRDINFO_TRADITIONAL));
	no_file_OK = (Ctrl->D.active && Ctrl->D.mode == 0 && GMT->common.R.active[RSET]);
	n_errors += gmt_M_check_condition (GMT, Ctrl->n_files == 0 && !no_file_OK,
	                                   "Must specify one or more input files\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->D.mode && Ctrl->n_files != 1,
	                                   "Option -D: The +n modifier requires a single grid file\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->E.active && Ctrl->n_files != 1,
	                                   "Option -E: Requires a single grid file\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->T.active && Ctrl->T.inc < 0.0,
	                                   "Option -T: The optional increment must be positive\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->T.mode & 2 && Ctrl->n_files != 1,
	                                   "Option -T: The optional alpha-trim value can only work with a single grid file\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->T.active && ((Ctrl->T.alpha[XLO] + Ctrl->T.alpha[XHI]) > 100.0),
	                                   "Option -T: The sum of the optional alpha-trim value(s) cannot exceed 100 %%\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->I.active && Ctrl->I.status == GRDINFO_GIVE_REG_ROUNDED &&
	                                   (Ctrl->I.inc[GMT_X] <= 0.0 || Ctrl->I.inc[GMT_Y] <= 0.0),
									   "Option -I: Must specify a positive increment(s)\n");
	n_errors += gmt_M_check_condition (GMT, (Ctrl->I.active || Ctrl->T.active) && Ctrl->M.active,
	                                   "Option -M: Not compatible with -I or -T\n");
	n_errors += gmt_M_check_condition (GMT, (Ctrl->I.active || Ctrl->T.active) && Ctrl->L.active,
	                                   "Option -L: Not compatible with -I or -T\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->T.active && Ctrl->I.active,
	                                   "Only one of -I -T can be specified\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->C.active && Ctrl->F.active,
	                                   "Only one of -C, -F can be specified\n");
	n_errors += gmt_M_check_condition (GMT, GMT->common.o.active && !num_report,
	                                   "The -o option requires -Cn\n");
	if (Ctrl->is_cube) {	/* Got data cube(s) - a few more checks */
		n_errors += gmt_M_check_condition (GMT, Ctrl->D.active,
		                                   "Option -D: Cannot be used with 3-D cubes\n");
		n_errors += gmt_M_check_condition (GMT, Ctrl->E.active,
		                                   "Option -E: Cannot be used with 3-D cubes\n");
		n_errors += gmt_M_check_condition (GMT, Ctrl->F.active,
		                                   "Option -F: Cannot be used with 3-D cubes\n");
		n_errors += gmt_M_check_condition (GMT, Ctrl->L.active,
		                                   "Option -L: Cannot be used with 3-D cubes\n");
		n_errors += gmt_M_check_condition (GMT, Ctrl->I.active && Ctrl->I.status == GRDINFO_GIVE_BOUNDBOX,
		                                   "Option -Ib: Cannot be used with 3-D cubes\n");
	}

time_to_return:
	return (n_errors ? GMT_PARSE_ERROR : GMT_NOERROR);
}

struct GMT_TILES {
	double wesn[4];
};

GMT_LOCAL void grdinfo_report_tiles (struct GMT_CTRL *GMT, struct GMT_GRID *G, double w, double e, double s, double n, struct GRDINFO_CTRL *Ctrl) {
	/* Find the tiles covering the present grid, if given */
	bool use = true, num_report;
	unsigned int nx, ny, i, j, js = 0, jn = 0, ie, iw;
	uint64_t row, col, node;
	double wesn[4], out[4], box[4], dx = 0.0, dy = 0.0;
	char text[GMT_LEN64] = {""}, record[GMT_BUFSIZ] = {""};
	struct GMT_RECORD *Out = NULL;

	num_report = (Ctrl->C.active);
	Out = gmt_new_record (GMT, num_report ? out : NULL, (num_report && Ctrl->C.mode != GRDINFO_TRAILING) ? NULL : record);

	nx = gmt_M_get_n (GMT, w, e, Ctrl->I.inc[GMT_X], GMT_GRID_PIXEL_REG);	/* Pass GMT_GRID_PIXEL_REG since the tiles are sort of large "pixels" inside ... */
	ny = gmt_M_get_n (GMT, s, n, Ctrl->I.inc[GMT_Y], GMT_GRID_PIXEL_REG);	/* ... the w/e/s/n bounds regardless of the registration of the grid being given */

	if (G) dx = G->header->inc[GMT_X] * G->header->xy_off, dy = G->header->inc[GMT_Y] * G->header->xy_off;
	/* dx,dy are needed when the grid is pixel-registered as the w/e/s/n bounds are off by 0.5 {dx,dy} relative to node coordinates */

	for (j = 0; j < ny; j++) {
		box[YLO] = wesn[YLO] = s + j * Ctrl->I.inc[GMT_Y];	box[YHI] = wesn[YHI] = wesn[YLO] + Ctrl->I.inc[GMT_Y];
		if (G && Ctrl->D.mode) {	/* Must determine if there is at least one data point inside this subset */
			if (wesn[YLO] < G->header->wesn[YLO]) wesn[YLO] = G->header->wesn[YLO];
			if (wesn[YHI] > G->header->wesn[YHI]) wesn[YHI] = G->header->wesn[YHI];
			js = gmt_M_grd_y_to_row (GMT, wesn[YLO] + dy, G->header);
			jn = gmt_M_grd_y_to_row (GMT, wesn[YHI] - dy, G->header);
		}
		for (i = 0; i < nx; i++) {
			box[XLO] = wesn[XLO] = w + i * Ctrl->I.inc[GMT_X];	box[XHI] = wesn[XHI] = wesn[XLO] + Ctrl->I.inc[GMT_X];
			if (G && Ctrl->D.mode) {	/* Must determine if there is at least one data point inside this subset */
				if (wesn[XLO] < G->header->wesn[XLO]) wesn[XLO] = G->header->wesn[XLO];
				if (wesn[XHI] > G->header->wesn[XHI]) wesn[XHI] = G->header->wesn[XHI];
				iw = gmt_M_grd_x_to_col (GMT, wesn[XLO] + dx, G->header);
				ie = gmt_M_grd_x_to_col (GMT, wesn[XHI] - dx, G->header);
				use = true;
				for (row = jn; row <= js; row++) {
					for (col = iw; col <= ie; col++) {
						node = gmt_M_ijp (G->header, row, col);
						if (!gmt_M_is_fnan (G->data[node]))
							goto L_use_it;
					}
				}
				use = false;	/* Could not find a single valid node */
L_use_it:			row = 0;	/* Get here by goto and use is still true */
			}
			if (use) {
				gmt_M_memcpy (out, box, 4, double);
				out[XLO] -= Ctrl->D.inc[GMT_X];	/* Adjust region for given overlap */
				out[XHI] += Ctrl->D.inc[GMT_X];
				out[YLO] -= Ctrl->D.inc[GMT_Y];
				out[YHI] += Ctrl->D.inc[GMT_Y];
				if (gmt_M_y_is_lat (GMT, GMT_IN)) {	/* Must make sure we don't get outside valid bounds */
					if (out[YLO] < -90.0)
						out[YLO] = -90.0;
					if (out[YHI] > 90.0)
						out[YHI] = 90.0;
				}
				if (gmt_M_x_is_lon (GMT, GMT_IN)) {	/* Must make sure we don't get outside valid bounds */
					if (fabs (out[XHI] - out[XLO]) > 360.0) {
						out[XLO] = (out[XLO] < 0.0) ? -180.0 : 0.0;
						out[XHI] = (out[XHI] < 0.0) ? +180.0 : 360.0;
					}
				}
				if (num_report && Ctrl->C.mode != GRDINFO_TRAILING)	/* Report numerical only */
					GMT_Put_Record (GMT->parent, GMT_WRITE_DATA, Out);
				else {
					sprintf (record, "-R");
					gmt_ascii_format_col (GMT, text, out[XLO], GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, "/");
					gmt_ascii_format_col (GMT, text, out[XHI], GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, "/");
					gmt_ascii_format_col (GMT, text, out[YLO], GMT_OUT, GMT_Y);	strcat (record, text);	strcat (record, "/");
					gmt_ascii_format_col (GMT, text, out[YHI], GMT_OUT, GMT_Y);	strcat (record, text);
					GMT_Put_Record (GMT->parent, GMT_WRITE_DATA, Out);
				}
			}
		}
	}
	gmt_M_free (GMT, Out);
}

GMT_LOCAL void grdinfo_smart_increments (struct GMT_CTRL *GMT, double inc[], unsigned int which, char *text) {
	char tmptxt[GMT_LEN64] = {""};
	if (gmt_M_is_geographic (GMT, GMT_IN) && ((which == 2 && inc[GMT_X] < 1.0 && inc[GMT_Y] < 1.0) || (which != 2 && inc[which] < 1.0))) {	/* See if we can report smart increments */
		unsigned int col, s, k, int_inc[2], use_unit[2], kind;
		bool ok[2] = {false, false};
		double new_inc;
		static int try[10] = {30, 20, 15, 10, 6, 5, 4, 3, 2, 1}, scl[2] = {60, 3600};
		static char *unit[2][2] = {{"m", "s"}, {" min", " sec"}};
		kind = (which == 2) ? 0 : 1;
		/* See if we can use arc units */
		for (col = ((which == 2) ? GMT_X : which); col <= ((which == 2) ? GMT_Y : which); col++) {
			for (s = 0, ok[col] = false; !ok[col] && s < 2; s++) {
				for (k = 0; !ok[col] && k < 10; k++) {
					new_inc = inc[col] * scl[s];	/* Now in minutes or seconds */
					int_inc[col] = try[k] * irint (new_inc / try[k]);	/* Nearest in given multiple */
					if (fabs ((new_inc - int_inc[col])/new_inc) < GMT_CONV4_LIMIT) {	/* Yes, hit bingo */
						ok[col] = true;	use_unit[col] = s;
					}
				}
			}
		}
		if (which != 2)	{	/* Single increment only, no slash situation */
			if (ok[which]) {	/* Single increment and it works */
				gmt_ascii_format_col (GMT, tmptxt, inc[which], GMT_OUT, GMT_Z);
				sprintf (text, "%s (%d%s)", tmptxt, int_inc[which], unit[kind][use_unit[which]]);
			}
			else
				gmt_ascii_format_col (GMT, text, inc[which], GMT_OUT, GMT_Z);
		}
		else if (ok[GMT_X] && ok[GMT_Y]) {	/* Both can use arc min or sec */
			if (int_inc[GMT_X] == int_inc[GMT_Y] && use_unit[GMT_X] == use_unit[GMT_Y])	/* Same increments */
				sprintf (text, "%d%s", int_inc[GMT_X], unit[kind][use_unit[GMT_X]]);
			else	/* Different so must use slash */
				sprintf (text, "%d%s/%d%s", int_inc[GMT_X], unit[kind][use_unit[GMT_X]], int_inc[GMT_Y], unit[kind][use_unit[GMT_Y]]);
		}
		else if (ok[GMT_X]) {	/* Only x-inc can use arc units */
			gmt_ascii_format_col (GMT, tmptxt, inc[GMT_Y], GMT_OUT, GMT_Z);
			sprintf (text, "%d%s/%s", int_inc[GMT_X], unit[kind][use_unit[GMT_X]], tmptxt);
		}
		else if (ok[GMT_Y]) {	/* Only y-inc can use arc units */
			gmt_ascii_format_col (GMT, tmptxt, inc[GMT_X], GMT_OUT, GMT_Z);
			sprintf (text, "%s/%d%s", tmptxt, int_inc[GMT_Y], unit[kind][use_unit[GMT_Y]]);
		}
		else {	/* Neither can use arc units */
			gmt_ascii_format_col (GMT, text, inc[GMT_X], GMT_OUT, GMT_Z);
			gmt_ascii_format_col (GMT, tmptxt, inc[GMT_Y], GMT_OUT, GMT_Z);
			strcat (text, "/");	strcat (text, tmptxt);
		}
	}
	else {	/* Cartesian */
		if (which == 2) {
			if (doubleAlmostEqual (inc[GMT_X], inc[GMT_Y]))
				gmt_ascii_format_col (GMT, text, inc[GMT_X], GMT_OUT, GMT_Z);
			else {
				gmt_ascii_format_col (GMT, text, inc[GMT_X], GMT_OUT, GMT_Z);
				gmt_ascii_format_col (GMT, tmptxt, inc[GMT_Y], GMT_OUT, GMT_Z);
				strcat (text, "/");	strcat (text, tmptxt);
			}
		}
		else
			gmt_ascii_format_col (GMT, text, inc[which], GMT_OUT, GMT_Z);
	}
}

EXTERN_MSC int gmtlib_is_nc_grid (struct GMT_CTRL *GMT, struct GMT_GRID_HEADER *header);

#define bailout(code) {gmt_M_free_options (mode); return (code);}
#define Return(code) {Free_Ctrl (GMT, Ctrl); gmt_end_module (GMT, GMT_cpy); bailout (code);}

EXTERN_MSC int GMT_grdinfo (void *V_API, int mode, void *args) {
	int error = 0, k_data, k_tile_id;
	unsigned int n_grds = 0, n_cols = 0, col, level, i_status, gtype, cmode = GMT_COL_FIX, geometry = GMT_IS_TEXT;
	unsigned int x_col_min, x_col_max, y_col_min, y_col_max, z_col_min, z_col_max, GMT_W = GMT_Z;
	bool subset, delay, num_report, is_cube;

	uint64_t ij, n_nan = 0, n = 0;

	double x_min = 0.0, y_min = 0.0, z_min = 0.0, v_min = 0.0, x_max = 0.0, y_max = 0.0, z_max = 0.0, v_max = 0.0, wesn[6];
	double global_xmin, global_xmax, global_ymin, global_ymax, global_zmin, global_zmax, global_vmin, global_vmax;
	double z_mean = 0.0, z_median = 0.0, z_mode = 0.0, z_stdev = 0.0, z_scale = 0.0, z_lmsscl = 0.0, z_rms = 0.0, out[30];

	char format[GMT_BUFSIZ] = {""}, text[GMT_LEN512] = {""}, record[GMT_BUFSIZ] = {""}, grdfile[PATH_MAX] = {""};
	char *type[2] = { "Gridline", "Pixel"}, *sep = NULL, *projStr = NULL, *answer[2] = {"", " no"};

	struct GRDINFO_CTRL *Ctrl = NULL;
	struct GMT_GRID *G = NULL, *W = NULL;
	struct GMT_GRID_HEADER *header = NULL;
	struct GMT_GRID_HEADER_HIDDEN *HH = NULL;
	struct GMT_CUBE *U = NULL;
	struct GMT_RECORD *Out = NULL;
	struct GMT_OPTION *opt = NULL;
	struct GMT_CTRL *GMT = NULL, *GMT_cpy = NULL;
	struct GMT_OPTION *options = NULL;
	struct GMTAPI_CTRL *API = gmt_get_api_ptr (V_API);	/* Cast from void to GMTAPI_CTRL pointer */

	/*----------------------- Standard module initialization and parsing ----------------------*/

	if (API == NULL) return (GMT_NOT_A_SESSION);
	if (mode == GMT_MODULE_PURPOSE) return (usage (API, GMT_MODULE_PURPOSE));	/* Return the purpose of program */
	options = GMT_Create_Options (API, mode, args);	if (API->error) return (API->error);	/* Set or get option list */

	if ((error = gmt_report_usage (API, options, 0, usage)) != GMT_NOERROR) bailout (error);	/* Give usage if requested */

	/* Parse the command-line arguments */

	if ((GMT = gmt_init_module (API, THIS_MODULE_LIB, THIS_MODULE_CLASSIC_NAME, THIS_MODULE_KEYS, THIS_MODULE_NEEDS, module_kw, &options, &GMT_cpy)) == NULL) bailout (API->error); /* Save current state */
	if (GMT_Parse_Common (API, THIS_MODULE_OPTIONS, options)) Return (API->error);
	Ctrl = New_Ctrl (GMT);	/* Allocate and initialize a new control structure */
	if ((error = parse (GMT, Ctrl, options)) != 0) Return (error);

	/*---------------------------- This is the grdinfo main code ----------------------------*/

	/* OK, done parsing, now process all input grids in a loop */

	is_cube = Ctrl->is_cube;	/* Shorthand */
	gmt_M_memset (wesn, 6, double);	/* Initialize */
	if (is_cube) {
		x_col_min = 14; x_col_max = 17; y_col_min = 15; y_col_max = 18; z_col_min = 16; z_col_max = 19; GMT_W = 3;
	}
	else {	/* grid output */
		x_col_min = 10; x_col_max = 12; y_col_min = 11; y_col_max = 13;
	}
	sep = GMT->current.setting.io_col_separator;
	gmt_M_memcpy (wesn, GMT->common.R.wesn, 6, double);	/* Current -R setting, if any */
	if (Ctrl->D.active && Ctrl->D.mode == 0 && GMT->common.R.active[RSET]) {
		global_xmin = GMT->common.R.wesn[XLO]; global_ymin = GMT->common.R.wesn[YLO];
		global_xmax = GMT->common.R.wesn[XHI] ; global_ymax = GMT->common.R.wesn[YHI];
		global_vmin = DBL_MAX;		global_vmax = -DBL_MAX;
	}
	else {
		global_xmin = global_ymin = global_zmin = global_vmin = DBL_MAX;
		global_xmax = global_ymax = global_zmax = global_vmax = -DBL_MAX;
	}
	delay = (Ctrl->D.mode == 1 || (Ctrl->T.mode & 2));	/* Delay the freeing of the (single) grid we read */
	num_report = (Ctrl->C.active && (Ctrl->D.active || Ctrl->C.mode != GRDINFO_TRADITIONAL));

	if (Ctrl->C.active) {
		/* w e s n {b t} w0 w1 dx dy {dz} n_columns n_rows {n_layers} [x0 y0 {z0} x1 y1 {z1}] [med scale] [mean std rms] [n_nan] */
		n_cols = (is_cube) ? 8 : 6;	/* w e s n {b t} w0 w1 */
		if (!Ctrl->I.active) {
			n_cols += (is_cube) ? 6 : 4;				/* Add dx dy {dz} n_columns n_rows {n_layers} */
			if (Ctrl->M.mode == GRDINFO_FORCE_REPORT) n_cols += (is_cube) ? 7 : 5;	/* Add x0 y0 {z0} x1 y1 {z1} nnan */
			if (Ctrl->L.norm & GRDINFO_MEDIAN) n_cols += 2;	/* Add median scale */
			if (Ctrl->L.norm & GRDINFO_MEAN)   n_cols += 3;	/* Add mean stdev rms */
			if (Ctrl->L.norm & GRDINFO_MODE)   n_cols += 2;	/* Add mode lmsscale */
			n_cols += 2;		/* Add registration and type */
		}
		if (Ctrl->C.mode == GRDINFO_NUMERICAL) cmode = GMT_COL_FIX_NO_TEXT;
		geometry = GMT_IS_NONE;
		if (Ctrl->C.mode == GRDINFO_TRADITIONAL) geometry = GMT_IS_TEXT;
		if (geometry == GMT_IS_TEXT) n_cols = 0;	/* A single string, unfortunately */
		if (Ctrl->D.active) {
			n_cols = 4;	/* Just reporting w e s n per tile */
			if (Ctrl->C.mode != GRDINFO_TRAILING) cmode = GMT_COL_FIX_NO_TEXT;
		}
	}
	else if (Ctrl->I.status == GRDINFO_GIVE_BOUNDBOX) {
		n_cols = 2;
		cmode = GMT_COL_FIX_NO_TEXT;
		geometry = GMT_IS_POLY;
		if (Ctrl->n_files > 1) gmt_set_segmentheader (GMT, GMT_OUT, true);
	}
	else if (Ctrl->E.active) {
		n_cols = 3;
		cmode = GMT_COL_FIX_NO_TEXT;
		geometry = GMT_IS_POINT;
	}
	GMT_Report (API, GMT_MSG_DEBUG, "Will write output record(s) with %d leading numerical columns and with%s trailing text\n", n_cols, answer[cmode>0]);

	GMT_Set_Columns (GMT->parent, GMT_OUT, n_cols, cmode);

	if (GMT_Init_IO (API, GMT_IS_DATASET, geometry, GMT_OUT, GMT_ADD_DEFAULT, 0, options) != GMT_NOERROR) {	/* Registers default output destination, unless already set */
		Return (API->error);
	}
	if (GMT_Begin_IO (API, GMT_IS_DATASET, GMT_OUT, GMT_HEADER_OFF) != GMT_NOERROR) {	/* Enables data output and sets access mode */
		Return (API->error);
	}

	Out = gmt_new_record (GMT, (n_cols) ? out : NULL, (cmode) == GMT_COL_FIX ? record : NULL);	/* the two items */

	for (opt = options; opt; opt = opt->next) {	/* Loop over arguments, skip options */

		if (opt->option != '<') continue;	/* We are only processing filenames here */

		gmt_set_cartesian (GMT, GMT_IN);	/* Reset since we may get a bunch of files, some geo, some not */
		k_tile_id = k_data = GMT_NOTSET;
		if ((k_data = gmt_remote_dataset_id (API, opt->arg)) != GMT_NOTSET || (k_tile_id = gmt_get_tile_id (API, opt->arg)) != GMT_NOTSET) {
			if (k_tile_id != GMT_NOTSET && !Ctrl->G.active) {
				GMT_Report (API, GMT_MSG_WARNING, "Information on tiled remote global grids requires -G since download or all tiles may be required\n");
				continue;
			}
			gmt_set_geographic (GMT, GMT_IN);	/* Since this will be returned as a memory grid */
		}

		if (is_cube) {
			if ((U = GMT_Read_Data (API, GMT_IS_CUBE, GMT_IS_FILE, GMT_IS_VOLUME, GMT_CONTAINER_ONLY, NULL, opt->arg, NULL)) == NULL) {
				Return (API->error);
			}
			header = U->header;
		}
		else {
			if ((G = GMT_Read_Data (API, GMT_IS_GRID, GMT_IS_FILE, GMT_IS_SURFACE, GMT_CONTAINER_ONLY, NULL, opt->arg, NULL)) == NULL) {
				Return (API->error);
			}
			header = G->header;
		}
		subset = gmt_M_is_subset (GMT, header, wesn);	/* Subset requested */
		if (subset && ((error = gmt_M_err_fail (GMT, gmt_adjust_loose_wesn (GMT, wesn, header), ""))))	/* Make sure wesn matches header spacing */
			Return (error);
		HH = gmt_get_H_hidden (header);
		GMT_Report (API, GMT_MSG_INFORMATION, "Processing grid %s\n", HH->name);

		if (header->ProjRefPROJ4 && !Ctrl->C.active && !Ctrl->T.active && !Ctrl->I.active)
			projStr = strdup (header->ProjRefPROJ4);		/* Copy proj string to print at the end */
		else if (header->ProjRefWKT && !Ctrl->C.active && !Ctrl->T.active && !Ctrl->I.active)
			projStr = strdup (header->ProjRefWKT);

		for (n = 0; n < GMT_Z + is_cube; n++)
			GMT->current.io.col_type[GMT_OUT][n] = gmt_M_type (GMT, GMT_IN, n);	/* Since grids may differ in types */

		if (num_report && n_grds == 0) {	/* Need to prepare col_type for output */
			if (Ctrl->M.mode == GRDINFO_FORCE_REPORT) {	/* Will write x,y[,z] min and max locations */
				GMT->current.io.col_type[GMT_OUT][x_col_min] = GMT->current.io.col_type[GMT_OUT][x_col_max] = GMT->current.io.col_type[GMT_OUT][GMT_X];
				GMT->current.io.col_type[GMT_OUT][y_col_min] = GMT->current.io.col_type[GMT_OUT][y_col_max] = GMT->current.io.col_type[GMT_OUT][GMT_Y];
				if (is_cube) GMT->current.io.col_type[GMT_OUT][z_col_min] = GMT->current.io.col_type[GMT_OUT][z_col_max] = GMT->current.io.col_type[GMT_OUT][GMT_Z];
			}
			if (is_cube) GMT->current.io.col_type[GMT_OUT][4] = GMT->current.io.col_type[GMT_OUT][5] = GMT->current.io.col_type[GMT_OUT][GMT_Z];
			GMT->current.io.col_type[GMT_OUT][2] = GMT->current.io.col_type[GMT_OUT][3] = GMT->current.io.col_type[GMT_OUT][GMT_Y];
			GMT->current.io.col_type[GMT_OUT][1] = GMT->current.io.col_type[GMT_OUT][GMT_X];
		}
		n_grds++;

		if (Ctrl->M.mode == GRDINFO_CONDITIONAL) {	/* Conditionally read and find min/max if not in header */
			if (doubleAlmostEqualZero (header->z_min, header->z_max)) {	/* Force search */
				GMT_Report (API, GMT_MSG_WARNING, "No actual range for data in header - must read matrix and determine range\n");
				Ctrl->M.mode = GRDINFO_FORCE;
			}
			else	/* else the header has the data range; no need to read file */
				GMT_Report (API, GMT_MSG_DEBUG, "Found valid actual data range in header - no need to read matrix\n");
		}

		if (Ctrl->E.active || Ctrl->M.mode || Ctrl->L.active || subset || Ctrl->D.mode || (Ctrl->T.mode & 2)) {	/* Need to read the data (all or subset) */
			if (is_cube) {
				if (GMT_Read_Data (API, GMT_IS_CUBE, GMT_IS_FILE, GMT_IS_VOLUME, GMT_DATA_ONLY, wesn, opt->arg, U) == NULL) {
					Return (API->error);
				}
			}
			else {
				if (GMT_Read_Data (API, GMT_IS_GRID, GMT_IS_FILE, GMT_IS_SURFACE, GMT_DATA_ONLY, wesn, opt->arg, G) == NULL) {
					Return (API->error);
				}
			}
		}

		if (Ctrl->E.active) {
			openmp_int row, col;
			unsigned int r, c;
			float z, z0 = -FLT_MAX * Ctrl->E.val;
			double *x = NULL, *y = NULL;
			x = gmt_grd_coord (GMT, header, GMT_X);
			y = gmt_grd_coord (GMT, header, GMT_Y);

			if (Ctrl->E.mode == GMT_Y) {
				gmt_M_row_loop (GMT, G, row) {	/* Along each row; y is the monotonically increasing coordinate */
					out[GMT_Y] = y[row];
					z = -FLT_MAX * Ctrl->E.val;
					gmt_M_col_loop (GMT, G, row, col, ij) {	/* Loop across this row */
						if (gmt_M_is_fnan (G->data[ij])) continue;
						if ((Ctrl->E.type == -1 && G->data[ij] > 0.0) || (Ctrl->E.type == +1 && G->data[ij] < 0.0)) continue;
						if ((Ctrl->E.val == -1 && G->data[ij] < z) || (Ctrl->E.val == +1 && G->data[ij] > z)) {
							z = G->data[ij];
							c = col;
						}
					}
					out[GMT_X] = x[c];
					out[GMT_Z] = z;
					if (z != z0) GMT_Put_Record (API, GMT_WRITE_DATA, Out);		/* Write this to output */
				}
			}
			else {	/* Along each column; x is the monotonically increasing coordinate */
				gmt_M_col_loop2 (GMT, G, col) {	/* For all possible columns */
					out[GMT_X] = x[col];
					z = -FLT_MAX * Ctrl->E.val;
					gmt_M_row_loop (GMT, G, row) {	/* Loop over all rows */
						ij = gmt_M_ijp (header, row, col);
						if (gmt_M_is_fnan (G->data[ij])) continue;
						if ((Ctrl->E.type == -1 && G->data[ij] > 0.0) || (Ctrl->E.type == +1 && G->data[ij] < 0.0)) continue;
						if ((Ctrl->E.val == -1 && G->data[ij] < z) || (Ctrl->E.val == +1 && G->data[ij] > z)) {
							z = G->data[ij];
							r = row;
						}
					}
					out[GMT_Y] = y[r];
					out[GMT_Z] = z;
					if (z != z0) GMT_Put_Record (API, GMT_WRITE_DATA, Out);		/* Write this to output */
				}
			}
			gmt_M_free (GMT, x);
			gmt_M_free (GMT, y);
			continue;	/* Back up, and since this is the only grid we will soon exit the loop over options */
		}

		if (Ctrl->T.mode & 2) strncpy (grdfile, opt->arg, PATH_MAX-1);

		if (Ctrl->M.mode == GRDINFO_FORCE_REPORT || Ctrl->M.mode == GRDINFO_FORCE || Ctrl->L.active) {	/* Must determine the [location of] global min and max values */
			uint64_t ij_min, ij_max, here = 0, node;
			unsigned int col, row;
			gmt_grdfloat *data = (is_cube) ? U->data : G->data;

			v_min = DBL_MAX;	v_max = -DBL_MAX;
			ij_min = ij_max = n = 0;
			for (level = 0; level < header->n_bands; level++) {
				for (row = 0; row < header->n_rows; row++) {
					for (col = 0, ij = gmt_M_ijp (header, row, 0); col < header->n_columns; col++, ij++) {
						node = ij + here;
						if (gmt_M_is_fnan (data[node])) continue;
						if (data[node] < v_min) {
							v_min = data[node];	ij_min = ij; if (is_cube) z_min = U->z[level];
						}
						if (data[node] > v_max) {
							v_max = data[node];	ij_max = ij; if (is_cube) z_max = U->z[level];
						}
						n++;
					}
				}
				here += header->size;
			}

			n_nan = header->n_bands * header->nm - n;
			if (n) {	/* Meaning at least one non-NaN node was found */
				col = (unsigned int)gmt_M_col (header, ij_min);
				row = (unsigned int)gmt_M_row (header, ij_min);
				x_min = gmt_M_grd_col_to_x (GMT, col, header);
				y_min = gmt_M_grd_row_to_y (GMT, row, header);
				col = (unsigned int)gmt_M_col (header, ij_max);
				row = (unsigned int)gmt_M_row (header, ij_max);
				x_max = gmt_M_grd_col_to_x (GMT, col, header);
				y_max = gmt_M_grd_row_to_y (GMT, row, header);
			}
			else	/* Not a single valid node */
				x_min = x_max = y_min = y_max = z_min = z_max = GMT->session.d_NaN;
			if (Ctrl->M.mode == GRDINFO_FORCE || Ctrl->L.active) {	/* Update header */
				header->z_min = v_min;
				header->z_max = v_max;
			}
		}

		if (Ctrl->L.norm && gmt_M_is_geographic (GMT, GMT_IN)) {	/* Must use spherical weights */
			W = gmt_duplicate_grid (GMT, G, GMT_DUPLICATE_ALLOC);
			gmt_get_cellarea (GMT, W);
		}

		if (Ctrl->L.norm & GRDINFO_MEDIAN) {	/* Calculate the median and MAD */
			z_median = gmt_grd_median (GMT, G, W, false);
			z_scale = gmt_grd_mad (GMT, G, W, &z_median, false);
		}
		if (Ctrl->L.norm & GRDINFO_MEAN) {	/* Calculate the mean, standard deviation, and rms */
			z_mean = gmt_grd_mean (GMT, G, W);	/* Compute the [weighted] mean */
			z_stdev = gmt_grd_std (GMT, G, W);	/* Compute the [weighted] stdev */
			z_rms = gmt_grd_rms (GMT, G, W);		/* Compute the [weighted] rms */
		}
		if (Ctrl->L.norm & GRDINFO_MODE) {	/* Calculate the mode and lmsscale */
			z_mode = gmt_grd_mode (GMT, G, W, false);
			z_lmsscl = gmt_grd_lmsscl (GMT, G, W, &z_mode, false);
		}
		if (W) gmt_free_grid (GMT, &W, true);

		if (gmt_M_x_is_lon (GMT, GMT_IN)) {
			if (gmt_grd_is_global(GMT, header) || (header->wesn[XLO] < 0.0 && header->wesn[XHI] <= 0.0))
				GMT->current.io.geo.range = GMT_IS_GIVEN_RANGE;
			else if ((header->wesn[XHI] - header->wesn[XLO]) > 180.0)
				GMT->current.io.geo.range = GMT_IS_GIVEN_RANGE;
			else if (header->wesn[XLO] < 0.0 && header->wesn[XHI] >= 0.0)
				GMT->current.io.geo.range = GMT_IS_M180_TO_P180_RANGE;
			else
				GMT->current.io.geo.range = GMT_IS_0_TO_P360_RANGE;
			gtype = 1;
		}
		else
			gtype = 0;

		/* OK, time to report results */

		i_status = Ctrl->I.status;
		if (Ctrl->I.active && Ctrl->I.status == GRDINFO_GIVE_REG_IMG) {
			if (!(strstr (header->command, "img2grd") || strstr (header->command, "img2mercgrd") ||
			      strstr (header->remark, "img2mercgrd"))) {
				GMT_Report (API, GMT_MSG_ERROR,
				            "Could not find a -Rw/e/s/n string produced by img tools - returning regular grid -R\n");
				i_status = GRDINFO_GIVE_REG_ORIG;
			}
		}

		if (Ctrl->I.active && i_status == GRDINFO_GIVE_REG_ORIG) {
			sprintf (record, "-R");
			gmt_ascii_format_col (GMT, text, header->wesn[XLO], GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, "/");
			gmt_ascii_format_col (GMT, text, header->wesn[XHI], GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, "/");
			gmt_ascii_format_col (GMT, text, header->wesn[YLO], GMT_OUT, GMT_Y);	strcat (record, text);	strcat (record, "/");
			gmt_ascii_format_col (GMT, text, header->wesn[YHI], GMT_OUT, GMT_Y);	strcat (record, text);
			if (is_cube) {
				strcat (record, "/");
				gmt_ascii_format_col (GMT, text, U->z_range[0], GMT_OUT, GMT_Z);	strcat (record, text);	strcat (record, "/");
				gmt_ascii_format_col (GMT, text, U->z_range[1], GMT_OUT, GMT_Z);	strcat (record, text);
			}
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
		}
		else if (Ctrl->I.active && i_status == GRDINFO_GIVE_REG_OBLIQUE) {
			sprintf (record, "-R");
			gmt_ascii_format_col (GMT, text, header->wesn[XLO], GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, "/");
			gmt_ascii_format_col (GMT, text, header->wesn[YLO], GMT_OUT, GMT_Y);	strcat (record, text);	strcat (record, "/");
			gmt_ascii_format_col (GMT, text, header->wesn[XHI], GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, "/");
			gmt_ascii_format_col (GMT, text, header->wesn[YHI], GMT_OUT, GMT_Y);	strcat (record, text);
			if (is_cube) {
				strcat (record, "/");
				gmt_ascii_format_col (GMT, text, U->z_range[0], GMT_OUT, GMT_Z);	strcat (record, text);	strcat (record, "/");
				gmt_ascii_format_col (GMT, text, U->z_range[1], GMT_OUT, GMT_Z);	strcat (record, text);
			}
			strcat (record, "+r");
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
		}
		else if (Ctrl->I.active && i_status == GRDINFO_GIVE_REG_IMG) {
			char *c = strrchr (header->remark, 'R');
			sprintf (record, "-%s", c);
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
		}
		else if (Ctrl->I.active && i_status == GRDINFO_GIVE_INCREMENTS) {
			sprintf (record, "-I");
			grdinfo_smart_increments (GMT, header->inc, 2, text);	strcat (record, text);
			if (is_cube) {
				strcat (record, "/");
				gmt_ascii_format_col (GMT, text, U->z_inc, GMT_OUT, GMT_Z);	strcat (record, text);
			}
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
		}
		else if (Ctrl->I.active && i_status == GRDINFO_GIVE_BOUNDBOX) {
			double *xx = NULL, *yy = NULL;
			unsigned int k, np = gmt_grid_perimeter (GMT, header, &xx, &yy);

			sprintf (record, "Bounding box for %s", HH->name);
			GMT_Put_Record (API, GMT_WRITE_SEGMENT_HEADER, record);
			/* Loop around perimeter building a closed polygon */
			for (k = 0; k < np; k++) {	/* perimeter goes CCW */
				out[GMT_X] = xx[k];	out[GMT_Y] = yy[k];
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
			gmt_M_free (GMT, xx);	gmt_M_free (GMT, yy);
		}
		else if (Ctrl->C.active && !Ctrl->I.active) {
			if (num_report) {	/* External interface, return as data with no leading text {...} is for 3-D cubes only */
				/* w e s n {b t} w0 w1 dx dy {dz} n_columns n_rows {n_layers} [x0 y0 {z0} x1 y1 {z1}] [med scale] [mean std rms] [n_nan] */
				gmt_M_memcpy (out, header->wesn, 4, double);	/* Place the w/e/s/n limits */
				col = 4;
				if (is_cube) {out[ZLO] = U->z_range[0];		out[ZHI] = U->z_range[1]; col += 2; }
				out[col++] = header->z_min;			out[col++] = header->z_max;
				out[col++] = header->inc[GMT_X];	out[col++] = header->inc[GMT_Y];	if (is_cube) out[col++] = U->z_inc;
				out[col++] = header->n_columns;		out[col++] = header->n_rows;		if (is_cube) out[col++] = header->n_bands;
				if (Ctrl->M.mode == GRDINFO_FORCE_REPORT) {
					out[col++] = x_min;	out[col++] = y_min;	if (is_cube) out[col++] = z_min;
					out[col++] = x_max;	out[col++] = y_max;	if (is_cube) out[col++] = z_max;
				}
				if (Ctrl->L.norm & GRDINFO_MEDIAN) {
					out[col++] = z_median;	out[col++] = z_scale;
				}
				if (Ctrl->L.norm & GRDINFO_MEAN) {
					out[col++] = z_mean;	out[col++] = z_stdev;	out[col++] = z_rms;
				}
				if (Ctrl->M.mode == GRDINFO_FORCE_REPORT) {
					out[col++] = (double)n_nan;
				}
				if (Ctrl->L.norm & GRDINFO_MODE) {
					out[col++] = z_mode;	out[col++] = z_lmsscl;
				}
				out[col++] = header->registration;
				out[col++] = gtype;	/* 0 = Cart, 1 = geo */
				if (Ctrl->C.mode == GRDINFO_TRAILING)
					sprintf (record, "%s", HH->name);
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
			else {	/* Command-line usage */
				record[0] = '\0';
				if (Ctrl->C.mode != GRDINFO_TRAILING)
					sprintf (record, "%s%s", HH->name, sep);
				gmt_ascii_format_col (GMT, text, header->wesn[XLO], GMT_OUT, GMT_X);
				strcat (record, text);	strcat (record, sep);
				gmt_ascii_format_col (GMT, text, header->wesn[XHI], GMT_OUT, GMT_X);
				strcat (record, text);	strcat (record, sep);
				gmt_ascii_format_col (GMT, text, header->wesn[YLO], GMT_OUT, GMT_Y);
				strcat (record, text);	strcat (record, sep);
				gmt_ascii_format_col (GMT, text, header->wesn[YHI], GMT_OUT, GMT_Y);
				strcat (record, text);	strcat (record, sep);
				if (is_cube) {
					gmt_ascii_format_col (GMT, text, U->z_range[0], GMT_OUT, GMT_Z);
					strcat (record, text);	strcat (record, sep);
					gmt_ascii_format_col (GMT, text, U->z_range[1], GMT_OUT, GMT_Z);
					strcat (record, text);	strcat (record, sep);
				}
				gmt_ascii_format_col (GMT, text, header->z_min, GMT_OUT, GMT_W);
				strcat (record, text);	strcat (record, sep);
				gmt_ascii_format_col (GMT, text, header->z_max, GMT_OUT, GMT_W);
				strcat (record, text);	strcat (record, sep);
				gmt_ascii_format_col (GMT, text, header->inc[GMT_X], GMT_OUT, GMT_X);
				if (isalpha ((int)text[strlen(text)-1])) text[strlen(text)-1] = '\0';	/* Chop of trailing WE flag here */
				strcat (record, text);	strcat (record, sep);
				gmt_ascii_format_col (GMT, text, header->inc[GMT_Y], GMT_OUT, GMT_Y);
				if (isalpha ((int)text[strlen(text)-1])) text[strlen(text)-1] = '\0';	/* Chop of trailing SN flag here */
				strcat (record, text);	strcat (record, sep);
				if (is_cube) {
					gmt_ascii_format_col (GMT, text, U->z_inc, GMT_OUT, GMT_Z);
					strcat (record, text);	strcat (record, sep);
				}
				gmt_ascii_format_col (GMT, text, (double)header->n_columns, GMT_OUT, GMT_W);
				strcat (record, text);	strcat (record, sep);
				gmt_ascii_format_col (GMT, text, (double)header->n_rows, GMT_OUT, GMT_W);
				strcat (record, text);
				if (is_cube) {
					gmt_ascii_format_col (GMT, text, (double)header->n_bands, GMT_OUT, GMT_W);
					strcat (record, text);	strcat (record, sep);
				}
				if (Ctrl->M.mode == GRDINFO_FORCE_REPORT) {
					strcat (record, sep);	gmt_ascii_format_col (GMT, text, x_min, GMT_OUT, GMT_X);	strcat (record, text);
					strcat (record, sep);	gmt_ascii_format_col (GMT, text, y_min, GMT_OUT, GMT_Y);	strcat (record, text);
					if (is_cube) { strcat (record, sep);	gmt_ascii_format_col (GMT, text, z_min, GMT_OUT, GMT_Z);	strcat (record, text); }
					strcat (record, sep);	gmt_ascii_format_col (GMT, text, x_max, GMT_OUT, GMT_X);	strcat (record, text);
					strcat (record, sep);	gmt_ascii_format_col (GMT, text, y_max, GMT_OUT, GMT_Y);	strcat (record, text);
					if (is_cube) { strcat (record, sep);	gmt_ascii_format_col (GMT, text, z_max, GMT_OUT, GMT_Z);	strcat (record, text); }
				}
				if (Ctrl->L.norm & GRDINFO_MEDIAN) {
					strcat (record, sep);	gmt_ascii_format_col (GMT, text, z_median, GMT_OUT, GMT_Z);	strcat (record, text);
					strcat (record, sep);	gmt_ascii_format_col (GMT, text,  z_scale, GMT_OUT, GMT_Z);	strcat (record, text);
				}
				if (Ctrl->L.norm & GRDINFO_MEAN) {
					strcat (record, sep);	gmt_ascii_format_col (GMT, text, z_mean, GMT_OUT, GMT_Z);	strcat (record, text);
					strcat (record, sep);	gmt_ascii_format_col (GMT, text, z_stdev, GMT_OUT, GMT_Z);	strcat (record, text);
					strcat (record, sep);	gmt_ascii_format_col (GMT, text,   z_rms, GMT_OUT, GMT_Z);	strcat (record, text);
				}
				if (Ctrl->M.mode == GRDINFO_FORCE_REPORT) { sprintf (text, "%s%" PRIu64, sep, n_nan);	strcat (record, text); }
				if (Ctrl->L.norm & GRDINFO_MODE) {
					strcat (record, sep);	gmt_ascii_format_col (GMT, text, z_mode,   GMT_OUT, GMT_Z);	strcat (record, text);
					strcat (record, sep);	gmt_ascii_format_col (GMT, text, z_lmsscl, GMT_OUT, GMT_Z);	strcat (record, text);
				}
				strcat (record, sep);	gmt_ascii_format_col (GMT, text, (double)header->registration, GMT_OUT, GMT_Z);	strcat (record, text);
				strcat (record, sep);	gmt_ascii_format_col (GMT, text, (double)gtype, GMT_OUT, GMT_Z);	strcat (record, text);
				if (Ctrl->C.mode == GRDINFO_TRAILING) { sprintf (text, "%s%s", sep, HH->name);	strcat (record, text); }
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
		}
		else if (!(Ctrl->T.active || (Ctrl->I.active && Ctrl->I.status == GRDINFO_GIVE_REG_ROUNDED))) {
			char *gtype[2] = {"Cartesian grid", "Geographic grid"};
			sprintf (record, "%s: Title: %s", HH->name, gmt_get_grd_title (header));		GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			sprintf (record, "%s: Command: %s", HH->name, gmt_get_grd_command (header));	GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			sprintf (record, "%s: Remark: %s", HH->name, gmt_get_grd_remark (header));		GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			if (header->registration == GMT_GRID_NODE_REG || header->registration == GMT_GRID_PIXEL_REG)
				sprintf (record, "%s: %s node registration used [%s]", HH->name, type[header->registration], gtype[gmt_M_is_geographic (GMT, GMT_IN)]);
			else
				sprintf (record, "%s: Unknown registration! Probably not a GMT grid", HH->name);
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			if (header->type != k_grd_unknown_fmt)
				sprintf (record, "%s: Grid file format: %s", HH->name, GMT->session.grdformat[header->type]);
			else
				sprintf (record, "%s: Unrecognized grid file format! Probably not a GMT grid", HH->name);
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			if (Ctrl->F.active) {
				if ((fabs (header->wesn[XLO]) < 500.0) && (fabs (header->wesn[XHI]) < 500.0) &&
				    (fabs (header->wesn[YLO]) < 500.0) && (fabs (header->wesn[YHI]) < 500.0)) {
					sprintf (record, "%s: x_min: %.7f", HH->name, header->wesn[XLO]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: x_max: %.7f", HH->name, header->wesn[XHI]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: x_inc: %.7f", HH->name, header->inc[GMT_X]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: name: %s",    HH->name, header->x_units);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: n_columns: %d", HH->name, header->n_columns);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: y_min: %.7f", HH->name, header->wesn[YLO]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: y_max: %.7f", HH->name, header->wesn[YHI]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: y_inc: %.7f", HH->name, header->inc[GMT_Y]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: name: %s",    HH->name, header->y_units);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: n_rows: %d",  HH->name, header->n_rows);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
				}
				else {
					sprintf (record, "%s: x_min: %.2f", HH->name, header->wesn[XLO]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: x_max: %.2f", HH->name, header->wesn[XHI]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: x_inc: %.2f", HH->name, header->inc[GMT_X]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: name: %s",    HH->name, header->x_units);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: n_columns: %d",  HH->name, header->n_columns);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: y_min: %.2f", HH->name, header->wesn[YLO]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: y_max: %.2f", HH->name, header->wesn[YHI]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: y_inc: %.2f", HH->name, header->inc[GMT_Y]);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: name: %s",    HH->name, header->y_units);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					sprintf (record, "%s: n_rows: %d",  HH->name, header->n_rows);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
				}
			}
			else {
				char *c = NULL;
				sprintf (record, "%s: x_min: ", HH->name);
				gmt_ascii_format_col (GMT, text, header->wesn[XLO], GMT_OUT, GMT_X);	strcat (record, text);
				strcat (record, " x_max: ");
				gmt_ascii_format_col (GMT, text, header->wesn[XHI], GMT_OUT, GMT_X);	strcat (record, text);
				strcat (record, " x_inc: ");	grdinfo_smart_increments (GMT, header->inc, GMT_X, text);	strcat (record, text);
				strcat (record, " name: ");
				if ((c = strstr (header->x_units, " [degrees"))) {	/* Strip off [degrees ...] from the name of the longitude variable */
					c[0] = '\0'; strcat (record, header->x_units); if (c) c[0] = ' ';
				}
				else	/* Just output what it says */
					strcat (record, header->x_units);
				sprintf (text, " n_columns: %d", header->n_columns);	strcat (record, text);
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
				sprintf (record, "%s: y_min: ", HH->name);
				gmt_ascii_format_col (GMT, text, header->wesn[YLO], GMT_OUT, GMT_Y);	strcat (record, text);
				strcat (record, " y_max: ");
				gmt_ascii_format_col (GMT, text, header->wesn[YHI], GMT_OUT, GMT_Y);	strcat (record, text);
				strcat (record, " y_inc: ");	grdinfo_smart_increments (GMT, header->inc, GMT_Y, text);	strcat (record, text);
				strcat (record, " name: ");
				if ((c = strstr (header->y_units, " [degrees"))) {	/* Strip off [degrees ...] from the name of the latitude variable */
					c[0] = '\0'; strcat (record, header->y_units); if (c) c[0] = ' ';
				}
				else	/* Just output what it says */
					strcat (record, header->y_units);
				sprintf (text, " n_rows: %d", header->n_rows);	strcat (record, text);
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
				if (is_cube) {
					sprintf (record, "%s: z_min: ", HH->name);
					gmt_ascii_format_col (GMT, text, U->z_range[0], GMT_OUT, GMT_Z);	strcat (record, text);
					strcat (record, " z_max: ");
					gmt_ascii_format_col (GMT, text, U->z_range[1], GMT_OUT, GMT_Z);	strcat (record, text);
					strcat (record, " z_inc: ");
					if (gmt_M_is_zero (U->z_inc)) strcat (record, "(variable)");
					else {gmt_ascii_format_col (GMT, text, U->z_inc, GMT_OUT, GMT_Z);	strcat (record, text); }
					strcat (record, " name: ");		strcat (record, U->units);
					sprintf (text, " n_levels: %d", header->n_bands);	strcat (record, text);
					GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					if (gmt_M_is_zero (U->z_inc)) {
						unsigned int k;
						sprintf (record, "%s: z_levels: ", HH->name);
						for (k = 0; k < header->n_bands; k++) {
							if (k) strcat (record, ", ");
							gmt_ascii_format_col (GMT, text, U->z[k], GMT_OUT, GMT_Z);
							strcat (record, text);
						}
						GMT_Put_Record (API, GMT_WRITE_DATA, Out);
					}
				}
			}

			if (Ctrl->M.mode == GRDINFO_FORCE_REPORT) {
				if (v_min == DBL_MAX)  v_min = GMT->session.d_NaN;
				if (v_max == -DBL_MAX) v_max = GMT->session.d_NaN;
				sprintf (record, "%s: v_min: ", HH->name);
				gmt_ascii_format_col (GMT, text, v_min, GMT_OUT, GMT_Z);	strcat (record, text);	strcat (record, " at x = ");
				gmt_ascii_format_col (GMT, text, x_min, GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, " y = ");
				gmt_ascii_format_col (GMT, text, y_min, GMT_OUT, GMT_Y);	strcat (record, text);
				if (is_cube) { strcat (record, " z = "); gmt_ascii_format_col (GMT, text, z_min, GMT_OUT, GMT_Z);	strcat (record, text);}
				strcat (record, " v_max: ");
				gmt_ascii_format_col (GMT, text, v_max, GMT_OUT, GMT_Z);	strcat (record, text);	strcat (record, " at x = ");
				gmt_ascii_format_col (GMT, text, x_max, GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, " y = ");
				gmt_ascii_format_col (GMT, text, y_max, GMT_OUT, GMT_Y);	strcat (record, text);
				if (is_cube) { strcat (record, " z = "); gmt_ascii_format_col (GMT, text, z_max, GMT_OUT, GMT_Z);	strcat (record, text);}
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
			else if (Ctrl->F.active) {
				sprintf (record, "%s: zmin: %g", HH->name, header->z_min);	GMT_Put_Record (API, GMT_WRITE_DATA, Out);
				sprintf (record, "%s: zmax: %g", HH->name, header->z_max);	GMT_Put_Record (API, GMT_WRITE_DATA, Out);
				sprintf (record, "%s: name: %s", HH->name, header->z_units);	GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
			else {
				sprintf (record, "%s: v_min: ", HH->name);
				gmt_ascii_format_col (GMT, text, header->z_min, GMT_OUT, GMT_W);	strcat (record, text);
				strcat (record, " v_max: ");
				gmt_ascii_format_col (GMT, text, header->z_max, GMT_OUT, GMT_W);	strcat (record, text);
				strcat (record, " name: ");	strcat (record, header->z_units);
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}

			/* print scale and offset */
			sprintf (format, "%s: scale_factor: %s add_offset: %s",
			         HH->name, GMT->current.setting.format_float_out, GMT->current.setting.format_float_out);
			sprintf (record, format, header->z_scale_factor, header->z_add_offset);
			if (header->z_scale_factor != 1.0 || header->z_add_offset != 0) {
				/* print packed z-range */
				sprintf (format, "%s packed z-range: [%s,%s]", record,
				         GMT->current.setting.format_float_out, GMT->current.setting.format_float_out);
				sprintf (record, format,
				         (header->z_min - header->z_add_offset) / header->z_scale_factor,
				         (header->z_max - header->z_add_offset) / header->z_scale_factor);
			}
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			if (n_nan || Ctrl->M.mode == GRDINFO_FORCE_REPORT) {
				double percent = 100.0 * n_nan / header->nm;
				sprintf (record, "%s: %" PRIu64 " nodes (%.1f%%) set to NaN", HH->name, n_nan, percent);
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
			if (Ctrl->L.norm & GRDINFO_MEDIAN) {
				sprintf (record, "%s: median: ", HH->name);
				gmt_ascii_format_col (GMT, text, z_median, GMT_OUT, GMT_Z);	strcat (record, text);
				strcat (record, " scale: ");
				gmt_ascii_format_col (GMT, text, z_scale, GMT_OUT, GMT_Z);	strcat (record, text);
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
			if (Ctrl->L.norm & GRDINFO_MEAN) {
				sprintf (record, "%s: mean: ", HH->name);
				gmt_ascii_format_col (GMT, text,  z_mean, GMT_OUT, GMT_Z);	strcat (record, text);
				strcat (record, " stdev: ");
				gmt_ascii_format_col (GMT, text, z_stdev, GMT_OUT, GMT_Z);	strcat (record, text);
				strcat (record, " rms: ");
				gmt_ascii_format_col (GMT, text,   z_rms, GMT_OUT, GMT_Z);	strcat (record, text);
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
			if (Ctrl->L.norm & GRDINFO_MODE) {
				sprintf (record, "%s: mode: ", HH->name);
				gmt_ascii_format_col (GMT, text,   z_mode, GMT_OUT, GMT_Z);	strcat (record, text);
				strcat (record, " lmsscale: ");
				gmt_ascii_format_col (GMT, text, z_lmsscl, GMT_OUT, GMT_Z);	strcat (record, text);
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
			if (strspn(GMT->session.grdformat[header->type], "nc") != 0) {
				/* type is netCDF: report chunk size and deflation level */
				if (HH->is_netcdf4) {
					sprintf (text, " chunk_size: %" PRIuS ",%" PRIuS " shuffle: %s deflation_level: %u",
					         HH->z_chunksize[0], HH->z_chunksize[1],
					         HH->z_shuffle ? "on" : "off", HH->z_deflate_level);
				}
				else
					text[0] = '\0';
				sprintf (record, "%s: format: %s%s",
						HH->name, HH->is_netcdf4 ? "netCDF-4" : "classic", text);
				GMT_Put_Record (API, GMT_WRITE_DATA, Out);
			}
			/* Print CPT status */
			sprintf (record, "%s: Default CPT: ", HH->name);	if (HH->cpt) strcat (record, HH->cpt);	GMT_Put_Record (API, GMT_WRITE_DATA, Out);
		} /* !(Ctrl->T.active || (Ctrl->I.active && Ctrl->I.status == GRDINFO_GIVE_REG_ROUNDED))) */
		else {
			if (header->z_min < global_vmin) global_vmin = header->z_min;
			if (header->z_max > global_vmax) global_vmax = header->z_max;
			if (header->wesn[XLO] < global_xmin) global_xmin = header->wesn[XLO];
			if (header->wesn[XHI] > global_xmax) global_xmax = header->wesn[XHI];
			if (header->wesn[YLO] < global_ymin) global_ymin = header->wesn[YLO];
			if (header->wesn[YHI] > global_ymax) global_ymax = header->wesn[YHI];
			if (is_cube) {
				if (U->z_range[0] < global_zmin) global_zmin = U->z_range[0];
				if (U->z_range[1] > global_zmax) global_zmax = U->z_range[1];
			}
		}
		if (!delay) {
			if (is_cube && GMT_Destroy_Data (API, &U) != GMT_NOERROR) {
				Return (API->error);
			}
			else if (!is_cube && GMT_Destroy_Data (API, &G) != GMT_NOERROR){
				Return (API->error);
			}
		}
	}

	if (global_vmin == DBL_MAX)  global_vmin = GMT->session.d_NaN;	/* Never got set */
	if (global_vmax == -DBL_MAX) global_vmax = GMT->session.d_NaN;

	if (Ctrl->C.active && (Ctrl->I.active && Ctrl->I.status == GRDINFO_GIVE_REG_ROUNDED)) {
		if (!Ctrl->D.active) {	/* Don't want to round if getting tile regions */
			global_xmin = floor (global_xmin / Ctrl->I.inc[GMT_X]) * Ctrl->I.inc[GMT_X];
			global_xmax = ceil  (global_xmax / Ctrl->I.inc[GMT_X]) * Ctrl->I.inc[GMT_X];
			global_ymin = floor (global_ymin / Ctrl->I.inc[GMT_Y]) * Ctrl->I.inc[GMT_Y];
			global_ymax = ceil  (global_ymax / Ctrl->I.inc[GMT_Y]) * Ctrl->I.inc[GMT_Y];
			if (is_cube && !gmt_M_is_zero (U->z_inc)) {
				global_zmin = floor (global_zmin / U->z_inc) * U->z_inc;
				global_zmax = ceil  (global_zmax / U->z_inc) * U->z_inc;
			}
		}
		if (!Ctrl->D.active) {	/* Must make sure we don't get outside valid bounds */
			if (gmt_M_y_is_lat (GMT, GMT_IN)) {	/* Must make sure we don't get outside valid bounds */
				if (global_ymin < -90.0) {
					global_ymin = -90.0;
					GMT_Report (API, GMT_MSG_WARNING, "Using -I caused south to become < -90.  Reset to -90.\n");
				}
				if (global_ymax > 90.0) {
					global_ymax = 90.0;
					GMT_Report (API, GMT_MSG_WARNING, "Using -I caused north to become > +90.  Reset to +90.\n");
				}
			}
			if (gmt_M_x_is_lon (GMT, GMT_IN)) {	/* Must make sure we don't get outside valid bounds */
				if (fabs (global_xmax - global_xmin) > 360.0) {
					GMT_Report (API, GMT_MSG_WARNING, "Using -I caused longitude range to exceed 360.  Reset to a range of 360.\n");
					global_xmin = (global_xmin < 0.0) ? -180.0 : 0.0;
					global_xmax = (global_xmin < 0.0) ? +180.0 : 360.0;
				}
			}
		}
		if (Ctrl->D.active)
			grdinfo_report_tiles (GMT, G, global_xmin, global_xmax, global_ymin, global_ymax, Ctrl);
		else if (num_report) {	/* External interface, return as data with no leading text */
			/* w e s n {b t} z0 z1 */
			out[XLO] = global_xmin;		out[XHI] = global_xmax;
			out[YLO] = global_ymin;		out[YHI] = global_ymax;
			col = 4;
			if (is_cube) out[col++] = global_zmin;		out[col++] = global_zmax;
			out[col++] = global_vmin;		out[col++] = global_vmax;
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
		}
		else {	/* Command-line usage */
			sprintf (record, "%d%s", n_grds, sep);
			gmt_ascii_format_col (GMT, text, global_xmin, GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, sep);
			gmt_ascii_format_col (GMT, text, global_xmax, GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, sep);
			gmt_ascii_format_col (GMT, text, global_ymin, GMT_OUT, GMT_Y);	strcat (record, text);	strcat (record, sep);
			gmt_ascii_format_col (GMT, text, global_ymax, GMT_OUT, GMT_Y);	strcat (record, text);	strcat (record, sep);
			if (is_cube) {
				gmt_ascii_format_col (GMT, text, global_zmin, GMT_OUT, GMT_Z);	strcat (record, text);	strcat (record, sep);
				gmt_ascii_format_col (GMT, text, global_zmax, GMT_OUT, GMT_Z);	strcat (record, text);	strcat (record, sep);
			}
			gmt_ascii_format_col (GMT, text, global_vmin, GMT_OUT, GMT_Z+is_cube);	strcat (record, text);	strcat (record, sep);
			gmt_ascii_format_col (GMT, text, global_vmax, GMT_OUT, GMT_Z+is_cube);	strcat (record, text);
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
		}
	}
	else if (Ctrl->T.active) {
		if (Ctrl->T.mode & 2) {	/* Must do alpha trimming first */
			size_t size = header->size * ((size_t)header->n_bands);
			gmt_grdfloat *tmp_grid = NULL;
			char *file_ptr = grdfile;	/* To avoid a warning */
			if (is_cube) {
				if (gmt_M_file_is_memory (file_ptr)) {	/* Must operate on a copy since sorting is required */
					tmp_grid = gmt_M_memory_aligned (GMT, NULL, size, gmt_grdfloat);
					gmt_M_memcpy (tmp_grid, G->data, size, gmt_grdfloat);
				}
				else
					tmp_grid = U->data;
			}
			else {	/* Grid */
				if (gmt_M_file_is_memory (file_ptr)) {	/* Must operate on a copy since sorting is required */
					tmp_grid = gmt_M_memory_aligned (GMT, NULL, size, gmt_grdfloat);
					gmt_M_memcpy (tmp_grid, G->data, size, gmt_grdfloat);
				}
				else
					tmp_grid = G->data;
			}
			gmt_sort_array (GMT, tmp_grid, size, GMT_FLOAT);	/* Sort so we can find quantiles */
			global_vmin = gmt_quantile_f (GMT, tmp_grid, Ctrl->T.alpha[XLO], size);		/* "Left" quantile */
			global_vmax = gmt_quantile_f (GMT, tmp_grid, 100.0-Ctrl->T.alpha[XHI], size);	/* "Right" quantile */
			if (is_cube && GMT_Destroy_Data (API, &U) != GMT_NOERROR) {
				if (gmt_M_file_is_memory (file_ptr))	/* Now free temp grid */
					gmt_M_free (GMT, tmp_grid);
				Return (API->error);
			}
			else if (!is_cube && GMT_Destroy_Data (API, &G) != GMT_NOERROR) {
				if (gmt_M_file_is_memory (file_ptr))	/* Now free temp grid */
					gmt_M_free (GMT, tmp_grid);
				Return (API->error);
			}
			if (gmt_M_file_is_memory (file_ptr))	/* Now free temp grid */
				gmt_M_free (GMT, tmp_grid);
		}
		if (Ctrl->T.mode & 1) {	/* Get a symmetrical range */
			if (Ctrl->T.inc > 0.0) {	/* Round limits first */
				global_vmin = floor (global_vmin / Ctrl->T.inc) * Ctrl->T.inc;
				global_vmax = ceil  (global_vmax / Ctrl->T.inc) * Ctrl->T.inc;
			}
			global_vmax = MAX (fabs (global_vmin), fabs (global_vmax));
			global_vmin = -global_vmax;
		}
		else {	/* Just use reported min/max values (possibly alpha-trimmed above) */
			if (Ctrl->T.inc > 0.0) {	/* Round limits first */
				global_vmin = floor (global_vmin / Ctrl->T.inc) * Ctrl->T.inc;
				global_vmax = ceil  (global_vmax / Ctrl->T.inc) * Ctrl->T.inc;
			}
		}
		sprintf (record, "-T");
		gmt_ascii_format_col (GMT, text, global_vmin, GMT_OUT, GMT_W);	strcat (record, text);	strcat (record, "/");
		gmt_ascii_format_col (GMT, text, global_vmax, GMT_OUT, GMT_W);	strcat (record, text);
		if (Ctrl->T.inc > 0.0) {
			strcat (record, "/");
			gmt_ascii_format_col (GMT, text, Ctrl->T.inc, GMT_OUT, GMT_W);	strcat (record, text);
		}
		GMT_Put_Record (API, GMT_WRITE_DATA, Out);
	}
	else if ((Ctrl->I.active && Ctrl->I.status == GRDINFO_GIVE_REG_ROUNDED)) {
		if (!Ctrl->D.active) {	/* Don't want to round if getting tile regions */
			global_xmin = floor (global_xmin / Ctrl->I.inc[GMT_X]) * Ctrl->I.inc[GMT_X];
			global_xmax = ceil  (global_xmax / Ctrl->I.inc[GMT_X]) * Ctrl->I.inc[GMT_X];
			global_ymin = floor (global_ymin / Ctrl->I.inc[GMT_Y]) * Ctrl->I.inc[GMT_Y];
			global_ymax = ceil  (global_ymax / Ctrl->I.inc[GMT_Y]) * Ctrl->I.inc[GMT_Y];
			if (is_cube && !gmt_M_is_zero (U->z_inc)) {
				global_zmin = floor (global_zmin / U->z_inc) * U->z_inc;
				global_zmax = ceil  (global_zmax / U->z_inc) * U->z_inc;
			}
		}
		if (!Ctrl->D.active) {	/* Must make sure we don't get outside valid bounds */
			if (gmt_M_y_is_lat (GMT, GMT_IN)) {	/* Must make sure we don't get outside valid bounds */
				if (global_ymin < -90.0) {
					global_ymin = -90.0;
					GMT_Report (API, GMT_MSG_WARNING, "Using -I caused south to become < -90.  Reset to -90.\n");
				}
				if (global_ymax > 90.0) {
					global_ymax = 90.0;
					GMT_Report (API, GMT_MSG_WARNING, "Using -I caused north to become > +90.  Reset to +90.\n");
				}
			}
			if (gmt_M_x_is_lon (GMT, GMT_IN)) {	/* Must make sure we don't get outside valid bounds */
				if (fabs (global_xmax - global_xmin) > 360.0) {
					GMT_Report (API, GMT_MSG_WARNING, "Using -I caused longitude range to exceed 360.  Reset to a range of 360.\n");
					global_xmin = (global_xmin < 0.0) ? -180.0 : 0.0;
					global_xmax = (global_xmin < 0.0) ? +180.0 : 360.0;
				}
			}
		}
		if (Ctrl->D.active)
			grdinfo_report_tiles (GMT, G, global_xmin, global_xmax, global_ymin, global_ymax, Ctrl);
		else {
			sprintf (record, "-R");
			gmt_ascii_format_col (GMT, text, global_xmin, GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, "/");
			gmt_ascii_format_col (GMT, text, global_xmax, GMT_OUT, GMT_X);	strcat (record, text);	strcat (record, "/");
			gmt_ascii_format_col (GMT, text, global_ymin, GMT_OUT, GMT_Y);	strcat (record, text);	strcat (record, "/");
			gmt_ascii_format_col (GMT, text, global_ymax, GMT_OUT, GMT_Y);	strcat (record, text);
			if (is_cube) {
				strcat (record, "/");
				gmt_ascii_format_col (GMT, text, global_zmin, GMT_OUT, GMT_Z);	strcat (record, text);	strcat (record, "/");
				gmt_ascii_format_col (GMT, text, global_zmax, GMT_OUT, GMT_Z);	strcat (record, text);
			}
			GMT_Put_Record (API, GMT_WRITE_DATA, Out);
		}
	}

	if (delay) {
		if (is_cube && GMT_Destroy_Data (API, &U) != GMT_NOERROR) {
			Return (API->error);
		}
		else if (!is_cube && GMT_Destroy_Data (API, &G) != GMT_NOERROR) {
			Return (API->error);
		}
	}

	if (!Ctrl->C.active && !Ctrl->T.active && projStr) {		/* Print the referencing info */
		Out->text = projStr;
		GMT_Put_Record (API, GMT_WRITE_DATA, Out);
		gmt_M_str_free (projStr);
	}

	gmt_M_free (GMT, Out);

	if (GMT_End_IO (API, GMT_OUT, 0) != GMT_NOERROR) {	/* Disables further data output */
		Return (API->error);
	}

	Return (GMT_NOERROR);
}
