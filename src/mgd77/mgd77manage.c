/*--------------------------------------------------------------------
 *
 *	Copyright (c) 2005-2025 by the GMT Team (https://www.generic-mapping-tools.org/team.html)
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
 *
 * Author:	Paul Wessel
 * Date:	18-OCT-2005
 * Version:	1.0
 *
 */

#include "gmt_dev.h"
#include "longopt/mgd77manage_inc.h"
#include "mgd77.h"
#include "mgd77_e77.h"	/* E77 Header Errata Codes */

#define THIS_MODULE_CLASSIC_NAME	"mgd77manage"
#define THIS_MODULE_MODERN_NAME	"mgd77manage"
#define THIS_MODULE_LIB		"mgd77"
#define THIS_MODULE_PURPOSE	"Manage the content of MGD77+ files"
#define THIS_MODULE_KEYS	""
#define THIS_MODULE_NEEDS	""
#define THIS_MODULE_OPTIONS "-RVbjn" GMT_OPT("Q")

#define N_PAR		7
#define COL_SCALE	0
#define COL_OFFSET	1
#define IMG_SPACING	2
#define IMG_SCALE	3
#define IMG_MODE	4
#define IMG_LAT		5
#define COL_TYPE	6

#define ADD_IGRF	2
#define ADD_CARTER	3
#define ADD_GRAV	4
#define ADD_RMAG	5

#define N_E77_MODES	5
#define E77_HEADER_MODE	0
#define E77_TREND_MODE	1
#define E77_NAV_MODE	2
#define E77_VALUES_MODE	3
#define E77_SLOPES_MODE	4

#define MODE_a		1
#define MODE_c		2
#define MODE_d		3
#define MODE_e		4
#define MODE_g		5
#define MODE_i		6
#define MODE_n		7
#define MODE_t		8

struct MGD77MANAGE_CTRL {	/* All control options for this program (except common args) */
	/* active is true if the option has been activated */
	struct MGD77MANAGE_A {	/* -A */
		bool active;
		bool replace;
		bool interpolate;
		bool ignore_verify;
		unsigned int mode;
		unsigned int kind;
		bool e77_skip_mode[N_E77_MODES];
		char *file;
		double parameters[N_PAR];
	} A;
	struct MGD77MANAGE_D {	/* -D */
		bool active;
		char *file;
	} D;
	struct MGD77MANAGE_E {	/* -E */
		bool active;
		char value;
	} E;
	struct MGD77MANAGE_F {	/* -F */
		bool active;
	} F;
	struct MGD77MANAGE_I {	/* -I */
		bool active;
		char c_abbrev[GMT_LEN64];
		char c_units[GMT_LEN64];
		char c_name[MGD77_COL_NAME_LEN];
		char c_comment[MGD77_COL_COMMENT_LEN];
		char c_size;
	} I;
	struct MGD77MANAGE_N {	/* -N */
		bool active;
		char code[2];
	} N;
};

static void *New_Ctrl (struct GMT_CTRL *GMT) {	/* Allocate and initialize a new control structure */
	struct MGD77MANAGE_CTRL *C = NULL;

	C = gmt_M_memory (GMT, NULL, 1, struct MGD77MANAGE_CTRL);

	/* Initialize values whose defaults are not 0/false/NULL */

	C->A.kind = GMT_IS_FLOAT;
	C->A.parameters[COL_SCALE] = 1.0;	/* Output column scaling */
	C->A.parameters[IMG_SCALE] = 1.0;	/* IMG data scaling */
	C->E.value = '9';
	C->N.code[0] = 'k';			/* km is default distance unit */
 	return (C);
}

static void Free_Ctrl (struct GMT_CTRL *GMT, struct MGD77MANAGE_CTRL *C) {	/* Deallocate control structure */
	if (!C) return;
	gmt_M_str_free (C->A.file);
	gmt_M_str_free (C->D.file);
	gmt_M_free (GMT, C);
}

static int usage (struct GMTAPI_CTRL *API, int level) {
	const char *name = gmt_show_name_and_purpose (API, THIS_MODULE_LIB, THIS_MODULE_CLASSIC_NAME, THIS_MODULE_PURPOSE);
	if (level == GMT_MODULE_PURPOSE) return (GMT_NOERROR);
	GMT_Usage (API, 0, "usage: %s <cruise(s)> [-Aa|c|d|D|e|E|g|i|n|t|T<info>[+f]] [-D<name1>,<name2>,...] "
		"[-E<no_char>] [-F] [-I<abbrev>/<name>/<units>/<size>/<scale>/<offset>/\"comment\"] "
		"[-N%s] [%s] [%s] [%s] [%s] [%s] [%s]\n",
	    name, GMT_LEN_UNITS2_DISPLAY, GMT_Rgeo_OPT, GMT_V_OPT, GMT_bi_OPT, GMT_j_OPT, GMT_n_OPT, GMT_PAR_OPT);

	if (level == GMT_SYNOPSIS) return (GMT_MODULE_SYNOPSIS);

	GMT_Message (API, GMT_TIME_NONE, "  REQUIRED ARGUMENTS:\n");
	MGD77_Cruise_Explain (API->GMT);
	GMT_Message (API, GMT_TIME_NONE, "\n  OPTIONAL ARGUMENTS:\n");
	GMT_Usage (API, 1, "\n-Aa|c|d|D|e|E|g|i|n|t|T<info>[+f]");
	GMT_Usage (API, -2, "Append a new data column to the given files.  Append +f to overwrite an "
		"existing column with same name with new data [Default will refuse if an "
		"existing column has the same abbreviation as the new data]. "
		"The code letters are:");
	GMT_Usage (API, 3, "a: Give filename with a new column to add.  We expect a single-column file "
		"with the same number of records as the MGD77 file. Only one cruise can be set. "
		"If filename is - we read from standard input.");
	GMT_Usage (API, 3, "c: Create a new column to be calculated from existing columns.  Add a code:");
	GMT_Usage (API, 4, "m: IGRF total field.");
	GMT_Usage (API, 4, "c: Carter correction.");
	GMT_Usage (API, 4, "g: IGF (\"normal gravity\"). Optionally append 1-4 to select the gravity formula to use:");
	GMT_Usage (API, 5, "1: Heiskanen 1924.");
	GMT_Usage (API, 5, "2: International 1930.");
	GMT_Usage (API, 5, "3: IGF1967.");
	GMT_Usage (API, 5, "4: IGF1980.");
	GMT_Usage (API, -4, "[Default uses formula specified in the MGD77 header, or 4 if not valid].");
	GMT_Usage (API, 4, "r: Recomputed magnetic anomaly rmag = mtfx - IGRF total field. "
		"Append x for which mtfx field to use (1 or 2) [1].");
	GMT_Usage (API, 3, "d: Give filename with (dist [see -N], data) for a new column.  We expect a two-column file "
		"with distances (in km) in first column and data values in 2nd.  Only one cruise can be set. "
		"If filename is - we read from standard input.  Only records with matching distance will have data assigned.");
	GMT_Usage (API, 3, "D: Same as d but we interpolate between the dist,data pairs to fill in all data records.");
	GMT_Usage (API, 3, "e: Ingest MGD77 error/correction information (e77) produced by mgd77sniffer.  We will look "
		"for the <cruise>.e77 file in the current directory or in $MGD77_HOME/E77. "
		"By default we will apply recommended header (h) and systematic fixes (f) and set all data bit flags. "
		"Append a combination of these flags to change the default accordingly:");
	GMT_Usage (API, 4, "h: Ignore all header recommendations.");
	GMT_Usage (API, 4, "f: Ignore all systematic fixes recommendations.");
	GMT_Usage (API, 4, "n: Ignore data record bitflags pertaining to navigation (time, lon, lat).");
	GMT_Usage (API, 4, "v: Ignore data record bitflags pertaining to data values.");
	GMT_Usage (API, 4, "s: Ignore data record bitflags pertaining to data slopes (gradients).");
	GMT_Usage (API, 4, "Use -DE to ignore the verification status of the e77 file [Default requires verification to be Y]. "
		"Note: Previous E77 information will be removed prior to processing this E77 information.");
	GMT_Usage (API, 3, "g: Sample a GMT grid along track (also see -n; use -R to select a sub-region). "
		"Append filename of the GMT grid.");
	GMT_Usage (API, 3, "i: Sample a Sandwell/Smith *.img Mercator grid along track (also see -n; use -R to select a sub-region). ");
	gmt_img_syntax (API->GMT, 4);
	GMT_Usage (API, 3, "n: Give filename with (rec_no, data) for a new column.  We expect a two-column file "
		"with record numbers (0 means 1st row) in first column and data values in 2nd.  Only one cruise can be set. "
		"If filename is - we read from standard input.  Only records with matching record numbers will have data assigned.");
	GMT_Usage (API, 3, "t: Give filename with (abstime, data) for a new column.  We expect a two-column file "
		"with dateTclock strings in first column and data values in 2nd.  Only one cruise can be set. "
		"If filename is - we read from standard input.  Only records with matching times will have data assigned.");
	GMT_Usage (API, 3, "T: Same as t but we interpolate between the time, data pairs to fill in all data records.");
	GMT_Usage (API, 1, "\n-D<name1>,<name2>,...");
	GMT_Usage (API, -2, "Delete the columns listed from all the cruise data files. "
		"The columns are removed before any data are added.  It is not a substitute for -A...+f. "
		"However, sometimes the shape of new data demands the old to be deleted first (you will be told).");
	GMT_Usage (API, 1, "\n-E<no_char>");
	GMT_Usage (API, -2, "Give character used to fill empty/missing string columns [9].");
	GMT_Usage (API, 1, "\n-F Force mode.  This allows you to even replace the standard MGD77 columns [only extended columns can be changed].");
	GMT_Usage (API, 1, "\n-I<abbrev>/<name>/<units>/<size>/<scale>/<offset>/\"comment\"");
	GMT_Usage (API, -2, "In addition to the file information above, you must also specify column information:");
	GMT_Usage (API, 3, "abbrev:  Short, abbreviated word (lower case only), like satfaa (%d char max).", MGD77_COL_ABBREV_LEN);
	GMT_Usage (API, 3, "name:    Descriptive name, like \"Geosat/ERS-1 Free-air gravity\" (%d char max).", MGD77_COL_NAME_LEN);
	GMT_Usage (API, 3, "units:   Units for the column (e.g., mGal, gamma, km) (%d char max).", MGD77_COL_NAME_LEN);
	GMT_Usage (API, 3, "size:    Either t(ext), b(yte), s(hort), f(loat), i(nt), or d(ouble).");
	GMT_Usage (API, 3, "scale:   Multiply data by this scale before writing to mgd77+ file.");
	GMT_Usage (API, 3, "offset:  Add after scaling before writing to mgd77+ file.");
	GMT_Usage (API, 3, "comment: Any text (in double quotes) for information about column (%d char max).", MGD77_COL_COMMENT_LEN);
	GMT_Usage (API, -2, "Note: Option -I is ignored by -Ae. "
		"Note for text: Interpolation is not allowed, and \"not-a-string\" is created from -E.");
	GMT_Usage (API, 1, "\n-N%s", GMT_LEN_UNITS2_DISPLAY);
	GMT_Usage (API, -2, "Append your choice for distance unit (if -Ad|D are set). Choose among "
		"m(e)ter, (f)oot, (k)m, (M)ile, (n)autical mile, or s(u)rvey foot [Default is -Nk].");
	GMT_Usage (API, -2, "Note: See -j for selecting distance calculation procedure.");
	GMT_Option (API, "Rg,V,bi,di,j,n,.");

	return (GMT_MODULE_USAGE);
}

/* Help functions to decode the -A and -I options */

