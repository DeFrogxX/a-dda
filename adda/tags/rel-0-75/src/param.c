/* FILE: param.c
 * AUTH: Maxim Yurkin
 * DESCR: Initialization, parsing and handling of input parameters.
 *        Also printout general information. Contains file locking routines.
 *
 * Copyright (C) 2006 University of Amsterdam
 * This code is covered by the GNU General Public License.
 */
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "os.h"
#include "io.h"
#include "const.h"
#include "comm.h"
#include "vars.h"
#include "crosssec.h"
#include "fft.h"
#include "param.h"

/* definitions for file locking */
#ifdef USE_LOCK
# ifdef WINDOWS
#  define FILEHANDLE HANDLE
# elif defined(POSIX)
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>    /* for error handling of fcntl call */
#  define FILEHANDLE int
# else
#  error Unknown operation system. File locking is not supported.
# endif
# define LOCK_WAIT 1              /* in seconds */
# define MAX_LOCK_WAIT_CYCLES 60
#else
# define FILEHANDLE int
#endif

/* GLOBAL VARIABLES */

opt_index opt;   /* main option index */

/* SEMI-GLOBAL VARIABLES */

/* defined and initialized in crosssec.c */
extern const char avg_string[];
/* defined and initialized in GenerateB.c */
extern const char beam_descr[];
/* defined and initialized in make_particle.c */
extern const int volcor_used;
extern const char sh_form_str[];

/* used in CalculateE.c */
int store_int_field; /* save full internal fields to text file */
int store_scat_grid; /* Store the scattered field for grid of angles */
int calc_Cext;       /* Calculate the extinction cross-section - allways do */
int calc_Cabs;       /* Calculate the absorption cross-section - allways do */
int calc_Csca;       /* Calculate the scattering cross-section by integration */
int calc_vec;        /* Calculate the unnormalized asymmetry-parameter */
int calc_asym;       /* Calculate the asymmetry-parameter */
int calc_mat_force;  /* Calculate the scattering force by matrix-evaluation */
int store_force;     /* Write radiation pressure per dipole to file */
int phi_int_type;    /* type of phi integration (each bit determines
                          whether to calculate with different multipliers) */
/* used in calculator.c */
int PolRelation;                 /* type of polarization relation */
int avg_inc_pol;                 /* whether to average CC over incident polarization */
char alldir_parms[MAX_FNAME];    /* name of file with alldir parameters */
char scat_grid_parms[MAX_FNAME]; /* name of file with parameters of scattering grid */
/* used in crosssec.c */
double prop_0[3];  /* initial incident direction (in laboratory reference frame) */
double incPolX_0[3],incPolY_0[3]; /* initial incident polarizations (in lab RF)*/
int ScatRelation;                 /* type of formulae for scattering quantities */
/* used in GenerateB.c */
int beam_Npars;
double beam_pars[MAX_N_BEAM_PARMS]; /* beam parameters */
/* used in io.c */
char logname[MAX_FNAME]="";      /* name of logfile */
/* used in iterative.c */
double eps;                      /* relative error to reach */
/* used in make_particle.c */
int shape;                       /* particle shape definition */
int sh_Npars;                    /* number of shape parameters */
double sh_pars[MAX_N_SH_PARMS];  /* storage for shape parameters */
int symmetry_enforced;           /* enforce use of all symmetries; suppresses NoSymmetry */
double sizeX;                    /* size of particle along x-axis */
double dpl;                      /* number of dipoles per lambda (wavelength) */
double lambda;                   /* incident wavelength (in vacuum) */
int jagged;                      /* size of big dipoles, used to construct a particle */
char aggregate_file[MAX_FNAME];  /* name of aggregate file */
char save_geom_fname[MAX_FNAME]; /* geometry file name to save dipole configuration */
char shapename[MAX_LINE];        /* name of the shape used */
int volcor;                      /* whether to use volume correction */
int save_geom;                   /* whether to save dipole configuration in .geom file */
opt_index opt_sh;                /* option index of shape option used */

/* LOCAL VARIABLES */

static char run_name[MAX_WORD];   /* first part of the dir name ('run' or 'test') */
static char avg_parms[MAX_FNAME]; /* name of file with orientation averaging parameters */
static char *exename;             /* name of executable (adda or adda.exe) */
  /* structure definitions */
struct subopt_struct {
  const char *name;   /* name of option */
  const char *usage;  /* how to use (argument list) */
  const char *help;   /* help string */
  const int narg;     /* possible number of argumetns ; UNDEF -> should not be checked */
  const int type;     /* type of suboption */
};
struct opt_struct {
  const char *name;         /* name of option */
  void (*func)(int Narg,char **argv);  /* pointer to a function, that parse this parameter */
  int used;                 /* flag to indicate, if the option was allready used */
  const char *usage;        /* how to use (argument list) */
  const char *help;         /* help string */
  const int narg;           /* possible number of argumetns ; UNDEF -> should not be checked */
  const struct subopt_struct *sub;  /* suboptions */
};
  /* initializations of suboptions; should be 'NULL terminated' */
static const char exeusage[]="[-<opt1> [<args1>] [-<opt2> [<args2>]...]]";
static const struct subopt_struct beam_opt[]={
  {"plane","","Infinite plane wave",0,B_PLANE},
  {"lminus","<width> [<x> <y> <z>]",
     "Simplest approximation of the Gaussian beam. The beam width is obligatory\n"\
     "and x, y, z coordinates of the center of the beam are optional parameters (all in um).\n"\
     "By default beam center coincides with the center of the computational box.",
     -1,B_LMINUS},
  {"davis3","<width> [<x> <y> <z>]",
     "3rd order approximation of the Gaussian beam (by Davis). The beam width is obligatory\n"\
     "and x, y, z coordinates of the center of the beam are optional parameters (all in um).\n"\
     "By default beam center coincides with the center of the computational box.",
     -1,B_DAVIS3},
  {"barton5","<width> [<x> <y> <z>]",
     "5th order approximation of the Gaussian beam (by Barton). The beam width is obligatory\n"\
     "and x, y, z coordinates of the center of the beam are optional parameters (all in um).\n"\
     "By default beam center coincides with the center of the computational box.\n"\
     "This option is recommended for the description of the Gaussian beam.",
     -1,B_BARTON5},
  {NULL,NULL,NULL,0,0}
};
static const struct subopt_struct shape_opt[]={
  {"box","","Homogenous cube (edges along the axes)",0,SH_BOX},
  {"coated","<d_in/d> [<x/d> <y/d> <z/d>]",
     "Sphere with a spherical inclusion; outer sphere has a diameter d (first domain).\n"\
     "The included sphere has a diameter d_in (optional position of the center: x,y,z).",
     UNDEF,SH_COATED},
  {"cylinder","<h/d>",
     "Homogenous cylinder with height (length) h and diameter d (its axis of symmetry\n"\
     "coincides with the z-axis).",1,SH_CYLINDER},
  {"ellipsoid","<y/x> <z/x>","Homogenous general ellipsoid with semi-axes x,y,z",2,SH_ELLIPSOID},
  {"line","","Line along the x-axis with the width of one dipole",0,SH_LINE},
  {"rbc","<h/d> <b/d> <c/d>",
     "Red Blood Cell, an axisymmetric (over z-axis) biconcave homogenous particle,\n"\
     "which is characterized by diameter d, maximum and minimum width h, b, and\n"\
     "diameter at the position of the maximum width c.",3,SH_RBC},
  {"read","<filename>","Read a particle geometry from file <filename>",1,SH_READ},
  {"sphere","","Homogenous sphere",0,SH_SPHERE},
  {"spherebox","<d_sph/Dx>",
     "Sphere (diameter d_sph) in a cube (size Dx, first domain)",1,SH_SPHEREBOX},
  {NULL,NULL,NULL,0,0}
};

