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
 * API functions to support the filter1d application.
 *
 * Author:	Walter H. F. Smith
 * Date:	1-JAN-2010
 * Version:	6 API
 *
 * Brief synopsis:  filter1d_func will read N columns of data from file
 * or stdin and return filtered output at user-selected positions.
 * The time variable can be in any specified column of the input.  Several
 * filters are available, with robustness as an option.
 *
 * Filters:
 * Convolutions: Boxcar, Cosine Arch, Gaussian, Median, or Mode.
 * Geospatial:   Median, Mode, Extreme values.
 * Robust:	Option replaces outliers with medians in filter.
 * Output:	At input times, or from t_start to t_stop by t_int.
 * Lack:	Option checks for data gaps in input series.
 * Symmetry:	Option checks for asymmetry in filter window.
 * Quality:	Option checks for low mean weight in window.
 *
 */

#include "gmt_dev.h"
#include "longopt/filter1d_inc.h"

#define THIS_MODULE_CLASSIC_NAME	"filter1d"
#define THIS_MODULE_MODERN_NAME	"filter1d"
#define THIS_MODULE_LIB		"core"
#define THIS_MODULE_PURPOSE	"Time domain filtering of 1-D data tables"
#define THIS_MODULE_KEYS	"<D{,>D},FD(1"
#define THIS_MODULE_NEEDS	""
#define THIS_MODULE_OPTIONS "-:>Vabdefghijoq" GMT_OPT("HMm")

/* Control structure for filter1d */

struct FILTER1D_CTRL {
	struct FILTER1D_D {	/* -D<inc> */
		bool active;
		double inc;
	} D;
	struct FILTER1D_E {	/* -E */
		bool active;
	} E;
	struct FILTER1D_F {	/* -F<type><width>[<mode>] */
		bool active;
		bool highpass;
		bool variable;
		char filter;	/* Character codes for the filter */
		double width;
		int mode;	/* -1/0/+1 */
		char *file;	/* File with custom filter coefficients or w(t) variable widths */
	} F;
	struct FILTER1D_L {	/* -L<lackwidth> */
		bool active;
		double value;
	} L;
	struct FILTER1D_N {	/* -N<t_col> or -Nc|<unit>[+a] */
		bool active;
		bool add_col;
		char unit;
		unsigned int mode;
		unsigned int spatial;
		int col;
	} N;
	struct FILTER1D_Q {	/* -Q<factor> */
		bool active;
		double value;
	} Q;
	struct FILTER1D_S {	/* -S<symmetry> */
		bool active;
		double value;
	} S;
	struct FILTER1D_T {	/* -T<tmin/tmax/tinc>[+a|n] */
		bool active;
		struct GMT_ARRAY T;
	} T;
};

#define FILTER1D_BOXCAR		0
#define FILTER1D_COS_ARCH	1
#define FILTER1D_GAUSSIAN	2
#define FILTER1D_CUSTOM		3
#define FILTER1D_MEDIAN		4
#define FILTER1D_MODE		5
#define FILTER1D_LOWER_ALL	6
#define FILTER1D_LOWER_POS	7
#define FILTER1D_UPPER_ALL	8
#define FILTER1D_UPPER_NEG	9
#define FILTER1D_N_FILTERS	10
#define FILTER1D_CONVOLVE	3		/* If filter_type > FILTER1D_CONVOLVE then a FILTER1D_MEDIAN, FILTER1D_MODE, or EXTREME filter is selected  */

struct FILTER1D_INFO {	/* Control structure for all aspects of the filter setup */
	bool use_ends;		/* True to start/stop at ends of series instead of 1/2 width inside  */
	bool check_asym;		/* true to test whether the data are asymmetric about the output time  */
	bool check_lack;		/* true to test for lack of data (gap) in the filter window */
	bool check_q;		/* true to test average weight or N in median */
	bool robust;			/* Look for outliers in data when true */
	bool equidist;		/* Data is evenly sampled in t */
	bool out_at_time;		/* true when output is required at evenly spaced intervals */
	bool f_operator;		/* true if custom weights coefficients sum to zero */
	bool highpass;		/* true if we are doing a highpass filter */
	bool variable;		/* true if we are doing a variable width filter */

	uint64_t *n_this_col;		/* Pointer to array of counters [one per column]  */
	uint64_t *n_left;		/* Pointer to array of counters [one per column]  */
	uint64_t *n_right;		/* Pointer to array of counters [one per column]  */
	uint64_t n_cols;		/* Number of columns of input  */
	uint64_t t_col;			/* Column of time abscissae (independent variable)  */
	uint64_t n_f_wts;		/* Number of filter weights  */
	uint64_t half_n_f_wts;		/* Half the number of filter weights  */
	uint64_t n_rows;		/* Number of rows of input  */
	size_t n_row_alloc;		/* Number of rows of data to allocate  */
	size_t n_work_alloc;		/* Number of rows of workspace to allocate  */
	int filter_type;		/* Flag indicating desired filter type  */
	int kind;			/* -1 skip +ve, +1 skip -ve, else use all  [for the l|L|u|U filter] */
	int way;			/* -1 find minimum, +1 find maximum  [for the l|L|u|U filter] */
	unsigned int mode_selection;
	unsigned int n_multiples;
	unsigned int nw;		/* Length of variable filter width array */

	double *ft;				/* Pointer for array of variable filter times  */
	double *fw;				/* Pointer for array of variable filter widths  */
	double *f_wt;			/* Pointer for array of filter coefficients  */
	double *min_loc;		/* Pointer for array of values, one per [column]  */
	double *max_loc;
	double *last_loc;
	double *this_loc;
	double *min_scl;
	double *max_scl;
	double *last_scl;
	double *this_scl;
	double **work;			/* Pointer to array of pointers to doubles for work  */
	double **data;			/* Pointer to array of pointers to doubles for data  */
	double dt;			/* Delta time resolution for filter computation  */
	double q_factor;		/* Quality level for mean weights or n in median  */
	double filter_width;		/* Full width of filter in user's units */
	double half_width;
	double t_start;			/* x-value of first output point */
	double t_stop;			/* x-value of last output point */
	double t_start_t;		/* user specified x-value of first output point if out_at_time == true */
	double t_stop_t;		/* user specified x-value of last output point if out_at_time == true */
	double t_int;			/* Output interval */
	double sym_coeff;		/* Symmetry coefficient  */
	double lack_width;		/* Lack of data width  */
	double extreme;			/* Extreme value [for the l|L|u|U filter] */

	struct GMT_DATASET *Fin;	/* Pointer to table with custom weight coefficients (optional) */
	struct GMT_DATASET *W;	/* Pointer to table with variable filter widths (optional) */
	struct GMT_ARRAY T;
};

static void *New_Ctrl (struct GMT_CTRL *GMT) {	/* Allocate and initialize a new control structure */
	struct FILTER1D_CTRL *C;

	C = gmt_M_memory (GMT, NULL, 1, struct FILTER1D_CTRL);

	/* Initialize values whose defaults are not 0/false/NULL */

	return (C);
}

static void Free_Ctrl (struct GMT_CTRL *GMT, struct FILTER1D_CTRL *C) {	/* Deallocate control structure */
	if (!C) return;
	gmt_M_str_free (C->F.file);
	gmt_free_array (GMT, &(C->T.T));
	gmt_M_free (GMT, C);
}

