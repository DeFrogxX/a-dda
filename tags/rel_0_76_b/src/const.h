/* FILE: const.h
 * AUTH: Maxim Yurkin
 * DESCR: All the constants used by ADDA code
 *        also defines some useful macros
 *
 * Copyright (C) 2006 University of Amsterdam
 * This code is covered by the GNU General Public License.
 */
#ifndef __const_h
#define __const_h

/* version number (string) */
#define ADDA_VERSION "0.76b"

/* basic constants */
#define FALSE        0
#define TRUE         1    /* better not use for comparison, since true may be != TRUE
                                use only for assignment */
#define UNDEF        -1   /* should be used only for variables,
                             which are naturally non-negative */
/* simple functions */
#define MIN(A,B) (((A) > (B)) ? (B) : (A))
#define MAX(A,B) (((A) < (B)) ? (B) : (A))
#define LENGTH(A) ((int)(sizeof(A)/sizeof(A[0])))    /* length of any array (converted to int) */

/* parallel definitions */
#if defined(MPI)
#define PARALLEL
#endif

#define ROOT 0  /* ringid of root processor */
  /* Using ROOT!=0 should work, however it was not thoroughly tested.
     Hence do not change without necessity */

/* math constants rounded for 32 decimals */
#define PI                  3.1415926535897932384626433832795
#define TWO_PI              6.283185307179586476925286766559
#define FOUR_PI             12.566370614359172953850573533118
#define EIGHT_PI            25.132741228718345907701147066236
#define FOUR_PI_OVER_THREE  4.1887902047863909846168578443727
#define PI_OVER_TWO         1.5707963267948966192313216916398
#define PI_OVER_FOUR        0.78539816339744830961566084581988
#define PI_OVER_SIX         0.52359877559829887307710723054658
#define TWO_OVER_PI         0.63661977236758134307553505349006
#define THREE_OVER_FOUR_PI  0.23873241463784300365332564505877
#define SIX_OVER_PI         1.9098593171027440292266051604702
#define ONE_THIRD           0.33333333333333333333333333333333
#define PI_OVER_180         0.017453292519943295769236907684886
#define INV_PI_180          57.295779513082320876798154814105

/* sizes of some arrays */
#define MAX_NMAT        10   /* maximum number of different materials (<256) */
#define MAX_N_SH_PARMS  25   /* maximum number of shape parameters */
#define MAX_N_BEAM_PARMS 4   /* maximum number of beam parameters */ 

/* sizes of filenames and other strings */
  /* maximum length of dirname; increase THIS if any errors appear */
#define MAX_DIRNAME      300
#define MAX_FNAME_SH     100   /* maximum length of filename (used for known names) */
#define MAX_TMP_FNAME_SH  15   /* maximum length of names of temporary files (short) */
#define MAX_SYSTEM_CALL   10   /* maximum string length of system call (itself) */
#define MAX_WORD          10   /* maximum length of a short word */
#define MAX_LINE          50   /* maximum length of a line */
  /* size of buffer for reading lines (longer lines are handled robustly) */
#define BUF_LINE       100
#define MAX_PARAGRAPH  500   /* maximum length of a paragraph (few lines) */
/* derived sizes */
  /* maximum string to create directory */
#define MAX_DIRSYS  (MAX_DIRNAME + MAX_SYSTEM_CALL)
  /* maximum length of filename (including directory name) */
#define MAX_FNAME   (MAX_DIRNAME + MAX_FNAME_SH)
  /* maximum length of temporary filename (including directory name) */
#define MAX_TMP_FNAME   (MAX_DIRNAME + MAX_TMP_FNAME_SH)
  /* maximum message that may include 2 filenames */
#define MAX_MESSAGE (2*MAX_FNAME + MAX_PARAGRAPH)

/* shape types */
#define SH_SPHERE     0        /* sphere */
#define SH_BOX        1        /* box (may be rectangular) */
#define SH_PRISMA     3        /* prisma (triangular) */
#define SH_LINE       4        /* line with width of one dipole */
#define SH_COATED     5        /* coated sphere */
#define SH_SPHEREBOX  6        /* sphere in a box */
#define SH_RBC        8        /* Red Blood Cell */
#define SH_ELLIPSOID  11       /* general ellipsoid */
#define SH_SDISK_ROT  14       /* disc cut of a sphere */
#define SH_CYLINDER   18       /* cylinder */
#define SH_READ       20       /* read from file */

/* which way to calculate coupleconstant */
#define POL_CM       0     /* Clausius Mossotti */
#define POL_RR       1     /* Radiative Reaction correction */
#define POL_LDR      2     /* Lattice Dispersion Relation */
#define POL_CLDR     3     /* Corrected Lattice Dispersion Relation */
#define POL_SO       4     /* Second Order formulation */

/* how to calculate scattering quantities */
#define SQ_DRAINE    0     /* classical, as Draine */
#define SQ_SO        1     /* Second Order formulation */

/* how to calculate interaction term */
#define G_POINT_DIP  0     /* as point dipoles */
#define G_SO         1     /* Second Order formulation */