/* EXTERNAL FUNCTIONS */

/* GenerateB.c */
void InitBeam(void);

/*========================================================================*/
   /* declarations of parsing functions; definitions are given below.
      defines are for conciseness */
#define PARSE_NAME(a) parse_##a
#define PARSE_FUNC(a) void PARSE_NAME(a)(int Narg,char **argv)
#define PAR(a) #a,PARSE_NAME(a),FALSE
PARSE_FUNC(alldir_inp);
PARSE_FUNC(asym);
PARSE_FUNC(beam);
PARSE_FUNC(chp_dir);
PARSE_FUNC(chp_load);
PARSE_FUNC(chp_type);
PARSE_FUNC(chpoint);
PARSE_FUNC(Cpr_mat);
PARSE_FUNC(Csca);
PARSE_FUNC(dir);
PARSE_FUNC(dpl);
PARSE_FUNC(eps);
PARSE_FUNC(grid);
PARSE_FUNC(h) ATT_NORETURN;
PARSE_FUNC(int);
PARSE_FUNC(iter);
PARSE_FUNC(jagged);
PARSE_FUNC(lambda);
PARSE_FUNC(m);
PARSE_FUNC(maxiter);
PARSE_FUNC(no_reduced_fft);
PARSE_FUNC(no_vol_cor);
PARSE_FUNC(ntheta);
PARSE_FUNC(orient);
PARSE_FUNC(phi_integr);
PARSE_FUNC(pol);
PARSE_FUNC(prognose);
PARSE_FUNC(prop);
PARSE_FUNC(save_geom);
PARSE_FUNC(scat);
PARSE_FUNC(scat_grid_inp);
PARSE_FUNC(shape);
PARSE_FUNC(size);
PARSE_FUNC(store_force);
PARSE_FUNC(store_int_field);
PARSE_FUNC(store_scat_grid);
PARSE_FUNC(sym);
PARSE_FUNC(test);
PARSE_FUNC(vec);
PARSE_FUNC(yz);
   /* initialization of options, their usage and help */