static int usage (struct GMTAPI_CTRL *API, int level) {
	const char *name = gmt_show_name_and_purpose (API, THIS_MODULE_LIB, THIS_MODULE_CLASSIC_NAME, THIS_MODULE_PURPOSE);
	if (level == GMT_MODULE_PURPOSE) return (GMT_NOERROR);
	GMT_Usage (API, 0, "usage: %s [<table>] -F<type><width>[+h] [-D<increment>] [-E] "
		"[-L<lack_width>] [-N<t_col>] [-Q<q_factor>] [-S<symmetry>] [-T[<min>/<max>/]<inc>|<file>|<list>[+a][+e|i|n]] "
		"[%s] [%s] [%s] [%s] [%s] [%s] [%s] [%s] [%s] [%s] [%s] [%s] [%s] [%s]\n",
		name, GMT_V_OPT, GMT_a_OPT, GMT_b_OPT, GMT_d_OPT, GMT_e_OPT, GMT_f_OPT, GMT_g_OPT, GMT_h_OPT, GMT_i_OPT,
		GMT_j_OPT, GMT_o_OPT, GMT_q_OPT, GMT_colon_OPT, GMT_PAR_OPT);

	if (level == GMT_SYNOPSIS) return (GMT_MODULE_SYNOPSIS);

	GMT_Message (API, GMT_TIME_NONE, "  REQUIRED ARGUMENTS:\n");
	GMT_Option (API, "<");
	GMT_Usage (API, 1, "\n-F<type><width>[+h]");
	GMT_Usage (API, -2, "Set filter type.  Choose from convolution and non-convolution filters "
		"and append filter <width> in same units as time column. "
		"If <width> is a file (and <type> is not f) it must have a time-series of filter widths. "
		"Append +h to select high-pass filtering [Default is low-pass filtering].");
	GMT_Usage (API, -2, "Convolution filters:");
	GMT_Usage (API, 3, "b: Boxcar : Weights are equal.");
	GMT_Usage (API, 3, "c: Cosine arch : Weights given by cosine arch.");
	GMT_Usage (API, 3, "f: Custom : Weights given in one-column file <name>.");
	GMT_Usage (API, 3, "g: Gaussian : Weights given by Gaussian function.");
	GMT_Usage (API, -2, "Non-convolution filters:");
	GMT_Usage (API, 3, "m: Median : Return the median value.");
	GMT_Usage (API, 3, "p: Maximum likelihood probability (mode) estimator : Return the mode:");
	GMT_Usage (API, 4, "+l Return the lowest mode if multiple modes are found [return average mode].");
	GMT_Usage (API, 4, "+u Return the uppermost mode if multiple modes are found [return average mode].");
	GMT_Usage (API, 3, "l: Lower : Return minimum of all points.");
	GMT_Usage (API, 3, "L: Lower+ : Return minimum of all positive points.");
	GMT_Usage (API, 3, "u: Upper : Return maximum of all points.n");
	GMT_Usage (API, 3, "U: Upper- : Return maximum of all negative points.");
	GMT_Usage (API, -2, "Upper case type B, C, F, G, M and P will use robust filter versions, "
		"i.e., replace outliers (2.5 L1 scale (MAD) of median) with median during filtering.");

	GMT_Message (API, GMT_TIME_NONE, "\n  OPTIONAL ARGUMENTS:\n");

	GMT_Usage (API, 1, "\n-D<increment>");
	GMT_Usage (API, -2, "Set fixed increment when series is NOT equidistantly sampled. "
		"Then <increment> will be the abscissae resolution, i.e., all abscissae "
		"will be rounded off to a multiple of <increment>.");
	GMT_Usage (API, 1, "\n-E Include ends of time series in output [Default loses half_width at each end].");
	GMT_Usage (API, 1, "\n-L<lack_width>");
	GMT_Usage (API, -2, "Check for lack of data condition.  If input data has a gap exceeding "
		"<width> then no output will be given at that point [Default does not check Lack].");
	GMT_Usage (API, 1, "\n-N<t_col>");
	GMT_Usage (API, -2, "Set the column that contains the independent variable (time) [0]. "
		"The left-most column is 0, the right-most is (<n_cols> - 1).");
	GMT_Usage (API, 1, "\n-Q<q_factor>");
	GMT_Usage (API, -2, "Assess quality of output value by checking mean weight in convolution. "
		"Append <q_factor> between 0 and 1.  If mean weight < q_factor, output is "
		"suppressed at this point [Default does not check quality].");
	GMT_Usage (API, 1, "\n-S<symmetry>");
	GMT_Usage (API, -2, "Check symmetry of data about window center.  Append a factor "
		"between 0 and 1.  If ( (abs(n_left - n_right)) / (n_left + n_right) ) > factor, "
		"then no output will be given at this point [Default does not check Symmetry].");
	GMT_Usage (API, 1, "\n-T[<min>/<max>/]<inc>[<unit>]|<file>|<list>[+a][+e|i|n]");
	GMT_Usage (API, -2, "Make evenly spaced output time steps from <min> to <max> by <inc> [Default uses input times]. "
		"Alternatively, give a <file> with output times in the first column or a comma-separated <list>. "
		"For absolute time filtering, append a valid time unit (%s) to the increment. "
		"For spatial filtering with distance computed from the first two columns, "
		"append a geospatial distance unit (%s) or c (for Cartesian distances) to the increment.", GMT_TIME_UNITS_DISPLAY, GMT_LEN_UNITS_DISPLAY);
	GMT_Usage (API, 3, "+e Keep increment exact [Default will adjust to fit range]. Only valid if just an <inc> is given.");
	GMT_Usage (API, 3, "+a Add internal geospatial distances as a final output column [no distances added].");
	GMT_Usage (API, 3, "+i Indicate <inc> is the reciprocal of desired <inc> (e.g., 3 for 0.3333.....).");
	GMT_Usage (API, 3, "+n Indicate <inc> is the number of t-values to produce instead.");
	GMT_Option (API, "V,a,bi,bo,d,e,f,g,h,i,j,o,q,:,.");

	return (GMT_MODULE_USAGE);
}

GMT_LOCAL char filter1d_set_unit_and_mode (const char *arg, unsigned int *mode) {
	unsigned int k = 0;
	*mode = GMT_GREATCIRCLE;	/* Default is great circle distances */
	switch (arg[0]) {
		case '-': *mode = GMT_FLATEARTH;	k = 1; break;
		case '+': *mode = GMT_GEODESIC;		k = 1; break;
	}
	return (arg[k]);
}