GMT_LOCAL int mgd77manage_decode_A_options (int mode, char *line, char *file, double parameters[]) {
	int error = 0, n;
	char *c = strstr (line, "+f");

	if (c) c[0] = '\0';	/* Chop off modifier */
	if (mode == 1) {	/* For *.img files since we need to know data scale and grid mode */
		/* -Ai<filename>,<scale>/<mode>[/<lat>][+f] */
		n = sscanf (line, "%[^,],%lf,%lf,%lf", file, &parameters[IMG_SCALE], &parameters[IMG_MODE], &parameters[IMG_LAT]);
		if (n < 3) error = 1;
	}
	else {	/* GMT grid or table: No data scale and mode to worry about */
		/* -Aa|c|d|D|e|g|n|t|T<filename>[+f] */
		strcpy (file, line);
	}
	if (c) c[0] = '+';	/* Chop off modifier */

	return (error);
}

GMT_LOCAL int mgd77manage_decode_I_options (struct GMT_CTRL *GMT, char *line, char *abbrev, char *name, char *units, char *size, char *comment, double parameters[]) {
	/* -I<abbrev>/<name>/<units>/<size>/<scale>/<offset>/\"comment\" */
	unsigned int i = 0, k, error, pos = 0;
	char p[GMT_BUFSIZ] = {""};

	while (i < 7 && gmt_strtok (line, "/", &pos, p)) {	/* Process the 7 items */
		switch (i) {
			case 0:
				strcpy (abbrev, p);
				/* Check abbrev for COARDS compliance as well as being lower case */
				for (k = error = 0; abbrev[k]; k++) {
					if (isupper ((int)abbrev[k])) error++;
					if (isalpha ((int)abbrev[k])) continue;
					if (isdigit ((int)abbrev[k]) && k > 0) continue;
					if (abbrev[k] == '_' && k > 0) continue;
					error++;
				}
				if (error) {
					GMT_Report (GMT->parent, GMT_MSG_ERROR,
					            "Abbreviation name should only contain lower case letters, digits, and underscores\n");
					return (true);
				}
				break;
			case 1:
				strcpy (name, p);
				break;
			case 2:
				strcpy (units, p);
				break;
			case 3:
				*size = p[0];
				break;
			case 4:
				parameters[COL_SCALE]  = atof (p);
				break;
			case 5:
				parameters[COL_OFFSET]  = atof (p);
				break;
			case 6:
				strcpy (comment, p);
				break;
		}
		i++;
	}

	switch (*size) {	/* Given size, set the NC type */
		case 'b':
			parameters[COL_TYPE] = NC_BYTE;
			break;
		case 'd':
			parameters[COL_TYPE] = NC_DOUBLE;
			break;
		case 'f':
			parameters[COL_TYPE] = NC_FLOAT;
			break;
		case 'i':
			parameters[COL_TYPE] = NC_INT;
			break;
		case 's':
			parameters[COL_TYPE] = NC_SHORT;
			break;
		case 't':
			parameters[COL_TYPE] = NC_CHAR;
			break;
		default:
			GMT_Report (GMT->parent, GMT_MSG_ERROR, "Unknown data type flag %c\n", *size);
			parameters[COL_TYPE] = MGD77_NOT_SET;
			break;
	}
	return ((lrint (parameters[COL_TYPE]) == MGD77_NOT_SET) || (i != 7));
}

GMT_LOCAL int mgd77manage_skip_if_missing (struct GMT_CTRL *GMT, char *name, char *file, struct MGD77_CONTROL *F, struct MGD77_DATASET **D) {
	/* Used when a needed column is not present and we must free memory and goto next file */
	int id;

	if ((id = MGD77_Get_Column (GMT, name, F)) == MGD77_NOT_SET) {
		GMT_Report (GMT->parent, GMT_MSG_ERROR, "Cruise %s is missing column %s which is required for selected operation - skipping\n", file, name);
		MGD77_Free_Dataset (GMT, D);	/* Free memory already allocated by MGD77_Read_File for this aborted effort */
	}
	return (id);
}

GMT_LOCAL int mgd77manage_got_default_answer (char *line, char *answer) {
	int i, k, len;

	len = (int)strlen (line) - 1;
	gmt_M_memset (answer, GMT_BUFSIZ, char);	/* No default answer */
	if (line[len] == ']') {	/* Got a default answer for this item */
		for (k = i = len; i && line[i] != '['; i--);
		strncpy (answer, &line[i+1], (size_t)(k - i - 1));
	}
	return (answer[0] != '\0');
}

static int parse (struct GMT_CTRL *GMT, struct MGD77MANAGE_CTRL *Ctrl, struct GMT_OPTION *options) {
	/* This parses the options provided to mgd77manage and sets parameters in CTRL.
	 * Any GMT common options will override values set previously by other commands.
	 * It also replaces any file names specified as input or output with the data ID
	 * returned when registering these sources/destinations with the API.
	 */

	unsigned int n_errors = 0, k;
	bool got_table, got_grid, strings;
	nc_type c_nc_type;
	char file[PATH_MAX] = {""}, *c = NULL;
	struct GMT_OPTION *opt = NULL;
	struct GMTAPI_CTRL *API = GMT->parent;

	for (opt = options; opt; opt = opt->next) {
		switch (opt->option) {

			case '<':	/* Input files */
			case '#':	/* Skip input files confused as numbers (e.g. 123456) */
				break;

			/* Processes program-specific parameters */

			case 'A':	/* Adding a new column */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->A.active);
				k = 0;
				if (opt->arg[k] == '+') {	/* Deprecated way of doing _f */
					Ctrl->A.replace = true;
					k++;
				}
				else if ((c = strstr (opt->arg, "+f"))) {
					Ctrl->A.replace = true;
					c[0] = '\0';	/* Chop off modifier */
				}
				switch (opt->arg[k]) {
					case 'a':	/* Plain column data file of exact same # of records */
						Ctrl->A.mode = MODE_a;
						n_errors += mgd77manage_decode_A_options (0, &opt->arg[k+1], file, Ctrl->A.parameters);
						break;
					case 'c':	/* Add reference field or correction term */
						Ctrl->A.mode = MODE_c;
						n_errors += mgd77manage_decode_A_options (0, &opt->arg[k+1], file, Ctrl->A.parameters);
						break;
					case 'D':	/* dist,val data file - interpolate to get values at all records */
						Ctrl->A.interpolate = true;
						/* Intentionally fall through - to 'd' */
					case 'd':	/* dist,val data file - only update records with matching distances */
						Ctrl->A.mode = MODE_d;
						n_errors += mgd77manage_decode_A_options (0, &opt->arg[k+1], file, Ctrl->A.parameters);
						break;
					case 'E':	/* Plain E77 error flag file from mgd77sniffer */
						Ctrl->A.ignore_verify = true;	/* Process raw e77 files that have not been verified */
						/* Intentionally fall through - to 'e' */
					case 'e':	/* Plain E77 error flag file from mgd77sniffer */
						Ctrl->A.mode = MODE_e;
						while (opt->arg[++k]) {
							switch (opt->arg[k]) {
								case 'h':	/* Ignore all header recommendations regardless of Y/N prefix */
									Ctrl->A.e77_skip_mode[E77_HEADER_MODE] = true;
									break;
								case 'f':	/* Ignore all systematic trend recommendations regardless of Y/N prefix */
									Ctrl->A.e77_skip_mode[E77_TREND_MODE] = true;
									break;
								case 'n':	/* Ignore all NAV flags */
									Ctrl->A.e77_skip_mode[E77_NAV_MODE] = true;
									break;
								case 'v':	/* Ignore all VALUE flags */
									Ctrl->A.e77_skip_mode[E77_VALUES_MODE] = true;
									break;
								case 's':	/* Ignore all SLOPE flags */
									Ctrl->A.e77_skip_mode[E77_SLOPES_MODE] = true;
									break;
								default:
									GMT_Report (API, GMT_MSG_ERROR, "-Ae modifiers must be combination of hfnvs\n");
									n_errors++;
									break;
							}
						}
						break;
					case 'g':	/* Sample along track from this GMT grid file */
						Ctrl->A.mode = MODE_g;
						n_errors += mgd77manage_decode_A_options (0, &opt->arg[k+1], file, Ctrl->A.parameters);
						break;
					case 'i':	/* Sample along track from this *.img grid file */
						Ctrl->A.mode = MODE_i;
						n_errors += mgd77manage_decode_A_options (1, &opt->arg[k+1], file, Ctrl->A.parameters);
						break;
					case 'n':	/* recno,val data file - only update records with matching rec number */
						Ctrl->A.mode = MODE_n;
						n_errors += mgd77manage_decode_A_options (0, &opt->arg[k+1], file, Ctrl->A.parameters);
						break;
					case 'T':	/* abstime,val data file - interpolate to get values at all records */
						Ctrl->A.interpolate = true;
						/* Intentionally fall through - to 't' */
					case 't':	/* abstime,val data file - only update records with matching times */
						Ctrl->A.mode = MODE_t;
						Ctrl->A.kind = GMT_IS_ABSTIME;
						n_errors += mgd77manage_decode_A_options (0, &opt->arg[k+1], file, Ctrl->A.parameters);
						break;
					default:
						GMT_Report (API, GMT_MSG_ERROR, "-A modifier must be a|c|d|D|e|g|i|n|t|T\n");
						n_errors++;
						break;
				}
				if (strlen (file)) Ctrl->A.file = strdup (file);
				if (c) c[0] = '+';	/* Restore modifier */
				break;

			case 'C':	/* Distance calculation method */
				if (gmt_M_compat_check (API->GMT, 6)) {
					GMT_Report (API, GMT_MSG_COMPAT, "The -C option is deprecated; use -j<mode> instead\n");
					GMT->common.j.active = true;
					if (opt->arg[0] == 'f') GMT->common.j.mode = GMT_FLATEARTH;
					if (opt->arg[0] == 'g') GMT->common.j.mode = GMT_GREATCIRCLE;
					if (opt->arg[0] == 'e') GMT->common.j.mode = GMT_GEODESIC;
					if (GMT->common.j.mode < 1 || GMT->common.j.mode > 3) {
						GMT_Report (API, GMT_MSG_ERROR, "Option -C: Flag must be f, g, or e\n");
						n_errors++;
					}
				}
				else {
					GMT_Report (API, GMT_MSG_ERROR, "Unrecognized option -C\n");
					n_errors++;
				}
				break;
			case 'D':	/* Columns to delete */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->D.active);
				n_errors += gmt_get_required_string (GMT, opt->arg, opt->option, 0, &Ctrl->D.file);
				break;

			case 'E':	/* character to generate no-string value */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->E.active);
				n_errors += gmt_get_required_char (GMT, opt->arg, opt->option, 0, &Ctrl->E.value);
				break;

			case 'F':	/* Force mode */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->F.active);
				n_errors += gmt_get_no_argument (GMT, opt->arg, opt->option, 0);
				break;

			case 'I':	/* Column attribute information */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->I.active);
				n_errors += mgd77manage_decode_I_options (GMT, opt->arg, Ctrl->I.c_abbrev, Ctrl->I.c_name, Ctrl->I.c_units,
				                              &Ctrl->I.c_size, Ctrl->I.c_comment, Ctrl->A.parameters);
				break;

			case 'N':	/* Set distance units */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->N.active);
				n_errors += gmt_get_required_char (GMT, opt->arg, opt->option, 0, &Ctrl->N.code[0]);
				if (Ctrl->N.code[0] == 'm' && gmt_M_compat_check (GMT, 4)) {
					GMT_Report (API, GMT_MSG_COMPAT, "Option -N: Unit m for miles is deprecated; use unit M instead\n");
					Ctrl->N.code[0] = 'M';
				}
				if (!strchr (GMT_LEN_UNITS2, (int)Ctrl->N.code[0])) {
					GMT_Report (API, GMT_MSG_ERROR, "Option -N: Unit must be from %s\n", GMT_LEN_UNITS2_DISPLAY);
					n_errors++;
				}
				break;

			default:	/* Report bad options */
				n_errors += gmt_default_option_error (GMT, opt);
				break;
		}
	}

	got_table = (Ctrl->A.mode == MODE_a || Ctrl->A.mode == MODE_d || Ctrl->A.mode == MODE_n || Ctrl->A.mode == MODE_t);	/* Got a table to read */
	got_grid = (Ctrl->A.mode == MODE_g || Ctrl->A.mode == MODE_i);					/* Got a grid to read */
	c_nc_type = (nc_type) lrint (Ctrl->A.parameters[COL_TYPE]);		/* NC data type */
	strings = (c_nc_type == NC_CHAR);				/* true if our new column contains strings */

	n_errors += gmt_M_check_condition (GMT, (got_table + got_grid) > 1, "You must select one, and only one, of the -A options\n");
	n_errors += gmt_M_check_condition (GMT, (Ctrl->A.interpolate + strings) > 1, "Cannot interpolate column if data are strings\n");
	n_errors += gmt_M_check_condition (GMT, got_table && Ctrl->A.mode == MODE_c, "Only one -A option can be specified\n");
	n_errors += gmt_M_check_condition (GMT, !got_grid && GMT->common.n.interpolant != BCR_BICUBIC, "Option -n: Requires -Ag|i\n");
	if (!(Ctrl->D.active || Ctrl->A.mode == MODE_e)) {
		n_errors += gmt_M_check_condition (GMT, strlen (Ctrl->I.c_abbrev) > MGD77_COL_ABBREV_LEN,
		                                 "Option -I: Column abbreviation too long - %d characters is maximum!\n", MGD77_COL_ABBREV_LEN);
		n_errors += gmt_M_check_condition (GMT, strlen (Ctrl->I.c_name) > MGD77_COL_NAME_LEN,
		                                 "Option -I: Column name too long - %d characters is maximum!\n", MGD77_COL_NAME_LEN);
		n_errors += gmt_M_check_condition (GMT, strlen (Ctrl->I.c_comment) > MGD77_COL_COMMENT_LEN,
		                                 "Option -I: Column comment too long - %d characters is maximum!\n", MGD77_COL_COMMENT_LEN);
	}
	return (n_errors ? GMT_PARSE_ERROR : GMT_NOERROR);
}