static struct opt_struct options[]={
  {PAR(alldir_inp),"<filename>",
     "Specifies a file with parameters of the grid of scattering angles\n"\
     "for calculating integral scattering quantities.\n"\
     "Default: " FD_ALLDIR_PARMS,1,NULL},
  {PAR(asym),"","Calculate the asymmetry vector. Implies '-Csca' and '-vec'",0,NULL},
  {PAR(beam),"<type> [<arg1>...]",
     "Sets a type of the incident beam. Four other float arguments must be specified\n"\
     "for all beam types except 'plane'. These are the width and x, y, z coordinates\n"\
     "of the center of the beam respectively (all in um).\n"\
     "Default: plane",UNDEF,beam_opt},
  {PAR(chp_dir),"<dirname>",
     "Sets directory for the checkpoint (both for saving and loading).\n"\
     "Default: " FD_CHP_DIR,1,NULL},
  {PAR(chp_load),"","Restart a simulation from a checkpoint",0,NULL},
  {PAR(chp_type),"{normal|regular|always}",
     "Sets type of the checkpoint. All types, except 'always', require '-chpoint'.\n"\
     "Default: normal",1,NULL},
  {PAR(chpoint),"<time>",
     "Specifies the time for checkpoints in format '#d#h#m#s'. All fields are optional,\n"\
     "numbers are integers, 's' can be omitted, the format is not case sensitive.\n"\
     "Examples: 12h30M, 1D10s, 3600",1,NULL},
  {PAR(Cpr_mat),"","Calculate the total radiation force",0,NULL},
  {PAR(Csca),"","Calculate scattering cross section (by integrating the scattered field)",0,NULL},
  {PAR(dir),"<dirname>",
     "Sets directory for output files.\n"\
     "Default: constructed automatically",1,NULL},
  {PAR(dpl),"<arg>",
     "Sets parameter 'dipoles per lambda', float.\n"\
     "Default: 10|m|, where 'm' is the first refractive index\n"\
     "         specified by the '-m' option.",1,NULL},
  {PAR(eps),"<arg>",
     "Specifies the stopping criterion for the iterative solver by setting the\n"\
     "relative error of the residual 'epsilon' to reach. <arg> is an exponent\n"\
     "of base 10 (float), i.e. epsilon=10^(-<arg>).\n"\
     "Default: 5 (epsilon=1E-5)",1,NULL},
  {PAR(grid),"<nx> [<ny> <nz>]",
     "Sets dimensions of the computation grid. Arguments should be even integers.\n"\
     "In most cases <ny> and <nz> can be omitted (they are automatically determined\n"\
     "by <nx> based on the proportions of the scatterer). This command line option\n"\
     "is not relevant when particle geometry is read from a file ('-shape read').\n"\
     "If '-jagged' option is used the grid dimension is effectively multiplied\n"\
     "by the specified number.\n"\
     "Default: 16 (if  size is not specified) or defined by\n"\
     "         '-size', '-lambda', and '-dpl'.",UNDEF,NULL},
  {PAR(h),"[<opt> [<subopt>]]",
     "Shows help. If used without arguments, ADDA shows a list of all available\n"\
     "command line options. If first argument is specified, help on specific command\n"\
     "line option <opt> is shown (only the name of the option should be given\n"\
     "without preceding dash). For some options (e.g. '-beam' or '-shape') specific\n"\
     "help on a particular suboption <subopt> may be shown.\n"\
     "Example: shape coated",UNDEF,NULL},
  {PAR(int),"{poi|so}",
     "Sets prescription to calculate interaction term. 'so' is under development.\n"\
     "Default: poi",1,NULL},
  {PAR(iter),"{cgnr|bicg|bicgstab|qmr}",
     "Sets the iterative solver.\n"\
     "Default: qmr",1,NULL},
  {PAR(jagged),"<arg>",
     "Sets a size of a big dipole in units of small dipoles, integer. It is used\n"\
     "to improve the discretization of the particle without changing the shape.\n"\
     "Default: 1",1,NULL},
  {PAR(lambda),"<arg>",
     "Sets incident wavelength in um, float.\n"\
     "Default: 2*pi",1,NULL},
  {PAR(m),"<m1Re> <m1Im> [<m1Re> <m1Im>...]",
     "Sets refractive indices, float. Each pair of arguments specifies real and\n"\
     "imaginary part of the refractive index of one of the domains. Maximum number\n"\
     "of different refractive indices (particle domains) is defined at compilation\n"
     "time by the parameter MAX_NMAT in file const.h (by default, 10).\n"\
     "Default: 1.5 0",UNDEF,NULL},
  {PAR(maxiter),"<arg>",
     "Sets the maximum number of iterations of the iterative solver, integer.\n"\
     "Default: very large, not realistic value",1,NULL},
  {PAR(no_reduced_fft),"",
     "Do not use symmetry of the interaction matrix to reduce the storage space\n"\
     "for the Fourier-transformed matrix.",0,NULL},
  {PAR(no_vol_cor),"",
     "Do not use 'dpl correction', which ensures (if used) that the volume of\n"\
     "the dipole representation of the particle is exactly correct.",0,NULL},
  {PAR(ntheta),"<arg>",
     "Sets the number of intervals into which range of scattering angles [0,180]\n"\
     "is equally divided, integer. This is used for scattering angles in yz-plane.\n"\
     "If particle is not symmetric and orientation averaging is not used, the range\n"\
     "is extended to 360 degrees (with the same length of elementary interval).\n"\
     "Default: from 90 to 720 depending on the size of the computational grid.",1,NULL},
  {PAR(orient),"{<alpha> <beta> <gamma>|avg [<filename>]}",
     "Either sets an orientation of the particle by three Euler angles 'alpha',\n"\
     "'beta','gamma' or specifies that orientation averaging should be performed .\n"\
     "<filename> sets a file with parameters for orientation averaging.\n"\
     "Default orientation: 0 0 0\n"\
     "Default <filename>: " FD_AVG_PARMS,UNDEF,NULL},
  {PAR(phi_integr),"<arg>",
     "Turns on and specifies the type of Mueller matrix integration over azimuthal\n"\
     "angle 'phi'. <arg> is an integer from 1 to 31, each bit of which, from lowest\n"\
     "to highest, indicates whether the integration should be performed with\n"\
     "multipliers 1, cos(2*phi), sin(2*phi), cos(4*phi), and sin(4*phi)\n"\
     "respectively.\n"\
     "Examples: 1 (one integration with no multipliers),\n"\
     "          6 (two integration with cos(2*phi) and sin(2*phi) multipliers).",1,NULL},
  {PAR(pol),"{cm|rrc|ldr [avgpol]|cldr|so}",
     "Type of polarization prescription. An optional flag 'avg' can be added for LDR\n"\
     "- it specifies that LDR polarizability should be averaged over incident\n"\
     "polarizations. 'so' is under development.\n"\
     "Default: ldr (without averaging).",UNDEF,NULL},
  {PAR(prognose),"",
     "Do not actually perform simulation (not even memory allocation) but only\n"\
     "estimate the required RAM. Implies '-test'.",0,NULL},
  {PAR(prop),"<x> <y> <z>",
     "Sets propagation direction of incident radiation, float. Normalization\n"\
     "(to the unity vector) is performed automatically.\n"\
     "Default: 0 0 1",3,NULL},
  {PAR(save_geom),"[<filename>]",
     "Saves dipole configuration to a file <filename> (a path relative to the\n"\
     "output directory). Can be used with '-prognose'.\n"\
     "Default: <type>.geom (<type> is a first argument to the '-shape' option).",UNDEF,NULL},
  {PAR(scat),"{dr|so}",
     "Sets prescription to calculate scattering quantities.\n"\
     "'so' is under development.\n"\
     "Default: dr",1,NULL},
  {PAR(scat_grid_inp),"<filename>",
     "Specifies a file with parameters of the grid of scattering angles for\n"\
     "calculating Mueller matrix (possibly integrated over 'phi').\n"\
     "Default: " FD_SCAT_PARMS,1,NULL},
  {PAR(shape),"<type> [<arg1>...]",
     "Sets shape of the particle, either predefined or 'read' from file.\n"\
     "All the parameters of predefined shapes are floats.\n"\
     "Default: sphere",UNDEF,shape_opt},
  {PAR(size),"<arg>",
     "Sets the size of the computational grid along the x-axis in um, float.\n"\
     "Default: determined by the values of '-grid', '-dpl', and '-lambda'.",1,NULL},
  {PAR(store_force),"","Calculate the radiation force on each dipole. Requires '-Cpr_mat'.",0,NULL},
  {PAR(store_int_field),"","Save internal fields to a file",0,NULL},
  {PAR(store_scat_grid),"",
     "Calculate Mueller matrix for a grid of scattering angles and save it to a file.",0,NULL},
  {PAR(sym),"{no|enf}",
     "Do not take into account ('no') or enforce ('enf') all particle symmetries",1,NULL},
  {PAR(test),"","Begin name of the output directory with 'test' instead of 'run'",0,NULL},
  {PAR(vec),"","Calculate the not-normalized asymmetry vector",0,NULL},
  {PAR(yz),"",
     "Calculate the Mueller matrix in yz-plane even if it is calculated for a\n"\
     "scattering grid. If the latter option is not enabled, scattering in yz-plane\n"\
     "is always calculated.",0,NULL}
};
      /* auxiliary functions */
/*============================================================*/

static const char *OptionName(void)
   /* produces full option name for error messages */
{
   static char buf[MAX_LINE];

   if (opt.l2==UNDEF) return options[opt.l1].name;
   else {
     sprintf(buf,"%s %s",options[opt.l1].name,options[opt.l1].sub[opt.l2].name);
     return buf;
   }
}

/*============================================================*/

void PrintErrorHelp(const char *fmt, ... )
   /* print anything to stderr (on root processor), then help on the arguments used, and stop;
      assumes that all processors call it */
{
  va_list args;
  const char *optname,*use;

  if (ringid==ROOT) {
    /* produce error message */
    va_start(args,fmt);
    fprintf(stderr,"ERROR: ");
    vfprintf(stderr,fmt,args);
    fprintf(stderr,"\n");
    va_end(args);
    /* add help message */
    if (opt.l1==UNDEF)     /* no option is found */
      fprintf(stderr,"Usage: %s %s\n"\
                     "Type '%s -h' for help\n",exename,exeusage,exename);
    else {  /* at least option is found */
      if (opt.l2==UNDEF) use=options[opt.l1].usage;
      else use=options[opt.l1].sub[opt.l2].usage;
      optname=OptionName();
      fprintf(stderr,"Usage: -%s %s\n"\
                     "Type '%s -h %s' for details\n",optname,use,exename,optname);
    }
    fflush(stderr);
  }
  /* wait for root to generate an error message */
  Synchronize();
  Stop(1);
}

/*============================================================*/

static void NargError(const int Narg,const char *expec)
      /* Print error of illegal number of arguments to an option (suboption);
         and display correct usage information */
{
  char buf[MAX_WORD]; /* not to allocate memory if needed */

  if (expec==NULL) {
    if (opt.l2==UNDEF) sprintf(buf,"%d",options[opt.l1].narg);
    else sprintf(buf,"%d",options[opt.l1].sub[opt.l2].narg);
    expec=buf;
  }
  PrintErrorHelp("Illegal number of arguments (%d) to '-%s' option (%s expected)",
                 Narg,OptionName(),expec);
}

/*============================================================*/
                /* following two functions are interfaces to NargError */