static int parse (struct GMT_CTRL *GMT, struct FILTER1D_CTRL *Ctrl, struct GMT_OPTION *options) {
	/* This parses the options provided to filter1d and sets parameters in CTRL.
	 * Any GMT common options will override values set previously by other commands.
	 * It also replaces any file names specified as input or output with the data ID
	 * returned when registering these sources/destinations with the API.
	 */

	unsigned int n_errors = 0;
	int sval = 0;
	char *c = NULL, p, txt[GMT_LEN64] = {""}, *t_arg = NULL;
	struct GMT_OPTION *opt = NULL;
	struct GMTAPI_CTRL *API = GMT->parent;

	for (opt = options; opt; opt = opt->next) {
		switch (opt->option) {

			case '<':	/* Skip input files */
				if (GMT_Get_FilePath (API, GMT_IS_DATASET, GMT_IN, GMT_FILE_REMOTE, &(opt->arg))) n_errors++;
				break;

			/* Processes program-specific parameters */

			case 'D':	/* Get fixed increment */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->D.active);
				n_errors += gmt_get_required_double (GMT, opt->arg, opt->option, 0, &Ctrl->D.inc);
				break;
			case 'E':	/* Include ends of series */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->E.active);
				n_errors += gmt_get_no_argument (GMT, opt->arg, opt->option, 0);
				break;
			case 'F':	/* Filter selection  */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->F.active);
				if (opt->arg[0] && strchr ("BbCcGgMmPpLlUuFf", opt->arg[0])) {	/* OK filter code */
					n_errors += gmt_get_required_char (GMT, opt->arg, opt->option, 0, &Ctrl->F.filter);
					if ((c = strstr (opt->arg, "+h"))) {	/* Want high-pass filter */
						Ctrl->F.highpass = true;
						c[0] = '\0';	/* Chop off +h */
					}
					if (gmt_access (GMT, &opt->arg[1], R_OK))	/* Not a file we can read */
						Ctrl->F.width = atof (&opt->arg[1]);
					else if (Ctrl->F.filter != 'f') {	/* Got variable filter width series */
						Ctrl->F.variable = true;
						Ctrl->F.file = strdup (&opt->arg[1]);
					}
					switch (Ctrl->F.filter) {	/* Get some further info from some filters */
						case 'P':
						case 'p':
							p = opt->arg[strlen(opt->arg-1)];	/* Last character */
							if (strstr (opt->arg, "+l")) Ctrl->F.mode = -1;
							else if (strstr (opt->arg, "+u")) Ctrl->F.mode = +1;
							else if (p == '-') Ctrl->F.mode = -1;	/* Old style */
							else if (p == '+') Ctrl->F.mode = +1;	/* Old style */
							break;
						case 'F':
						case 'f':
							Ctrl->F.width = DBL_MAX;	/* To avoid range test errors before reading coefficients */
							if (opt->arg[1] && !gmt_access (GMT, &opt->arg[1], R_OK))
								Ctrl->F.file = strdup (&opt->arg[1]);
							else {
								GMT_Report (API, GMT_MSG_ERROR, "Option -F[Ff]: Could not find file %s.\n", &opt->arg[1]);
								++n_errors;
							}
							break;
					}
					n_errors += gmt_M_check_condition (GMT, Ctrl->F.file == NULL && Ctrl->F.width <= 0.0,
					                                   "Option -F: Filterwidth must be positive\n");
				}
				else {
					GMT_Report (API, GMT_MSG_ERROR, "Option -F: Correct syntax: -FX<width>, X one of BbCcGgMmPpFflLuU\n");
					++n_errors;
				}
				break;
			case 'I':	/* DEPRECATED: Activate the ignore option.  This is now -di<value> and happens during reading */
				if (gmt_M_compat_check (GMT, 4)) {	/* GMT4 LEVEL */
					GMT_Report (API, GMT_MSG_COMPAT, "-I option is deprecated; use -di<ignoreval> instead.\n");
					sprintf (txt, "i%s", opt->arg);
					n_errors += gmt_parse_d_option (GMT, txt);
				}
				else
					n_errors += gmt_default_option_error (GMT, opt);
				break;
			case 'L':	/* Check for lack of data */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->L.active);
				n_errors += gmt_get_required_double (GMT, opt->arg, opt->option, 0, &Ctrl->L.value);
				break;
			case 'N':	/* Select column with independent coordinate [0] */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->N.active);
				if (gmt_M_compat_check (GMT, 4)) {	/* GMT4 LEVEL */
					if (strchr (opt->arg, '/')) { /* Gave obsolete format */
						int sval0;
						GMT_Report (API, GMT_MSG_COMPAT, "-N<ncol>/<tcol> option is deprecated; use -N<tcol> instead.\n");
						if (sscanf (opt->arg, "%d/%d", &sval0, &sval) != 2) {
							GMT_Report (API, GMT_MSG_ERROR, "Option -N: Syntax is -N<tcol>\n");
							++n_errors;
						}
					}
					else if (!strchr (opt->arg, '/'))
						n_errors += gmt_get_required_int (GMT, opt->arg, opt->option, 0, &sval);
				}
				else
					n_errors += gmt_get_required_int (GMT, opt->arg, opt->option, 0, &sval);
				if (gmt_M_compat_check (GMT, 5) && (strstr (opt->arg, "+a") || (opt->arg[0] == 'g' || opt->arg[0] == 'c'))) {	/* Deprecated syntax */
					GMT_Report (API, GMT_MSG_COMPAT, "-Nc|g[+a] option is deprecated; use -T[<min>/<max>/]<int>[+a|n] instead.\n");
					if ((c = strstr (opt->arg, "+a"))) {	/* Want to output any spatial distances computed */
						c[0] = '\0';	/* Chop off the modifier */
						Ctrl->N.add_col = true;
					}
					if ((opt->arg[0] == 'g' || opt->arg[0] == 'c')) {	/* Spatial filtering */
						Ctrl->N.spatial = 1;
						if (opt->arg[0] == 'g')	{	/* Geospatial filtering */
							if (opt->arg[1])
								Ctrl->N.unit = filter1d_set_unit_and_mode (&opt->arg[1], &Ctrl->N.mode);
							else	/* Default to meter and great circle distances */
								Ctrl->N.unit = filter1d_set_unit_and_mode ("e", &Ctrl->N.mode);
							Ctrl->N.spatial = 2;
						}
						else
							Ctrl->N.unit = 'X';	/* For Cartesian distances */
						if (c) c[0] = '+';	/* Restore */
					}
				}
				else {
					n_errors += gmt_M_check_condition (GMT, sval < 0, "Option -N: Time column cannot be negative.\n");
					Ctrl->N.col = sval;
				}
				break;
			case 'Q':	/* Assess quality of output */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->Q.active);
				n_errors += gmt_get_required_double (GMT, opt->arg, opt->option, 0, &Ctrl->Q.value);
				break;
			case 'S':	/* Activate symmetry test */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->S.active);
				n_errors += gmt_get_required_double (GMT, opt->arg, opt->option, 0, &Ctrl->S.value);
				break;
			case 'T':	/* Set output knots */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->T.active);
				t_arg = opt->arg;
				break;

			default:	/* Report bad options */
				n_errors += gmt_default_option_error (GMT, opt);
				break;
		}
	}

	if (Ctrl->T.active)	/* Do this one here since we need Ctrl->N.col to be set first, if selected */
		n_errors += gmt_parse_array (GMT, 'T', t_arg, &(Ctrl->T.T), GMT_ARRAY_TIME | GMT_ARRAY_DIST | GMT_ARRAY_ROUND | GMT_ARRAY_UNIQUE, Ctrl->N.col);
	if (Ctrl->N.spatial) {	/* Obsolete -N settings propagated to -T */
		Ctrl->T.T.spatial = Ctrl->N.spatial;
		Ctrl->T.T.unit = Ctrl->N.unit;
		Ctrl->T.T.distmode = Ctrl->N.mode;
	}
	if (Ctrl->N.add_col) Ctrl->T.T.add = true;	/* Obsolete -N+a settings propagated to -T */

	/* Check arguments */

	n_errors += gmt_M_check_condition (GMT, !Ctrl->F.active, "Option -F is required\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->D.active && Ctrl->D.inc <= 0.0, "Option -D: must give positive increment\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->T.active && Ctrl->T.T.set == 3 && (Ctrl->T.T.max - Ctrl->T.T.min) < Ctrl->F.width, "Option -T: Output interval < filterwidth\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->L.active && (Ctrl->L.value < 0.0 || Ctrl->L.value > Ctrl->F.width) , "Option -L: Unreasonable lack-of-data interval\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->S.active && (Ctrl->S.value < 0.0 || Ctrl->S.value > 1.0) , "Option -S: Enter a factor between 0 and 1\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->Q.active && (Ctrl->Q.value < 0.0 || Ctrl->Q.value > 1.0), "Option -Q: Enter a factor between 0 and 1\n");

	return (n_errors ? GMT_PARSE_ERROR : GMT_NOERROR);
}