#define bailout(code) {gmt_M_free_options (mode); return (code);}
#define Return(code) {Free_Ctrl (GMT, Ctrl); gmt_end_module (GMT, GMT_cpy); bailout (code);}

EXTERN_MSC int GMT_mgd77manage (void *V_API, int mode, void *args) {
	int cdf_var_id, n_dims = 0, dims[2];		/* netCDF variables should be declared as int */
	size_t start[2] = {0, 0}, count[2] = {0, 0};	/* NetCDF offset variables are size_t */

	int i, k = 0, column, result, set, check, error = 0;
	int width, GF_version = MGD77_NOT_SET, n_fields = 0, n_paths = 0;
	bool transform, verified, strings = false, got_grid, got_table;
	bool two_cols = false, constant, ok_to_read = true, interpolate = false;

	unsigned int MTF_col = 1, pos, c_kind = 0, row, col;
	uint64_t argno, n_expected_fields, n_delete = 0, n_bad, n_sampled = 0, n_changed = 0, n = 0, rec, jrec;
	size_t n_alloc = GMT_CHUNK;

	time_t now;

	nc_type c_nc_type;

	char line[GMT_BUFSIZ] = {""}, p[GMT_BUFSIZ] = {""}, history[GMT_BUFSIZ] = {""}, **list = NULL;
	char not_given[GMT_LEN64] = {""}, word[GMT_BUFSIZ] = {""}, **tmp_string = NULL, *text = NULL;
	signed char LEN = 0, OLDLEN = 0;

	double x, y, match_value, single_val, dist_scale = 1.0;
	double *xtmp = NULL, *coldnt = NULL, *colvalue = NULL, *in = NULL, limits[2];

	FILE *fp = NULL, *fp_err = NULL;

	struct MGD77_CONTROL In;
	struct MGD77_DATASET *D = NULL;
	struct GMT_GRID *G = NULL;
	struct MGD77_CARTER Carter;
	struct MGD77MANAGE_CTRL *Ctrl = NULL;
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

	/*---------------------------- This is the mgd77manage main code ----------------------------*/

	gmt_set_pad (GMT, 2U);	/* Ensure space for BCs in case an API passed pad == 0 */
	MGD77_Init (GMT, &In);	/* Initialize MGD77 Machinery */

	/* Default e77_skip_mode will apply header and fix corrections if prefix is Y and set all data bits */

	/* Check that the options selected are mutually consistent */

	got_table = (Ctrl->A.mode == MODE_a || Ctrl->A.mode == MODE_d || Ctrl->A.mode == MODE_n || Ctrl->A.mode == MODE_t);	/* Got a table to read */
	got_grid = (Ctrl->A.mode == MODE_g || Ctrl->A.mode == MODE_i);					/* Got a grid to read */
	c_nc_type = (nc_type) lrint (Ctrl->A.parameters[COL_TYPE]);		/* NC data type */
	strings = (c_nc_type == NC_CHAR);				/* true if our new column contains strings */

	n_paths = MGD77_Path_Expand (GMT, &In, options, &list);	/* Get list of requested IDs */

	if (n_paths <= 0) {
		GMT_Report (API, GMT_MSG_ERROR, "No cruises given\n");
		MGD77_Path_Free (GMT, (uint64_t)n_paths, list);
		Return (GMT_NO_INPUT);
	}

	if (got_table && n_paths != 1) {
		GMT_Report (API, GMT_MSG_ERROR, "With -Aa|d|D|n|t|T you can only select one cruise at the time.\n");
		MGD77_Path_Free (GMT, (uint64_t)n_paths, list);
		Return (GMT_PARSE_ERROR);
	}
	MGD77_Set_Unit (GMT, Ctrl->N.code, &dist_scale, -1);	/* Gets scale which multiplies meters to chosen distance unit */

	memset (not_given, (int)Ctrl->E.value, GMT_LEN64);	/* Text representing "no text value" */
	not_given[GMT_LEN64-1] = '\0';
	fp_err = (In.verbose_dest == 1) ? GMT->session.std[GMT_OUT] : GMT->session.std[GMT_ERR];

	if (Ctrl->A.mode == MODE_c) {	/* Calculate values to be stored */
		int version = MGD77_NOT_SET, mfield = 1;
		/* "file" is here either m, c, or g[1-4] */
		if (Ctrl->A.file[0] == 'm' && Ctrl->A.file[1] == '\0') {
			c_kind = ADD_IGRF;
		}
		else if (Ctrl->A.file[0] == 'c' && Ctrl->A.file[1] == '\0') {
			c_kind = ADD_CARTER;
			MGD77_carter_init (GMT, &Carter);	/* Initialize Carter machinery */
		}
		else if (Ctrl->A.file[0] == 'g' && (Ctrl->A.file[1] == '\0' || ((version = (Ctrl->A.file[1] - '0')) >=
		         MGD77_IGF_HEISKANEN && version <= MGD77_IGF_1980)) ) {
			c_kind = ADD_GRAV;
			GF_version = version;
		}
		else if (Ctrl->A.file[0] == 'r' && (Ctrl->A.file[1] == '\0' || ((mfield = (Ctrl->A.file[1] - '0')) >= 1 && mfield <= 2)) ) {
			c_kind = ADD_RMAG;
			MTF_col = mfield;
		}
		else {
			GMT_Report (API, GMT_MSG_ERROR, "-Ac expects m, c, or g[1-4]\n");
			MGD77_Path_Free (GMT, (uint64_t)n_paths, list);
			Return (GMT_PARSE_ERROR);
		}
	}
	else if (Ctrl->A.mode == MODE_e) {	/* Do E77 work by ignoring previous E77 settings */
		In.use_flags[MGD77_M77_SET] = In.use_flags[MGD77_CDF_SET] = false;		/* Turn use of flag bits OFF */
		In.use_corrections[MGD77_M77_SET] = In.use_corrections[MGD77_CDF_SET] = false;	/* Turn use of systematic corrections OFF */
	}
	else if (Ctrl->A.mode == MODE_g) {	/* Read regular GMT grid */
		double wesn[4];
		gmt_M_memset (wesn, 4, double);
		if (GMT->common.R.active[RSET]) gmt_M_memcpy (wesn, GMT->common.R.wesn, 4, double);	/* Current -R setting for subset */
		if ((G = GMT_Read_Data (API, GMT_IS_GRID, GMT_IS_FILE, GMT_IS_SURFACE, GMT_DATA_ONLY, wesn, Ctrl->A.file, NULL)) == NULL) {	/* Get data */
			MGD77_Path_Free (GMT, (uint64_t)n_paths, list);
			Return (API->error);
		}
		interpolate = (GMT->common.n.threshold > 0.0);
	}
	else if (Ctrl->A.mode == MODE_i) {	/* Read Sandwell/Smith IMG file */
		double wesn[4];
		gmt_M_memset (wesn, 4, double);
		if (GMT->common.R.active[RSET]) gmt_M_memcpy (wesn, GMT->common.R.wesn, 4, double);	/* Current -R setting for subset */
		if ((G = gmt_create_grid (GMT)) == NULL) Return (API->error);
		gmt_read_img (GMT, Ctrl->A.file, G, wesn, Ctrl->A.parameters[IMG_SCALE], urint(Ctrl->A.parameters[IMG_MODE]),
		              Ctrl->A.parameters[IMG_LAT], true);
		interpolate = (GMT->common.n.threshold > 0.0);
	}
	else if (got_table) {	/* Got a one- or two-column table to read */
		uint64_t n_ave = 0;
		double last_dnt = -DBL_MAX, sum_z = 0.0;

		if (Ctrl->A.file[0] == '-') {   /* Just read from standard input */
			fp = GMT->session.std[GMT_IN];
#ifdef SET_IO_MODE
			gmt_setmode (GMT, GMT_IN);
#endif
		}
		else {
			if ((fp = gmt_fopen (GMT, Ctrl->A.file, GMT->current.io.r_mode)) == NULL) {
				GMT_Report (API, GMT_MSG_ERROR, "Cannot open file %s\n", Ctrl->A.file);
				MGD77_Path_Free (GMT, (uint64_t)n_paths, list);
				Return (GMT_ERROR_ON_FOPEN);
			}
		}

		GMT->current.io.first_rec = true;
		if (GMT->current.setting.io_header[GMT_IN]) {	/* Skip any header records */
			for (i = 0; i < (int)GMT->current.setting.io_n_header_items; i++) {
				if (!gmt_fgets (GMT, line, GMT_BUFSIZ, fp)) {
					GMT_Report (API, GMT_MSG_ERROR, "Read error for headers\n");
					if (fp != GMT->session.std[GMT_IN]) gmt_fclose (GMT, fp);
					MGD77_Path_Free (GMT, (uint64_t)n_paths, list);
					Return (GMT_DATA_READ_ERROR);
				}
			}
		}

		two_cols = (Ctrl->A.mode == MODE_d || Ctrl->A.mode == MODE_n || Ctrl->A.mode == MODE_t);
		n = (two_cols) ? -1 : 0;
		n_alloc = GMT_CHUNK;
		n_expected_fields = (GMT->common.b.ncol[GMT_IN]) ? GMT->common.b.ncol[GMT_IN] : GMT_MAX_COLUMNS;
		colvalue = gmt_M_memory (GMT, NULL, n_alloc, double);
		if (two_cols) {	/* Got an abscissae column as well (dnt: d = dist, n = rec number, t = time) */
			coldnt = gmt_M_memory (GMT, NULL, n_alloc, double);
			gmt_set_column_type (GMT, GMT_IN, GMT_X, Ctrl->A.kind);
		}
		if (strings && !two_cols) {	/* Must read strings directly from file since GMT->current.io.input would barf */
			ok_to_read = false;
			tmp_string = gmt_M_memory (GMT, NULL, n_alloc, char *);
			while (gmt_fgets (GMT, word, GMT_BUFSIZ, fp)) {
				if (gmt_is_a_blank_line (line) || strchr (GMT->current.setting.io_head_marker_in, word[0])) continue;	/* Skip all headers or blank lines  */
				width = (int)strlen (word);
				tmp_string[n] = gmt_M_memory (GMT, NULL, width + 1, char);
				strcpy (tmp_string[n], word);
				if (width > LEN) LEN = (signed char)width;
				n++;
			}
		}
		else if (strings) {		/* Pretend to read one column and get the text string form the text record */
			tmp_string = gmt_M_memory (GMT, NULL, n_alloc, char *);
			n_expected_fields = 1;
		}

		if (ok_to_read) in = GMT->current.io.input (GMT, fp, &n_expected_fields, &n_fields);

		while (ok_to_read && !gmt_M_rec_is_eof (GMT)) {	/* Not yet EOF */

			while (gmt_M_rec_is_segment_header (GMT) && !gmt_M_rec_is_eof(GMT)) {
				in = GMT->current.io.input (GMT, fp, &n_expected_fields, &n_fields);
			}
			if ((GMT->current.io.status & GMT_IO_EOF)) continue;	/* At EOF */

			if (GMT->current.io.status & GMT_IO_MISMATCH) {
				GMT_Report (API, GMT_MSG_ERROR, "Mismatch between actual (%d) and expected (%d) fields near line %d\n",
				            n_fields, n_expected_fields, n);
				MGD77_Path_Free (GMT, (uint64_t)n_paths, list);
				MGD77_end (GMT, &In);
				gmt_M_free (GMT, colvalue);
				if (two_cols) gmt_M_free (GMT, coldnt);
				if (strings) gmt_M_free (GMT, tmp_string);
				Return (GMT_RUNTIME_ERROR);
			}

			if (strings) {	/* number in col1, string in col2 */
				coldnt[n]   = in[0];
				sscanf (GMT->current.io.curr_text, "%*s %s", word);
				tmp_string[n] = gmt_M_memory (GMT, NULL, strlen(word) + 1, char);
				strcpy (tmp_string[n], word);
			}
			else if (two_cols) {
				if (in[0] > last_dnt) {	/* Start of new averaging scheme (if needed) */
					if (n_ave) {
						colvalue[n] = sum_z / n_ave;
						coldnt[n]   = last_dnt;
					}
					n_ave = 0;
					sum_z = 0.0;
					n++;
					last_dnt = in[0];
				}
				sum_z += in[1];	/* Add up possibly multiple values for same dnt */
				n_ave++;
			}
			else
				colvalue[n] = in[0];
			if (!two_cols) n++;
			if (n == n_alloc) {
				n_alloc <<= 1;
				if (strings)
					tmp_string = gmt_M_memory (GMT, tmp_string, n_alloc, char *);
				else
					colvalue = gmt_M_memory (GMT, colvalue, n_alloc, double);
				if (two_cols) coldnt = gmt_M_memory (GMT, coldnt, n_alloc, double);
			}

			in = GMT->current.io.input (GMT, fp, &n_expected_fields, &n_fields);
		}
		gmt_fclose (GMT, fp);
		if (two_cols && n_ave) { colvalue[n] = sum_z / n_ave; coldnt[n++] = last_dnt;}
		if (!strings) colvalue = gmt_M_memory (GMT, colvalue, n, double);
		if (two_cols) coldnt = gmt_M_memory (GMT, coldnt, n, double);
	}

	MGD77_Ignore_Format (GMT, MGD77_FORMAT_ANY);	/* Reset to all formats OK, then ... */
	MGD77_Ignore_Format (GMT, MGD77_FORMAT_M77);	/* disallow ASCII MGD77 files */
	MGD77_Ignore_Format (GMT, MGD77_FORMAT_M7T);	/* disallow ASCII MGD77T files */
	MGD77_Ignore_Format (GMT, MGD77_FORMAT_TBL);	/* and ASCII tables */

	In.format = MGD77_FORMAT_CDF;	/* Only file type allowed as input */

	for (argno = 0; argno < (uint64_t)n_paths; argno++) {		/* Process each ID */

		if (MGD77_Open_File (GMT, list[argno], &In, MGD77_READ_MODE)) continue;

		GMT_Report (API, GMT_MSG_INFORMATION, "Now processing cruise %s\n", list[argno]);

		D = MGD77_Create_Dataset (GMT);
		In.n_out_columns = 0;

		if (MGD77_Read_File (GMT, list[argno], &In, D)) {
			GMT_Report (API, GMT_MSG_ERROR, "Failure while reading data set for cruise %s\n", list[argno]);
			MGD77_Free_Dataset (GMT, &D);
			gmt_M_free (GMT, colvalue);
			if (tmp_string) gmt_M_free (GMT, tmp_string);
			Return (GMT_DATA_READ_ERROR);
		}

		/* Start reading data from file */

		column = MGD77_Get_Column (GMT, Ctrl->I.c_abbrev, &In);
		set    = MGD77_Get_Set (GMT, Ctrl->I.c_abbrev);

		if (Ctrl->A.mode != MODE_e && column != MGD77_NOT_SET) {	/* A column with same abbreviation is already present in the file */
			if (set == MGD77_M77_SET && !Ctrl->F.active) {
				GMT_Report (API, GMT_MSG_ERROR, "Column %s is part of the standard MGD77 set and cannot be removed unless you use -F!\n", Ctrl->I.c_abbrev);
				MGD77_Free_Dataset (GMT, &D);
				if (tmp_string) gmt_M_free (GMT, tmp_string);
				Return (GMT_RUNTIME_ERROR);
			}
			if (!Ctrl->A.replace) {
				GMT_Report (API, GMT_MSG_ERROR, "A columned named %s is already present in %s.  use -A+ to overwrite [default is to skip]\n",
				            Ctrl->I.c_abbrev, list[argno]);
				MGD77_Free_Dataset (GMT, &D);	/* Free memory already allocated by MGD77_Read_File for this aborted effort */
				if (tmp_string) gmt_M_free (GMT, tmp_string);
				continue;
			}
			n_dims = (D->H.info[In.order[column].set].col[In.order[column].item].constant) ? 0 : 1;
			if (D->H.info[In.order[column].set].col[In.order[column].item].text) n_dims++;
		}

		if (Ctrl->D.active) {	/* Must create a new file with everything except the fields to be deleted */
			int id, c;
			bool reset_column = false;
			char oldfile[PATH_MAX+4] = {""};

			if (column != MGD77_NOT_SET) {	/* Get info about this existing column to see if it is compatible with new data */
				n_dims = (D->H.info[In.order[column].set].col[In.order[column].item].constant) ? 0 : 1;
				if (D->H.info[In.order[column].set].col[In.order[column].item].text) n_dims++;
			}

			pos = 0; n_delete = 0;
			(void) time (&now);
			sprintf (history, "%s [%s] removed columns", ctime(&now), In.user);
			for (i = 0; history[i]; i++) if (history[i] == '\n') history[i] = ' ';	/* Remove the \n returned by ctime() */
			while ((gmt_strtok (Ctrl->D.file, ",", &pos, p))) {	/* For each named column */
				k = MGD77_Get_Column (GMT, p, &In);
				if (k == MGD77_NOT_SET) {
					GMT_Report (API, GMT_MSG_ERROR, "No column named %s in %s - cannot delete it. \n", p, list[argno]);
					continue;
				}
				c = In.order[k].set;
				id = In.order[k].item;
				D->H.info[c].col[id].abbrev[0] = D->H.info[c].col[id].name[0] = D->H.info[c].col[id].units[0] = D->H.info[c].col[id].comment[0] = '\0';
				D->H.info[c].col[id].pos = D->H.info[c].col[id].var_id = MGD77_NOT_SET;
				D->H.info[c].bit_pattern = 0;
				D->H.info[c].col[id].present = false;
				D->H.info[c].n_col--;
				D->H.n_fields--;
				D->n_fields--;
				In.n_out_columns--;
				for (col = k; col < In.n_out_columns; col++) {	/* Move remaining columns over */
					D->values[col] = D->values[col+1];
					gmt_M_str_free (In.desired_column[col]);	/* Need to free and realloc because next var name might be longer */
					In.desired_column[col] = strdup(In.desired_column[col+1]);
					In.order[col].set = In.order[col+1].set;
					In.order[col].item = In.order[col+1].item;
				}
				strcat (history, " ");
				strcat (history, p);
				n_delete++;
				GMT_Report (API, GMT_MSG_ERROR, "Removed column %s in %s\n", p, list[argno]);
				if (k == column && c == set) {	/* Just removed the old column by the same name, must unset column */
					reset_column = true;
				}
			}

			/* Rename the old file for now */

			sprintf (oldfile, "%s.old", In.path);
			if (gmt_rename_file (GMT, In.path, oldfile, GMT_RENAME_FILE)) {
				MGD77_Free_Dataset (GMT, &D);
				if (tmp_string) gmt_M_free (GMT, tmp_string);
				Return (GMT_RUNTIME_ERROR);
			}

			/* Update header history */

			k = (int)strlen (history);
			for (i = 0; i < k; i++) if (history[i] == '\n') history[i] = ' ';	/* Remove the \n returned by ctime() */
			history[k++] = '\n';	history[k] = '\0';				/* Add LF at end of line */
			k += (int)strlen (D->H.history);
			D->H.history = gmt_M_memory (GMT, D->H.history, k+1, char);
			strcat (D->H.history, history);		/* MGD77_Write_FILE_cdf will use this to create the history attribute, thus preserving earlier history */

			if (MGD77_Write_File (GMT, In.path, &In, D)) {	/* Create the new, slimmer file */
				GMT_Report (API, GMT_MSG_ERROR, "Failure while writing slimmer version of %s\n", list[argno]);
				MGD77_Free_Dataset (GMT, &D);
				if (tmp_string) gmt_M_free (GMT, tmp_string);
				Return (GMT_DATA_WRITE_ERROR);
			}

			/* Now we can safely remove the old file */

			if (gmt_remove_file (GMT, oldfile))	{
				GMT_Report (API, GMT_MSG_ERROR, "Failure while removing the old version of %s\n", list[argno]);
				MGD77_Free_Dataset (GMT, &D);
				if (tmp_string) gmt_M_free (GMT, tmp_string);
				Return (GMT_RUNTIME_ERROR);
			}

			MGD77_Free_Dataset (GMT, &D);
			if (column == MGD77_NOT_SET) continue;	/* Nothing more to do for this file */

			/* Now reread header etc since things have changed in the file */

			In.n_out_columns = 0;
			D = MGD77_Create_Dataset (GMT);
			if (MGD77_Read_File (GMT, list[argno], &In, D)) {
				GMT_Report (API, GMT_MSG_ERROR, "Failure while reading data set for cruise %s\n", list[argno]);
				MGD77_Free_Dataset (GMT, &D);
				Return (GMT_DATA_READ_ERROR);
			}
			if (reset_column)
				column = MGD77_NOT_SET;
			else
				n_changed++;
		}

		if (c_kind == ADD_IGRF) {	/* Append IGRF column */
			int ix, iy, it;
			double date, *xvar = NULL, *yvar = NULL, *tvar = NULL, IGRF[7];

			if ((ix = mgd77manage_skip_if_missing (GMT, "lon",  list[argno], &In, &D)) == MGD77_NOT_SET) continue;
			if ((iy = mgd77manage_skip_if_missing (GMT, "lat",  list[argno], &In, &D)) == MGD77_NOT_SET) continue;
			if ((it = mgd77manage_skip_if_missing (GMT, "time", list[argno], &In, &D)) == MGD77_NOT_SET) continue;

			xvar = D->values[ix];
			yvar = D->values[iy];
			tvar = D->values[it];
			colvalue = gmt_M_memory (GMT, NULL, D->H.n_records, double);

			for (rec = n_sampled = 0; rec < D->H.n_records; rec++) {
				date = MGD77_time_to_fyear (GMT, &In, tvar[rec]);	/* Get date as decimal year */
				colvalue[rec] = (MGD77_igrf10syn (GMT, 0, date, 1, 0.0, xvar[rec], yvar[rec], IGRF)) ? GMT->session.d_NaN : IGRF[MGD77_IGRF_F];
				n_sampled++;
			}
			GMT_Report (API, GMT_MSG_INFORMATION, "Estimated IGRF at %d locations out of %d for cruise %s\n", n_sampled, D->H.n_records, list[argno]);
		}
		else if (c_kind == ADD_GRAV) {	/* Append IGF column */
			int ix, iy, use;
			double *xvar = NULL, *yvar = NULL;

			if ((ix = mgd77manage_skip_if_missing (GMT, "lon", list[argno], &In, &D)) == MGD77_NOT_SET) continue;
			if ((iy = mgd77manage_skip_if_missing (GMT, "lat", list[argno], &In, &D)) == MGD77_NOT_SET) continue;

			if (GF_version == MGD77_NOT_SET) {
				use = (In.original) ? MGD77_ORIG : MGD77_REVISED;
				GF_version = D->H.mgd77[use]->Gravity_Theoretical_Formula_Code - '0';
				if (GF_version < MGD77_IGF_HEISKANEN || GF_version > MGD77_IGF_1980) {
					GMT_Report (API, GMT_MSG_WARNING, "Invalid Gravity Theoretical Formula Code (%c) - default to %d\n",
					            D->H.mgd77[use]->Gravity_Theoretical_Formula_Code, MGD77_IGF_1980);
					GF_version = MGD77_IGF_1980;
				}
			}
			xvar = D->values[ix];
			yvar = D->values[iy];
			colvalue = gmt_M_memory (GMT, NULL, D->H.n_records, double);

			for (rec = 0; rec < D->H.n_records; rec++) colvalue[rec] = MGD77_Theoretical_Gravity (GMT, xvar[rec], yvar[rec], GF_version);
			GMT_Report (API, GMT_MSG_INFORMATION, "Estimated IGRF at %d locations out of %d for cruise %s\n",
			            D->H.n_records, D->H.n_records, list[argno]);
		}
		else if (c_kind == ADD_CARTER) {	/* Append Carter correction column */
			int ix, iy, it;
			double *xvar = NULL, *yvar = NULL, *tvar = NULL;

			if ((ix = mgd77manage_skip_if_missing (GMT, "lon", list[argno], &In, &D)) == MGD77_NOT_SET) continue;
			if ((iy = mgd77manage_skip_if_missing (GMT, "lat", list[argno], &In, &D)) == MGD77_NOT_SET) continue;
			if ((it = mgd77manage_skip_if_missing (GMT, "twt", list[argno], &In, &D)) == MGD77_NOT_SET) continue;

			xvar = D->values[ix];
			yvar = D->values[iy];
			tvar = D->values[it];
			colvalue = gmt_M_memory (GMT, NULL, D->H.n_records, double);

			for (rec = 0; rec < D->H.n_records; rec++) colvalue[rec] = MGD77_carter_correction (GMT, xvar[rec], yvar[rec], 1000.0 * tvar[rec], &Carter);
			GMT_Report (API, GMT_MSG_INFORMATION, "Estimated IGRF at %d locations out of %d for cruise %s\n", D->H.n_records, D->H.n_records, list[argno]);
		}
		else if (c_kind == ADD_RMAG) {	/* Append recomputed residual mag column */
			int ix, iy, it, im;
			double date, *xvar = NULL, *yvar = NULL, *tvar = NULL, *mvar = NULL, IGRF[7];
			char field[5] = {""};

			if ((ix = mgd77manage_skip_if_missing (GMT, "lon",  list[argno], &In, &D)) == MGD77_NOT_SET) continue;
			if ((iy = mgd77manage_skip_if_missing (GMT, "lat",  list[argno], &In, &D)) == MGD77_NOT_SET) continue;
			if ((it = mgd77manage_skip_if_missing (GMT, "time", list[argno], &In, &D)) == MGD77_NOT_SET) continue;
			sprintf (field, "mtf%d", MTF_col);
			if ((im = mgd77manage_skip_if_missing (GMT, field, list[argno], &In, &D)) == MGD77_NOT_SET) continue;

			xvar = D->values[ix];
			yvar = D->values[iy];
			tvar = D->values[it];
			mvar = D->values[im];
			colvalue = gmt_M_memory (GMT, NULL, D->H.n_records, double);

			for (rec = n_sampled = 0; rec < D->H.n_records; rec++) {
				date = MGD77_time_to_fyear (GMT, &In, tvar[rec]);	/* Get date as decimal year */
				check = MGD77_igrf10syn (GMT, 0, date, 1, 0.0, xvar[rec], yvar[rec], IGRF);
				colvalue[rec] = (check) ? GMT->session.d_NaN : mvar[rec] - IGRF[MGD77_IGRF_F];
				n_sampled++;
			}
			GMT_Report (API, GMT_MSG_INFORMATION, "Estimated recomputed magnetic anomaly at %d locations out of %d for cruise %s\n",
			            n_sampled, D->H.n_records, list[argno]);
		}
		else if (got_grid) {	/* Sample grid along track (or Mercator-projected) track */
			int ix, iy;
			double *xvar = NULL, *yvar = NULL;
			struct GMT_GRID_HEADER_HIDDEN *HH = gmt_get_H_hidden (G->header);

			if ((ix = mgd77manage_skip_if_missing (GMT, "lon", list[argno], &In, &D)) == MGD77_NOT_SET) continue;
			if ((iy = mgd77manage_skip_if_missing (GMT, "lat", list[argno], &In, &D)) == MGD77_NOT_SET) continue;

			xvar = D->values[ix];
			yvar = D->values[iy];
			colvalue = gmt_M_memory (GMT, NULL, D->H.n_records, double);

			for (rec = n_sampled = 0; rec < D->H.n_records; rec++) {
				colvalue[rec] = GMT->session.d_NaN;	/* In case we are outside grid */

				/* If point is outside grd area, shift it using periodicity or skip if not periodic. */

				if (Ctrl->A.mode == MODE_i)	/* Mercator IMG grid */
					gmt_geo_to_xy (GMT, xvar[rec], yvar[rec], &x, &y);
				else {		/* Regular geographic grd */
					x = xvar[rec];
					y = yvar[rec];
				}
				if (y < G->header->wesn[YLO] || y > G->header->wesn[YHI]) continue;

				while ((x < G->header->wesn[XLO]) && (HH->nxp > 0)) x += (G->header->inc[GMT_X] * HH->nxp);
				if (x < G->header->wesn[XLO]) continue;

				while ((x > G->header->wesn[XHI]) && (HH->nxp > 0)) x -= (G->header->inc[GMT_X] * HH->nxp);
				if (x > G->header->wesn[XHI]) continue;

				if (interpolate) {	/* IMG has been corrected, and GRD is good to go */
					colvalue[rec] = gmt_bcr_get_z (GMT, G, x, y);
				}
				else {	/* Take IMG nearest node and do special stuff (values already set during read) */
					col = (unsigned int)gmt_M_grd_x_to_col (GMT, x, G->header);
					row = (unsigned int)gmt_M_grd_y_to_row (GMT, y, G->header);
					colvalue[rec] = G->data[gmt_M_ijp(G->header,row,col)];
				}
				n_sampled++;
			}
			GMT_Report (API, GMT_MSG_INFORMATION, "Sampled grid at %d locations out of %d for cruise %s\n", n_sampled, D->H.n_records, list[argno]);
		}
		else if (Ctrl->A.mode == MODE_a) {	/* Just got a single column to paste in, assuming the row numbers match */
			if (n != D->H.n_records) {
				GMT_Report (API, GMT_MSG_ERROR, "Extra column data records (%d) do not match # of cruise records (%d) for %s\n",
				            n, D->H.n_records, list[argno]);
				Return (GMT_RUNTIME_ERROR);
			}
			GMT_Report (API, GMT_MSG_INFORMATION, "Appended column data for all %d records for cruise %s\n", D->H.n_records, list[argno]);
		}
		else if (Ctrl->A.mode == MODE_d || Ctrl->A.mode == MODE_n || Ctrl->A.mode == MODE_t) {	/* Got either (time,data) or (dist,data) */
			int ix, iy, it;
			double *x = NULL, *y = NULL, *d = NULL;
			size_t LEN_size = LEN;
			colvalue = gmt_M_memory (GMT, colvalue, D->H.n_records, double);
			if (Ctrl->A.mode == MODE_d) {	/* Must create distances in user's units */
				if ((ix = mgd77manage_skip_if_missing (GMT, "lon", list[argno], &In, &D)) == MGD77_NOT_SET) continue;
				if ((iy = mgd77manage_skip_if_missing (GMT, "lat", list[argno], &In, &D)) == MGD77_NOT_SET) continue;
				x = D->values[ix];
				y = D->values[iy];
				if ((d = gmt_dist_array_2 (GMT, x, y, D->H.n_records, dist_scale, GMT->common.h.mode)) == NULL) {
					error = gmt_M_err_fail (GMT, GMT_MAP_BAD_DIST_FLAG, "");
					Return (error);
				}
				x = d;
			}
			else if (Ctrl->A.mode == MODE_t) {	/* Time */
				if ((it = mgd77manage_skip_if_missing (GMT, "time", list[argno], &In, &D)) == MGD77_NOT_SET) continue;
				x = D->values[it];
			}
			if (Ctrl->A.interpolate) {	/* Using given table to interpolate the values at all mgd77 records */
				y = gmt_M_memory (GMT, NULL, D->H.n_records, double);
				result = gmt_intpol (GMT, coldnt, colvalue, NULL, n, D->H.n_records, x, y, 0.0, GMT->current.setting.interpolant);
				if (result != 0) {
					GMT_Report (API, GMT_MSG_ERROR, "Failure in gmt_intpol near row %d!\n", result+1);
				Return (GMT_RUNTIME_ERROR);
				}
				gmt_M_memcpy (colvalue, y, D->H.n_records, double);
				gmt_M_free (GMT, y);
			}
			else if (strings && n < D->H.n_records) {	/* Only update the exact matching records */
				text = gmt_M_memory (GMT, NULL, D->H.n_records * LEN_size, char);
				for (rec = jrec = n_sampled = 0; rec < D->H.n_records && jrec < n; rec++) {
					match_value = (Ctrl->A.mode == MODE_n) ? rec+1 : x[rec];
					strncpy (&text[rec*LEN_size], not_given, LEN_size);	/* In case we have no data at this time */
					while (coldnt[rec] < match_value && jrec < n) jrec++;
					if (coldnt[jrec] == match_value) {
						strncpy (&text[rec*LEN_size], tmp_string[jrec], LEN_size);
						n_sampled++;
					}
				}
				gmt_M_free (GMT, tmp_string);
				GMT_Report (API, GMT_MSG_INFORMATION, "Appended column data for %d locations out of %d for cruise %s\n",
				            n_sampled, D->H.n_records, list[argno]);
			}
			else if (strings) {	/* One to one match */
				text = gmt_M_memory (GMT, NULL, D->H.n_records * LEN_size, char);
				for (rec = 0; rec < n; rec++) strncpy (&text[rec*LEN_size], tmp_string[rec], LEN_size);
				gmt_M_free (GMT, tmp_string);
				GMT_Report (API, GMT_MSG_INFORMATION, "Appended column data for %d locations out of %d for cruise %s\n",
				            n_sampled, D->H.n_records, list[argno]);
			}
			else {	/* Only update the exact matching records */
				y = gmt_M_memory (GMT, NULL, D->H.n_records, double);
				for (rec = jrec = n_sampled = 0; rec < D->H.n_records && jrec < n; rec++) {
					match_value = (Ctrl->A.mode == MODE_n) ? rec+1 : x[rec];
					y[rec] = GMT->session.d_NaN;	/* In case we have no data at this time */
					while (coldnt[jrec] < match_value && jrec < n) jrec++;
					if (coldnt[jrec] == match_value) {	/* Found our guy */
						y[rec] = colvalue[jrec];
						n_sampled++;
					}
				}
				gmt_M_memcpy (colvalue, y, D->H.n_records, double);
				GMT_Report (API, GMT_MSG_INFORMATION, "Appended column data for %d locations out of %d for cruise %s\n",
				            n_sampled, D->H.n_records, list[argno]);
				gmt_M_free (GMT, y);
			}
			if (Ctrl->A.mode == MODE_d) gmt_M_free (GMT, d);
		}
		else if (Ctrl->A.mode == MODE_e)
		{
			/* Read any header scale/offset factors to be set.
			 * Decode error flags to give bit flag, store in colvalue
			 */
			FILE *fp_e = NULL;
			int cdf_var_id, cdf_adjust;
			char ID[16] = {""}, date[16] = {""}, field[GMT_LEN64] = {""}, efile[PATH_MAX] = {""};
			char E77[256] = {""}, timestamp[GMT_LEN64] = {""}, answer[GMT_BUFSIZ] = {""}, code[GMT_BUFSIZ] = {""}, kind, YorN;
			int number, type, it, id, key, n_E77_flags, day, month, year, item;
			int n_E77_headers, n_E77_scales, n_E77_offsets, n_E77_recalcs, n_unprocessed, e_error = 0, old_flags;
			uint64_t n_recs, rec, from, to;
			unsigned int *flags = NULL, pattern;
			size_t length;
			bool has_time;
			struct MGD77_HEADER_PARAMS *P = NULL;
			double rec_time, del_t, value, *tvar = NULL;

			if (D->H.E77 && strlen(D->H.E77) > 0 && !Ctrl->A.replace) {
				GMT_Report (API, GMT_MSG_ERROR, "E77 corrections are already present in %s.  use -A+e to overwrite with new corrections\n", list[argno]);
				MGD77_Free_Dataset (GMT, &D);	/* Free memory allocated by MGD77_Read_File for this aborted effort */
				continue;
			}

			sprintf (efile, "%s.e77", list[argno]);
			if ((fp_e = gmt_fopen (GMT, efile, "r")) == NULL) {	/* Not in current directory, try MGD77_HOME/E77 */
				sprintf (efile, "%s/E77/%s.e77", In.MGD77_HOME, list[argno]);
				if ((fp_e = gmt_fopen (GMT, efile, "r")) == NULL) {	/* Not here either */
					GMT_Report (API, GMT_MSG_ERROR, "The file %s.e77 could not be found in current directory or in MGD77_HOME/E77 - skipped\n", list[argno]);
					MGD77_Free_Dataset (GMT, &D);	/* Free memory allocated by MGD77_Read_File for this aborted effort */
					continue;
				}

			}
			/* We will first do many checks to make sure this E77 file goes with the specified cruise and that
			 * all the verification steps has been taken to make this E77 a correction file
			 */

			P = D->H.mgd77[MGD77_ORIG];	/* Because E77 is absolute and not incremental we start from original settings */
			if (!gmt_fgets (GMT, line, GMT_BUFSIZ, fp_e)) {
				GMT_Report (API, GMT_MSG_ERROR, "Could not read record #1 from %s.e77 - aborting\n", list[argno]);
				e_error++;
			}
			sscanf (&line[1], "%*s %s %*s %*s %*s %*s %*s %s %*s %" SCNu64, ID, date, &n_recs);
			if (strcmp (In.NGDC_id, ID)) {
				GMT_Report (API, GMT_MSG_ERROR, "E77 Conflict %s : ID = %s versus %s - aborting\n", efile, ID, In.NGDC_id);
				e_error++;
			}
			/* Make sure the File creation dates from the data file and the E77 match */
			day = atoi (&date[6]);
			date[6] = 0;
			month = atoi (&date[4]);
			date[4] = 0;
			year = atoi (date);

			if (!(year == atoi (P->File_Creation_Year) && month == atoi (P->File_Creation_Month) && day == atoi (P->File_Creation_Day))) {
				GMT_Report (API, GMT_MSG_ERROR, "E77 Conflict %s: File Creation Date: %s versus %s%s%s - aborting\n", efile, date,
					P->File_Creation_Year, P->File_Creation_Month, P->File_Creation_Day);
				e_error++;
			}
			if (n_recs != D->H.n_records) {
				GMT_Report (API, GMT_MSG_ERROR, "E77 Conflict %s: n_recs = %d versus %d = aborting\n", efile, n_recs, D->H.n_records);
				e_error++;
			}
			verified = false;
			while (gmt_fgets (GMT, line, GMT_BUFSIZ, fp_e) && strncmp (line, "# Errata: Header", 14U)) {	/* Read until we get to Header record section */
				if (line[0] == '#') continue;	/* Skip comments */
				gmt_chop (line);		/* Rid the world of CR/LF */
				if (!strncmp (line, "Y Errata table verification status", 34U)) verified = true;
			}
			if (!verified && !Ctrl->A.ignore_verify) {
				GMT_Report (API, GMT_MSG_ERROR, "E77 file %s not yet verified.  E77 not applied\n", efile);
				e_error++;
			}

			if (e_error) {
				GMT_Report (API, GMT_MSG_ERROR, "The file %s has too many errors.  E77 not applied\n", efile);
				MGD77_Free_Dataset (GMT, &D);	/* Free memory allocated by MGD77_Read_File for this aborted effort */
				continue;
			}

			/* OK, we got this far so the meta data for this E77 file seems OK */

			/* Quickly scan through file to make sure there are no unprocessed recommendations or bad records before making changes */

			e_error = n_unprocessed = 0;
			while (gmt_fgets (GMT, line, GMT_BUFSIZ, fp_e)) {
				if (line[0] == '#' || line[0] == '\n') continue;	/* Comments or blank lines are OK */
				if (line[1] == '-') {		/* Header record */
					if (!(line[0] == 'Y' || line[0] == 'N') && !Ctrl->A.ignore_verify) {		/* Unprocessed recommendation? */
						GMT_Message (API, GMT_TIME_NONE, "%s: UNDECIDED: %s\n", list[argno], line);
						if (line[0] == '?') n_unprocessed++;
						e_error++;
					}
					sscanf (line, "%c-%c-%[^-]-%[^-]-%d", &YorN, &kind, ID, field, &item);
				}
				else				/* Data record */
					sscanf (line, "%c %s %s %" SCNu64 " %s", &YorN, ID, timestamp, &rec, code);
				if (strcmp (In.NGDC_id, ID)) {
					GMT_Report (API, GMT_MSG_ERROR, "E77 Conflict %s : ID = %s versus %s in header records!\n", efile, ID, In.NGDC_id);
					e_error++;
				}
			}

			if (e_error) {
				GMT_Report (API, GMT_MSG_ERROR, "The file %s has too many errors.  E77 not applied\n", efile);
				gmt_fclose (GMT, fp_e);
				MGD77_Free_Dataset (GMT, &D);	/* Free memory allocated by MGD77_Read_File for this aborted effort */
				continue;
			}
			if (n_unprocessed) {
				GMT_Report (API, GMT_MSG_ERROR, "The file %s has unprocessed E77 recommendations.  E77 not applied\n", efile);
				gmt_fclose (GMT, fp_e);
				MGD77_Free_Dataset (GMT, &D);	/* Free memory allocated by MGD77_Read_File for this aborted effort */
				continue;
			}

			/* OK, here we believe the E77 file contains the correct information for this cruise. Rewind and start from top */

			gmt_M_rewind (fp_e);
			while (gmt_fgets (GMT, line, GMT_BUFSIZ, fp_e) && strncmp (line, "# Errata: Header", 14U));	/* Read until we get to Header record section */

			flags = gmt_M_memory (GMT, NULL, D->H.n_records, unsigned int);
			n_E77_flags = n_E77_headers = n_E77_scales = n_E77_offsets = n_E77_recalcs = 0;

			MGD77_nc_status (GMT, gmt_nc_open (GMT, In.path, NC_WRITE, &In.nc_id));	/* Open the file */
			MGD77_nc_status (GMT, nc_redef (In.nc_id));				/* Enter define mode */
			(void)MGD77_Remove_E77 (GMT, &In);				/* Remove any previously revised header parameters */
			while (gmt_fgets (GMT, line, GMT_BUFSIZ, fp_e) && strncmp (line, "# Errata: Data", 14U)) {	/* Read until we get to data record section */
				if (line[0] == '#' || line[0] == '\n') continue;	/* Skip comments */
				gmt_chop (line);					/* Rid the world of CR/LF */
				/* Example of expected line
				   Y-E-06050010-H15-01: Invalid Gravity Departure Base Station Value: (0000000) [1000009]
				*/
				sscanf (line, "%c-%c-%[^-]-%[^-]-%d", &YorN, &kind, ID, field, &item);
				if (strcmp (In.NGDC_id, ID)) {
					GMT_Report (API, GMT_MSG_ERROR, "E77 Conflict %s : ID = %s versus %s in header records - skipped\n", efile, ID, In.NGDC_id);
					e_error++;
					continue;
				}
				if (field[0] == 'H') {
					type = E77_HEADER_MODE;
					number = atoi (&field[1]);
				}
				else {
					type = 1;
					number = item;
				}
				if (Ctrl->A.e77_skip_mode[type]) continue;
				if (!Ctrl->A.e77_skip_mode[type] && YorN == 'N') continue;
				if (kind == 'W') {	/* Output the warning (if Y) and goto next line*/
					if (gmt_M_is_verbose (GMT, GMT_MSG_WARNING) && (YorN == 'Y' || (Ctrl->A.ignore_verify && YorN == '?')))
						GMT_Message (API, GMT_TIME_NONE, "%s: %s\n", list[argno], line);
					continue;
				}
				if (!mgd77manage_got_default_answer (line, answer)) continue;

				/* Here we must do something */

				if (type == E77_HEADER_MODE) {	/* Header meta data fixes */

					key = MGD77_Param_Key (GMT, number, (int)item);	/* Returns -ve if sequence not found or item not found, >=0 otherwise */

					switch (key) {
						case MGD77_BAD_HEADER_RECNO:
							GMT_Report (API, GMT_MSG_WARNING, "Sequence number %d is outside range - skipped\n", number);
							break;
						case MGD77_BAD_HEADER_ITEM:
							GMT_Report (API, GMT_MSG_WARNING, "Sequence number %d, Item %d is not supported - skipped\n", number, item);
							break;
						default:	/* Update internal structure as well as netCDF file attributes */
							length = (MGD77_Header_Lookup[key].length == 1) ? 1 : strlen (answer);
							strncpy (MGD77_Header_Lookup[key].ptr[MGD77_REVISED], answer, length);
							MGD77_Put_Param (GMT, &In, MGD77_Header_Lookup[key].name, length, MGD77_Header_Lookup[key].ptr[MGD77_ORIG],
							                 length, MGD77_Header_Lookup[key].ptr[MGD77_REVISED], 2);
							n_E77_headers++;
							break;
					}
				}
				else {			/* Systematic fixes */
					if ((id = MGD77_Get_Column (GMT, field, &In)) == MGD77_NOT_SET) {
						GMT_Report (API, GMT_MSG_WARNING, "Correction found for %s which is not in this cruise?\n", field);
					}
					else {
						(void) MGD77_Info_from_Abbrev (GMT, field, &(D->H), &set, &item);
						value = atof (answer);
						switch (number) {
							case E77_HDR_PDR:	/* Must deal with undetected Precision Depth Recorder wrap-arounds - this also force recalc of depth when data is read*/
								MGD77_nc_status (GMT, nc_put_att_double (In.nc_id, NC_GLOBAL, "PDR_wrap", NC_DOUBLE, 1U, &value));
								cdf_adjust = MGD77_COL_ADJ_TWT;
								MGD77_nc_status (GMT, nc_put_att_int (In.nc_id, D->H.info[set].col[id].var_id, "adjust", NC_INT, 1U, &cdf_adjust));
								n_E77_recalcs++;
								if ((id = MGD77_Get_Column (GMT, "depth", &In)) == MGD77_NOT_SET) {
									GMT_Report (API, GMT_MSG_WARNING, "Correction implied for %s which is not in this cruise?\n", field);
									break;
								}
								/* Intentionally fall through - we want to fall through and also set depth adjustment */
							case E77_HDR_CARTER:	/* Recalculate Carter depth from twt */
								cdf_adjust = MGD77_COL_ADJ_DEPTH;
								MGD77_nc_status (GMT, nc_put_att_int (In.nc_id, D->H.info[set].col[id].var_id, "adjust", NC_INT, 1U, &cdf_adjust));
								n_E77_recalcs++;
								break;
							case E77_HDR_ANOM_MAG:	/* Recalculate anomaly mag as mtf1 - igrf */
								cdf_adjust = MGD77_COL_ADJ_MAG;
								MGD77_nc_status (GMT, nc_put_att_int (In.nc_id, D->H.info[set].col[id].var_id, "adjust", NC_INT, 1U, &cdf_adjust));
								n_E77_recalcs++;
								break;
							case E77_HDR_ANOM_FAA:	/* Recalculate anomaly faa as gobs - igf */
								cdf_adjust = MGD77_COL_ADJ_FAA;
								MGD77_nc_status (GMT, nc_put_att_int (In.nc_id, D->H.info[set].col[id].var_id, "adjust", NC_INT, 1U, &cdf_adjust));
								n_E77_recalcs++;
								break;
							case E77_HDR_ANOM_FAA_EOT:	/* Recalculate anomaly faa as gobs - igf + eot */
								cdf_adjust = MGD77_COL_ADJ_FAA_EOT;
								MGD77_nc_status (GMT, nc_put_att_int (In.nc_id, D->H.info[set].col[id].var_id, "adjust", NC_INT, 1U, &cdf_adjust));
								n_E77_recalcs++;
								break;
							case E77_HDR_SCALE:	/* Correction scale factor */
								if (D->H.info[set].col[id].corr_factor == 1.0) {	/* Must add a new attribute to the file */
									D->H.info[set].col[id].corr_factor = value;
									MGD77_nc_status (GMT, nc_put_att_double (In.nc_id, D->H.info[set].col[id].var_id, "corr_factor", NC_DOUBLE,
									                 1U, &D->H.info[set].col[id].corr_factor));
								}
								n_E77_scales++;
								break;
							case E77_HDR_DCSHIFT:	/* Correction offset */
								if (D->H.info[set].col[id].corr_offset == 0.0) {	/* Must add a new attribute to the file */
									D->H.info[set].col[id].corr_offset = value;
									MGD77_nc_status (GMT, nc_put_att_double (In.nc_id, D->H.info[set].col[id].var_id, "corr_offset", NC_DOUBLE,
									                 1U, &D->H.info[set].col[id].corr_offset));
								}
								n_E77_offsets++;
								break;
							case E77_HDR_GRID_OFFSET:	/* Range of bad values - set flags to BAD  */
							case E77_HDR_FLAGRANGE:		/* Range of bad values - set flags to BAD  */
								sscanf (answer, "%" SCNu64 "-%" SCNu64, &from, &to);
								if (from < 1 || from > D->H.n_records || to < 1 || to > D->H.n_records || to < from) {
									GMT_Report (API, GMT_MSG_ERROR, "Record range %s is invalid.  Correction skipped\n", answer);
									break;
								}
								pattern = mgd77_set_bit (id);
 								for (rec = from-1; rec < to; rec++, n_E77_flags++) flags[rec] |= pattern;	/* -1 to get C indices */
								break;
							default:
								break;
						}
					}
				}
			}
			/* Now start on data record section */
			if ((it = mgd77manage_skip_if_missing (GMT, "time", list[argno], &In, &D)) == MGD77_NOT_SET)
				has_time = false;
			else {	/* See if we really have time or if they are all NaN */
				tvar = D->values[it];
				for (rec = 0, has_time = false; !has_time && rec < D->H.n_records; rec++) if (!gmt_M_is_dnan (tvar[rec])) has_time = true;
			}
			while (gmt_fgets (GMT, line, GMT_BUFSIZ, fp_e)) {	/* Read until EOF */
				sscanf (line, "%c %s %s %" SCNu64 " %s", &YorN, ID, timestamp, &rec, code);
				if (strcmp (In.NGDC_id, ID)) {
					GMT_Report (API, GMT_MSG_ERROR, "E77 Conflict %s : ID = %s versus %s in data records - skipped\n", efile, ID, In.NGDC_id);
					e_error++;
					continue;
				}
				if (YorN == 'N') continue;			/* Already decided NOT to use this correction */
				if (YorN == '?' && !Ctrl->A.ignore_verify) {		/* Undecided: Output the warning and goto next line unless we ignore verification */
					GMT_Message (API, GMT_TIME_NONE, "%s UNDECIDED: %s\n", list[argno], line);
					continue;
				}
				/* Here, YorN is 'Y' (or '?' if Ctrl->A.ignore_verify is true) */
				rec--;	/* E77 starts with rec = 1 for first data record */
				if (has_time) {
					if (!strcmp(timestamp,"NaN")) {
						GMT_Report (API, GMT_MSG_WARNING, "%s: E77 time stamp %s, using recno\n", ID, timestamp);
					}
					else {	/* Must try to interpret the timestamp */
						if (gmt_verify_expectations (GMT, GMT_IS_ABSTIME, gmt_scanf (GMT, timestamp, GMT_IS_ABSTIME, &rec_time), timestamp)) {
							GMT_Report (API, GMT_MSG_ERROR, "%s: E77 time stamp (%s) in wrong format? - skipped\n", ID, timestamp);
							continue;
						}
						del_t = fabs (tvar[rec] - rec_time);
						if (del_t > (0.06 + GMT_CONV8_LIMIT)) {	/* 0.06 is finest time step in MGD77 file so we allow that much slop */
							GMT_Report (API, GMT_MSG_ERROR, "%s: E77 time stamp and record number do not match record time (del_t = %g s) - skipped\n", ID, del_t);
							continue;
						}
					}
				}
				pos = 0;
				item = -1;	/* So we increment to 0 inside the loop */
				while (gmt_strtok (code, "-", &pos, p)) {
					item++;
					if (Ctrl->A.e77_skip_mode[item+2]) continue;	/* Ignore this sort of code */
					if (p[0] == '0') continue;
					for (k = 0; k < (int)strlen(p); k++) {	/* Loop over one or more codes */

						if (item == 0) {	/* NAV */
							switch (p[k]) {
								case 'A':	/* Time out of range */
									flags[rec] |= mgd77_set_bit(NCPOS_TIME);
									n_E77_flags++;
									break;
								case 'B':
									GMT_Report (API, GMT_MSG_WARNING, "%s: Decreasing time %s - Source Institution need to sort records\n",
									            list[argno], timestamp);
									break;
								case 'C':	/* Excessive speed - flag time, lon, lat */
									flags[rec] |= mgd77_set_bit(NCPOS_TIME);
									flags[rec] |= mgd77_set_bit(NCPOS_LON);
									flags[rec] |= mgd77_set_bit(NCPOS_LAT);
									n_E77_flags++;
									break;
								case 'D':	/* On land */
									flags[rec] |= mgd77_set_bit(NCPOS_LON);
									flags[rec] |= mgd77_set_bit(NCPOS_LAT);
									n_E77_flags++;
									break;
								case 'E':	/* Undefined nav - flag time, lon, lat */
									flags[rec] |= mgd77_set_bit(NCPOS_TIME);
									flags[rec] |= mgd77_set_bit(NCPOS_LON);
									flags[rec] |= mgd77_set_bit(NCPOS_LAT);
									n_E77_flags++;
									break;
								default:
									GMT_Report (API, GMT_MSG_ERROR, "%s: Unrecognized NAV code %c - skipped\n", list[argno], p[k]);
									break;
							}
						}
						else if (p[k] < 'A' || p[k] > 'X') {
							GMT_Report (API, GMT_MSG_ERROR, "%s: Unrecognized error field %c - skipped\n", list[argno], p[k]);
						}
						else {			/* EO, RANGE, or SLOPE */
							if (p[k] >= 'A' && p[k] <= 'X')	{ /* Valid codes */
								if (p[k] > 'G')
 									key = p[k] - 'A' - 4;	/* H (lat) = 3, J (ptc) = 5, etc (only expect J-X though) */
								else if (p[k] < 'C')
									key = p[k] - 'A' + 1;	/* A (rectype) = 1, B (TZ) = 2 */
								else
  									key = 0;	/* C-G (yyyy,mm,dd,hh,mi) all map to time 0 */
								flags[rec] |= mgd77_set_bit(key);
								n_E77_flags++;
							}
							else {
								GMT_Report (API, GMT_MSG_ERROR, "%s: Unrecognized error field %c - skipped\n", list[argno], p[k]);
							}
						}
					}
				}
			}
			gmt_fclose (GMT, fp_e);

			/* Update E77 history */

			(void) time (&now);
			sprintf (E77, "%s [%s] E77 corrections applied to header: %d scale: %d offset: %d recalc: %d flags: %d",
			         ctime(&now), In.user, n_E77_headers, n_E77_scales, n_E77_offsets, n_E77_recalcs, n_E77_flags);
			for (i = 0; E77[i]; i++) if (E77[i] == '\n') E77[i] = ' ';	/* Remove the \n returned by ctime() */
			length = strlen (E77);
			D->H.E77 = gmt_M_memory (GMT, D->H.E77, length + 1, char);
			strcpy (D->H.E77, E77);
			MGD77_nc_status (GMT, nc_put_att_text (In.nc_id, NC_GLOBAL, "E77", length, D->H.E77));

			old_flags =  (nc_inq_varid (In.nc_id, "MGD77_flags", &cdf_var_id) == NC_NOERR);	/* true if flag variable exists already */

			if (n_E77_flags) {	/* Add flags to netCDF file */
				if (old_flags) {	/* Flag variable exists already - simply replace existing flags with the new ones */
					if (D->flags[0])	/* Was allocated and read */
						gmt_M_memcpy (D->flags[0], flags, D->H.n_records, int);
					else	/* Was not allocated */
						D->flags[0] = flags;
				}
				else {	/* We need to define the flags for the first time */
					dims[0] = In.nc_recid;
					MGD77_nc_status (GMT, nc_def_var (In.nc_id, "MGD77_flags", NC_INT, 1, dims, &cdf_var_id));	/* Define an array variable */
					gmt_M_memset (answer, GMT_BUFSIZ, char);	/* No default answer */
					strcpy (answer, "MGD77 flags (ON = Bad, OFF = Good) derived from E77 errata");
					MGD77_nc_status (GMT, nc_put_att_text (In.nc_id, cdf_var_id, "comment", strlen (answer), answer));
					D->flags[0] = flags;
				}
				MGD77_nc_status (GMT, nc_enddef (In.nc_id));	/* End define mode. */
				start[0] = 0;
				count[0] = D->H.n_records;
				MGD77_nc_status (GMT, nc_put_vara_int (In.nc_id, cdf_var_id, start, count, (int *)D->flags[0]));
			}
			else if (old_flags) {	/* Had flags from before which we cannot delete */
				MGD77_nc_status (GMT, nc_enddef (In.nc_id));	/* End define mode. */
				GMT_Report (API, GMT_MSG_WARNING, "File %s contains flags from an earlier E77 but this E77 do not contain any flags.\n", list[argno]);
				GMT_Report (API, GMT_MSG_WARNING, "The flags in the file %s will all be set to zero but cannot be removed.\n", list[argno]);
				GMT_Report (API, GMT_MSG_WARNING, "If possible, recreate the MGD77+ file %s from the MGD77 original, then reapply E77.\n", list[argno]);
				start[0] = 0;
				count[0] = D->H.n_records;
				gmt_M_memset (D->flags[0], D->H.n_records, int);	/* Reset all flags to 0 (GOOD) */
				MGD77_nc_status (GMT, nc_put_vara_int (In.nc_id, cdf_var_id, start, count, (int *)D->flags[0]));
			}

			MGD77_Free_Dataset (GMT, &D);
			MGD77_Close_File (GMT, &In);
			n_changed++;
			continue;	/* Nothing more to do for this file */
		}

		/* Specify the information for the extra column. */

		constant = (LEN == 0) ? MGD77_dbl_are_constant (GMT, colvalue, D->H.n_records, limits) : MGD77_txt_are_constant (GMT, text, D->H.n_records, (int)LEN);	/* Do we need to store 1 or n values? */

		if (column != MGD77_NOT_SET) {	/* Is it possible just to replace the existing column? */
			error = 0;
			if (LEN) {	/* Text data */
				if (OLDLEN != LEN) {
					GMT_Report (API, GMT_MSG_ERROR, "Revised text column %s differs in width (%d) from the old values (%d).\n",
					            Ctrl->I.c_abbrev, (int)LEN, (int)OLDLEN);
					error = true;
				}
				if (constant && n_dims == 2) {
					GMT_Report (API, GMT_MSG_ERROR, "Revised text column %s is constant whereas old values were in an array\n",
					            Ctrl->I.c_abbrev);
					error = true;
				}
				if (!constant && n_dims == 1) {
					GMT_Report (API, GMT_MSG_ERROR, "Revised text column %s is an array whereas old values is a constant\n",
					            Ctrl->I.c_abbrev);
					error = true;
				}
			}
			else {	/* floating-point data */
				if (constant && n_dims == 1) {
					GMT_Report (API, GMT_MSG_ERROR, "Revised data column %s is constant whereas old values were in an array\n",
					            Ctrl->I.c_abbrev);
					error = true;
				}
				if (!constant && n_dims == 0) {
					GMT_Report (API, GMT_MSG_ERROR, "Revised data column %s is an array whereas old values is a constant\n",
					            Ctrl->I.c_abbrev);
					error = true;
				}
			}
			if (error) {
				GMT_Report (API, GMT_MSG_ERROR, "You must first use -D to delete the old information before adding the new information\n");

				MGD77_Free_Dataset (GMT, &D);
				MGD77_Close_File (GMT, &In);
				continue;
			}
		}

		/* OK, here we may either replace an exiting column or add a new one */

		if (MGD77_Open_File (GMT, list[argno], &In, MGD77_UPDATE_MODE)) return (-1);	/* Only creates the full path to the new file */

		MGD77_nc_status (GMT, gmt_nc_open (GMT, In.path, NC_WRITE, &In.nc_id));	/* Open the file */
		MGD77_nc_status (GMT, nc_redef (In.nc_id));				/* Enter define mode */

		dims[0] = In.nc_recid;	dims[1] = LEN;
		start[0] = start[1] = 0;
		count[0] = D->H.n_records;	count[1] = LEN;

		if (column == MGD77_NOT_SET) {	/* Adding a new column */
			if (constant) {	/* Simply store one value */
				if (LEN)	/* Text variable */
					MGD77_nc_status (GMT, nc_def_var (In.nc_id, Ctrl->I.c_abbrev, c_nc_type, 1, &dims[1], &cdf_var_id));	/* Define a single text variable */
				else
					MGD77_nc_status (GMT, nc_def_var (In.nc_id, Ctrl->I.c_abbrev, c_nc_type, 0, NULL, &cdf_var_id));		/* Define a single variable */
			}
			else {	/* Must store array */
				if (LEN)	/* Text variable */
					MGD77_nc_status (GMT, nc_def_var (In.nc_id, Ctrl->I.c_abbrev, c_nc_type, 2, dims, &cdf_var_id));		/* Define a 2-D text variable */
				else
					MGD77_nc_status (GMT, nc_def_var (In.nc_id, Ctrl->I.c_abbrev, c_nc_type, 1, dims, &cdf_var_id));		/* Define a number array variable */
			}
		}
		else	/* Reuse, get id */
			MGD77_nc_status (GMT, nc_inq_varid (In.nc_id, Ctrl->I.c_abbrev, &cdf_var_id));

		if (Ctrl->I.c_name[0])
			MGD77_nc_status (GMT, nc_put_att_text  (In.nc_id, cdf_var_id, "long_name", strlen (Ctrl->I.c_name), Ctrl->I.c_name));
		if (Ctrl->I.c_units[0])
			MGD77_nc_status (GMT, nc_put_att_text (In.nc_id, cdf_var_id, "units", strlen (Ctrl->I.c_units), Ctrl->I.c_units));
		MGD77_nc_status (GMT, nc_put_att_double (In.nc_id, cdf_var_id, "actual_range", NC_DOUBLE, 2U, limits));
		if (Ctrl->I.c_comment[0])
			MGD77_nc_status (GMT, nc_put_att_text (In.nc_id, cdf_var_id, "comment", strlen (Ctrl->I.c_comment), Ctrl->I.c_comment));
		MGD77_nc_status (GMT, nc_put_att_double (In.nc_id, cdf_var_id, "_FillValue", c_nc_type, 1U, &MGD77_NaN_val[c_nc_type]));
		MGD77_nc_status (GMT, nc_put_att_double (In.nc_id, cdf_var_id, "missing_value", c_nc_type, 1U, &MGD77_NaN_val[c_nc_type]));
		if (Ctrl->A.parameters[COL_SCALE]  != 1.0)
			MGD77_nc_status (GMT, nc_put_att_double (In.nc_id, cdf_var_id, "scale_factor", NC_DOUBLE, 1U, &Ctrl->A.parameters[COL_SCALE]));
		if (Ctrl->A.parameters[COL_OFFSET] != 0.0)
			MGD77_nc_status (GMT, nc_put_att_double (In.nc_id, cdf_var_id, "add_offset",   NC_DOUBLE, 1U, &Ctrl->A.parameters[COL_OFFSET]));

		/* Update history */

		(void) time (&now);
		sprintf (history, "%s [%s] Column %s added", ctime(&now), In.user, Ctrl->I.c_abbrev);
		k = (int)strlen (history);
		for (i = 0; i < k; i++) if (history[i] == '\n') history[i] = ' ';	/* Remove the \n returned by ctime() */
		history[k++] = '\n';	history[k] = '\0';    /* Add LF at end of line */
		k += (int)(strlen (D->H.history) + 1);             /* +1 because the '\0' of 'history' that is also copied by strcat */
		D->H.history = gmt_M_memory (GMT, D->H.history, k, char);
		strcat (D->H.history, history);
		MGD77_nc_status (GMT, nc_put_att_text (In.nc_id, NC_GLOBAL, "history", strlen (D->H.history), D->H.history));

		MGD77_nc_status (GMT, nc_enddef (In.nc_id));	/* End define mode.  Now we can write/update data */

		transform = (! (Ctrl->A.parameters[COL_SCALE] == 1.0 && Ctrl->A.parameters[COL_OFFSET] == 0.0));	/* true if we must transform before writing */
		n_bad = 0;
		if (constant) {	/* Simply store one value */
			if (LEN)
				MGD77_nc_status (GMT, nc_put_vara_schar (In.nc_id, cdf_var_id, start, &count[1], (signed char *)text));	/* Just write one text string */
			else {
				n_bad = MGD77_do_scale_offset_before_write (GMT, &single_val, colvalue, 1U, Ctrl->A.parameters[COL_SCALE],
				                                            Ctrl->A.parameters[COL_OFFSET], c_nc_type);
				MGD77_nc_status (GMT, nc_put_var1_double (In.nc_id, cdf_var_id, start, &single_val));
			}
		}
		else {	/* Must store array */
			if (LEN)
				MGD77_nc_status (GMT, nc_put_vara_schar (In.nc_id, cdf_var_id, start, count, (signed char *)text));
			else if (transform) {
				xtmp = gmt_M_memory (GMT, NULL, count[0], double);
				n_bad = MGD77_do_scale_offset_before_write (GMT, xtmp, colvalue, D->H.n_records, Ctrl->A.parameters[COL_SCALE],
				                                            Ctrl->A.parameters[COL_OFFSET], c_nc_type);
				MGD77_nc_status (GMT, nc_put_vara_double (In.nc_id, cdf_var_id, start, count, xtmp));
				gmt_M_free (GMT, xtmp);
			}
			else
				MGD77_nc_status (GMT, nc_put_vara_double (In.nc_id, cdf_var_id, start, count, colvalue));
		}
		if (n_bad) {	/* Report what we found */
			if (In.verbose_level & 1)
				fprintf (fp_err, "%s: %s [%s] had %" PRIu64 " values outside valid range <%g,%g> for the chosen type (set to NaN = %g)\n",
				THIS_MODULE_CLASSIC_NAME, In.NGDC_id, Ctrl->I.c_abbrev, n_bad, MGD77_Low_val[c_nc_type], MGD77_High_val[c_nc_type], MGD77_NaN_val[c_nc_type]);
		}

		MGD77_Close_File (GMT, &In);
		MGD77_Free_Dataset (GMT, &D);
		n_changed++;
		GMT_Report (API, GMT_MSG_INFORMATION, "Data column %s added to %s\n", Ctrl->I.c_abbrev, list[argno]);
	}

	gmt_M_free (GMT, colvalue);
	if (two_cols) gmt_M_free (GMT, coldnt);

	if (Ctrl->D.active)
		GMT_Report (API, GMT_MSG_INFORMATION, "Removed %d data columns from %d MGD77 files\n", n_delete, n_changed);
	else if (Ctrl->A.mode == MODE_e)
		GMT_Report (API, GMT_MSG_INFORMATION, "E77 corrections applied to %d MGD77 files\n", n_changed);
	else
		GMT_Report (API, GMT_MSG_INFORMATION, "Sampled data for %d MGD77 files\n", n_changed);

	MGD77_Path_Free (GMT, (uint64_t)n_paths, list);
	MGD77_end (GMT, &In);
	gmt_set_pad (GMT, API->pad);	/* Reset to session default pad before output */

	gmt_M_free (GMT, tmp_string);		/* A Coverity issue pointed out that 'tmp_string' could not had been freed yet */
	Return (GMT_NOERROR);
}