INLINE void TestNarg(const int Narg)
     /* check if Narg given to an option is correct */
{
  if (options[opt.l1].narg!=UNDEF && Narg!=options[opt.l1].narg)
    NargError(Narg,NULL);
}

/*============================================================*/

INLINE void TestNarg_sub(const int Narg)
     /* check if Narg given to a suboption is correct */
{
  if (options[opt.l1].sub[opt.l2].narg!=UNDEF && Narg!=options[opt.l1].sub[opt.l2].narg)
    NargError(Narg,NULL);
}

/*============================================================*/

static void NotSupported(const char *type,const char *given)
     /* print error message that "type 'given' is not supported"
        type should start with a capital letter */
{
  PrintErrorHelp("%s '%s' is not supported",type,given);
}

/*============================================================*/

INLINE void TestStrLength(const char *str,const unsigned int size)
    /* check if string fits in buffer of size 'size', otherwise produces error message
       'opt' is command line option that checks its argument */
{
  if (strlen(str)>=size)
    PrintErrorHelp("Too long argument to '-%s' option (only %ud chars allowed).\n"\
                   "If you really need it you may increase MAX_DIRNAME in const.h and recompile",
                   OptionName(),size-1);
}

/*============================================================*/

INLINE void ScanfError(const char *str,const char *fmt,void *val)
    /* scanf an option argument and checks for errors */
{
  if (sscanf(str,fmt,val)!=1)
    PrintErrorHelp("Non-numeric argument (%s) is given to option '-%s'",str,OptionName());
}

/*============================================================*/

INLINE int IsOption(const char *str)
   /* checks if string is an option. First should be '-' and then letter (any case);
      it enables use of negative numbers as subparameters */
{
  return (str[0]=='-' && isalpha(str[1]));
}
/*============================================================*/

static int TimeField(const char c)
   /* analyze one time multiplier */
{
  if (c=='d' || c=='D') return 86400;
  else if (c=='h' || c=='H') return 3600;
  else if (c=='m' || c=='M') return 60;
  else if (c=='s' || c=='S' || c==0) return 1;
  else PrintErrorHelp("Illegal time format specifier (%c)",c);
  /* never reached */
  return 0;
}

/*============================================================*/

static int ScanTime(const char *str)
   /* scans time in seconds from a string "%d[d,D[%d]][h,H[%d]][m,M[%d]][s,S] */
{
#define TIME_N_TYPES 4   /* not so easy to change */
  int tim,t[TIME_N_TYPES],n,i;
  char c[TIME_N_TYPES];

  for (i=0;i<TIME_N_TYPES;i++) c[i]=0;
  n=sscanf(str,"%d%c%d%c%d%c%d%c",t,c,t+1,c+1,t+2,c+2,t+3,c+3);
  if (n<1) PrintErrorHelp("Wrong time format '%s'",str);
  tim=0;
  i=0;
  while (n>0) {
    tim+=t[i]*TimeField(c[i]);
    n-=2;
    i++;
  }
  return tim;
#undef TIME_N_TYPES
}

/*============================================================*/

static void PrintTime(char *s,const time_t *time_ptr)
{
   struct tm *t;

   t=gmtime(time_ptr);
   s[0]=0; /* initialize string */
   if (t->tm_yday>0) sprintf(s,"%dd ",t->tm_yday);
   if (t->tm_hour>0) sprintf(s+strlen(s),"%dh ",t->tm_hour);
   if (t->tm_min>0) sprintf(s+strlen(s),"%dm ",t->tm_min);
   if (t->tm_sec>0) sprintf(s+strlen(s),"%ds ",t->tm_sec);
}

/*========================================================================*/
   /* parsing functions definitions*/