/* Various functions which will be accessed via pointers depending on chosen filter */

GMT_LOCAL double filter1d_boxcar_weight (double radius, double half_width) {
	return ((radius > half_width) ? 0.0 : 1.0);
}

GMT_LOCAL double filter1d_cosine_weight (double radius, double half_width) {
	return ((radius > half_width) ? 0.0 : 1.0 + cos (radius * M_PI / half_width));
}

GMT_LOCAL double filter1d_gaussian_weight (double radius, double half_width) {
	return ((radius > half_width) ? 0.0 : exp (-4.5 * radius * radius / (half_width * half_width)));
}

GMT_LOCAL void filter1d_allocate_data_space (struct GMT_CTRL *GMT, struct FILTER1D_INFO *F) {
	uint64_t i;

	for (i = 0; i < F->n_cols; ++i) F->data[i] = gmt_M_memory (GMT, F->data[i], F->n_row_alloc, double);
}

GMT_LOCAL void filter1d_allocate_more_work_space (struct GMT_CTRL *GMT, struct FILTER1D_INFO *F) {
	uint64_t i;

	for (i = 0; i < F->n_cols; ++i) F->work[i] = gmt_M_memory (GMT, F->work[i], F->n_work_alloc, double);
}

GMT_LOCAL int filter1d_set_up_filter (struct GMT_CTRL *GMT, struct FILTER1D_INFO *F) {
	uint64_t i, i1, i2;
	double t_0, t_1, time, w_sum;
	double (*get_weight[3]) (double, double);	/* Pointers to desired weight function.  */

	t_0 = F->data[F->t_col][0];
	t_1 = F->data[F->t_col][F->n_rows-1];
	if (F->equidist) F->dt = (t_1 - t_0) / (F->n_rows - 1);

	if (F->filter_type == FILTER1D_CUSTOM) {	/* Use coefficients we read from file */
		F->n_f_wts = F->Fin->n_records;
		F->f_wt = gmt_M_memory (GMT, F->f_wt, F->n_f_wts, double);
		gmt_M_memcpy (F->f_wt, F->Fin->table[0]->segment[0]->data[GMT_X], F->n_f_wts, double);
		for (i = 0, w_sum = 0.0; i < F->n_f_wts; ++i) w_sum += F->f_wt[i];
		F->f_operator = (gmt_M_is_zero (w_sum));	/* If weights sum to zero it is an operator like {-1 1] or [1 -2 1] */
		if (w_sum > 1.0) {	/* Must normalize filter weights */
			GMT_Report (GMT->parent, GMT_MSG_INFORMATION, "Must normalize custom filter since weight sum > 1 [%g]\n", w_sum);
			for (i = 0; i < F->n_f_wts; ++i) F->f_wt[i] /= w_sum;
		}
		F->half_n_f_wts = F->n_f_wts / 2;
		F->half_width = F->half_n_f_wts * F->dt;
		F->filter_width = 2.0 * F->half_width;
	}
	else if (F->filter_type <= FILTER1D_CONVOLVE) {
		get_weight[FILTER1D_BOXCAR] = &filter1d_boxcar_weight;
		get_weight[FILTER1D_COS_ARCH] = &filter1d_cosine_weight;
		get_weight[FILTER1D_GAUSSIAN] = &filter1d_gaussian_weight;
		F->half_width = 0.5 * F->filter_width;
		F->half_n_f_wts = lrint (F->half_width / F->dt);
		F->n_f_wts = 2 * F->half_n_f_wts + 1;

		F->f_wt = gmt_M_memory (GMT, F->f_wt, F->n_f_wts, double);
		for (i = 0; i <= F->half_n_f_wts; ++i) {
			time = i * F->dt;
			i1 = F->half_n_f_wts - i;
			i2 = F->half_n_f_wts + i;
			F->f_wt[i1] = F->f_wt[i2] = ( *get_weight[F->filter_type]) (time, F->half_width);
		}
	}
	else
		F->half_width = 0.5 * F->filter_width;

	/* Initialize start/stop time */

	if (F->out_at_time) {
		/* populate F->t_start and F->t_stop */
		/* Here, F->T.set == 1 means we were only given an increment, otherwise we got min/max/inc.
		 * Thus, for the latter we want whatever stop we pick to be an integer steps of inc from min. */
		if (F->T.set == 1 || F->t_start_t < t_0) /* Not set or user defined t_start_t outside bounds */
			F->t_start = (F->T.set == 1) ? floor (t_0 / F->t_int) * F->t_int : F->t_start_t + floor ((t_0 - F->t_start_t) / F->t_int) * F->t_int;
		else
			F->t_start = F->t_start_t;
		if (F->T.set == 1 || F->t_stop_t > t_1) /* Not set or user defined t_stop_t outside bounds */
			F->t_stop = (F->T.set == 1) ? ceil (t_1 / F->t_int) * F->t_int : F->t_stop_t - floor ((F->t_stop_t - t_1) / F->t_int) * F->t_int;
		else
			F->t_stop = F->t_stop_t;

		if (!F->use_ends) {
			/* remove filter half width from bounds */
			F->t_start += F->half_width;
			F->t_stop  -= F->half_width;
		}
	}
	else {
		if (F->use_ends) {
			F->t_start = t_0;
			F->t_stop = t_1;
		}
		else {
			uint64_t row;
			double small = (F->data[F->t_col][1] - F->data[F->t_col][0]) * GMT_CONV8_LIMIT;
			for (row = 0; (F->data[F->t_col][row] - t_0 + small) < F->half_width; ++row);
			F->t_start = F->data[F->t_col][row];
			for (row = F->n_rows - 1; row > 0 && (t_1 - F->data[F->t_col][row] + small) < F->half_width; --row);
			F->t_stop = F->data[F->t_col][row];
		}
	}

	GMT_Report (GMT->parent, GMT_MSG_INFORMATION, "F width: %g Resolution: %g Start: %g Stop: %g\n", F->filter_width, F->dt, F->t_start, F->t_stop);

	return (0);
}

GMT_LOCAL int filter1d_lack_check (struct FILTER1D_INFO *F, uint64_t i_col, uint64_t left, uint64_t right) {
	uint64_t last_row, this_row;
	bool lacking = false;
	double last_t;

	last_row = left;
	while (!(gmt_M_is_dnan (F->data[i_col][last_row])) && last_row < (right - 1)) ++last_row;

	last_t = F->data[F->t_col][last_row];
	this_row = last_row + 1;
	while (!(lacking) && this_row < (right - 1)) {
		while (!(gmt_M_is_dnan (F->data[i_col][this_row])) && this_row < (right - 1)) ++this_row;

		if ( (F->data[F->t_col][this_row] - last_t) > F->lack_width)
			lacking = true;
		else {
			last_t = F->data[F->t_col][this_row];
			++this_row;
		}
	}
	return (lacking);
}