/* ldr constants */
#define LDR_B1       1.8915316
#define LDR_B2      -0.1648469
#define LDR_B3       1.7700004

/* 2nd_order constants */
#define SO_B1        1.5867182
#define SO_B2        0.13488017
#define SO_B3        0.11895826

/* two boundaries for separation between G_SO 'close', 'median', and 'far' */
#define G_BOUND_CLOSE 1         /* k*R^2/d < GB_CLOSE => 'close' */
#define G_BOUND_MEDIAN 1        /* k*R < GB_MEDIAN => 'median' */

/* iterative methods; see iterative.c for info */
#define IT_CGNR      0
#define IT_BICGSTAB  1
#define IT_BICG_CS   2
#define IT_QMR_CS    3

/* type of E field calculation */
#define CE_NORMAL    0    /* normal */
#define CE_PARPER    1    /* use symmetry to calculate both incident polarizations
                             from one calculation of internal fields */

/* path and size of tables */
#define TAB_PATH     "tables/"
#define TAB_FNAME(a) "t" #a "f.dat"    /* a is a number, e.g. TAB_FNAME(2) -> "t2f.dat" */
#define TAB_SIZE     142
#define TAB_RMAX     10

/* beam types */
#define B_PLANE      0
#define B_LMINUS     1
#define B_DAVIS3     2
#define B_BARTON5    3

/* types of scattering grid */
#define SG_GRID      0      /* grid of angles */
#define SG_PAIRS     1      /* set of independent pairs */
/* types of angles set */
#define SG_RANGE     0      /* range with uniformly spaced points */
#define SG_VALUES    1      /* any set of values */

/* types of phi_integr (should be different one-bit numbers) */
#define PHI_UNITY    1     /* just integrate */
#define PHI_COS2     2     /* integrate with cos(2*phi) */
#define PHI_SIN2     4     /* integrate with sin(2*phi) */
#define PHI_COS4     8     /* integrate with cos(4*phi) */
#define PHI_SIN4     16    /* integrate with sin(4*phi) */

/* types of checkpoint (to save) */
#define CHP_NONE     0  /* do not save checkpoint */
#define CHP_NORMAL   1  /* save checkpoint if not finished in time and exit */
#define CHP_REGULAR  2  /* save checkpoints in regular time intervals
                             (until finished or halted) */
#define CHP_ALWAYS   3  /* save checkpoint either if finished or time elapsed
                             and calculate all the scattering quantities */

/* return values for functions */
#define CHP_EXIT    -2        /* exit after saving checkpoint */

/* default values; other are specified in InitVariables (param.c) */
#define DEF_GRID   (16*jagged)

/* numbers less than this value (compared to unity) are considered to be zero */
#define ROUND_ERR 1E-15

/* output and input file and dir names (can only be changed in compile time) */
#define F_EXPCOUNT      "ExpCount"
#define F_EXPCOUNT_LCK  F_EXPCOUNT ".lck"
#define F_CS            "CrossSec"
#define F_FRP           "VisFrp"
#define F_INTFLD        "IntField"
    /* suffixes */
#define F_XSUF          "-X"
#define F_YSUF          "-Y"
    /* logs */
#define F_LOG           "log"
#define F_LOG_ERR       "logerr.%d"    /* ringid as argument */
#define F_LOG_ORAVG     "log_orient_avg"
#define F_LOG_INT_CSCA  "log_int_Csca"
#define F_LOG_INT_ASYM  "log_int_asym"
    /* log suffixes */
#define F_LOG_X         "_x"
#define F_LOG_Y         "_y"
#define F_LOG_Z         "_z"

    /* mueller files */
#define F_MUEL          "mueller"
#define F_MUEL_SG       "mueller_scatgrid"
#define F_MUEL_INT      "mueller_integr"
#define F_MUEL_C2       "mueller_integr_c2"
#define F_MUEL_S2       "mueller_integr_s2"
#define F_MUEL_C4       "mueller_integr_c4"
#define F_MUEL_S4       "mueller_integr_s4"
    /* temporary files; used in printf with ringid as argument */
#define F_INTFLD_TMP    "f%d.tmp"
#define F_GEOM_TMP      "g%d.tmp"
    /* checkpoint files */
#define F_CHP_LOG       "chp.log"
#define F_CHP           "chp.%d"   /* ringid as argument */

/* default file and dir names; can be changed by command line options */
#define FD_ALLDIR_PARMS "alldir_params.dat"
#define FD_AVG_PARMS    "avg_params.dat"
#define FD_SCAT_PARMS   "scat_params.dat"
#define FD_CHP_DIR      "chpoint"

/**************   Global Defines and Data structures (all for LogError) *****************/

#define POSIT  __FILE__,__LINE__    /* position of the error in source code */
/* who definitions */
#define ALL  0       /* each processor may report this error */
#define ONE  1       /* only root processor reports an error */
   /* derived; for simplicity */
#define ALL_POS  ALL,POSIT
#define ONE_POS  ONE,POSIT
/* error codes */
#define EC_ERROR 1    /* error */
#define EC_WARN  2    /* warning */
#define EC_INFO  3    /* slight warning, that does not interfere at all with normal execution */

#endif /*__const_h*/