PARSE_FUNC(alldir_inp)
{
  TestStrLength(argv[1],MAX_FNAME);
  strcpy(alldir_parms,argv[1]);
}
PARSE_FUNC(asym)
{
  calc_asym = TRUE;
  calc_vec = TRUE;
  calc_Csca = TRUE;
}
PARSE_FUNC(beam)
{
  int i,j,found;

  Narg--;
  found=FALSE;
  i=-1;
  while (beam_opt[++i].name!=NULL) if (strcmp(argv[1],beam_opt[i].name)==0) {
    /* set suboption and beamtype */
    opt.l2=i;
    beamtype=beam_opt[i].type;
    beam_Npars=Narg;
    /* check number of arguments */
    TestNarg_sub(Narg);
    if (beamtype!=B_PLANE) {
      if (Narg!=1 && Narg!=4) NargError(Narg,"1 or 4");
    }
    /* parse and check consistency */
    for (j=0;j<Narg;j++) ScanfError(argv[j+2],"%lf",beam_pars+j);
    if (Narg>0) TestPositive(beam_pars[0],"beam width");
    /* stop search */
    found=TRUE;
    break;
  }
  if(!found) NotSupported("Beam type",argv[1]);
}
PARSE_FUNC(chp_dir)
{
  TestStrLength(argv[1],MAX_DIRNAME);
  strcpy(chp_dir,argv[1]);
}
PARSE_FUNC(chp_load)
{
  load_chpoint = TRUE;
}
PARSE_FUNC(chp_type)
{
  if (strcmp(argv[1],"normal")==0) chp_type=CHP_NORMAL;
  else if (strcmp(argv[1],"regular")==0) chp_type=CHP_REGULAR;
  else if (strcmp(argv[1],"always")==0) chp_type=CHP_ALWAYS;
  else NotSupported("Checkpoint type",argv[1]);
}
PARSE_FUNC(chpoint)
{
  chp_time=ScanTime(argv[1]);
  if (chp_time<=0) {
    chp_time=UNDEF;
    if (chp_type==CHP_NONE) chp_type=CHP_ALWAYS;
  }
  else if (chp_type==CHP_NONE) chp_type=CHP_NORMAL;
}
PARSE_FUNC(Cpr_mat)
{
  calc_mat_force = TRUE;
}
PARSE_FUNC(Csca)
{
  calc_Csca = TRUE;
}
PARSE_FUNC(dir)
{
  TestStrLength(argv[1],MAX_DIRNAME);
  strcpy(directory,argv[1]);
}
PARSE_FUNC(dpl)
{
  ScanfError(argv[1],"%lf",&dpl);
  TestPositive(dpl,"dpl");
}
PARSE_FUNC(eps)
{
  double tmp;

  ScanfError(argv[1],"%lf",&tmp);
  TestPositive(tmp,"eps exponent");
  eps=pow(10,-tmp);
}
PARSE_FUNC(grid)
{
  if (Narg!=1 && Narg!=3) NargError(Narg,"1 or 3");
  ScanfError(argv[1],"%i",&boxX);  /* boxes are further multiplied by jagged if needed */
  TestPositive_i(boxX,"gridX");
  if (Narg==3) {
    ScanfError(argv[2],"%i",&boxY);
    TestPositive_i(boxY,"gridY");
    ScanfError(argv[3],"%i",&boxZ);
    TestPositive_i(boxY,"gridY");
  }
}
PARSE_FUNC(h)
{
  int i,j,found;

  if (Narg>2) NargError(Narg,"not more than 2");
  /* do all output on root processor */
  if (ringid==ROOT) {
    found=FALSE;
    if (Narg>=1) {
      for(i=0;i<LENGTH(options);i++) if (strcmp(argv[1],options[i].name)==0) {
        if (Narg==2) {
          j=-1;
          while (options[i].sub[++j].name!=NULL) if (strcmp(argv[2],options[i].sub[j].name)==0) {
            printf("  -%s %s %s\n%s\n",options[i].name,options[i].sub[j].name,
                   options[i].sub[j].usage,options[i].sub[j].help);
            found=TRUE;
            break;
          }
          if (!found) printf("No help is available for suboption '%s'\n",argv[2]);
        }
        if (!found) {
          printf("  -%s %s\n%s\n",options[i].name,options[i].usage,options[i].help);
          if (options[i].sub!=NULL) {
            printf("Available suboptions:\n");
            j=-1;
            while (options[i].sub[++j].name!=NULL)
              printf("  %s %s\n",options[i].sub[j].name,options[i].sub[j].usage);
            printf("Type '%s -h %s <subopt>' for details\n",exename,options[i].name);
          }
        }
        found=TRUE;
        break;
      }
      if (!found) printf("Unknown option '%s'\n",argv[1]);
    }
    if (!found) {
      printf("Usage: '%s %s'\n"\
             "Available options:\n",exename,exeusage);
      for (i=0;i<LENGTH(options);i++) printf("  -%s %s\n",options[i].name,options[i].usage);
      printf("Type '%s -h <opt>' for details\n",exename);
    }
  }
  /* exit */
  Stop(0);
}
PARSE_FUNC(int)
{
  if (strcmp(argv[1],"poi")==0) IntRelation=G_POINT_DIP;
  else if (strcmp(argv[1],"so")==0) IntRelation=G_SO;
  else NotSupported("Interaction term prescription",argv[1]);
}
PARSE_FUNC(iter)
{
  if (strcmp(argv[1],"cgnr")==0) IterMethod=IT_CGNR;
  else if (strcmp(argv[1],"bicgstab")==0) IterMethod=IT_BICGSTAB;
  else if (strcmp(argv[1],"bicg")==0) IterMethod=IT_BICG_CS;
  else if (strcmp(argv[1],"qmr")==0) IterMethod=IT_QMR_CS;
  else NotSupported("Iterative method",argv[1]);
}
PARSE_FUNC(jagged)
{
  ScanfError(argv[1],"%d",&jagged);
  TestPositive_i(jagged,"jagged");
}
PARSE_FUNC(lambda)
{
  ScanfError(argv[1],"%lf",&lambda);
  TestPositive(lambda,"wavelength");
}
PARSE_FUNC(m)
{
  int i;

  if (Narg%2!=0 || Narg==0) NargError(Narg,"even");
  Nmat=Narg/2;
  if (Nmat>MAX_NMAT)
    PrintErrorHelp("Too many materials (%d), maximum %d are supported.\n"\
                   "You may increase parameter MAX_NMAT in const.h and recompile",Nmat,MAX_NMAT);
  for (i=0;i<Nmat;i++) {
    ScanfError(argv[2*i+1],"%lf",&ref_index[i][RE]);
    ScanfError(argv[2*i+2],"%lf",&ref_index[i][IM]);
  }
}
PARSE_FUNC(maxiter)
{
  ScanfError(argv[1],"%i",&maxiter);
  TestPositive_i(maxiter,"maximum number of iterations");
}
PARSE_FUNC(no_reduced_fft)
{
  reduced_FFT=FALSE;
}
PARSE_FUNC(no_vol_cor)
{
  volcor=FALSE;
}
PARSE_FUNC(ntheta)
{
  ScanfError(argv[1],"%i",&nTheta);
  TestPositive_i(nTheta,"number of theta intervals");
  nTheta++;
}
PARSE_FUNC(orient)
{
  if (Narg==0) NargError(Narg,"at least 1");
  if (strcmp(argv[1],"avg")==0) {
    if (Narg>2) PrintErrorHelp(
      "Illegal number of arguments (%d) to '-orient avg' option (0 or 1 expected)",Narg-1);
    orient_avg=TRUE;
    if (Narg==2) {
      TestStrLength(argv[2],MAX_FNAME);
      strcpy(avg_parms,argv[2]);
    }
  }
  else {
    if (Narg!=3) NargError(Narg,"3");
    ScanfError(argv[1],"%lf",&alph_deg);
    ScanfError(argv[2],"%lf",&bet_deg);
    ScanfError(argv[3],"%lf",&gam_deg);
  }
}
PARSE_FUNC(phi_integr)
{
  phi_integr = TRUE;
  ScanfError(argv[1],"%d",&phi_int_type);
  TestRange_i(phi_int_type,"type of integration over phi",1,31);
}
PARSE_FUNC(pol)
{
  if (Narg!=1 && Narg!=2) NargError(Narg,"1 or 2");
  if (strcmp(argv[1],"cm")==0) PolRelation=POL_CM;
  else if (strcmp(argv[1],"rrc")==0) PolRelation=POL_RR;
  else if (strcmp(argv[1],"ldr")==0) PolRelation=POL_LDR;
  else if (strcmp(argv[1],"cldr")==0) PolRelation=POL_CLDR;
  else if (strcmp(argv[1],"so")==0) PolRelation=POL_SO;
  else NotSupported("Polarization relation",argv[1]);
  if (Narg==2) {
    if (strcmp(argv[2],"avgpol")==0) avg_inc_pol=TRUE;
    else PrintErrorHelp("Unknown argument '%s' to '-pol %s' option",argv[2],argv[1]);
  }
}
PARSE_FUNC(prognose)
{
  prognose=TRUE;
  strcpy(run_name,"test");
}
PARSE_FUNC(prop)
{
  double tmp;

  ScanfError(argv[1],"%lf",prop_0);
  ScanfError(argv[2],"%lf",&prop_0[1]);
  ScanfError(argv[3],"%lf",&prop_0[2]);
  tmp=DotProd(prop_0,prop_0);
  if (tmp==0) PrintErrorHelp("Given propagation vector is null");
  tmp=1/sqrt(tmp);
  prop_0[0]*=tmp;
  prop_0[1]*=tmp;
  prop_0[2]*=tmp;
}
PARSE_FUNC(save_geom)
{
  if (Narg>1) NargError(Narg,"0 or 1");
  save_geom=TRUE;
  if (Narg==1) {
    TestStrLength(argv[1],MAX_FNAME);
    strcpy(save_geom_fname,argv[1]);
  }
}
PARSE_FUNC(scat)
{
  if (strcmp(argv[1],"dr")==0) ScatRelation=SQ_DRAINE;
  else if (strcmp(argv[1],"so")==0) ScatRelation=SQ_SO;
  else NotSupported("Scattering quantities relation",argv[1]);
}
PARSE_FUNC(scat_grid_inp)
{
  TestStrLength(argv[1],MAX_FNAME);
  strcpy(scat_grid_parms,argv[1]);
}
PARSE_FUNC(shape)
{
  int i,j,found;

  Narg--;
  found=FALSE;
  i=-1;
  while (shape_opt[++i].name!=NULL) if (strcmp(argv[1],shape_opt[i].name)==0) {
    /* set shape and shape option index */
    shape=shape_opt[i].type;
    opt.l2=i;
    opt_sh=opt;
    sh_Npars=Narg;
    /* check number of arguments */
    TestNarg_sub(Narg);
    if (shape==SH_COATED) {
      if (Narg!=1 && Narg!=4) NargError(Narg,"1 or 4");
    }
    /* parse; consistency of shape arguments is checked in InitShape() */
    if (shape==SH_READ) {
      TestStrLength(argv[2],MAX_FNAME);
      strcpy(aggregate_file,argv[2]);
    }
    else for (j=0;j<Narg;j++) ScanfError(argv[j+2],"%lf",sh_pars+j);
    /* stop search */
    found=TRUE;
    break;
  }
  if(!found) NotSupported("Shape type",argv[1]);
  /* set shapename; takes place only if shapename was matched above */
  strcpy(shapename,argv[1]);
}
PARSE_FUNC(size)
{
  ScanfError(argv[1],"%lf",&sizeX);
  TestPositive(sizeX,"particle size");
}
PARSE_FUNC(store_force)
{
  store_force = TRUE;
}
PARSE_FUNC(store_int_field)
{
  store_int_field=TRUE;
}
PARSE_FUNC(store_scat_grid)
{
  store_scat_grid = TRUE;
}
PARSE_FUNC(sym)
{
  if (strcmp(argv[1],"no")==0) NoSymmetry=TRUE;
  else if (strcmp(argv[1],"enf")==0) symmetry_enforced=TRUE;
  else NotSupported("Symmetry option",argv[1]);
}
PARSE_FUNC(test)
{
  strcpy(run_name,"test");
}
PARSE_FUNC(vec)
{
  calc_vec = TRUE;
}
PARSE_FUNC(yz)
{
  yzplane = TRUE;
}
#undef PAR
#undef PARSE_FUNC
#undef PARSE_NAME

       /* end of parsing functions */