GMT_LOCAL void filter1d_get_robust_estimates (struct GMT_CTRL *GMT, struct FILTER1D_INFO *F, uint64_t j, uint64_t n, int both) {
	uint64_t i, n_smooth;
	bool sort_me = true;
	double low, high, last, temp;

	if (F->filter_type > FILTER1D_MODE)
		temp = gmt_extreme (GMT, F->work[j], n, F->extreme, F->kind, F->way);
	else if (F->filter_type == FILTER1D_MODE) {
		n_smooth = n / 2;
		gmt_mode (GMT, F->work[j], n, n_smooth, sort_me, F->mode_selection, &F->n_multiples, &temp);
	}
	else {
		low = F->min_loc[j];
		high = F->max_loc[j];
		last = F->last_loc[j];

		gmt_median (GMT, F->work[j], n, low, high, last, &temp);
	}

	F->last_loc[j] = F->this_loc[j] = temp;

	if (both) {
		for (i = 0; i < n; ++i) F->work[j][i] = fabs (F->work[j][i] - F->this_loc[j]);
		low = F->min_scl[j];
		high = F->max_scl[j];
		last = F->last_scl[j];
		gmt_median (GMT, F->work[j], n, low, high, last, &temp);
		F->last_scl[j] = F->this_scl[j] = temp;
	}
}

GMT_LOCAL int filter1d_do_the_filter (struct GMTAPI_CTRL *C, struct FILTER1D_INFO *F) {
	uint64_t i_row, left, right, n_l, n_r, k = 0, n_for_call, n_good_ones, last_k;
	uint64_t iq, i_col, diff;
	int64_t i_f_wt, n_in_filter;
	int result;
	bool *good_one = NULL;	/* Pointer to array of logicals [one per column]  */
	double t_time, delta_time, wt, val, med, scl, small, symmetry;
	double *wt_sum = NULL;		/* Pointer for array of weight sums [each column]  */
	double *data_sum = NULL;	/* Pointer for array of data * weight sums [columns]  */
	double *outval = NULL;
	struct GMT_RECORD *Out = NULL;
	struct GMT_CTRL *GMT = C->GMT;

	left = right = 0;		/* Left/right end of filter window */
	iq = lrint (F->q_factor);

	if (F->out_at_time) {	/* Set up equidistant output times given the min/max/inc given */
		if (gmt_create_array (GMT, 'T', &(F->T), &(F->t_start), &(F->t_stop)))
			return GMT_RUNTIME_ERROR;
		k = 0;	/* Start at first output point */
		last_k = F->T.n - 1;
	}
	else {	/* Duplicate an output time array from the input times but exclude ends (unless -E) */
		F->T.array = gmt_M_memory (GMT, NULL, F->n_rows, double);
		gmt_M_memcpy (F->T.array, F->data[F->t_col], F->n_rows, double);
		for (k = 0; F->data[F->t_col][k] < F->t_start; ++k);	/* Bypass points outside */
		for (last_k = F->n_rows-1; last_k && F->data[F->t_col][last_k] > F->t_stop; --last_k);	/* Bypass points outside */
	}
	small = (F->T.array[1] - F->T.array[0]) * GMT_CONV8_LIMIT;

	outval = gmt_M_memory (GMT, NULL, F->n_cols, double);
	good_one = gmt_M_memory (GMT, NULL, F->n_cols, bool);
	wt_sum = gmt_M_memory (GMT, NULL, F->n_cols, double);
	data_sum = gmt_M_memory (GMT, NULL, F->n_cols, double);
	Out = gmt_new_record (GMT, NULL, NULL);

	while (k <= last_k) {
		if (F->variable) {	/* Must obtain the filter width for this time */
			if (F->T.array[k] < F->ft[0] || F->T.array[k] > F->ft[F->nw-1]) {
				k++;
				continue;	/* Outside filter width array */
			}
			if ((result = gmt_intpol (GMT, F->ft, F->fw, NULL, F->nw, 1, &(F->T.array[k]), &(F->filter_width), 0.0, GMT->current.setting.interpolant))) {
				GMT_Report (GMT->parent, GMT_MSG_WARNING, "Interpolation failure in gmt_intpol (error %d) - skipping segment", result);
				continue;
			}
			filter1d_set_up_filter (GMT, F);
			if (F->T.array[k] < F->t_start || F->T.array[k] > F->t_stop) {
				k++;
				continue;	/* Outside full filter window */
			}
		}

		/* Determine data window for convolution/filter */
		while (left  < F->n_rows && (F->T.array[k] - F->data[F->t_col][left]  - small) > F->half_width)  ++left;
		while (right < F->n_rows && (F->data[F->t_col][right] - F->T.array[k] - small) <= F->half_width) ++right;
		n_in_filter = (int64_t)(right - left);
		if ( n_in_filter <= 0 || (F->check_lack && ( (F->filter_width / n_in_filter) > F->lack_width) ) ) {
			k++;	/* Nothing to filter, go to next output time */
			continue;
		}
		t_time = F->T.array[k];	/* Current output time */
		for (i_col = 0; i_col < F->n_cols; ++i_col) {
			F->n_this_col[i_col] = 0;
			wt_sum[i_col] = data_sum[i_col] = 0.0;
			if (i_col == F->t_col)
				good_one[i_col] = false;
			else if (F->check_lack)
				good_one[i_col] = !(filter1d_lack_check (F, i_col, left, right));
			else
				good_one[i_col] = true;
			if (F->check_asym) F->n_left[i_col] = F->n_right[i_col] = 0;
		}

		if (F->robust || F->filter_type > FILTER1D_CONVOLVE) {
			if (n_in_filter > (int64_t)F->n_work_alloc) {
				F->n_work_alloc = (size_t)n_in_filter;
				filter1d_allocate_more_work_space (GMT, F);
			}
			for (i_row = left; i_row < right; ++i_row) {
				for (i_col = 0; i_col < F->n_cols; ++i_col) {
					if (!(good_one[i_col])) continue;
					if (!gmt_M_is_dnan (F->data[i_col][i_row])) {
						F->work[i_col][F->n_this_col[i_col]] = F->data[i_col][i_row];
						F->n_this_col[i_col]++;
						if (F->check_asym) {
							if (F->data[F->t_col][i_row] < t_time) F->n_left[i_col]++;
							if (F->data[F->t_col][i_row] > t_time) F->n_right[i_col]++;
						}
					}
				}
			}
			if (F->check_asym) {
				for (i_col = 0; i_col < F->n_cols; ++i_col) {
					if (!(good_one[i_col])) continue;
					n_l = F->n_left[i_col];
					n_r = F->n_right[i_col];
					diff = (n_l >= n_r) ? n_l - n_r : n_r - n_l;
					symmetry = ((double)diff)/(n_l + n_r);
					if (symmetry > F->sym_coeff) good_one[i_col] = false;
				}
			}
			if ((F->filter_type > FILTER1D_CONVOLVE) && F->check_q) {
				for (i_col = 0; i_col < F->n_cols; ++i_col) {
					if (F->n_this_col[i_col] < iq) good_one[i_col] = false;
				}
			}

			for (i_col = 0; i_col < F->n_cols; ++i_col) {
				if (good_one[i_col]) {
					n_for_call = F->n_this_col[i_col];
					filter1d_get_robust_estimates (GMT, F, i_col, n_for_call, F->robust);
				}
			}

		}	/* That's it for the robust work  */

		if (F->filter_type > FILTER1D_CONVOLVE) {

			/* Need to count how many good ones; use data_sum area  */

			Out->data = data_sum;
			n_good_ones = 0;
			for (i_col = 0; i_col < F->n_cols; ++i_col) {
				if (i_col == F->t_col)
					data_sum[i_col] = t_time;
				else if (good_one[i_col]) {
					data_sum[i_col] = (F->highpass) ? F->data[i_col][k] - F->this_loc[i_col] : F->this_loc[i_col];
					++n_good_ones;
				}
				else
					data_sum[i_col] = GMT->session.d_NaN;
			}
			if (n_good_ones) GMT_Put_Record (C, GMT_WRITE_DATA, Out);
		}
		else {
			if (F->robust) for (i_col = 0; i_col < F->n_cols; ++i_col) F->n_this_col[i_col] = 0;

			for (i_row = left; i_row < right; ++i_row) {
				delta_time = t_time - F->data[F->t_col][i_row];
				i_f_wt = F->half_n_f_wts + lrint (floor (0.5 + delta_time/F->dt));
				if ((i_f_wt < 0) || (i_f_wt >= (int)F->n_f_wts)) continue;

				for(i_col = 0; i_col < F->n_cols; ++i_col) {
					if (!good_one[i_col]) continue;
					if (!gmt_M_is_dnan (F->data[i_col][i_row])) {
						wt = F->f_wt[i_f_wt];
						val = F->data[i_col][i_row];
						if (F->robust) {
							med = F->this_loc[i_col];
							scl = F->this_scl[i_col];
							val = ((fabs(val-med)) > (2.5 * scl)) ? med : val;
						}
						else if (F->check_asym) {	/* This wasn't already done  */
							if (F->data[F->t_col][i_row] < t_time) F->n_left[i_col]++;
							if (F->data[F->t_col][i_row] > t_time) F->n_right[i_col]++;
						}
						wt_sum[i_col] += wt;
						data_sum[i_col] += (wt * val);
						F->n_this_col[i_col]++;
					}
				}
			}
			n_good_ones = 0;
			for (i_col = 0; i_col < F->n_cols; ++i_col) {
				if (!good_one[i_col]) continue;
				if (!F->n_this_col[i_col]) {
					good_one[i_col] = false;
					continue;
				}
				if (F->check_asym && !(F->robust) ) {
					n_l = F->n_left[i_col];
					n_r = F->n_right[i_col];
					diff = (n_l >= n_r) ? n_l - n_r : n_r - n_l;
					symmetry = ((double)diff)/(n_l + n_r);
					if (symmetry > F->sym_coeff) {
						good_one[i_col] = false;
						continue;
					}
				}
				if (F->check_q && ((wt_sum[i_col] / F->n_this_col[i_col]) < F->q_factor)) {
					good_one[i_col] = false;
					continue;
				}
				++n_good_ones;
			}
			if (n_good_ones) {
				Out->data = outval;
				for (i_col = 0; i_col < F->n_cols; ++i_col) {
					if (i_col == F->t_col)
						outval[i_col] = t_time;
					else if (good_one[i_col]) {
						outval[i_col] = (F->f_operator) ? data_sum[i_col] : data_sum[i_col] / wt_sum[i_col];
						if (F->highpass) outval[i_col] = F->data[i_col][k] - outval[i_col];
					}
					else
						outval[i_col] = GMT->session.d_NaN;
				}
				GMT_Put_Record (C, GMT_WRITE_DATA, Out);
			}
		}
		k++;	/* Go to next output point */
	}

	gmt_M_free (GMT, outval);
	gmt_M_free (GMT, good_one);
	gmt_M_free (GMT, wt_sum);
	gmt_M_free (GMT, data_sum);
	gmt_M_free (GMT, Out);
	gmt_M_free (GMT, F->T.array);

	return (0);
}

