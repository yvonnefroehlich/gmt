/*--------------------------------------------------------------------
 *
 *    Copyright (c) 1999-2025 by T. Henstock
 *    See README file for copying and redistribution conditions.
 *--------------------------------------------------------------------*/
/* pssegy program to plot segy files in postscript with variable trace spacing option
 * uses the GMT postscriptlight imagemask routines to plot a
 * 1-bit depth bitmap which will not obliterate stuff underneath!
 *
 * Author:	Tim Henstock (then@noc.soton.ac.uk)
 * Date:	1-July-1996
 * Version:	1.0
 *
 * Bug fixes:	1.1, 11-6-96 remove undesirable normalization introduced by -Z option. Add -U option for reduction.
 *
 * enhancements: 1.2, 7-20-98 add option to match location of traces from file
 *
 *               1.3, 1/7/99 check number of samples trace by trace to cope with SEGY with variable trace length
 *
 *               2.0, 5/7/99 update for GMT 3.3.1
 *
 *               2.1 10/4/2001 fix unreduce bug, modify to byte-swap if necessary, make 64-bit safe
 *
 *                   10/7/2009 fix bug when reading trace locations from arbitrary header locations,
 *                             8 bytes copied, should be 4 bytes
 *		2.2 7/30/2010 Ported to GMT 5 P. Wessel (global variables removed)
 *
 * This program is free software and may be copied or redistributed under the terms
 * of the GNU LGPL license.
 */

#include "gmt_dev.h"
#include "longopt/pssegy_inc.h"
#include "segy_io.h"

#define THIS_MODULE_CLASSIC_NAME	"pssegy"
#define THIS_MODULE_MODERN_NAME	"segy"
#define THIS_MODULE_LIB		"segy"
#define THIS_MODULE_PURPOSE	"Plot a SEGY file in 2-D"
#define THIS_MODULE_KEYS	">X}"
#define THIS_MODULE_NEEDS	"JR"
#define THIS_MODULE_OPTIONS "->BJKOPRUVXYpt" GMT_OPT("c")

#define B_ID	0	/* Indices into Q values */
#define I_ID	1
#define U_ID	2
#define X_ID	3
#define Y_ID	4

#define PLOT_CDP	1
#define PLOT_OFFSET	2

struct PSSEGY_CTRL {
	struct PSSEGY_In {	/* -In */
		bool active;
		char *file;
	} In;
	struct PSSEGY_A {	/* -A */
		bool active;
	} A;
	struct PSSEGY_C {	/* -C<cpt> */
		bool active;
		double value;
	} C;
	struct PSSEGY_D {	/* -D */
		bool active;
		double value;
	} D;
	struct PSSEGY_E {	/* -E */
		bool active;
		double value;
	} E;
	struct PSSEGY_F {	/* -F<fill> */
		bool active;
		double rgb[4];
	} F;
	struct PSSEGY_I {	/* -I */
		bool active;
	} I;
	struct PSSEGY_L {	/* -L */
		bool active;
		uint32_t value;
	} L;
	struct PSSEGY_M {	/* -M */
		bool active;
		uint32_t value;
	} M;
	struct PSSEGY_N {	/* -N */
		bool active;
	} N;
	struct PSSEGY_Q {	/* -Qb|i|u|x|y */
		bool active[5];
		double value[5];
	} Q;
	struct PSSEGY_S {	/* -S */
		bool active;
		unsigned int mode;
		int value;
	} S;
	struct PSSEGY_T {	/* -T */
		bool active;
		char *file;
	} T;
	struct PSSEGY_W {	/* -W */
		bool active;
	} W;
	struct PSSEGY_Z {	/* -Z */
		bool active;
	} Z;
};

static void *New_Ctrl (struct GMT_CTRL *GMT) {	/* Allocate and initialize a new control structure */
	struct PSSEGY_CTRL *C;

	C = gmt_M_memory (GMT, NULL, 1, struct PSSEGY_CTRL);

	/* Initialize values whose defaults are not 0/false/NULL */

	C->A.active = !GMT_BIGENDIAN;
	C->M.value = 10000;
	C->Q.value[I_ID] = 300.0; /* Effective dots-per-inch of image */
	C->Q.value[X_ID] = 1.0; /* Ctrl->Q.value[X_ID], Ctrl->Q.value[Y_ID] are trace and sample interval */
	return (C);
}

static void Free_Ctrl (struct GMT_CTRL *GMT, struct PSSEGY_CTRL *C) {	/* Deallocate control structure */
	if (!C) return;
	gmt_M_str_free (C->In.file);
	gmt_M_str_free (C->T.file);
	gmt_M_free (GMT, C);
}