/*=============================================================*/

static FILEHANDLE CreateLockFile(const char *fname)
   /* create locks file
      works only if USE_LOCK is enabled */
{
#ifdef USE_LOCK
  FILEHANDLE fd;
  int i;

# ifdef WINDOWS
  i=0;
  while ((fd=CreateFile(fname,GENERIC_WRITE,FILE_SHARE_WRITE,NULL,CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL,NULL))==INVALID_HANDLE_VALUE) {
    Sleep(LOCK_WAIT*1000);
    if (i++ == MAX_LOCK_WAIT_CYCLES)
      LogError(EC_ERROR,ONE_POS,"Lock file %s permanently exists",fname);
  }
# elif defined(POSIX)
  struct flock lock;

  /* open file exclusively */
  i=0;
  while ((fd=open(fname,O_WRONLY | O_CREAT | O_EXCL,0666))==-1) {
    sleep(LOCK_WAIT);
    if (i++ == MAX_LOCK_WAIT_CYCLES)
      LogError(EC_ERROR,ONE_POS,"Lock file %s permanently exists",fname);
  }
  /* specify lock - file is additionally locked to work robustly over NFS */
  lock.l_type=F_WRLCK;
  lock.l_whence=SEEK_SET;
  lock.l_start=0;
  lock.l_len=0;
  /* obtain lock; fcntl waits but it can be interrupted by a system signal */
  i=0;
  while (fcntl(fd,F_SETLKW,&lock)==-1) {
    if (errno!=EINTR) LogError(EC_ERROR,ONE_POS,"Obtaining file lock failed");
    sleep(LOCK_WAIT);
    if (i++ == MAX_LOCK_WAIT_CYCLES)
      LogError(EC_ERROR,ONE_POS,"Lock file %s permanently exists",fname);
  }
# endif
  /* return file handle */
  return fd;
#else
  return 0;
#endif
}

/*============================================================*/

static void RemoveLockFile(FILEHANDLE fd,const char *fname)
   /* closes and remove lock file
      works only if USE_LOCK is enabled */
{
#ifdef USE_LOCK
# ifdef WINDOWS
  /* close file */
  CloseHandle(fd);
# elif defined(POSIX)
  /* close file; all locks are automatically released */
  close(fd);
# endif
  /* remove lock file */
  RemoveErr(fname,ONE_POS);
#endif
}

/*============================================================*/

void InitVariables(void)
   /* some defaults are specified also in const.h */
{
  /* defaults */
  prop_0[0]=0;        /* by default beam propagates along z-axis */
  prop_0[1]=0;
  prop_0[2]=1;
  directory[0]=0;
  lambda=2*PI;
    /* initialize ref_index of scatterer */
  Nmat=1;
  ref_index[0][RE]=1.5;
  ref_index[0][IM]=0.0;
    /* initialize to null to determine further whether it is initialized */
  logfile=NULL;
  logname[0]=0;

  boxX=boxY=boxZ=UNDEF;
  sizeX=UNDEF;
  dpl=UNDEF;
  strcpy(run_name,"run");
  nTheta=UNDEF;
  eps=1E-5;
  shape=SH_SPHERE;
  strcpy(shapename,"sphere");
  store_int_field=FALSE;
  PolRelation=POL_LDR;
  ScatRelation=SQ_DRAINE;
  IntRelation=G_POINT_DIP;
  IterMethod=IT_QMR_CS;
  NoSymmetry=FALSE;
  symmetry_enforced=FALSE;
  prognose=FALSE;
  maxiter=UNDEF;
  jagged=1;
  beamtype=B_PLANE;
  strcpy(alldir_parms,FD_ALLDIR_PARMS);
  strcpy(avg_parms,FD_AVG_PARMS);
  strcpy(scat_grid_parms,FD_SCAT_PARMS);
  strcpy(chp_dir,FD_CHP_DIR);
  chp_time=UNDEF;
  chp_type=CHP_NONE;
  orient_avg=FALSE;
  alph_deg=bet_deg=gam_deg=0.0;
  volcor=TRUE;
  reduced_FFT=TRUE;
  save_geom=FALSE;
  save_geom_fname[0]=0;
  yzplane=UNDEF;
  all_dir=FALSE;
  scat_grid=FALSE;
  phi_integr=FALSE;
  store_scat_grid=FALSE;
  calc_Cext=TRUE;
  calc_Cabs=TRUE;
  calc_Csca=FALSE;
  calc_vec=FALSE;
  calc_asym=FALSE;
  calc_mat_force=FALSE;
  store_force=FALSE;
  load_chpoint=FALSE;
  memory=0;
}

/*============================================================*/