GMT_LOCAL int filter1d_allocate_space (struct GMT_CTRL *GMT, struct FILTER1D_INFO *F) {
	F->n_this_col = gmt_M_memory (GMT, NULL, F->n_cols, uint64_t);
	F->data = gmt_M_memory_aligned (GMT, NULL, F->n_cols, double *);

	if (F->check_asym) F->n_left = gmt_M_memory (GMT, NULL, F->n_cols, uint64_t);
	if (F->check_asym) F->n_right = gmt_M_memory (GMT, NULL, F->n_cols, uint64_t);

	if (F->robust || (F->filter_type > FILTER1D_CONVOLVE) ) {	/* Then we need workspace  */
		uint64_t i;

		F->work = gmt_M_memory (GMT, NULL, F->n_cols, double *);
		for (i = 0; i < F->n_cols; ++i) F->work[i] = gmt_M_memory (GMT, NULL, F->n_work_alloc, double);
		F->min_loc = gmt_M_memory (GMT, NULL, F->n_cols, double);
		F->max_loc = gmt_M_memory (GMT, NULL, F->n_cols, double);
		F->last_loc = gmt_M_memory (GMT, NULL, F->n_cols, double);
		F->this_loc = gmt_M_memory (GMT, NULL, F->n_cols, double);
		F->min_scl = gmt_M_memory (GMT, NULL, F->n_cols, double);
		F->max_scl = gmt_M_memory (GMT, NULL, F->n_cols, double);
		F->this_scl = gmt_M_memory (GMT, NULL, F->n_cols, double);
		F->last_scl = gmt_M_memory (GMT, NULL, F->n_cols, double);
	}
	return (0);
}

GMT_LOCAL void filter1d_free_space (struct GMT_CTRL *GMT, struct FILTER1D_INFO *F) {
	uint64_t i;
	if (!F) return;
	if (F->robust || (F->filter_type > FILTER1D_CONVOLVE) ) {
		for (i = 0; i < F->n_cols; ++i)	gmt_M_free (GMT, F->work[i]);
		gmt_M_free (GMT, F->work);
	}
	for (i = 0; i < F->n_cols; ++i)	gmt_M_free (GMT, F->data[i]);
	gmt_M_free (GMT, F->data);
	gmt_M_free (GMT, F->n_this_col);
	gmt_M_free (GMT, F->n_left);
	gmt_M_free (GMT, F->n_right);
	gmt_M_free (GMT, F->min_loc);
	gmt_M_free (GMT, F->max_loc);
	gmt_M_free (GMT, F->last_loc);
	gmt_M_free (GMT, F->this_loc);
	gmt_M_free (GMT, F->min_scl);
	gmt_M_free (GMT, F->max_scl);
	gmt_M_free (GMT, F->last_scl);
	gmt_M_free (GMT, F->this_scl);
	gmt_M_free (GMT, F->f_wt);
}

GMT_LOCAL void filter1d_load_parameters (struct FILTER1D_INFO *F, struct FILTER1D_CTRL *Ctrl, uint64_t n_cols) {
	F->filter_width = Ctrl->F.width;
	F->dt = Ctrl->D.inc;
	F->equidist = !Ctrl->D.active;
	F->use_ends = Ctrl->E.active;
	F->check_lack = Ctrl->L.active;
	F->lack_width = Ctrl->L.value;
	F->n_cols = n_cols;
	F->t_col = Ctrl->N.col;
	F->q_factor = Ctrl->Q.value;
	F->check_q = Ctrl->Q.active;
	F->check_asym = Ctrl->S.active;
	F->sym_coeff = Ctrl->S.value;
	F->t_start_t = F->t_start = Ctrl->T.T.min;
	F->t_stop_t = F->t_stop = Ctrl->T.T.max;
	F->t_int = Ctrl->T.T.inc;
	F->T = Ctrl->T.T;
	F->out_at_time = Ctrl->T.active;
	F->highpass = Ctrl->F.highpass;
	F->variable = Ctrl->F.variable;
}

/* Must free allocated memory before returning */
#define bailout(code) {gmt_M_free_options (mode); return (code);}
#define Return(code,...) {Free_Ctrl (GMT, Ctrl); gmt_end_module (GMT, GMT_cpy); GMT_Report (API, GMT_MSG_ERROR, __VA_ARGS__); bailout (code);}
#define Return2(code) {Free_Ctrl (GMT, Ctrl); gmt_end_module (GMT, GMT_cpy); bailout (code);}