static int usage (struct GMTAPI_CTRL *API, int level) {
	const char *name = gmt_show_name_and_purpose (API, THIS_MODULE_LIB, THIS_MODULE_CLASSIC_NAME, THIS_MODULE_PURPOSE);
	if (level == GMT_MODULE_PURPOSE) return (GMT_NOERROR);
	GMT_Usage (API, 0, "usage: %s [<segyfile>] -D<dev> -F<color> | -W %s %s [-A] [-C<clip>] [-E<slop>] "
		"[-I] %s[-L<nsamp>] [-M<ntraces>] [-N] %s%s[-Q<mode><value>] [-S<header>] [-T<tracefile>] "
		"[%s] [%s] [-W] [%s] [%s] [-Z] %s[%s] [%s] [%s]\n", name, GMT_Jx_OPT, GMT_Rx_OPT, API->K_OPT,
		API->O_OPT, API->P_OPT, GMT_U_OPT, GMT_V_OPT, GMT_X_OPT, GMT_Y_OPT, API->c_OPT, GMT_p_OPT, GMT_t_OPT, GMT_PAR_OPT);

	if (level == GMT_SYNOPSIS) return (GMT_MODULE_SYNOPSIS);

	GMT_Message (API, GMT_TIME_NONE, "  REQUIRED ARGUMENTS:\n");
	GMT_Usage (API, 1, "\nNote: Must specify either -W or -F.");
	GMT_Usage (API, 1, "\n<segyfile> is an IEEE SEGY file [or standard input].");
	GMT_Usage (API, 1, "\n-D<dev>");
	GMT_Usage (API, -2, "Set <dev> to give deviation in X units of plot for 1.0 on scaled trace.");
	GMT_Usage (API, 1, "\n-F<color>");
	GMT_Usage (API, -2, "Set <color> to fill variable area with a single color for the bitmap.");
	GMT_Usage (API, 1, "\n-W Plot wiggle trace.");
	GMT_Option (API, "JX,R");
	if (gmt_M_showusage (API)) GMT_Usage (API, -2, "Note: Units for y are s or km.");
	GMT_Message (API, GMT_TIME_NONE, "\n  OPTIONAL ARGUMENTS:\n");
	GMT_Usage (API, 1, "\n-A Flip the default byte-swap state (default assumes data have a bigendian byte-order).");
	GMT_Usage (API, 1, "\n-C<clip>");
	GMT_Usage (API, -2, "Clip scaled trace excursions at <clip>, applied after bias.");
	GMT_Usage (API, 1, "\n-E<slop>");
	GMT_Usage (API, -2, "Set <error> slop to allow for -T. Recommended in case of arithmetic errors!");
	GMT_Usage (API, 1, "\n-I Fill negative rather than positive excursions.");
	GMT_Option (API, "K");
	GMT_Usage (API, 1, "\n-L<nsamp>");
	GMT_Usage (API, -2, "Specify <nsamp> to override number of samples.");
	GMT_Usage (API, 1, "\n-M<ntraces>");
	GMT_Usage (API, -2, "Fix the number of traces. -M0 will read number in binary header, while "
		"-M<ntraces> will attempt to read only <ntraces> traces [Default reads all traces].");
	GMT_Usage (API, 1, "\n-N Trace normalize the plot, with order of operations: [normalize][bias][clip](deviation).");
	GMT_Option (API, "O,P");
	GMT_Usage (API, 1, "\n-Q<mode><value>");
	GMT_Usage (API, -2, "Append <mode><value> to change any of 5 different modes:");
	GMT_Usage (API, 3, "b: Append <bias> to bias scaled traces (-Bb-0.1 subtracts 0.1 from values) [0].");
	GMT_Usage (API, 3, "i: Append <dpi> to change image dots-per-inch [300].");
	GMT_Usage (API, 3, "u: Append <redvel> to apply reduction velocity (-ve removes reduction already present) [0].");
	GMT_Usage (API, 3, "x: Append <mult> to multiply trace locations by <mult> [1].");
	GMT_Usage (API, 3, "y: Append <dy> to override sample interval.");
	GMT_Usage (API, 1, "\n-S<header>");
	GMT_Usage (API, -2, "Append <header> to set variable spacing. "
		"<header> is c for cdp or o for offset.");
	GMT_Usage (API, 1, "\n-T<tracefile>");
	GMT_Usage (API, -2, "Look in <filename> for a list of locations to select traces "
		"(same units as header * X, i.e., values printed by previous -V run).");
	GMT_Option (API, "U,V");
	GMT_Option (API, "X");
	GMT_Usage (API, 1, "\n-Z Suppress plotting traces whose rms amplitude is 0.");
	GMT_Option (API, "c,p,t,.");

	return (GMT_MODULE_USAGE);
}