void ParseParameters(const int argc,char **argv)
  /* parses input parameters */
{
  int i,j,Narg;
  int found;
  char *p1,*p2;

  /* get name of executable; remove all path overhead */
  if ((p1=strrchr(argv[0],'\\'))==NULL) p1=argv[0];
  if ((p2=strrchr(argv[0],'/'))==NULL) p2=argv[0];
  exename=MAX(p1,p2)+1;
  /* initialize option */
  opt.l1=UNDEF;
  /* check first argument */
  if (argc>1 && !IsOption(argv[1]))
    PrintErrorHelp("Illegal format of first argument '%s'",argv[1]);
  /* read command line */
  for (i=1;i<argc;i++) {
    /* get number of arguments */
    Narg=0;
    while ((i+(++Narg))<argc && !IsOption(argv[i+Narg]));
    Narg--;

    argv[i]++; /* shift to remove "-" in the beginning of the string */
    found=FALSE;
    opt.l1=opt.l2=UNDEF;
    for (j=0;j<LENGTH(options);j++) if (strcmp(argv[i],options[j].name)==0) {
      opt.l1=j;
      /* check consistency, if enabled for this parameter */
      TestNarg(Narg);
      /* parse this parameter */
      (*options[j].func)(Narg,argv+i);
      /* check duplicate options */
      if (options[j].used) PrintError("Option '-%s' is used more than once",argv[i]);
      else options[j].used=TRUE;
      /* stop search */
      found=TRUE;
      break;
    }
    if(!found) PrintErrorHelp("Unknown option '-%s'",argv[i]);
    argv[i]--; /* shift back */
    i+=Narg;
  }  /* end of reading command line arguments */
}

/*============================================================*/

void VariablesInterconnect(void)
  /* finish parameters initialization based on their interconnections */
{
  double temp;

  /* initialize WaveNum ASAP */
  WaveNum  = TWO_PI/lambda;
  /* parameter interconnections */
  if (prop_0[2]!=1 && orient_avg) PrintError("'-prop' and '-orient avg' can not be used together");
  if (chp_time==UNDEF && chp_type!=CHP_NONE && chp_type!=CHP_ALWAYS)
    PrintError("You must specify time for this checkpoint type");
  if (IntRelation==G_SO) reduced_FFT=FALSE;
  /* scale boxes by jagged */
  if (jagged!=1) {
    if (boxX!=UNDEF) boxX*=jagged;
    if (boxY!=UNDEF) boxY*=jagged;
    if (boxZ!=UNDEF) boxZ*=jagged;
  }
  if (calc_Csca || calc_vec) all_dir = TRUE;
  if (store_scat_grid || phi_integr) {
    scat_grid = TRUE;
    if (yzplane==UNDEF) yzplane = FALSE;
  }
  else if (yzplane==UNDEF) yzplane = TRUE;

  /*determine two incident polarizations. Equivalent to rotation of X,Y,Z basis by
  angles Theta and Phi from (0,0,1) to given propagation vector */
  if (fabs(prop_0[2])>=1) {     /* can not be >1 except for machine precision */
    incPolX_0[0]=prop_0[2];
    incPolY_0[1]=1;
    incPolX_0[1]=incPolX_0[2]=incPolY_0[0]=incPolY_0[2]=0.0;
  }
  else {
    temp=sqrt(1-prop_0[2]*prop_0[2]);
    incPolX_0[0]=prop_0[0]*prop_0[2]/temp;
    incPolX_0[1]=prop_0[1]*prop_0[2]/temp;
    incPolX_0[2]=-temp;
    incPolY_0[0]=-prop_0[1]/temp;
    incPolY_0[1]=prop_0[0]/temp;
    incPolY_0[2]=0.0;
  }
  /* initialize beam description */
  InitBeam();
  /* initialize averaging over orientation */
  if (orient_avg) {
    ReadAvgParms(avg_parms);
    NoSymmetry=TRUE;
    avg_inc_pol=TRUE;
  }
  else {
    /* else - initialize rotation stuff */
    InitRotation();
    if (prop[2]!=1) NoSymmetry=TRUE;
  }
}

/*============================================================*/

void DirectoryLog(const int argc,char **argv)
   /* create input directory and start logfile */
{
  int  i,Nexp;
  FILE *Nexpfile;
  char sbuffer[MAX_LINE];
  char *ptmp;
  FILEHANDLE lockid;
#ifdef PARALLEL
  char *ptmp2;
#endif  
#ifdef WINDOWS  /* for obtaining computer name */
  TCHAR cname[MAX_COMPUTERNAME_LENGTH+1];
  DWORD cname_size=MAX_COMPUTERNAME_LENGTH+1;
#endif

  /* devise directory name (for output files) */
  if (directory[0]==0) {
    /* ROOT processor works with ExpCount */
    if (ringid==ROOT) {
      /* lock file */
      lockid=CreateLockFile(F_EXPCOUNT_LCK);
      /* read ExpCount */
      if ((Nexpfile=fopen(F_EXPCOUNT,"r"))!=NULL) {
        if (fscanf(Nexpfile,"%i",&Nexp)!=1) Nexp=0;
        FCloseErr(Nexpfile,F_EXPCOUNT,ONE_POS);
      }
      else Nexp=0;
      /* put new number in Nexpfile */
      Nexpfile=FOpenErr(F_EXPCOUNT,"w",ONE_POS);
      fprintf(Nexpfile,"%i",Nexp+1);
      FCloseErr(Nexpfile,F_EXPCOUNT,ONE_POS);
      /* unlock */
      RemoveLockFile(lockid,F_EXPCOUNT_LCK);
    }
    /* cast Nexp to all processors */
    MyBcast(&Nexp,int_type,1);
    /* create directory name */
    sprintf(sbuffer,"m%.4g",ref_index[0][RE]);
    ptmp=strchr(sbuffer,'.');
    if (ptmp!=NULL) *ptmp='_';
    sprintf(directory,"%s%03i_%s_g%i%s",run_name,Nexp,shapename,boxX,sbuffer);
#ifdef PARALLEL
    /* add PBS or SGE job id to the directory name if available */
    if ((ptmp=getenv("PBS_JOBID"))!=NULL) {
      /* jobid is truncated at first "." */
      if ((ptmp2=strchr(ptmp,'.'))!=NULL) *ptmp2=0;
    }
    else ptmp=getenv("JOB_ID");
    if (ptmp!=NULL) sprintf(directory+strlen(directory),"id%s",ptmp);
#endif
  }
  /* make new directory and print info */
  if (ringid==ROOT) {
    MkDirErr(directory,ONE_POS);
    printf("all data is saved in '%s'\n",directory);
  }
  /* make logname; do it for all processors to enable additional logging in LogError */
  if (ringid==0) sprintf(logname,"%s/" F_LOG,directory);
  else sprintf(logname,"%s/" F_LOG_ERR,directory,ringid);
  /* start logfile */
  if (ringid==ROOT) {
    /* open logfille */
    logfile=FOpenErr(logname,"w",ONE_POS);
    /* log version number */
    fprintf(logfile,"Generated by ADDA v." ADDA_VERSION "\n");
#ifdef PARALLEL
    /* write number of processors */
    fprintf(logfile,"The program was run on: %d processors",nprocs);
    /* add PBS or SGE host name if present */
    if ((ptmp=getenv("PBS_O_HOST"))!=NULL || (ptmp=getenv("SGE_O_HOST"))!=NULL)
      fprintf(logfile," from %s\n",ptmp);
    else fprintf(logfile,"\n");
#else  /* sequential */
    /* write computer name */
# ifdef WINDOWS
    GetComputerName(cname,&cname_size);
    fprintf(logfile,"The program was run on: %s\n",cname);
# else /* POSIX and others */
    if ((ptmp=getenv("HOST"))!=NULL)
      fprintf(logfile,"The program was run on: %s\n",ptmp);
# endif
#endif
    /* log command line */
    fprintf(logfile,"command: '");
    for(i=0;i<argc;i++) fprintf(logfile,"%s ",argv[i]);
    fprintf(logfile,"'\n");
  }
  Synchronize(); /* needed to wait for creation of the output directory */
  LogPending();
}