EXTERN_MSC int GMT_filter1d (void *V_API, int mode, void *args) {
	uint64_t col, tbl, row, seg, tseg;
	int error;
	unsigned int save_col[2], n_out_cols;

	double last_time, new_time, in;

	struct GMT_OPTION *options = NULL;
	struct FILTER1D_INFO F;
	struct GMT_DATASET *D = NULL;
	struct GMT_DATASEGMENT *S = NULL;
	struct FILTER1D_CTRL *Ctrl = NULL;

	struct GMT_CTRL *GMT = NULL, *GMT_cpy = NULL;
	struct GMTAPI_CTRL *API = gmt_get_api_ptr (V_API);	/* Cast from void to GMTAPI_CTRL pointer */

	/*----------------------- Standard module initialization and parsing ----------------------*/

	if (API == NULL) return (GMT_NOT_A_SESSION);
	if (mode == GMT_MODULE_PURPOSE) return (usage (API, GMT_MODULE_PURPOSE));	/* Return the purpose of program */
	options = GMT_Create_Options (API, mode, args);	if (API->error) return (API->error);	/* Set or get option list */

	if ((error = gmt_report_usage (API, options, 0, usage)) != GMT_NOERROR) bailout (error);	/* Give usage if requested */

	/* Parse the command-line arguments */

	if ((GMT = gmt_init_module (API, THIS_MODULE_LIB, THIS_MODULE_CLASSIC_NAME, THIS_MODULE_KEYS, THIS_MODULE_NEEDS, module_kw, &options, &GMT_cpy)) == NULL) bailout (API->error); /* Save current state */
	if (GMT_Parse_Common (API, THIS_MODULE_OPTIONS, options)) Return (API->error, "Error parsing filter1d options\n");
	Ctrl = New_Ctrl (GMT);		/* Allocate and initialize a new control structure */
	if ((error = parse (GMT, Ctrl, options)) != 0) Return (error, "Error parsing filter1d options\n");

	/*---------------------------- This is the filter1d main code ----------------------------*/

	gmt_M_memset (&F, 1, struct FILTER1D_INFO);	/* Init control structure to NULL */
	F.n_work_alloc = GMT_CHUNK;
	F.equidist = true;

	GMT_Report (API, GMT_MSG_INFORMATION, "Processing input table data\n");

	/* Register all data sources */
	if (GMT_Init_IO (API, GMT_IS_DATASET, GMT_IS_LINE, GMT_IN, GMT_ADD_DEFAULT, 0, options) != GMT_NOERROR) {
		Return (API->error, "Error initializing input\n");
	}
	/* Read the input data into memory */
	if ((D = GMT_Read_Data (API, GMT_IS_DATASET, GMT_IS_FILE, 0, GMT_READ_NORMAL, NULL, NULL, NULL)) == NULL) {
		Return (API->error, "Error Reading input\n");
	}
	if (D->n_columns < 2) {
		GMT_Report (API, GMT_MSG_ERROR, "Input data have %d column(s) but at least 2 are needed\n", (int)D->n_columns);
		Return (GMT_DIM_TOO_SMALL, "Not enough input columns\n");
	}
	n_out_cols = (unsigned int)D->n_columns;
	if (Ctrl->T.T.spatial) {	/* Must add extra column to store distances and then compute them */
		Ctrl->N.col = (int)D->n_columns;
		gmt_adjust_dataset (GMT, D, D->n_columns + 1);
		if (gmt_init_distaz (GMT, Ctrl->T.T.unit, Ctrl->T.T.distmode, GMT_MAP_DIST) == GMT_NOT_A_VALID_TYPE) {
			Return (GMT_NOT_A_VALID_TYPE, "Bad distance unit");
		}
		for (tbl = 0; tbl < D->n_tables; ++tbl) {	/* For each input table */
			for (seg = 0; seg < D->table[tbl]->n_segments; ++seg) {	/* For each segment */
				S = D->table[tbl]->segment[seg];
				gmt_M_free (GMT, S->data[Ctrl->N.col]);
				S->data[Ctrl->N.col] = gmt_dist_array (GMT, S->data[GMT_X], S->data[GMT_Y], S->n_rows, true);
			}
		}
		if (Ctrl->T.T.add) n_out_cols++;
	}

	filter1d_load_parameters (&F, Ctrl, D->n_columns);	/* Pass parameters from Control structure to Filter structure */

	if (GMT->common.b.active[GMT_IN] && GMT->common.b.ncol[GMT_IN] < F.n_cols) Return (GMT_N_COLS_VARY,
		"Binary input data must have at least %d fields\n", F.n_cols);

	if (strchr ("BCGMPF", Ctrl->F.filter)) {	/* First deal with robustness request */
		F.robust = true;
		Ctrl->F.filter = (char)tolower ((int)Ctrl->F.filter);
	}
	switch (Ctrl->F.filter) {	/* Set filter parameters */
		case 'b':
			F.filter_type = FILTER1D_BOXCAR;
			break;
		case 'c':
			F.filter_type = FILTER1D_COS_ARCH;
			break;
		case 'g':
			F.filter_type = FILTER1D_GAUSSIAN;
			break;
		case 'm':
			F.filter_type = FILTER1D_MEDIAN;
			break;
		case 'p':
			F.filter_type = FILTER1D_MODE;
			F.mode_selection = Ctrl->F.mode;
			break;
		case 'l':
			F.filter_type = FILTER1D_LOWER_ALL;
			F.way = -1;
			F.extreme = DBL_MAX;
			break;
		case 'L':
			F.filter_type = FILTER1D_LOWER_POS;
			F.way = -1;
			F.kind = +1;
			break;
		case 'u':
			F.filter_type = FILTER1D_UPPER_ALL;
			F.way = +1;
			F.extreme = -DBL_MAX;
			break;
		case 'U':
			F.filter_type = FILTER1D_UPPER_NEG;
			F.way = +1;
			F.kind = -1;
			break;
		case 'f':
			F.filter_type = FILTER1D_CUSTOM;
			if ((error = GMT_Set_Columns (API, GMT_IN, 1, GMT_COL_FIX_NO_TEXT)) != 0) Return (error, "Error in GMT_Set_Columns");
			save_col[GMT_X] = gmt_get_column_type (GMT, GMT_IN, GMT_X);	/* Save col type in case it is a time column */
			gmt_set_column_type (GMT, GMT_IN, GMT_X, GMT_IS_FLOAT);	/* Always read the weights as floats */
			gmt_disable_bghio_opts (GMT);	/* Do not want any -b -g -h -i -o to affect the reading from -F files */
			if ((F.Fin = GMT_Read_Data (API, GMT_IS_DATASET, GMT_IS_FILE, GMT_IS_NONE, GMT_READ_NORMAL, NULL, Ctrl->F.file, NULL)) == NULL) {
				Return (API->error, "Error Reading input\n");
			}
			gmt_reenable_bghio_opts (GMT);	/* Recover settings provided by user (if -b -g -h -i were used at all) */
			gmt_set_column_type (GMT, GMT_IN, GMT_X, save_col[GMT_X]);	/* Reset this col type to whatever it actually is */
			GMT_Report (API, GMT_MSG_INFORMATION, "Read %" PRIu64 " filter weights from file %s.\n", F.Fin->n_records, Ctrl->F.file);
			break;
	}
	if (F.filter_type > FILTER1D_CONVOLVE) F.robust = false;
	if (F.variable) {
		if ((error = GMT_Set_Columns (API, GMT_IN, 2, GMT_COL_FIX_NO_TEXT)) != 0) Return (error, "Error in GMT_Set_Columns");
		save_col[GMT_X] = gmt_get_column_type (GMT, GMT_IN, GMT_X);	/* Save col type in case it is a time column */
		save_col[GMT_Y] = gmt_get_column_type (GMT, GMT_IN, GMT_Y);	/* Save col type in case it is a time column */
		gmt_set_column_type (GMT, GMT_IN, GMT_X, gmt_get_column_type (GMT, GMT_IN, F.t_col));	/* Same units as time-series "t"*/
		gmt_set_column_type (GMT, GMT_IN, GMT_Y, GMT_IS_FLOAT);	/* Always read the widths as floats */
		gmt_disable_bghio_opts (GMT);	/* Do not want any -b -g -h -i -o to affect the reading from -F files */
		if ((F.W = GMT_Read_Data (API, GMT_IS_DATASET, GMT_IS_FILE, GMT_IS_NONE, GMT_READ_NORMAL, NULL, Ctrl->F.file, NULL)) == NULL) {
			Return (API->error, "Error Reading input\n");
		}
		gmt_reenable_bghio_opts (GMT);	/* Recover settings provided by user (if -b -g -h -i were used at all) */
		gmt_set_column_type (GMT, GMT_IN, GMT_X, save_col[GMT_X]);	/* Reset this col type to whatever it actually is */
		gmt_set_column_type (GMT, GMT_IN, GMT_Y, save_col[GMT_Y]);	/* Reset this col type to whatever it actually is */
		GMT_Report (API, GMT_MSG_INFORMATION, "Read %" PRIu64 " filter weights from file %s.\n", F.W->n_records, Ctrl->F.file);
		if (! (F.W->n_segments == 1 || F.W->n_segments == D->n_segments)) {
			Return (API->error, "Variable filter width requires either one segment (shared with all data segments) or one width segment per input data segments\n");
		}
	}

	GMT->current.io.skip_if_NaN[GMT_X] = GMT->current.io.skip_if_NaN[GMT_Y] = false;	/* Turn off default GMT NaN-handling */
	GMT->current.io.skip_if_NaN[F.t_col] = true;			/* ... But disallow NaN in "time" column */
	GMT->common.b.ncol[GMT_OUT] = F.n_cols;
	if (GMT_Init_IO (API, GMT_IS_DATASET, GMT_IS_LINE, GMT_OUT, GMT_ADD_DEFAULT, 0, options) != GMT_NOERROR) {	/* Establishes data output */
		Return (API->error, "Error initializing input\n");
	}
	if (GMT_Begin_IO (API, GMT_IS_DATASET, GMT_OUT, GMT_HEADER_ON) != GMT_NOERROR) {	/* Enables data output and sets access mode */
		Return (API->error, "Error in Begin_IO\n");
	}
	if (GMT_Set_Geometry (API, GMT_OUT, GMT_IS_LINE) != GMT_NOERROR) {	/* Sets output geometry */
		Return (API->error, "Error in GMT_Set_Geometry\n");
	}
	if (GMT_Set_Columns (API, GMT_OUT, n_out_cols, GMT_COL_FIX_NO_TEXT) != GMT_NOERROR) {
		Return (API->error, "Error in GMT_Set_Columns\n");
	}

	filter1d_allocate_space (GMT, &F);	/* Gets column-specific flags and uint64_t space */

	gmt_increase_abstime_format_precision (GMT, Ctrl->N.col, F.t_int);	/* In case we need more sub-second precision output */

	GMT_Report (API, GMT_MSG_INFORMATION, "Filter the data columns\n");

	for (tbl = tseg = 0; tbl < D->n_tables; ++tbl) {	/* For each input table */
		for (seg = 0; seg < D->table[tbl]->n_segments; ++seg, tseg++) {	/* For each segment */
			/* Duplicate data and set up arrays and parameters needed to filter this segment */
			S = D->table[tbl]->segment[seg];
			if (S->n_rows > F.n_row_alloc) {
				F.n_row_alloc = MAX (GMT_CHUNK, S->n_rows);
				filter1d_allocate_data_space (GMT, &F);
			}

			if (F.robust || (F.filter_type == FILTER1D_MEDIAN) ) {
				for (col = 0; col < F.n_cols; ++col) {
					F.min_loc[col] = DBL_MAX;
					F.max_loc[col] = -DBL_MAX;
				}
			}
			last_time = -DBL_MAX;
			if (Ctrl->T.T.spatial == 2)	/* Ensure longitudes are in the same quadrants */
				gmt_eliminate_lon_jumps (GMT, S->data[GMT_X], S->n_rows);

			for (row = F.n_rows = 0; row < S->n_rows; ++row, ++F.n_rows) {
				in = S->data[F.t_col][row];
				if (gmt_M_is_dnan (in)) continue;	/* Skip records with time == NaN */
				new_time = in;
				if (new_time < last_time) Return (GMT_DATA_READ_ERROR, "Error! Time decreases at line # %" PRIu64 "\n\tUse UNIX utility sort and then try again.\n", row);
				last_time = new_time;
				for (col = 0; col < F.n_cols; ++col) {
					in = S->data[col][row];
					F.data[col][F.n_rows] = in;
					if (F.robust || (F.filter_type == FILTER1D_MEDIAN) ) {
						if (in > F.max_loc[col]) F.max_loc[col] = in;
						if (in < F.min_loc[col]) F.min_loc[col] = in;
					}
				}
			}
			GMT_Report (API, GMT_MSG_INFORMATION, "Read %" PRIu64 " records from table %" PRIu64 ", segment %" PRIu64 "\n", F.n_rows, tbl, seg);

			/* FILTER: Initialize scale parameters and last_loc based on min and max of data  */

			if (F.robust || (F.filter_type == FILTER1D_MEDIAN) ) {
				for (col = 0; col < F.n_cols; ++col) {
					F.min_scl[col] = 0.0;
					F.max_scl[col]  = 0.5 * (F.max_loc[col] - F.min_loc[col]);
					F.last_scl[col] = 0.5 * F.max_scl[col];
					F.last_loc[col] = 0.5 * (F.max_loc[col] + F.min_loc[col]);
				}
			}

			if (F.variable) {
				/* Set pointers to these arrays */
				unsigned int ks = (F.W->n_segments > 1) ? tseg : 0;	/* Either a shared segment or one per input segment */
				F.ft = F.W->table[0]->segment[ks]->data[GMT_X];
				F.fw = F.W->table[0]->segment[ks]->data[GMT_Y];
				F.nw = F.W->table[0]->segment[ks]->n_rows;
			}
			if (!F.variable && filter1d_set_up_filter (GMT, &F)) {
				filter1d_free_space (GMT, &F);
				Return (GMT_RUNTIME_ERROR, "Fatal error during coefficient setup.\n");
			}

			if (GMT->current.io.multi_segments[GMT_OUT]) GMT_Put_Record (API, GMT_WRITE_SEGMENT_HEADER, S->header);

			if (filter1d_do_the_filter (API, &F)) {
				filter1d_free_space (GMT, &F);
				Return (GMT_RUNTIME_ERROR, "Fatal error in filtering routine.\n");
			}
		}
	}

	if (GMT_End_IO (API, GMT_OUT, 0) != GMT_NOERROR) {	/* Disables further data output */
		Return (API->error, "Error in End_IO\n");
	}

	if (F.n_multiples > 0) GMT_Report (API, GMT_MSG_INFORMATION, "%d separate modes found by the mode filter\n", F.n_multiples);

	filter1d_free_space (GMT, &F);

	Return2 (GMT_NOERROR);
}