static int parse (struct GMT_CTRL *GMT, struct PSSEGY_CTRL *Ctrl, struct GMT_OPTION *options) {
	/* This parses the options provided to pssegy and sets parameters in Ctrl.
	 * Note Ctrl has already been initialized and non-zero default values set.
	 * Any GMT common options will override values set previously by other commands.
	 * It also replaces any file names specified as input or output with the data ID
	 * returned when registering these sources/destinations with the API.
	 */

	unsigned int n_errors = 0;
	struct GMT_OPTION *opt = NULL;
	struct GMTAPI_CTRL *API = GMT->parent;

	for (opt = options; opt; opt = opt->next) {	/* Process all the options given */

		switch (opt->option) {

			case '<':	/* Input files */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->In.active);
				n_errors += gmt_get_required_file (GMT, opt->arg, opt->option, 0, GMT_IS_DATASET, GMT_IN, GMT_FILE_REMOTE, &(Ctrl->In.file));
				break;

			/* Processes program-specific parameters */

			case 'A':	/* Swap data */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->A.active);
				break;
			case 'C':	/* trace clip */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->C.active);
				n_errors += gmt_get_required_double (GMT, opt->arg, opt->option, 0, &Ctrl->C.value);
				break;
			case 'D':	/* trace scaling */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->D.active);
				n_errors += gmt_get_required_double (GMT, opt->arg, opt->option, 0, &Ctrl->D.value);
				break;
			case 'E':
				n_errors += gmt_M_repeated_module_option (API, Ctrl->E.active);
				n_errors += gmt_get_required_double (GMT, opt->arg, opt->option, 0, &Ctrl->E.value);
				break;
			case 'F':
				n_errors += gmt_M_repeated_module_option (API, Ctrl->F.active);
				if (gmt_getrgb (GMT, opt->arg, Ctrl->F.rgb)) {
					n_errors++;
					gmt_rgb_syntax (GMT, 'F', " ");
				}
				break;
			case 'I':
				n_errors += gmt_M_repeated_module_option (API, Ctrl->I.active);
				n_errors += gmt_get_no_argument (GMT, opt->arg, opt->option, 0);
				break;
			case 'L':
				n_errors += gmt_M_repeated_module_option (API, Ctrl->L.active);
				n_errors += gmt_get_required_uint (GMT, opt->arg, opt->option, 0, &Ctrl->L.value);
				Ctrl->L.value = atoi (opt->arg);
				break;
			case 'M':
				n_errors += gmt_M_repeated_module_option (API, Ctrl->M.active);
				n_errors += gmt_get_required_uint (GMT, opt->arg, opt->option, 0, &Ctrl->M.value);
				Ctrl->M.value = atoi (opt->arg);
				break;
			case 'N':	/* trace norm. */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->N.active);
				n_errors += gmt_get_no_argument (GMT, opt->arg, opt->option, 0);
				break;
			case 'Q':
				switch (opt->arg[0]) {
					case 'b':	/* Trace bias */
						n_errors += gmt_M_repeated_module_option (API, Ctrl->Q.active[B_ID]);
						Ctrl->Q.value[B_ID] = atof (&opt->arg[1]);
						break;
					case 'i':	/* Image dpi */
						n_errors += gmt_M_repeated_module_option (API, Ctrl->Q.active[I_ID]);
						Ctrl->Q.value[I_ID] = atof (&opt->arg[1]);
						break;
					case 'u':	/* reduction velocity application */
						n_errors += gmt_M_repeated_module_option (API, Ctrl->Q.active[U_ID]);
						Ctrl->Q.value[U_ID] = atof (&opt->arg[1]);
						break;
					case 'x': /* over-rides of header info */
						n_errors += gmt_M_repeated_module_option (API, Ctrl->Q.active[X_ID]);
						Ctrl->Q.value[X_ID] = atof (&opt->arg[1]);
						break;
					case 'y': /* over-rides of header info */
						n_errors += gmt_M_repeated_module_option (API, Ctrl->Q.active[Y_ID]);
						Ctrl->Q.value[Y_ID] = atof (&opt->arg[1]);
						break;
				}
				break;
			case 'S':
				n_errors += gmt_M_repeated_module_option (API, Ctrl->S.active);
				switch (opt->arg[0]) {
					case 'o':
						Ctrl->S.mode = PLOT_OFFSET;
						break;
					case 'c':
						Ctrl->S.mode = PLOT_CDP;
						break;
					case 'b':
						Ctrl->S.value = atoi (&opt->arg[1]);
						break;
				}
				break;
			case 'T':	/* plot traces only at listed locations */
				n_errors += gmt_M_repeated_module_option (API, Ctrl->T.active);
				n_errors += gmt_get_required_file (GMT, opt->arg, opt->option, 0, GMT_IS_DATASET, GMT_IN, GMT_FILE_REMOTE, &(Ctrl->T.file));
				break;
			case 'W':
				n_errors += gmt_M_repeated_module_option (API, Ctrl->W.active);
				n_errors += gmt_get_no_argument (GMT, opt->arg, opt->option, 0);
				break;
			case 'Z':
				n_errors += gmt_M_repeated_module_option (API, Ctrl->Z.active);
				n_errors += gmt_get_no_argument (GMT, opt->arg, opt->option, 0);
				break;

			default:	/* Report bad options */
				n_errors += gmt_default_option_error (GMT, opt);
				break;
		}
	}
	n_errors += gmt_M_check_condition (GMT, Ctrl->E.value < 0.0, "Option -E: Slop cannot be negative\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->I.active && !Ctrl->F.active, "Must specify -F with -I\n");
	n_errors += gmt_M_check_condition (GMT, !Ctrl->F.active && !Ctrl->W.active, "Must specify -F or -W\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->D.value <= 0.0, "Option -D: Must specify a positive deviation\n");

	return (n_errors ? GMT_PARSE_ERROR : GMT_NOERROR);
}

/* function to return rms amplitude of n_samp values from the array data */
GMT_LOCAL float pssegy_rms (float *data, uint32_t n_samp) {
	uint32_t ix;
	double sumsq = 0.0;

	for (ix = 0; ix < n_samp; ix++)
		sumsq += ((double) data[ix])*((double) data[ix]);
	sumsq /= ((double) n_samp);
	sumsq = sqrt (sumsq);
	return (float) sumsq;
}

GMT_LOCAL int pssegy_paint (int ix, int iy, unsigned char *bitmap, int bm_nx, int bm_ny) {	/* pixel to paint */
	int byte, quot, rem;
	static unsigned char bmask[8]={128, 64, 32, 16, 8, 4, 2, 1};

	quot = ix / 8;
	rem = ix - quot * 8;

	if ((quot >= bm_nx-1) || (iy >= bm_ny-1) || (ix < 0) || (iy < 0)) return (-1);	/* outside bounds of plot array */

	byte = (bm_ny - iy - 1) * bm_nx + quot;	/* find byte to paint - flip vertical! */
	bitmap[byte] = bitmap[byte] | bmask[rem];
	return (0);
}

GMT_LOCAL void pssegy_wig_bmap (struct GMT_CTRL *GMT, double x0, float data0, float data1, double y0, double y1, double dpi, unsigned char *bitmap, int bm_nx, int bm_ny) { /* apply current sample with all options to bitmap */

	int px0, px1, py0, py1, ix, iy;
	double xp0, xp1, yp0, yp1, slope;

	gmt_geo_to_xy (GMT, x0+ (double)data0, y0, &xp0, &yp0); /* returns 2 ends of line segment in plot coords */
	gmt_geo_to_xy (GMT, x0+ (double)data1, y1, &xp1, &yp1);
	slope = (yp1 - yp0) / (xp1 - xp0);

	px0 = irint (xp0 * dpi);
	px1 = irint (xp1 * dpi);
	py0 = irint (yp0 * dpi);
	py1 = irint (yp1 * dpi);

	/* now have the pixel locations for the two samples - join with a line..... */
	if (fabs (slope) <= 1.0) { /* more pixels needed in x direction */
		if (px0 < px1) {
			for (ix = px0; ix <= px1; ix++) {
				iy = py0 + irint (slope * (float) (ix - px0));
				pssegy_paint (ix, iy, bitmap, bm_nx, bm_ny);
			}
		}
		else {
			for (ix = px1; ix <= px0; ix++) {
				iy = py0 + irint (slope * (float) (ix - px0));
				pssegy_paint (ix, iy, bitmap, bm_nx, bm_ny);
			}

		}
	}
	else { /* more pixels needed in y direction */
		if (py0 < py1) {
			for (iy = py0; iy <= py1; iy++) {
				ix = px0 + irint (((float) (iy - py0)) / slope);
				pssegy_paint (ix, iy, bitmap, bm_nx, bm_ny);
			}
		}
		else {
			for (iy=py1; iy<=py0; iy++) {
				ix = px0 + irint (((float) (iy - py0)) / slope);
				pssegy_paint (ix, iy, bitmap, bm_nx, bm_ny);
			}
		}
	}
}

GMT_LOCAL void pssegy_shade_bmap (struct GMT_CTRL *GMT, double x0, float data0, float data1, double y0, double y1, int negative, double dpi, unsigned char *bitmap, int bm_nx, int bm_ny) { /* apply current samples with all options to bitmap */

	int px0, px00, py0, py1, ixx, ix, iy;
	double xp0, xp00, xp1, yp0, yp1, interp, slope;

	if ((data0 * data1) < 0.0) {
		/* points to plot are on different sides of zero - interpolate to find out where zero is */
		interp = y0 + (double)data0 * ((y0 - y1) / (double)(data1 - data0));
		if (((data0 < 0.0) && negative) || ((data0 > 0.0) && !negative)) {
			/* plot from top to zero */
			y1 = interp;
			data1 = 0.0f;
		}
		else {
			y0 = interp;
			data0 = 0.0f;
		}
	}


	gmt_geo_to_xy (GMT, x0+(double)data0, y0, &xp0, &yp0); /* returns 2 ends of line segment in plot coords */
	gmt_geo_to_xy (GMT, x0+(double)data1, y1, &xp1, &yp1);
	gmt_geo_to_xy (GMT, x0, y0, &xp00, &yp0); /* to get position of zero */

	slope = (yp1 - yp0) / (xp1 - xp0);

	px0  = irint (0.49 + xp0  * dpi);
	px00 = irint (0.49 + xp00 * dpi);
	py0  = irint (0.49 + yp0  * dpi);
	py1  = irint (0.49 + yp1  * dpi);


	/*  can rasterize simply by looping over values of y */
	if (py0 < py1) {
		for (iy = py0; iy <= py1; iy++) {
			ixx = px0 + irint (((float) (iy - py0)) / slope);
			if (ixx < px00) {
				for (ix = ixx; ix <= px00; ix++) pssegy_paint (ix, iy, bitmap, bm_nx, bm_ny);
			}
			else {
				for (ix = px00; ix <= ixx; ix++) pssegy_paint (ix, iy, bitmap, bm_nx, bm_ny);
			}
		}
	}
	else {
		for (iy = py1; iy <= py0; iy++) {
			ixx = px0 + irint (((float) (iy - py0)) / slope);
			if (ixx < px00) {
				for (ix = ixx; ix <= px00; ix++) pssegy_paint (ix, iy, bitmap, bm_nx, bm_ny);
			}
			else {
				for (ix = px00; ix<= ixx; ix++) pssegy_paint (ix, iy, bitmap, bm_nx, bm_ny);
			}
		}
	}
}

GMT_LOCAL void pssegy_plot_trace (struct GMT_CTRL *GMT, float *data, double dy, double x0, int n_samp, int do_fill, int negative, int plot_wig, float toffset, double dpi, unsigned char *bitmap, int bm_nx, int bm_ny) {
	/* shell function to loop over all samples in the current trace, determine plot options
	 * and call the appropriate bitmap routine */

	int iy, paint_wiggle;
	double y0 = 0.0, y1;

	for (iy = 1; iy < n_samp; iy++) {	/* loop over samples on trace - refer to pairs iy-1, iy */
		y1 = dy * (float) iy + toffset;
		if (plot_wig) pssegy_wig_bmap (GMT, x0, data[iy-1],data[iy], y0, y1, dpi, bitmap, bm_nx, bm_ny); /* plotting wiggle */
		if (do_fill) {	/* plotting VA -- check data points first */
			paint_wiggle = ((!negative && ((data[iy-1] >= 0.0) || (data[iy] >= 0.0))) || (negative && ((data[iy-1] <= 0.0) || (data[iy] <= 0.0))));
			if (paint_wiggle) pssegy_shade_bmap (GMT, x0, data[iy-1], data[iy], y0, y1, negative, dpi, bitmap, bm_nx, bm_ny);
		}
		y0=y1;
	}
}

#define bailout(code) {gmt_M_free_options (mode); return (code);}
#define Return(code) {Free_Ctrl (GMT, Ctrl); gmt_end_module (GMT, GMT_cpy); bailout (code);}

EXTERN_MSC int GMT_pssegy (void *V_API, int mode, void *args) {
	bool plot_it = false;
	unsigned int i, nm, ix, iy;
	uint32_t n_samp = 0, n_tracelist = 0;
	int error = 0, check, bm_nx, bm_ny;

	float scale = 1.0f, toffset = 0.0f, *data = NULL;
	double xlen, ylen, xpix, ypix, x0, test, *tracelist = NULL, trans[3] = {-1.0, -1.0, -1.0};

	unsigned char *bitmap = NULL;

	char reelhead[3200] = {""};
	SEGYHEAD *header = NULL;
	SEGYREEL binhead;

	FILE *fpi = NULL, *fpt = NULL;

	struct PSSEGY_CTRL *Ctrl = NULL;
	struct GMT_CTRL *GMT = NULL, *GMT_cpy = NULL;		/* General GMT internal parameters */
	struct GMT_OPTION *options = NULL;
	struct PSL_CTRL *PSL = NULL;				/* General PSL internal parameters */
	struct GMTAPI_CTRL *API = gmt_get_api_ptr (V_API);	/* Cast from void to GMTAPI_CTRL pointer */

	/*----------------------- Standard module initialization and parsing ----------------------*/

	if (API == NULL) return (GMT_NOT_A_SESSION);
	if (mode == GMT_MODULE_PURPOSE) return (usage (API, GMT_MODULE_PURPOSE));	/* Return the purpose of program */
	options = GMT_Create_Options (API, mode, args);	if (API->error) return (API->error);	/* Set or get option list */

	if ((error = gmt_report_usage (API, options, 0, usage)) != GMT_NOERROR) bailout (error);	/* Give usage if requested */

	/* Parse the command-line arguments; return if errors are encountered */

	if ((GMT = gmt_init_module (API, THIS_MODULE_LIB, THIS_MODULE_CLASSIC_NAME, THIS_MODULE_KEYS, THIS_MODULE_NEEDS, module_kw, &options, &GMT_cpy)) == NULL) bailout (API->error); /* Save current state */
	if (GMT_Parse_Common (API, THIS_MODULE_OPTIONS, options)) Return (API->error);
	Ctrl = New_Ctrl (GMT);	/* Allocate and initialize a new control structure */
	if ((error = parse (GMT, Ctrl, options)) != 0) Return (error);

	/*---------------------------- This is the pssegy main code ----------------------------*/

	if (Ctrl->In.active) {
		GMT_Report (API, GMT_MSG_INFORMATION, "Will read segy file %s\n", Ctrl->In.file);
		if ((fpi = gmt_fopen (GMT, Ctrl->In.file, "rb")) == NULL) {
			GMT_Report (API, GMT_MSG_ERROR, "Cannot find segy file %s\n", Ctrl->In.file);
			Return (GMT_ERROR_ON_FOPEN);
		}
	}
	else {
		GMT_Report (API, GMT_MSG_INFORMATION, "Will read segy file from standard input\n");
		if (fpi == NULL) fpi = stdin;
	}

	if (Ctrl->T.active && (fpt = gmt_fopen (GMT, Ctrl->T.file, "r")) == NULL) {
		GMT_Report (API, GMT_MSG_ERROR, "Cannot find trace list file %s\n", Ctrl->T.file);
		if (fpi != stdin) fclose (fpi);
		Return (GMT_ERROR_ON_FOPEN);
	}

	if (!gmt_M_is_linear (GMT)) GMT_Report (API, GMT_MSG_WARNING, "You asked for a non-rectangular projection. \n It will probably still work, but be prepared for problems\n");
	if (Ctrl->Q.value[Y_ID]) GMT_Report (API, GMT_MSG_INFORMATION, "Overriding sample interval dy = %f\n", Ctrl->Q.value[Y_ID]);


	if (Ctrl->T.active) { /* must read in file of desired trace locations */
		tracelist = gmt_M_memory (GMT, NULL, GMT_CHUNK, double);
		n_tracelist = GMT_CHUNK;
		ix = 0;
		while ((fscanf (fpt, "%lf", &test)) != EOF) {
			tracelist[ix] = test;
			ix++;
			if (ix == n_tracelist) {	/* need more memory in array */
				n_tracelist += GMT_CHUNK;
				tracelist = gmt_M_memory (GMT, tracelist, n_tracelist, double);
			}
		}
		fclose(fpt);
		n_tracelist = (int)ix;
		GMT_Report (API, GMT_MSG_INFORMATION, "read in %d trace locations\n", n_tracelist);
	}

	/* set up map projection and PS plotting */
	if (gmt_map_setup (GMT, GMT->common.R.wesn)) {
		gmt_M_free (GMT, tracelist);
		if (fpi != stdin) fclose (fpi);
		Return (GMT_PROJECTION_ERROR);
	}
	if ((PSL = gmt_plotinit (GMT, options)) == NULL) {
		if (Ctrl->T.active) gmt_M_free (GMT, tracelist);
		if (fpi != stdin) fclose (fpi);
		Return (GMT_RUNTIME_ERROR);
	}
	gmt_plane_perspective (GMT, GMT->current.proj.z_project.view_plane, GMT->current.proj.z_level);
	gmt_set_basemap_orders (GMT, GMT_BASEMAP_FRAME_AFTER, GMT_BASEMAP_GRID_AFTER, GMT_BASEMAP_ANNOT_AFTER);
	gmt_plotcanvas (GMT);	/* Fill canvas if requested */
	gmt_map_basemap (GMT);

	/* define area for plotting and size of array for bitmap */
	xlen = GMT->current.proj.rect[XHI] - GMT->current.proj.rect[XLO];
	xpix = xlen * Ctrl->Q.value[I_ID];	/* pixels in x direction */
	/* xpix /= 8.0;
	bm_nx = 1 +(int) xpix;*/
	bm_nx = irint (ceil (xpix / 8.0)); /* store 8 pixels per byte in x direction but must have
		whole number of bytes per scan */
	ylen = GMT->current.proj.rect[YHI] - GMT->current.proj.rect[YLO];
	ypix = ylen * Ctrl->Q.value[I_ID];	/* pixels in y direction */
	bm_ny = irint (ypix);
	nm = bm_nx * bm_ny;

	/* read in reel headers from segy file */
	if ((check = segy_get_reelhd (fpi, reelhead)) != true) Return (GMT_RUNTIME_ERROR);
	if ((check = segy_get_binhd (fpi, &binhead)) != true) Return (GMT_RUNTIME_ERROR);

	if (Ctrl->A.active) {
		/* this is a little-endian system, and we need to byte-swap ints in the header - we only
		   use a few of these*/
		GMT_Report (API, GMT_MSG_INFORMATION, "Swapping bytes for ints in the headers\n");
		binhead.num_traces = bswap16 (binhead.num_traces);
		binhead.nsamp = bswap16 (binhead.nsamp);
		binhead.dsfc = bswap16 (binhead.dsfc);
		binhead.sr = bswap16 (binhead.sr);
	}

	/* set parameters from the reel headers */
	if (!Ctrl->M.value) Ctrl->M.value = binhead.num_traces;

	GMT_Report (API, GMT_MSG_INFORMATION, "Number of traces in header is %d\n", Ctrl->M.value);

	if (!Ctrl->L.value) {/* number of samples not overridden*/
		Ctrl->L.value = binhead.nsamp;
		GMT_Report (API, GMT_MSG_INFORMATION, "Number of samples per trace is %d\n", Ctrl->L.value);
	}
	else if ((Ctrl->L.value != binhead.nsamp) && (binhead.nsamp))
		GMT_Report (API, GMT_MSG_INFORMATION, "nsampr input %d, nsampr in header %d\n", Ctrl->L.value,  binhead.nsamp);

	if (!Ctrl->L.value) { /* no number of samples still - a problem! */
		GMT_Report (API, GMT_MSG_ERROR, "Number of samples per trace unknown\n");
		if (Ctrl->T.active) gmt_M_free (GMT, tracelist);
		if (fpi != stdin) fclose (fpi);
		Return (GMT_RUNTIME_ERROR);
	}

	GMT_Report (API, GMT_MSG_INFORMATION, "Number of samples for reel is %d\n", Ctrl->L.value);

	if (binhead.dsfc != 5) GMT_Report (API, GMT_MSG_WARNING, "Data not in IEEE format\n");

	if (!Ctrl->Q.value[Y_ID]) {
		Ctrl->Q.value[Y_ID] = (double) binhead.sr; /* sample interval of data (microseconds) */
		Ctrl->Q.value[Y_ID] /= 1000000.0;
		GMT_Report (API, GMT_MSG_INFORMATION, "Sample interval is %f s\n", Ctrl->Q.value[Y_ID]);
	}
	else if ((Ctrl->Q.value[Y_ID] != binhead.sr) && (binhead.sr)) /* value in header overridden by input */
		GMT_Report (API, GMT_MSG_INFORMATION, "dy input %f, dy in header %f\n", Ctrl->Q.value[Y_ID], (float)binhead.sr);

	if (!Ctrl->Q.value[Y_ID]) { /* still no sample interval at this point is a problem! */
		GMT_Report (API, GMT_MSG_ERROR, "No sample interval in reel header\n");
		if (Ctrl->T.active) gmt_M_free (GMT, tracelist);
		Return (GMT_RUNTIME_ERROR);
	}

	bitmap = gmt_M_memory (GMT, NULL, nm, unsigned char);

	ix = 0;
	while ((ix < Ctrl->M.value) && (header = segy_get_header (fpi)) != 0) {
		/* read traces one by one */
		if (Ctrl->S.mode == PLOT_OFFSET) {
			/* plot traces by offset, cdp, or input order */
			int32_t tmp = header->sourceToRecDist;
			if (Ctrl->A.active) {
				uint32_t *p = (uint32_t *)&tmp;
				*p = bswap32 (*p);
			}
			x0 = (double) tmp;
		}
		else if (Ctrl->S.mode == PLOT_CDP) {
			int32_t tmp = header->cdpEns;
			if (Ctrl->A.active) {
				uint32_t *p = (uint32_t *)&tmp;
				*p = bswap32 (*p);
			}
			x0 = (double) tmp;
		}
		else if (Ctrl->S.value) {
			/* get value starting at Ctrl->S.value of header into a double */
			uint32_t tmp;
			memcpy (&tmp, &header[Ctrl->S.value], sizeof (uint32_t));
			x0 = (Ctrl->A.active) ? (double) bswap32 (tmp) : (double) tmp;
		}
		else
			x0 = (1.0 + (double) ix);

		x0 *= Ctrl->Q.value[X_ID];

		if (Ctrl->A.active) {
			/* need to permanently byte-swap some things in the trace header
				 do this after getting the location of where traces are plotted
				 in case the general Ctrl->S.value case overlaps a defined header
				 in a strange way */
			uint32_t *p = (uint32_t *)&header->sourceToRecDist;
			*p = bswap32 (*p);
			header->sampleLength = bswap16 (header->sampleLength);
			header->num_samps = bswap32 (header->num_samps);
		}

		/* now check that on list to plot if list exists */
		if (n_tracelist) {
			plot_it = false;
			for (i = 0; i< n_tracelist; i++) {
				if (fabs (x0 - tracelist[i]) <= Ctrl->E.value) plot_it = true;
			}
		}

		if (Ctrl->Q.value[U_ID]) {
			toffset = (float) - (fabs ((double)(header->sourceToRecDist)) / Ctrl->Q.value[U_ID]);
			GMT_Report (API, GMT_MSG_INFORMATION, "time shifted by %f\n", toffset);
		}

		data = segy_get_data (fpi, header); /* read a trace */
		/* get number of samples in _this_ trace (e.g. OMEGA has strange ideas about SEGY standard)
		or set to number in reel header */
		if ((n_samp = segy_samp_rd (header)) != 0) n_samp = Ctrl->L.value;

		if (Ctrl->A.active) {
			/* need to swap the order of the bytes in the data even though assuming IEEE format */
			uint32_t tmp;
			for (iy = 0; iy < n_samp; ++iy) {
				memcpy (&tmp, &data[iy], sizeof(uint32_t));
				tmp = bswap32 (tmp);
				memcpy (&data[iy], &tmp, sizeof(uint32_t));
			}
		}

		if (Ctrl->N.active || Ctrl->Z.active) {
			scale = pssegy_rms (data, n_samp);
			GMT_Report (API, GMT_MSG_INFORMATION, "rms value is %f\n", scale);
		}
		for (iy = 0; iy < n_samp; iy++) { /* scale bias and clip each sample in the trace */
			if (Ctrl->N.active) data[iy] /= scale;
			data[iy] += (float)Ctrl->Q.value[B_ID];
			if (Ctrl->C.active && (fabs(data[iy]) > Ctrl->C.value)) data[iy] = (float)(Ctrl->C.value*data[iy] / fabs (data[iy])); /* apply bias and then clip */
			data[iy] *= (float)Ctrl->D.value;
		}

		if ((!Ctrl->Z.active || scale) && (plot_it || !n_tracelist)) {
			GMT_Report (API, GMT_MSG_INFORMATION, "trace %d plotting at %f \n", ix+1, x0);
			pssegy_plot_trace (GMT, data, Ctrl->Q.value[Y_ID], x0, (int)n_samp, (int)Ctrl->F.active, (int)Ctrl->I.active, (int)Ctrl->W.active, toffset, Ctrl->Q.value[I_ID], bitmap, bm_nx, bm_ny);
		}
		gmt_M_str_free (data);
		gmt_M_str_free (header);
		ix++;
	}

	gmt_map_clip_on (GMT, GMT->session.no_rgb, 3); /* set a clip at the map boundary since the image space overlaps a little */
	PSL_plotbitimage (PSL, 0.0, 0.0, xlen, ylen, 1, bitmap, 8*bm_nx, bm_ny, trans, Ctrl->F.rgb);
	/* have to multiply by 8 since postscriptlight version of ps_imagemask is based on a _pixel_ count, whereas pssegy uses _byte_ count internally */
	gmt_map_clip_off (GMT);
	gmt_map_basemap (GMT);

	if (fpi != stdin) fclose (fpi);

	gmt_plane_perspective (GMT, -1, 0.0);
	gmt_plotend (GMT);

	gmt_M_free (GMT, bitmap);
	if (Ctrl->T.active) gmt_M_free (GMT, tracelist);

	Return (GMT_NOERROR);
}

EXTERN_MSC int GMT_segy (void *V_API, int mode, void *args) {
	/* This is the GMT6 modern mode name */
	struct GMTAPI_CTRL *API = gmt_get_api_ptr (V_API);	/* Cast from void to GMTAPI_CTRL pointer */
	if (API->GMT->current.setting.run_mode == GMT_CLASSIC && !API->usage) {
		GMT_Report (API, GMT_MSG_ERROR, "Shared GMT module not found: segy\n");
		return (GMT_NOT_A_VALID_MODULE);
	}
	return GMT_pssegy (V_API, mode, args);
}