/*============================================================*/

void PrintInfo(void)
  /* print info to stdout and logfile */
{
   int  i;
   char sbuffer[MAX_LINE];

   if (ringid==ROOT) {
    /* print basic parameters */
    printf("lambda: %.10g   m0: %.10g%+.10gi   Dipoles/lambda: %g\n",
           lambda,ref_index[0][RE],ref_index[0][IM],dpl);
    printf("Required relative error: %g\n",eps);
    printf("Total number of occupied dipoles: %d\n",nvoid_Ndip);
    /* log basic parameters */
    fprintf(logfile,"lambda: %.10g\n",lambda);
    fprintf(logfile,"shape: ");
    fprintf(logfile,sh_form_str,sizeX);
    fprintf(logfile,"\nbox dimensions: %ix%ix%i\n",boxX,boxY,boxZ);
    fprintf(logfile,"refractive index: ");
    if (Nmat==1) fprintf(logfile,"%.10g%+.10gi\n",ref_index[0][RE],ref_index[0][IM]);
    else {
      fprintf(logfile,"1. %.10g%+.10gi\n",ref_index[0][RE],ref_index[0][IM]);
      for (i=1;i<Nmat;i++) fprintf(logfile,
         "                  %d. %.10g%+.10gi\n",i+1,ref_index[i][RE],ref_index[i][IM]);
    }
    fprintf(logfile,"Dipoles/lambda: %g\n",dpl);
    if (volcor_used) fprintf(logfile,"\t(Volume correction used)\n");
    fprintf(logfile,"Required relative error: %g\n",eps);
    fprintf(logfile,"Total number of occupied dipoles: %d\n",nvoid_Ndip);
    fprintf(logfile,"Volume-equivalent size parameter: %.10g\n",ka_eq);
    /* log incident beam and polarization polarization */
    fprintf(logfile,"\n---In laboratory reference frame:---\nIncident beam: %s\n",beam_descr);
    fprintf(logfile,"Incident propagation vector: (%g,%g,%g)\n",
            prop_0[0],prop_0[1],prop_0[2]);
    fprintf(logfile,"Incident polarization Y(par): (%g,%g,%g)\n",
            incPolY_0[0],incPolY_0[1],incPolY_0[2]);
    fprintf(logfile,"Incident polarization X(per): (%g,%g,%g)\n\n",
            incPolX_0[0],incPolX_0[1],incPolX_0[2]);
    /* log particle orientation */
    if (orient_avg) fprintf(logfile,"Particle orientation - averaged\n%s\n",avg_string);
    else {
      /* log incident polarization after transformation */
      if (alph_deg!=0 || bet_deg!=0 || gam_deg!=0) {
        fprintf(logfile,"Particle orientation (deg): alpha=%g, beta=%g, gamma=%g\n\n"\
                        "---In particle reference frame:---\n",alph_deg,bet_deg,gam_deg);
        if (beam_asym) fprintf(logfile,"Incident Beam center position: (%g,%g,%g)\n",
                               beam_center[0],beam_center[1],beam_center[2]);
        fprintf(logfile,"Incident propagation vector: (%g,%g,%g)\n",
                prop[0],prop[1],prop[2]);
        fprintf(logfile,"Incident polarization Y(par): (%g,%g,%g)\n",
                incPolY[0],incPolY[1],incPolY[2]);
        fprintf(logfile,"Incident polarization X(per): (%g,%g,%g)\n\n",
                incPolX[0],incPolX[1],incPolX[2]);
      }
      else fprintf(logfile,"Particle orientation: default\n\n");
    }
    /* log Polarization relation */
    if (PolRelation==POL_CM)
      fprintf(logfile,"Polarization relation: 'Clausius-Mossotti'\n");
    else if (PolRelation==POL_RR)
      fprintf(logfile,"Polarization relation: 'Radiative Reaction Correction'\n");
    else if (PolRelation==POL_LDR) {
      fprintf(logfile,"Polarization relation: 'Lattice Dispersion Relation'");
      if (avg_inc_pol) fprintf(logfile," (averaged over incident polarization)");
      fprintf(logfile,"\n");
    }
    else if (PolRelation==POL_CLDR)
      fprintf(logfile,"Polarization relation: 'Corrected Lattice Dispersion Relation'\n");
    else if (PolRelation==POL_SO)
      fprintf(logfile,"Polarization relation: 'Second Order'\n");
    /* log Scattering Quantities formulae */
    if (ScatRelation==SQ_DRAINE)
      fprintf(logfile,"Scattering quantities formulae: 'by Draine'\n");
    else if (ScatRelation==SQ_SO)
      fprintf(logfile,"Scattering quantities formulae: 'Second Order'\n");
    /* log Interaction term prescription */
    if (IntRelation==G_POINT_DIP)
      fprintf(logfile,"Interaction term prescription: 'as Point dipoles'\n");
    else if (IntRelation==G_SO)
      fprintf(logfile,"Interaction term prescription: 'Second Order'\n");
    /* log FFT method */
#ifdef FFTW3
    fprintf(logfile,"FFT algorithm: FFTW3\n");
#elif defined(FFT_TEMPERTON)
    fprintf(logfile,"FFT algorithm: by C.Temperton\n");
#endif
    /* log Iterative Method */
    if (IterMethod==IT_CGNR)
      fprintf(logfile,"Iterative Method: CGNR\n");
    else if (IterMethod==IT_BICGSTAB)
      fprintf(logfile,"Iterative Method: Bi-CG Stabilized\n");
    else if (IterMethod==IT_BICG_CS)
      fprintf(logfile,"Iterative Method: Bi-CG (complex symmetric)\n");
    else if (IterMethod==IT_QMR_CS)
      fprintf(logfile,"Iterative Method: QMR (complex symmetric)\n");
    /* log Symmetry options */
    if (symmetry_enforced) fprintf(logfile,"Symmetry is enforced by user (warning!)\n");
    else if (NoSymmetry) fprintf(logfile,"No symmetries are used\n");
    /* log Checkpoint options */
    if (load_chpoint) fprintf(logfile,"Simulation is continued from a checkpoint\n");
    if (chp_type!=CHP_NONE) {
      fprintf(logfile,"Checkpoint is turned on:\n");
      if (chp_type==CHP_NORMAL) fprintf(logfile,"    type = normal\n");
      else if (chp_type==CHP_REGULAR) fprintf(logfile,"    type = regular\n");
      else if (chp_type==CHP_ALWAYS) fprintf(logfile,"    type = always\n");
      if (chp_time==UNDEF) fprintf(logfile,"    time = no limit\n");
      else {
        PrintTime(sbuffer,&chp_time);
        fprintf(logfile,"    time = %s(%ld sec)\n",sbuffer,chp_time);
      }
    }
    if (load_chpoint || chp_type!=CHP_NONE)
      fprintf(logfile,"    directory = '%s'\n",chp_dir);
  }
}

