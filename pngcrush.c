/* pngcrush.c - a simple program to recompress png files
 *
 * This program reads in a PNG image, and writes it out again, with the
 * optimum filter_type and zlib_level.  It uses brute force (trying
 * filter_type none, and libpng adaptive filtering, with compression
 * levels 3 and 9).  It does the most time-consuming method last in case
 * it turns out to be the best.
 *
 * Optionally, it can remove unwanted chunks or add gAMA, sRGB, bKGD,
 * tEXt/zTXt, and tRNS chunks.
 *
 * Uses libpng and zlib.  This program was based upon libpng's pngtest.c.
 *
 * Thanks to Greg Roelofs for various bug fixes, suggestions, and
 * occasionally creating Linux executables.
 */

#define PNGCRUSH_VERSION "1.4.0"

/*
 * COPYRIGHT NOTICE, DISCLAIMER, AND LICENSE:
 *
 * Copyright (C) 1998, 1999, 2000 Glenn Randers-Pehrson (randeg@alum.rpi.edu)
 *
 * The pngcrush program is supplied "AS IS".  The Author disclaims all
 * warranties, expressed or implied, including, without limitation, the
 * warranties of merchantability and of fitness for any purpose.  The
 * Author assumes no liability for direct, indirect, incidental, special,
 * exemplary, or consequential damages, which may result from the use of
 * the pngcrush program, even if advised of the possibility of such damage.
 *
 * Permission is hereby granted to anyone to use, copy, modify, and distribute
 * this source code, or portions hereof, for any purpose, without fee, subject
 * to the following restrictions:
 *
 * 1. The origin of this source code must not be misrepresented.
 *
 * 2. Altered versions must be plainly marked as such and must not be
 *    misrepresented as being the original source.
 *
 * 3. This Copyright notice, disclaimer, and license may not be removed
 *    or altered from any source or altered source distribution.
 */

/* To do:
 *
 * Version 1.4.*: check for unused alpha channel and ok-to-reduce-depth.
 *   Rearrange palette to put most-used color first and transparent color
 *   second (see ImageMagick 5.1.1 and later).
 *   Finish pplt (partial palette) feature.
 *
 * Version 1.4.*: Use an alternate write function for the trial passes, that
 *   simply counts bytes rather than actually writing to a file, to save wear
 *   and tear on disk drives.
 *
 * Version 1.4.*: Allow in-place file replacement or as a filter, as in
 *    "pngcrush -overwrite file.png"
 *    "pngcreator | pngcrush > output.png"
 *
 * Version 1.4.*: Remove text-handling and color-handling features and put
 *   those in a separate program or programs, to avoid unnecessary
 *   recompressing.
 *
 * Change log:
 *
 * Version 1.4.0 (built with libpng-1.0.6 + libpng-1.0.6-patch-a)
 *
 * Version 1.3.6 (built with libpng-1.0.5v)
 *
 *   RGB to Grayscale conversion is more accurate (15-bit instead of 8-bit)
 *   and now uses only integer arithmetic.
 *
 *   #ifdef'ed out PNG_READ_DITHER
 *
 *   Changed "Compressed" to "Uncompressed" in help for -itxt.
 *
 *   Stifled some compiler warnings
 *
 * Version 1.3.5 (built with libpng-1.0.5s)
 *
 *   Add test on stat_buf.st_size to verify fpin==fpout, because stat in
 *   MSVC++6.0 standard version returns stat_buf.st_ino=0 for all files.
 *
 *   Revised pngcrush.h to make it easier to control PNG_ZBUF_SIZE and
 *   PNG_NO_FLOATING_POINT_SUPPORTED from a makefile.
 *
 *   Restored ability to enter "replace_gamma" value as a float even when
 *   floating point arithmetic is not enabled.
 *
 *   Enabled removing tEXt, zTXt, or iTXt chunks by chunk type, i.e.,
 *   "-rem tEXt" only removes tEXt chunks, while "-rem text" removes all
 *   three types of text chunk.
 *
 *   Removed definition of TOO_FAR from pngcrush.h
 *
 *   Uses new libpng error handler; if a file has errors, pngcrush now will
 *   continue on and compress the remaining files instead of bailing out.
 *
 * Version 1.3.4 (built with libpng-1.0.5m)
 *
 *   Do not allow pngcrush to overwrite the input file.
 *
 * Version 1.3.3 (built with libpng-1.0.5m)
 *
 *   Restored ability to enter gamma as a float even when floating point
 *   arithmetic is not enabled.
 *
 * Version 1.3.2 (built with libpng-1.0.5k)
 *   
 *   Renamed "dirname" to "directory_name" to avoid conflict with "dirname"
 *   that appears in string.h on some platforms.
 *
 *   Fixed "PNG_NO_FLOAING_POINT" typo in pngcrush.h
 *
 *   #ifdef'ed out parts of the help screen for options that are unsupported.
 *
 * Version 1.3.1 (built with libpng-1.0.5k): Eliminated some spurious warnings
 *   that were being issued by libpng-1.0.5j.  Added  -itxt, -ztxt, and
 *   -zitxt descriptions to the help screen.
 *
 *   Dropped explicit support for pCAL, hIST, sCAL, sPLT, iCCP, tIME, and
 *   cHRM chunks and handle them as unknown but safe-to-copy instead, using
 *   new png_handle_as_unknown function available in libpng-1.0.5k.
 *
 * Version 1.3.0 (built with libpng-1.0.5j): Added support for handling
 *   unknown chunks.
 *
 *   pngcrush is now fixed-point only, unless PNG_NO_FLOATING_POINT_SUPPORTED
 *   is undefined in pngcrush.h.
 *
 *   Added support for the iCCP, iTXt, sCAL, and sPLT chunks, which
 *   are now supported by libpng (since libpng-1.0.5j).  None of these have
 *   been adequately tested.
 *
 *   #ifdef'ed out more unused code (weighted filters and progressive read;
 *   this saves about 15k in the size of the executable).
 *
 *   Moved the special definitions from pngconf.h into a new pngcrush.h
 *
 *   Disallow 256-byte compression window size when writing, to work around
 *   an apparent zlib bug.  Either deflate was producing incorrect results in a
 *   21x21 4-bit image or inflate was decoding it incorrectly; the uncompressed
 *   stream is 252 bytes, which is uncomfortably close to the resulting
 *   256-byte compression  window.  This workaround can be removed when zlib
 *   is fixed.
 *
 *   The "-m method" can be used any of the 124 methods, without having to
 *   specify the filter, level, and strategy, instead of just the first 10.
 *
 * Version 1.2.1 (built with libpng-1.0.5f): Fixed -srgb parameter so it
 *   really does take an argument, and so it continues to use "0" if an
 *   integer does not follow the -srgb.
 *
 *   Added "-plte_len n" argument for truncating the PLTE.  Be sure not to
 *   truncate it to less than the greatest index actually appearing in IDAT.
 *
 * Version 1.2.0: Removed registration requirement.  Added open source
 *   license.  Redefined TOO_FAR=32k in deflate.c.
 *
 * Changes prior to going "open source":
 *
 * Version 1.1.8: built with libpng-1.0.5a.  Runs OK with pngvcrd.c.
 *
 * Version 1.1.7: added ability to add tEXt/zTXt chunks.  Fixed bug with
 * closing a file that wasn't opened when using "pngcrush -n".  Fixed
 * bug with tEXt/zTXt chunks after IDAT not being copied.
 * Added alpha to the displayed palette table.  Rebuilt with libpng-1.0.5.
 *
 * Version 1.1.6: fixed bug with one file left open after each image is
 * processed
 *
 * Version 1.1.5: Shorten or remove tRNS chunks that are all opaque or have
 * opaque entries at the end.  Added timing report.
 *
 * Version 1.1.4: added ability to restrict brute_force to one or more filter
 *   types, compression levels, or compression strategies.
 */

#if defined(__DJGPP__)
#  if ((__DJGPP__ == 2) && (__DJGPP_MINOR__ == 0))
#    include <libc/dosio.h>      /* for _USE_LFN, djgpp 2.0 only */
#  endif
#  define SLASH "\\"
#else
#  define SLASH "/"
#endif
#if !defined(__TURBOC__) && !defined(_MSC_VER) && !defined(_MBCS)
#  include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#if defined(_MBCS) || defined(WIN32) || defined(__WIN32__)
#  include <direct.h>
#endif

#define DEFAULT_MODE   0
#define DIRECTORY_MODE 1
#define EXTENSION_MODE 2
#define FOPEN(file, how) fopen(file, how)
#define FCLOSE(file) {fclose(file); file=NULL;--number_of_open_files;};
#define P1 if(verbose > 1)printf
#define P2 if(verbose > 2)printf

/* we don't need the extra libpng tranformations
 * so they are ifdef'ed out in a special version of pngconf.h */

#define PNG_INTERNAL
#include "png.h"

/* so we can load pngcrush with pre-1.0.6 versions of libpng */
#ifndef png_jmpbuf
#  define png_jmpbuf(png_ptr) ((png_ptr)->jmpbuf)
#endif

#ifdef __TURBOC__
#include <mem.h>
#endif

/* defined so I can write to a file on gui/windowing platforms */
/*  #define STDERR stderr  */
#define STDERR stdout   /* for DOS */

/* input and output filenames */
static PNG_CONST char *progname = "pngtest.png";
static PNG_CONST char *inname = "pngtest.png";
static PNG_CONST char *outname = "pngout.png";
static PNG_CONST char *directory_name = "pngcrush.bak";
static PNG_CONST char *extension = "_C.png";

static int all_chunks_are_safe=0;
static int number_of_open_files;
static int do_pplt = 0;
char pplt_string[1024];
char *ip, *op, *dot;
char in_string[256];
char prog_string[256];
char out_string[256];
char in_extension[256];
static int text_inputs=0;
int text_where[10];  /* 0: no text; 1: before PLTE; 2: after PLTE */
int text_compression[10]; /* -1: uncompressed tEXt; 0: compressed zTXt
                              1: uncompressed iTXt; 2: compressed iTXt */
char text_text[20480];  /* It would be nice to png_malloc this, but we don't
                         * have a png_ptr yet when we need it. */
char text_keyword[800];
#ifdef PNG_iTXt_SUPPORTED
char text_lang[800];
char text_lang_key[800];
#endif
int best;

char buffer[256];
char *str_return;

#ifndef PNG_JMPBUF_SUPPORTED
#ifndef PNG_SETJMP_NOT_SUPPORTED
/* Old setjmp interface */
jmp_buf jmpbuf;
#endif
#endif

static png_uint_32 total_input_length = 0;
static png_uint_32 total_output_length = 0;
static int pngcrush_mode = DEFAULT_MODE;
static int resolution = 0;
static int remove_chunks = 0;
static int output_color_type;
static int output_bit_depth;
static int force_output_color_type=8;
static int force_output_bit_depth=0;
static int input_color_type;
static int input_bit_depth;
static int trial;
static int first_trial=0;
static int verbose=1;
static int help=0;
static int things_have_changed=0;
static int default_compression_window=15;
static int force_compression_window=0;
static int final_method=0;
static int brute_force=0;
static int brute_force_level=0;
static int brute_force_filter=0;
static int brute_force_strategy=0;
static int brute_force_levels[10]={1,1,1,1,1,1,1,1,1,1};
static int brute_force_filters[6]={1,1,1,1,1,1};
static int brute_force_strategies[3]={1,1,1};
static int method=10;
static int pauses=0;
static int nosave=0;
static png_bytep row_buf;
static int z_strategy;
static int best_of_three;
static int methods_specified=0;
static int intent=-1;
static int plte_len=-1;
#ifdef PNG_gAMA_SUPPORTED
static int specified_gamma=0;
static int force_specified_gamma=0;
static int double_gamma=0;
#endif
static int names;
static int have_trns=0;
static png_uint_16 trns_index=0;
static png_uint_16 trns_red=0;
static png_uint_16 trns_green=0;
static png_uint_16 trns_blue=0;
static png_uint_16 trns_gray=0;
static int have_bkgd=0;
static png_uint_16 bkgd_red=0;
static png_uint_16 bkgd_green=0;
static png_uint_16 bkgd_blue=0;

static png_colorp palette;
static int num_palette;
static png_byte trns_array[256];

#ifdef REORDER_PALETTE
static png_byte palette_reorder[256];
#endif

static png_structp read_ptr, write_ptr;
static png_infop read_info_ptr, write_info_ptr;
static png_infop end_info_ptr;
static png_infop write_end_info_ptr;
static FILE *fpin, *fpout;
png_uint_32 measure_idats(FILE *fpin);
png_uint_32 png_measure_idat(png_structp png_ptr, png_infop info_ptr);
# define MAX_METHODS   200
# define MAX_METHODSP1 201
# define DEFAULT_METHODS 10
static png_uint_32 idat_length[MAX_METHODSP1];
static int filter_method, zlib_level;
static png_bytep png_row_filters=NULL;
static float t_start, t_stop, t_decode, t_encode, t_misc;

static int max_idat_size = PNG_ZBUF_SIZE;
int ia;

/* START of code to validate memory allocation and deallocation */
#ifdef PNG_USER_MEM_SUPPORTED

/* Allocate memory.  For reasonable files, size should never exceed
   64K.  However, zlib may allocate more then 64K if you don't tell
   it not to.  See zconf.h and png.h for more information.  zlib does
   need to allocate exactly 64K, so whatever you call here must
   have the ability to do that.

   This piece of code can be compiled to validate max 64K allocations
   by setting MAXSEG_64K in zlib zconf.h *or* PNG_MAX_MALLOC_64K. */
typedef struct memory_information {
   png_uint_32                    size;
   png_voidp                 pointer;
   struct memory_information FAR *next;
} memory_information;
typedef memory_information FAR *memory_infop;

static memory_infop pinformation = NULL;
static int current_allocation = 0;
static int maximum_allocation = 0;

extern PNG_EXPORT(png_voidp,png_debug_malloc) PNGARG((png_structp png_ptr,
   png_uint_32 size));
extern PNG_EXPORT(void,png_debug_free) PNGARG((png_structp png_ptr,
   png_voidp ptr));

png_voidp
png_debug_malloc(png_structp png_ptr, png_uint_32 size) {

   /* png_malloc has already tested for NULL; png_create_struct calls
      png_debug_malloc directly, with png_ptr == NULL which is OK */

   if (size == 0)
      return (png_voidp)(NULL);

   /* This calls the library allocator twice, once to get the requested
      buffer and once to get a new free list entry. */
   {
      memory_infop pinfo = png_malloc_default(png_ptr, sizeof *pinfo);
      pinfo->size = size;
      current_allocation += size;
      if (current_allocation > maximum_allocation)
         maximum_allocation = current_allocation;
      pinfo->pointer = png_malloc_default(png_ptr, size);
      pinfo->next = pinformation;
      pinformation = pinfo;
      /* Make sure the caller isn't assuming zeroed memory. */
      png_memset(pinfo->pointer, 0xdd, pinfo->size);
      if(verbose > 2)
         fprintf(STDERR, "Pointer %x allocated\n", pinfo->pointer);
      return (png_voidp)(pinfo->pointer);
   }
}

/* Free a pointer.  It is removed from the list at the same time. */
void
png_debug_free(png_structp png_ptr, png_voidp ptr)
{
   if (png_ptr == NULL)
      fprintf(STDERR, "NULL pointer to png_debug_free.\n");
   if (ptr == 0) {
#if 0 /* This happens all the time. */
      fprintf(STDERR, "WARNING: freeing NULL pointer\n");
#endif
      return;
   }

   /* Unlink the element from the list. */
   {
      memory_infop FAR *ppinfo = &pinformation;
      for (;;) {
         memory_infop pinfo = *ppinfo;
         if (pinfo->pointer == ptr) {
            *ppinfo = pinfo->next;
            current_allocation -= pinfo->size;
            if (current_allocation < 0)
               fprintf(STDERR, "Duplicate free of memory\n");
            /* We must free the list element too, but first kill
               the memory that is to be freed. */
            memset(ptr, 0x55, pinfo->size);
            png_free_default(png_ptr, pinfo);
            if(verbose > 2)
               fprintf(STDERR, "Pointer %x freed\n", ptr);
            break;
         }
         if (pinfo->next == NULL) {
            fprintf(STDERR, "Pointer %x not found\n", ptr);
            break;
         }
         ppinfo = &pinfo->next;
      }
   }

   /* Finally free the data. */
   png_free_default(png_ptr, ptr);
}
#endif /* PNG_USER_MEM_SUPPORTED */
/* END of code to test memory allocation/deallocation */

void png_crush_pause(void);
void png_crush_pause(void)
{
   if(pauses > 0)
   {
      char keystroke;
      fprintf(STDERR, "Press [ENTER] key to continue.\n");
      keystroke=(char)getc(stdin);
      if (keystroke)
        /* stifle compiler warning */ return;
   }
}
#define PNG_CRUSH_CLEANUP \
      fprintf(STDERR, "%s -> %s: libpng read error\n", inname, outname); \
      if(row_buf != NULL)png_free(read_ptr, row_buf); \
      row_buf = (png_bytep)NULL; \
      png_destroy_info_struct(write_ptr, &write_end_info_ptr); \
      png_destroy_write_struct(&write_ptr, &write_info_ptr); \
      if(nosave == 0) \
      { \
         FCLOSE(fpout); \
      } \
      png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr); \
      FCLOSE(fpin); \
      if(verbose > 1) \
        fprintf(STDERR, "returning after longjump\n");

int keep_chunk(png_const_charp name, char *argv[]);

int keep_chunk(png_const_charp name, char *argv[])
{
    int i;
    if(verbose > 2 && first_trial)
       fprintf(STDERR, "   Read the %s chunk.\n", name);
    if(remove_chunks == 0) return 1;
    if(verbose > 1 && first_trial)
       fprintf(STDERR, "     Check for removal of the %s chunk.\n", name);
    for (i=1; i<=remove_chunks; i++)
    {
    if(!strncmp(argv[i],"-r",2))
      {
         int alla = 0;
         int allb = 0;
         i++;
         if(!strncmp(argv[i],"alla",4)) alla++;  /* all ancillaries */
         if(!strncmp(argv[i],"all",3)) allb++;   /* all but gamma */
         if(!strncmp(argv[i],name,4) ||
           (!strncmp(name,"PLTE",4) && (!strncmp(argv[i],"plte",4))) ||
           (!strncmp(name,"bKGD",4) && (!strncmp(argv[i],"bkgd",4) || allb)) ||
           (!strncmp(name,"cHRM",4) && (!strncmp(argv[i],"chrm",4) || allb)) ||
           (!strncmp(name,"gAMA",4) && (!strncmp(argv[i],"gama",4) || alla)) ||
           (!strncmp(name,"gIFg",4) && (!strncmp(argv[i],"gifg",4) || allb)) ||
           (!strncmp(name,"gIFt",4) && (!strncmp(argv[i],"gift",4) || allb)) ||
           (!strncmp(name,"gIFx",4) && (!strncmp(argv[i],"gifx",4) || allb)) ||
           (!strncmp(name,"hIST",4) && (!strncmp(argv[i],"hist",4) || allb)) ||
           (!strncmp(name,"iCCP",4) && (!strncmp(argv[i],"iccp",4) || allb)) ||
           (!strncmp(name,"iTXt",4) && (!strncmp(argv[i],"itxt",4) || allb)) ||
           (!strncmp(name,"iTXt",4) && (!strncmp(argv[i],"text",4)        )) ||
           (!strncmp(name,"oFFs",4) && (!strncmp(argv[i],"offs",4) || allb)) ||
           (!strncmp(name,"pHYs",4) && (!strncmp(argv[i],"phys",4) || allb)) ||
           (!strncmp(name,"pCAL",4) && (!strncmp(argv[i],"pcal",4) || allb)) ||
           (!strncmp(name,"sBIT",4) && (!strncmp(argv[i],"sbit",4) || allb)) ||
           (!strncmp(name,"sCAL",4) && (!strncmp(argv[i],"scal",4) || allb)) ||
           (!strncmp(name,"sRGB",4) && (!strncmp(argv[i],"srgb",4) || allb)) ||
           (!strncmp(name,"sPLT",4) && (!strncmp(argv[i],"splt",4) || allb)) ||
           (!strncmp(name,"tEXt",4) && (!strncmp(argv[i],"text",4) || allb)) ||
           (!strncmp(name,"tIME",4) && (!strncmp(argv[i],"time",4) || allb)) ||
           (!strncmp(name,"tRNS",4) && (!strncmp(argv[i],"trns",4)        )) ||
           (!strncmp(name,"zTXt",4) && (!strncmp(argv[i],"ztxt",4) || allb)) ||
           (!strncmp(name,"zTXt",4) && (!strncmp(argv[i],"text",4)        )))
         {
           things_have_changed=1;
           if(verbose > 0 && first_trial)
              fprintf(STDERR, "   Removed the %s chunk.\n", name);
           return 0;
         }
      }
    }
    if(verbose > 1 && first_trial)
       fprintf(STDERR, "   Preserving the %s chunk.\n", name);
    return 1;
}

void
show_result(void)
{
   if(total_output_length)
   {
   if(total_input_length == total_output_length)
      fprintf(STDERR,
         "   Overall result: no change\n");
   else if(total_input_length > total_output_length)
      fprintf(STDERR,
         "   Overall result: %4.2f%% reduction, %ld bytes\n",
         (100.0 - (100.0*total_output_length)/total_input_length),
         total_input_length - total_output_length);
   else
      fprintf(STDERR,
      "   Overall result: %4.2f%% increase, %ld bytes\n",
         -(100.0 - (100.0*total_output_length)/total_input_length),
         total_output_length - total_input_length);
   }
   t_stop = (float)clock();
   t_misc += (t_stop - t_start);
   t_start = t_stop;
   fprintf(STDERR,"   CPU time used = %.3f seconds",
      (t_misc+t_decode+t_encode)/(float)CLOCKS_PER_SEC);
   fprintf(STDERR," (decoding %.3f,\n",
      t_decode/(float)CLOCKS_PER_SEC);
   fprintf(STDERR,"          encoding %.3f,",
      t_encode/(float)CLOCKS_PER_SEC);
   fprintf(STDERR," other %.3f seconds)\n\n",
      t_misc/(float)CLOCKS_PER_SEC);
#ifdef PNG_USER_MEM_SUPPORTED
   if (current_allocation != 0) {
      memory_infop pinfo = pinformation;
      fprintf(STDERR, "MEMORY ERROR: %d bytes still allocated\n",
         current_allocation);
      while (pinfo != NULL) {
         fprintf(STDERR, " %8d bytes at %x\n", pinfo->size, pinfo->pointer);
         free(pinfo->pointer);
         pinfo = pinfo->next;
         }
   }
#endif
}

int
main(int argc, char *argv[])
{
   png_uint_32 y;
   png_uint_32 width, height;
   int num_pass, pass;
   int bit_depth, color_type;
   int try_method[MAX_METHODSP1];
   int fm[MAX_METHODSP1];
   int lv[MAX_METHODSP1];
   int zs[MAX_METHODSP1];
   int ntrial;
   int lev, strat, filt;
#ifdef PNG_gAMA_SUPPORTED
   png_fixed_point file_gamma=0;
#endif
   char *cp;
   int i;
   row_buf = (png_bytep)NULL;
   number_of_open_files=0;

   if (strcmp(png_libpng_ver, PNG_LIBPNG_VER_STRING))
   {
      fprintf(STDERR,
         "Warning: versions are different between png.h and png.c\n");
      fprintf(STDERR, "  png.h version: %s\n", PNG_LIBPNG_VER_STRING);
      fprintf(STDERR, "  png.c version: %s\n\n", png_libpng_ver);
   }

   t_start = (float)clock();

   prog_string[0] = '\0';
   str_return = strcat(prog_string,argv[0]);
   progname = prog_string;
   for(i=0, cp=prog_string; *cp!='\0'; i++, cp++)
   {
      if(*cp == '\\' || *cp == '/') progname = ++cp;
      if(*cp == '.') *cp='\0';
   }

   for(i=0; i<MAX_METHODS; i++)
   {
      try_method[i]=1;
      fm[i]=5;
      zs[i]=1;
      lv[i]=9;
   }

   fm[1]=0; fm[2]=1; fm[4]=0; fm[5]=1; fm[7]=0; fm[8]=1;
   lv[1]=4; lv[2]=4; lv[3]=4; lv[9]=2;
   zs[1]=0; zs[2]=0; zs[5]= 0; zs[6]=0; zs[7]=0; zs[9]=2;
   method=11;
   for(filt=0; filt<6; filt++)
     {
        zs[method]=2;
        lv[method]=2;
        fm[method]=filt;
        method++;
     }
   for(lev=1; lev<10; lev++)
     {
        for(strat=0; strat<2; strat++)
        {
           for(filt=0; filt<6; filt++)
           {
              zs[method]=strat;
              lv[method]=lev;
              fm[method]=filt;
              method++;
           }
        }
     }
#define BUMP_I i++;if(i >= argc) {printf("insufficient parameters\n");exit(1);}
   names=1;
   for (i=1; i<argc; i++)
   {
   if(!strncmp(argv[i],"-",1))
         names++;
   if(!strncmp(argv[i],"-fast",5))
      /* try two fast filters */
      {
         methods_specified=1;
         try_method[16]=0;
         try_method[53]=0;
      }
   else if(!strncmp(argv[i],"-huffman",8))
      /* try all filters with huffman */
      {
         methods_specified=1;
         for(method=11; method<16; method++)
         {
            try_method[method]=0;
         }
      }

   else if( !strncmp(argv[i],"-bkgd",5) ||
            !strncmp(argv[i],"-bKGD",5))
      {
         names+=3;
         have_bkgd=1;
         bkgd_red=(png_uint_16)atoi(argv[++i]);
         bkgd_green=(png_uint_16)atoi(argv[++i]);
         bkgd_blue=(png_uint_16)atoi(argv[++i]);
      }

   else if(!strncmp(argv[i],"-brute",6))
      /* brute force try everything */
      {
         int lev, strat, filt;
         methods_specified=1;
         brute_force++;
         for(method=11; method < 125; method++)
              try_method[method]=0;
         if(brute_force_filter==0)
           for (filt=0; filt<6; filt++)
             brute_force_filters[filt]=0;      
         if(brute_force_level==0)
           for (lev=0; lev<10; lev++)
              brute_force_levels[lev]=0;      
         if(brute_force_strategy == 0)
           for (strat=0; strat<3; strat++)
              brute_force_strategies[strat]=0;      
      }
   else if(!strncmp(argv[i],"-bit_depth",10))
      {
         names++;
         BUMP_I;
         force_output_bit_depth=atoi(argv[i]);
      }
   else if(!strncmp(argv[i],"-c",2))
      {
         names++;
         BUMP_I;
         force_output_color_type=atoi(argv[i]);
      }
#ifdef PNG_gAMA_SUPPORTED
   else if(!strncmp(argv[i],"-dou",4))
      {
         double_gamma++;
         things_have_changed=1;
      }
#endif
   else if(!strncmp(argv[i],"-d",2))
      {
         BUMP_I;
         pngcrush_mode=DIRECTORY_MODE;
         directory_name= argv[names++];
      }
   else if(!strncmp(argv[i],"-e",2))
      {
         BUMP_I;
         pngcrush_mode=EXTENSION_MODE;
         extension= argv[names++];
      }
   else if(!strncmp(argv[i],"-force",6))
      {
         things_have_changed=1;
      }
   else if(!strncmp(argv[i],"-f",2))
      {
         int specified_filter=atoi(argv[++i]);
         int lev, strat, filt;
         if(specified_filter > 5 || specified_filter < 0)
            specified_filter = 5;
         names++;
         if(brute_force == 0)
            fm[method]=specified_filter;
         else
         {
            for (filt=0; filt<6; filt++)
               brute_force_filters[filt]=1;      
            brute_force_filters[specified_filter]=0;
            method=11;
            for(filt=0; filt<6; filt++)
            {
               try_method[method]= brute_force_filters[filt] |
                  brute_force_strategies[2];
               method++;
            }
            for(lev=1; lev<10; lev++)
            {
               for(strat=0; strat<2; strat++)
               {
                  for(filt=0; filt<6; filt++)
                  {
                     try_method[method]=brute_force_levels[lev] |
                             brute_force_filters[filt] |
                             brute_force_strategies[strat];
                     method++;
                  }
               }
            }
            brute_force_filter++;
         }
      }
   else if(!strncmp(argv[i],"-l",2))
      {
         int lev, strat, filt;
         int specified_level=atoi(argv[++i]);
         if(specified_level > 9 || specified_level < 0)
            specified_level = 9;
         names++;
         if(brute_force == 0)
            lv[method]=specified_level;
         else
         {
            if(brute_force_level == 0)
               for (lev=0; lev<10; lev++)
                  brute_force_levels[lev]=1;      
            brute_force_levels[specified_level]=0;
            method=11;
            for(filt=0; filt<6; filt++)
            {
               lv[method]=specified_level;
               method++;
            }
            for(lev=1; lev<10; lev++)
            {
               for(strat=0; strat<2; strat++)
               {
                  for(filt=0; filt<6; filt++)
                  {
                     try_method[method]=brute_force_levels[lev] |
                             brute_force_filters[filt] |
                             brute_force_strategies[strat];
                     method++;
                  }
               }
            }
            brute_force_level++;
         }
      }
#ifdef PNG_gAMA_SUPPORTED
   else if(!strncmp(argv[i],"-g",2))
      {
         names++;
         BUMP_I;
         if (intent < 0)
            {
               int c;
               char number[16];
               char *n=number;
               int nzeroes=-1;
               int length=strlen(argv[i]);
               for (c=0; c<length; c++)
                  {
                     if( *(argv[i]+c) == '.')
                        {
                           nzeroes=5;
                        }
                     else if (nzeroes != 0)
                        {
                           *n++=*(argv[i]+c);
                           nzeroes--;
                        }
                  }
               for (c=0; c<nzeroes; c++)
                  *n++='0';
               *n='\0';
               specified_gamma=atoi(number);
            }
      }
#endif
   else if(!strncmp(argv[i],"-h",2))
      {
         help++;
         verbose++;
      }
   else if(!strncmp(argv[i],"-max",4))
      {
         names++;
         BUMP_I;
         max_idat_size = atoi(argv[i]);
         if (max_idat_size > PNG_ZBUF_SIZE) max_idat_size=PNG_ZBUF_SIZE;
      }
   else if(!strncmp(argv[i],"-m",2))
      {
         names++;
         BUMP_I;
         method=atoi(argv[i]);
         methods_specified=1;
         brute_force=0;
         try_method[method]=0;
      }
   else if(!strncmp(argv[i],"-nosave",2))
      {
      /* no save; I just use this for testing decode speed */
      nosave++;
      pngcrush_mode=EXTENSION_MODE;
      }
   else if(!strncmp(argv[i],"-plte_len",9))
      {
         names++;
         BUMP_I;
         plte_len=atoi(argv[i]);
      }
   else if(!strncmp(argv[i],"-pplt",3))
      {
         names++;
         do_pplt++;
         BUMP_I;
         strcpy(pplt_string,argv[i]);
         things_have_changed=1;
      }
   else if(!strncmp(argv[i],"-p",2))
      {
      pauses++;
      }
#ifdef PNG_gAMA_SUPPORTED
   else if(!strncmp(argv[i],"-rep",4))
      {
         names++;
         BUMP_I;
         {
            int c;
            char number[16];
            char *n=number;
            int nzeroes=-1;
            int length=strlen(argv[i]);
            for (c=0; c<length; c++)
               {
                  if( *(argv[i]+c) == '.')
                     {
                        nzeroes=5;
                     }
                  else if (nzeroes != 0)
                     {
                        *n++=*(argv[i]+c);
                        nzeroes--;
                     }
               }
            for (c=0; c<nzeroes; c++)
               *n++='0';
            *n='\0';
            force_specified_gamma=atoi(number);
         }
         things_have_changed=1;
      }
#endif
#ifdef PNG_pHYs_SUPPORTED
   else if(!strncmp(argv[i],"-res",4))
      {
         names++;
         BUMP_I;
         resolution=atoi(argv[i]);
      }
#endif
   else if(!strncmp(argv[i],"-r",2))
      {
         remove_chunks=i;
         names++;
         BUMP_I;
      }
   else if( !strncmp(argv[i],"-save",5))
         all_chunks_are_safe++;
#ifdef PNG_sRGB_SUPPORTED
   else if( !strncmp(argv[i],"-srgb",5) ||
            !strncmp(argv[i],"-sRGB",5))
      {
#ifdef PNG_gAMA_SUPPORTED
         specified_gamma=45455L;
#endif
         intent=0;
         BUMP_I;
         if(!strncmp(argv[i],"0",1) ||
            !strncmp(argv[i],"1",1) ||
            !strncmp(argv[i],"2",1) ||
            !strncmp(argv[i],"3",1))
           {
             names++;
             intent=(int)atoi(argv[i]);
           }
         else
           i--;
      }
#endif
   else if(!strncmp(argv[i],"-s",2))
         verbose=0;
   else if( !strncmp(argv[i],"-text",5) || !strncmp(argv[i],"-tEXt",5) ||
            !strncmp(argv[i],"-ztxt",5) || !strncmp(argv[i],"-zTXt",5) ||
            !strncmp(argv[i],"-zitxt",6) || !strncmp(argv[i],"-ziTXt",6) ||
            !strncmp(argv[i],"-itxt",5) || !strncmp(argv[i],"-iTXt",5))
      {
         i+=2; BUMP_I; i-=3;
         if(strlen(argv[i+2]) < 80 && strlen(argv[i+3]) < 2048 &&
            text_inputs < 10)
         {
         if( !strncmp(argv[i],"-zi",3))
         {
            text_compression[text_inputs] = PNG_ITXT_COMPRESSION_zTXt;
              names+=2;
         }
         else if( !strncmp(argv[i],"-z",2))
            text_compression[text_inputs] = PNG_TEXT_COMPRESSION_zTXt;
         else if( !strncmp(argv[i],"-t",2))
            text_compression[text_inputs] = PNG_TEXT_COMPRESSION_NONE;
         else
         {
           text_compression[text_inputs] = PNG_ITXT_COMPRESSION_NONE;
           names+=2;
         }
         names+=3;
         if( !strncmp(argv[++i],"b",1))
            text_where[text_inputs]=1;
         if( !strncmp(argv[i],"a",1))
            text_where[text_inputs]=2;
         strcpy(&text_keyword[text_inputs*80],argv[++i]);
#ifdef PNG_iTXt_SUPPORTED
         if(text_compression[text_inputs] <= 0)
           {
             text_lang[text_inputs*80] = '\0';
             text_lang_key[text_inputs*80] = '\0';
           }
         else
           {
             strcpy(&text_lang[text_inputs*80],argv[++i]);
             /* libpng-1.0.5j and later */
             strcpy(&text_lang_key[text_inputs*80],argv[++i]);
           }
#endif
         strcpy(&text_text[text_inputs*2048],argv[++i]);
         text_inputs++;
         }
         else
         {
            if(text_inputs > 9)
              fprintf(STDERR,
              "too many text/zTXt inputs; only 10 allowed\n");
            else
              fprintf(STDERR,
              "keyword exceeds 79 characters or text exceeds 2047 characters\n");
            i+=3;
            names+=3;
            if( !strncmp(argv[i],"-i",2) || !strncmp(argv[i],"-zi",3))
            {
              i++;
              BUMP_I;
              names+=2;
            }
         }
      }
   else if( !strncmp(argv[i],"-trns",5) ||
            !strncmp(argv[i],"-tRNS",5))
      {
         names+=5;
         have_trns=1;
         trns_index=(png_uint_16)atoi(argv[++i]);
         trns_red=(png_uint_16)atoi(argv[++i]);
         trns_green=(png_uint_16)atoi(argv[++i]);
         trns_blue=(png_uint_16)atoi(argv[++i]);
         trns_gray=(png_uint_16)atoi(argv[++i]);
      }
   else if(!strncmp(argv[i],"-version",8))
      {
         fprintf(STDERR,"libpng ");
         fprintf(STDERR, PNG_LIBPNG_VER_STRING );
         fprintf(STDERR,", uses zlib ");
         fprintf(STDERR, ZLIB_VERSION );
         fprintf(STDERR,"\n");
      }
   else if(!strncmp(argv[i],"-v",2))
      {
         verbose++;
      }
   else if(!strncmp(argv[i],"-w",2))
      {
         default_compression_window=atoi(argv[++i]);
         force_compression_window++;
         names++;
      }
   else if(!strncmp(argv[i],"-z",2))
      {
         int lev, strat, filt;
         int specified_strategy=atoi(argv[++i]);
         if(specified_strategy > 2 || specified_strategy < 0)
            specified_strategy = 0;
         names++;
         if(brute_force == 0)
            zs[method]=specified_strategy;
         else
         {
            if(brute_force_strategy == 0)
               for (strat=0; strat<2; strat++)
                  brute_force_strategies[strat]=1;      
            brute_force_strategies[specified_strategy]=0;
            method=11;
            for(filt=0; filt<6; filt++)
            {
               if(specified_strategy != 2)
                  try_method[method]=1;
               method++;
            }
            for(lev=1; lev<10; lev++)
            {
               for(strat=0; strat<2; strat++)
               {
                  for(filt=0; filt<6; filt++)
                  {
                     try_method[method]=brute_force_levels[lev] |
                             brute_force_filters[filt] |
                             brute_force_strategies[strat];
                     method++;
                  }
               }
            }
         }
         brute_force_strategy++;
      }
   }

   if(verbose > 0)
   {
      fprintf(STDERR, 
        "\n | %s %s, Copyright (C) 1998, 1999, 2000 Glenn Randers-Pehrson\n",
        progname, PNGCRUSH_VERSION);
      fprintf(STDERR, " | This is a free, open-source program.  Permission is\n");
      fprintf(STDERR, " | granted to everyone to use pngcrush without fee.\n");
      fprintf(STDERR, 
        " | This program was built with libpng version %s,\n",
            PNG_LIBPNG_VER_STRING);
      fprintf(STDERR,
        " |    Copyright (C) 1995, Guy Eric Schalnat, Group 42 Inc.,\n");
      fprintf(STDERR,
        " |    Copyright (C) 1996, 1997 Andreas Dilger,\n");
      fprintf(STDERR,
        " |    Copyright (C) 1998, 1999, 2000 Glenn Randers-Pehrson,\n");
      fprintf(STDERR, 
        " | and zlib version %s, Copyright (C) 1998,\n",
            ZLIB_VERSION);
      fprintf(STDERR,
        " |    Jean-loup Gailly and Mark Adler.\n");
#if defined(__DJGPP__)
      fprintf(STDERR,
        " | It was compiled with gcc version %s and as version %s\n",
         __VERSION__, "2.81");
      /* is there a macro for "as" versions? */
      fprintf(STDERR,
        " | under DJGPP %d.%d, Copyright (C) 1995, D. J. Delorie\n",
        __DJGPP__,__DJGPP_MINOR__);
      fprintf(STDERR,
        " | and loaded with PMODE/DJ, by Thomas Pytel and Matthias Grimrath\n");
      fprintf(STDERR,
        " |    Copyright (C) 1996, Matthias Grimrath.\n");
#endif
      fprintf(STDERR,"\n");
      }

   if     (default_compression_window == 32) default_compression_window=15;
   else if(default_compression_window == 16) default_compression_window=14;
   else if(default_compression_window ==  8) default_compression_window=13;
   else if(default_compression_window ==  4) default_compression_window=12;
   else if(default_compression_window ==  2) default_compression_window=11;
   else if(default_compression_window ==  1) default_compression_window=10;
   else if(default_compression_window == 512) default_compression_window= 9;
   /* Use of compression window size 256 is not recommended. */
   else if(default_compression_window == 256) default_compression_window= 8;
   else if(default_compression_window != 15)
   {
   fprintf(STDERR,"Invalid window size (%d); using window size=4\n",
          default_compression_window);
   default_compression_window=12;
   }

   if(pngcrush_mode == DEFAULT_MODE && argc - names == 2)
   {
      inname= argv[names];
      outname=argv[names+1];
   }

   if(pngcrush_mode == DEFAULT_MODE && (argc - names == 1 || nosave))
   {
      inname= argv[names];
   }

   if((nosave == 0 && pngcrush_mode == DEFAULT_MODE && argc - names != 2) ||
       help > 0)
   {
     fprintf(STDERR,
       "\nusage: %s [options] infile.png outfile.png\n",progname);
     fprintf(STDERR,
       "       %s -e ext [other options] files.png ...\n",progname);
     fprintf(STDERR,
       "       %s -d dir [other options] files.png ...\n",progname);
     if(verbose > 1)
     {
      png_crush_pause();
        fprintf(STDERR, "\noptions:\n");
     }
     else
        fprintf(STDERR, "options:\n");
     fprintf(STDERR,
       "        -brute (Use brute-force, try 114 different methods [11-124])\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Very time-consuming and generally not worthwhile.\n");
     fprintf(STDERR,
       "               You can restrict this option to certain filter types,\n");
     fprintf(STDERR,
       "               compression levels, or strategies by following it with\n");
     fprintf(STDERR,
       "               \"-f filter\", \"-l level\", or \"-z strategy\".\n\n");
     }
     fprintf(STDERR,
       "            -c color_type of output file [0, 2, 4, or 6]\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Color type for the output file.  Future versions\n");
     fprintf(STDERR,
       "               will also allow color_type 3, if there are 256 or\n");
     fprintf(STDERR,
       "               fewer colors present in the input file.  Color types\n");
     fprintf(STDERR,
       "               4 and 6 are padded with an opaque alpha channel if\n");
     fprintf(STDERR,
       "               the input file does not have alpha information.\n");
     fprintf(STDERR,
       "               You can use 0 or 4 to convert color to grayscale.\n");
     fprintf(STDERR,
       "               Use 0 or 2 to delete an unwanted alpha channel.\n");
     fprintf(STDERR,
       "               Default is to use same color type as the input file.\n\n");
     }
     fprintf(STDERR,
       "            -d directory_name (where output files will go)\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               If a directory name is given, then the output\n");
     fprintf(STDERR,
       "               files are placed in it, with the same filenames as\n");
     fprintf(STDERR,
       "               those of the original files. For example,\n");
     fprintf(STDERR,
       "               you would type 'pngcrush -directory CRUSHED *.png'\n");
     fprintf(STDERR,
       "               to get *.png => CRUSHED/*.png\n\n");
     }
     png_crush_pause();
     fprintf(STDERR,
       " -double_gamma (used for fixing gamma in PhotoShop 5.0/5.02 files)\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               It has been claimed that the PS5 bug is actually\n");
     fprintf(STDERR,
       "               more complex than that, in some unspecified way.\n\n");
     }
     fprintf(STDERR,
       "            -e extension  (used for creating output filename)\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               e.g., -ext .new means *.png => *.new\n");
     fprintf(STDERR,
       "               and -e _C.png means *.png => *_C.png\n\n");
     }
     fprintf(STDERR,
       "            -f user_filter [0-5]\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               filter to use with the method specified in the\n");
     fprintf(STDERR,
       "               preceding '-m method' or '-brute_force' argument.\n");
     fprintf(STDERR,
       "               0: none; 1-4: use specified filter; 5: adaptive.\n\n");
     }
     fprintf(STDERR,
       "        -force (Write a new output file even if larger than input)\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Otherwise the input file will be copied to output\n");
     fprintf(STDERR,
       "               if it is smaller than any generated file and no chunk\n");
     fprintf(STDERR,
       "               additions, removals, or changes were requested.\n\n");
     }
     fprintf(STDERR,
       "            -g gamma (float or fixed*100000, e.g., 0.45455 or 45455)\n");
     if(verbose > 1)
     fprintf(STDERR,
       "\n               Value to insert in gAMA chunk, only if the input\n");
     if(verbose > 1)
     fprintf(STDERR,
       "               file has no gAMA chunk.  To replace an existing\n");
     if(verbose > 1)
     fprintf(STDERR,
       "               gAMA chunk, use the '-replace_gamma' option.\n\n");
     png_crush_pause();
     fprintf(STDERR,
       "         -itxt b[efore_IDAT]|a[fter_IDAT] \"keyword\" \"text\"\n");
     if(verbose > 1)
     fprintf(STDERR,
       "\n               Uncompressed iTXt chunk to insert (see -text).\n\n");
     fprintf(STDERR,
       "            -l zlib_compression_level [0-9]\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               zlib compression level to use with method specified\n");
     fprintf(STDERR,
       "               with the preceding '-m method' or '-brute_force'\n");
     fprintf(STDERR,
       "               argument.\n\n");
     }
     fprintf(STDERR,
       "            -m method [0 through %d]\n",MAX_METHODS);
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               %s method to try (0 means try all of 1-10).\n",progname);
     fprintf(STDERR,
       "               Can be repeated as in '-m 1 -m 4 -m 7'.\n");
     fprintf(STDERR,
       "               This can be useful if you run out of memory when %s\n",
                       progname);
     fprintf(STDERR,
       "               tries methods 2, 3, 5, 6, 8, 9, or 10 which use \n");
     fprintf(STDERR,
       "               filtering and are memory intensive.  Methods\n");
     fprintf(STDERR,
       "               1, 4, and 7 use no filtering; methods 11 and up use \n");
     fprintf(STDERR,
       "               specified filter, compression level, and strategy.\n\n");
      png_crush_pause();
     }

     fprintf(STDERR,
       "          -max maximum_IDAT_size [1 through %d]\n",PNG_ZBUF_SIZE);
     if(verbose > 1)
        fprintf(STDERR,"\n");

     fprintf(STDERR,
       "            -n (no save; does not do compression or write output PNG)\n");
     if(verbose > 1)
        fprintf(STDERR,
       "\n               Useful in conjunction with -v option to get info.\n\n");

     fprintf(STDERR,
       "            -plte_len n (truncate PLTE)\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Truncates the PLTE.  Be sure not to truncate it to\n");
     fprintf(STDERR,
       "\n               less than the greatest index present in IDAT.\n");
 
     }
     fprintf(STDERR,
       "            -q (quiet)\n");
     if(verbose > 1)
        fprintf(STDERR,"\n");

     if(verbose > 1)
        fprintf(STDERR,"\n");
     fprintf(STDERR,
       "          -rem chunkname (or \"alla\" or \"allb\")\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Name of an ancillary chunk or optional PLTE to be\n");
     fprintf(STDERR,
       "               removed.  Be careful with this.  Please don't use \n");
     fprintf(STDERR,
       "               this feature to remove transparency, gamma, copyright,\n");
     fprintf(STDERR,
       "               or other valuable information.  To remove several\n");
     fprintf(STDERR,
       "               different chunks, repeat: -rem tEXt -rem pHYs.\n");
     fprintf(STDERR,
       "               Known chunks (those in the PNG 1.1 spec or extensions\n");
     fprintf(STDERR,
       "               document) can be named with all lower-case letters,\n");
     fprintf(STDERR,
       "               so \"-rem bkgd\" is equivalent to \"-rem bKGD\".  But\n");
     fprintf(STDERR,
       "               note: \"-rem text\" removes all forms of text chunks;\n");
     fprintf(STDERR,
       "               Exact case is required to remove unknown chunks.\n");
     fprintf(STDERR,
       "               To do surgery with a chain-saw, \"-rem alla\" removes\n");
     fprintf(STDERR,
       "               all known ancillary chunks except for tRNS, and\n");
     fprintf(STDERR,
       "               \"-rem allb\" removes all but tRNS and gAMA.\n\n");
     }
      png_crush_pause();
     fprintf(STDERR,
       "-replace_gamma gamma (float or fixed*100000) even if gAMA is present.\n");
     if(verbose > 1)
        fprintf(STDERR,"\n");
     fprintf(STDERR,
       "          -res dpi\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Write a pHYs chunk with the given resolution.\n\n");
     }
#if 0
     fprintf(STDERR,
       "         -save (keep all copy-unsafe chunks)\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Save otherwise unknown ancillary chunks that\n");
     fprintf(STDERR,
       "               would be considered copy-unsafe.  This option makes\n");
     fprintf(STDERR,
       "               all chunks 'known' to %s, so they can be copied.\n\n",
                       progname);
     }
#endif
      png_crush_pause();

     fprintf(STDERR,
       "         -srgb [0, 1, 2, or 3]\n");
     if(verbose > 1)
     fprintf(STDERR,
       "\n               Value of 'rendering intent' for sRGB chunk.\n\n");
     fprintf(STDERR,
       "         -text b[efore_IDAT]|a[fter_IDAT] \"keyword\" \"text\"\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               tEXt chunk to insert.  keyword < 80 chars,\n");
     fprintf(STDERR,
       "\n               text < 2048 chars. For now, you can only add ten\n");
     fprintf(STDERR,
       "               tEXt, iTXt, or zTXt chunks per pngcrush run.\n\n");
     }
     fprintf(STDERR,
       "         -trns index red green blue gray\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Insert a tRNS chunk, if no tRNS chunk found in file.\n");
     fprintf(STDERR,
       "               You must give all five parameters regardless of the\n");
     fprintf(STDERR,
       "               color type, scaled to the output bit depth.\n\n");
     }

     fprintf(STDERR,
       "            -v (display more detailed information)\n");
     if(verbose > 1)
     fprintf(STDERR,
       "\n               Repeat the option (use \"-v -v\") for even more.\n\n");
     fprintf(STDERR,
       "      -version (display the pngcrush version)\n");
     if(verbose > 1)
        fprintf(STDERR,"\n");
     fprintf(STDERR,
       "            -w compression_window_size [32, 16, 8, 4, 2, 1, 512]\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Size of the sliding compression window, in kbytes\n");
     fprintf(STDERR,
       "               (or bytes, in case of 512).  It's best to\n");
     fprintf(STDERR,
       "               use the default (32) unless you run out of memory.\n");
     fprintf(STDERR,
       "               The program will use a smaller window anyway when\n");
     fprintf(STDERR,
       "               the uncompressed file is smaller than 16k.\n\n");
     fprintf(STDERR,
       "            -z zlib_strategy [0, 1, or 2]\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               zlib compression strategy to use with the preceding\n");
     fprintf(STDERR,
       "               '-m method' argument.\n\n");
     }
     fprintf(STDERR,
       "        -zitxt b[efore_IDAT]|a[fter_IDAT] \"keyword\" \"text\"\n");
     if(verbose > 1)
     fprintf(STDERR,
       "\n               Compressed iTXt chunk to insert (see -text).\n\n");
     fprintf(STDERR,
       "         -ztxt b[efore_IDAT]|a[fter_IDAT] \"keyword\" \"text\"\n");
     if(verbose > 1)
     fprintf(STDERR,
       "\n               zTXt chunk to insert (see -text).\n\n");
     png_crush_pause();
     }
     fprintf(STDERR,
       "            -h (help)\n");
     if(verbose > 1)
     fprintf(STDERR,
       "\n               Display this information.\n\n");
     fprintf(STDERR,
       "            -p (pause)\n");
     if(verbose > 1)
     {
     fprintf(STDERR,
       "\n               Wait for [enter] key before continuing display.\n");
     fprintf(STDERR,
       "               e.g., type '%s -pause -help', if the help\n",progname);
     fprintf(STDERR,
       "               screen scrolls out of sight.\n\n");
     }

     if(pngcrush_mode == DEFAULT_MODE && argc - names != 2 && nosave == 0)
         exit(1);
   }

   for (ia=0; ia<255; ia++)
      trns_array[ia]=255;

   for(;;)  /* loop on input files */

   {
      first_trial = 1;

      if(png_row_filters != NULL)
      {
         free(png_row_filters); png_row_filters=NULL;
      }

      inname=argv[names++];

      if(inname == NULL) 
      {
         if(verbose > 0) show_result();
         return 0;
      }

      if(pngcrush_mode == EXTENSION_MODE)
      {
          ip=in_string;
          in_string[0]='\0';
          str_return = strcat(in_string,inname);
          ip = in_string;
          op = dot = out_string;
          while(*ip != '\0')
          {
             *op++ = *ip++;
             if(*ip == '.')dot=op;
          }
          *op = '\0';

          if(dot != out_string)
             *dot = '\0';

          in_extension[0]='\0';
          if(dot != out_string)
          {
             str_return = strcat(in_extension,++dot);
          }

          str_return = strcat(out_string,extension);
          outname=out_string;
      }

      if(pngcrush_mode == DIRECTORY_MODE)
      {
          struct stat stat_buf;
          if(stat(directory_name, &stat_buf) != 0)
          {
#if defined(_MBCS) || defined(WIN32) || defined(__WIN32__)
             if(_mkdir(directory_name) != 0)
#else
             if(mkdir(directory_name, 0x1ed) != 0)
#endif
             {
                fprintf(STDERR,"could not create directory %s\n",directory_name);
                return 1;
             }
          }
          out_string[0] = '\0'; 
          str_return = strcat(out_string,directory_name);
          str_return = strcat(out_string,SLASH);

          in_string[0] = '\0'; 
          str_return = strcat(in_string,inname);
          ip = op = in_string;
          while(*ip != '\0')
          {
             if(*ip == '\\' || *ip == '/')op=ip+1;
             ip++;
          }

          str_return = strcat(out_string,op);
          outname=out_string;
      }

      output_color_type=force_output_color_type;
      output_bit_depth=force_output_bit_depth;

      if(nosave < 2)
      {
         png_debug1(0, "Opening file %s for length measurement\n",inname);

         if ((fpin = FOPEN(inname, "rb")) == NULL)
         {
            fprintf(STDERR, "Could not find file: %s\n", inname);
            continue;
         }
         number_of_open_files++;

         idat_length[0]=measure_idats(fpin);

         FCLOSE(fpin);

         if(verbose > 0)
         {
            fprintf(STDERR,"   %s IDAT length in input file = %8lu\n",
               inname,idat_length[0]);
            fflush(STDERR);
         }

         if(idat_length[0] == 0) continue;
      }
      else
         idat_length[0]=1;

      if(!methods_specified || try_method[0] == 0)
      {
         for (i=1; i<=DEFAULT_METHODS; i++) try_method[i]=0;
         try_method[6]=try_method[0];
      }

      best_of_three=1;

      for(trial=1; trial<=MAX_METHODS; trial++)
      {
      idat_length[trial]=(png_uint_32)0xffffffff;
      if(trial == MAX_METHODS)
      {
         png_uint_32 best_length;
         /* check lengths */
         best=0;
         best_length=(png_uint_32)0xffffffff;
         for (ntrial=things_have_changed; ntrial<MAX_METHODS; ntrial++)
           if(idat_length[ntrial]<=best_length)
           {
               best_length=idat_length[ntrial];
               best=ntrial;
           }

         if(idat_length[best] == idat_length[0] && things_have_changed == 0
            && best != final_method && nosave == 0)
         {
            struct stat stat_in, stat_out;
            /* just copy input to output */

            P2("prepare to copy input to output\n");
            png_crush_pause();

            if ((fpin = FOPEN(inname, "rb")) == NULL)
            {
               fprintf(STDERR, "Could not find input file %s\n", inname);
               continue;
            }

            number_of_open_files++;
            if ((fpout = FOPEN(outname, "wb")) == NULL)
            {
               fprintf(STDERR, "Could not open output file %s\n", outname);
               FCLOSE(fpin);
               return 1;
            }

            number_of_open_files++;
            P2("copying input to output...");

            stat(inname, &stat_in);
            stat(outname, &stat_out);
            if((stat_in.st_ino != stat_out.st_ino) || 
               (stat_in.st_size != stat_out.st_size))
            {
               for(;;)
               {
                  png_size_t num_in;

                  num_in = fread(buffer, 1, 1, fpin);
                  if (!num_in)
                     break;
                  fwrite(buffer, 1, 1, fpout);

               }
            }
            P2("copy complete.\n");
            png_crush_pause();
            FCLOSE(fpin);
            FCLOSE(fpout);
            break;
         }

         if(best == final_method)
         {
            break;
         }
         else
         {
             filter_method=fm[best];
             zlib_level=lv[best];
             if(zs[best] == 0)z_strategy=Z_DEFAULT_STRATEGY;
             if(zs[best] == 1)z_strategy=Z_FILTERED;
             if(zs[best] == 2)z_strategy=Z_HUFFMAN_ONLY;
         }
      }
      else
      {
          if(trial > 2 && trial < 5 && idat_length[trial-1]
              < idat_length[best_of_three])best_of_three = trial-1;
          if(try_method[trial])continue;
          if(!methods_specified && try_method[0])
          {
             if((trial == 4 || trial == 7) && best_of_three != 1) continue;
             if((trial == 5 || trial == 8) && best_of_three != 2) continue;
             if((trial == 6 || trial == 9 || trial == 10) && best_of_three != 3)
                continue;
          }
          filter_method=fm[trial];
          zlib_level=lv[trial];
          if(zs[trial] == 0)z_strategy=Z_DEFAULT_STRATEGY;
          if(zs[trial] == 1)z_strategy=Z_FILTERED;
          if(zs[trial] == 2)z_strategy=Z_HUFFMAN_ONLY;
          final_method=trial;
          if(nosave == 0)
            P2("   Begin trial %d, filter %d, strategy %d, level %d\n",
              trial, filter_method, z_strategy, zlib_level);
      }

      P2("prepare to open files.\n");
         png_crush_pause();

      if ((fpin = FOPEN(inname, "rb")) == NULL)
      {
         fprintf(STDERR, "Could not find input file %s\n", inname);
         continue;
      }
      number_of_open_files++;
      if(nosave == 0)
       {
         struct stat stat_in, stat_out;
         stat(inname, &stat_in);
         stat(outname, &stat_out);
         if(stat_in.st_ino == stat_out.st_ino)
            if((stat_in.st_ino == stat_out.st_ino) && 
               (stat_in.st_size == stat_out.st_size))
         {
            /* MSVC++6.0 will erroneously return 0 for both files, so
               it is possible that we will erroneously reject the attempt
               when inputsize and outputsize are equal, for different files
             */
            fprintf(STDERR, "\n   Cannot overwrite input file %s\n", inname);
            P1("   st_ino=%d, st_size=%d\n\n", (int)stat_in.st_ino,
               (int)stat_in.st_size);
            FCLOSE(fpin);
            return 1;
         }

         if ((fpout = FOPEN(outname, "wb")) == NULL)
         {
            fprintf(STDERR, "Could not open output file %s\n", outname);
            FCLOSE(fpin);
            return 1;
         }

         number_of_open_files++;
        }

      P2("files are opened.\n");
            png_crush_pause();

      png_debug(0, "Allocating read and write structures\n");
#ifdef PNG_USER_MEM_SUPPORTED
   read_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, (png_voidp)NULL,
      (png_error_ptr)NULL, (png_error_ptr)NULL, (png_voidp)NULL,
      (png_malloc_ptr)png_debug_malloc, (png_free_ptr)png_debug_free);
#else
   read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL,
      (png_error_ptr)NULL, (png_error_ptr)NULL);
#endif

#if defined(PNG_NO_STDIO)
      png_set_error_fn(read_ptr, (png_voidp)inname, png_default_error,
         png_default_warning);
#endif

   if(nosave == 0)
   {
#ifdef PNG_USER_MEM_SUPPORTED
   write_ptr = png_create_write_struct_2(PNG_LIBPNG_VER_STRING, (png_voidp)NULL,
      (png_error_ptr)NULL, (png_error_ptr)NULL, (png_voidp)NULL,
      (png_malloc_ptr)png_debug_malloc, (png_free_ptr)png_debug_free);
#else
   write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL,
      (png_error_ptr)NULL, (png_error_ptr)NULL);
#endif

#if defined(PNG_NO_STDIO)
      png_set_error_fn(write_ptr, (png_voidp)outname, png_default_error,
          png_default_warning);
#endif
   }
      png_debug(0, "Allocating read_info, write_info and end_info structures\n");
      read_info_ptr = png_create_info_struct(read_ptr);
      end_info_ptr = png_create_info_struct(read_ptr);
   if(nosave == 0)
   {
      write_info_ptr = png_create_info_struct(write_ptr);
      write_end_info_ptr = png_create_info_struct(write_ptr);
   }

      P2("structures created.\n");
            png_crush_pause();

      png_debug(0, "Setting jmpbuf for read and write structs\n");
#ifndef PNG_SETJMP_NOT_SUPPORTED
#  ifdef USE_FAR_KEYWORD
   if (setjmp(jmpbuf))
#  else
#    ifdef PNG_JMPBUF_SUPPORTED
   /* New setjmp interface */
   if (setjmp(png_jmpbuf(read_ptr)))
#    else
   /* old interface */
   if (setjmp(read_ptr->jmpbuf))
#    endif
#  endif
      {
          PNG_CRUSH_CLEANUP
          continue;
      }

#if defined(USE_FAR_KEYWORD)
      png_memcpy(png_jmpbuf(read_ptr),jmpbuf,sizeof(jmp_buf));
#endif
   if(nosave == 0)
#  ifdef USE_FAR_KEYWORD
   if (setjmp(jmpbuf))
#  else
#    ifdef PNG_JMPBUF_SUPPORTED
   /* New setjmp interface */
   if (setjmp(png_jmpbuf(write_ptr)))
#    else
   /* Old interface */
   if (setjmp(write_ptr->jmpbuf))
#    endif
#  endif
         {
             PNG_CRUSH_CLEANUP
             continue;
         }
#if defined(USE_FAR_KEYWORD)
      png_memcpy(png_jmpbuf(write_ptr),jmpbuf,sizeof(jmp_buf));
#endif
      P2("jmp_buf has been set.\n");
      png_crush_pause();
#endif

      png_debug(0, "Initializing input and output streams\n");
#if !defined(PNG_NO_STDIO)
      png_init_io(read_ptr, fpin);
      if(nosave == 0)
         png_init_io(write_ptr, fpout);
#else
      png_set_read_fn(read_ptr, (png_voidp)fpin, png_default_read_data);
      if(nosave == 0)
         png_set_write_fn(write_ptr, (png_voidp)fpout,  png_default_write_data,
#if defined(PNG_WRITE_FLUSH_SUPPORTED)
            png_default_flush);
#else
            NULL);
#endif
#endif

      P2("io has been initialized.\n");
      png_crush_pause();

     /* We don't need to check CRC's because they were already checked
        in the png_measure_idat function */

      read_ptr->flags |= PNG_FLAG_CRC_ANCILLARY_NOWARN |
                         PNG_FLAG_CRC_ANCILLARY_USE    |
                         PNG_FLAG_CRC_CRITICAL_IGNORE;

   /* reinitialize zbuf - compression buffer */

      if(read_ptr->zbuf_size < (png_size_t)max_idat_size)
      {
      P2("reinitializing read zbuf.\n");
      png_free(read_ptr, read_ptr->zbuf);
      read_ptr->zbuf_size = (png_size_t)max_idat_size;
      read_ptr->zbuf = 
        (png_bytep)png_malloc(read_ptr, (png_uint_32)read_ptr->zbuf_size);
      }
      if(nosave == 0)
       {
         if(write_ptr->zbuf_size > (png_size_t)max_idat_size)
         {
            P2("reinitializing write zbuf.\n");
            png_free(write_ptr, write_ptr->zbuf);
            write_ptr->zbuf_size = (png_size_t)max_idat_size;
            write_ptr->zbuf =
              (png_bytep)png_malloc(write_ptr,
                 (png_uint_32)write_ptr->zbuf_size);
         }
       }
#if defined(PNG_READ_UNKNOWN_CHUNKS_SUPPORTED)
      png_set_keep_unknown_chunks(read_ptr, HANDLE_CHUNK_ALWAYS,
         (png_bytep)NULL, 0);
#endif
#if defined(PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED)
      if(nosave == 0)
        {
        if(all_chunks_are_safe != 0)
           png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS,
            (png_bytep)NULL, 0);
        else
        {
#ifndef PNG_UINT_IHDR
#ifdef PNG_USE_LOCAL_ARRAYS
#if !defined(PNG_cHRM_SUPPORTED)
          PNG_cHRM;
#endif
#if !defined(PNG_hIST_SUPPORTED)
          PNG_hIST;
#endif
#if !defined(PNG_iCCP_SUPPORTED)
          PNG_iCCP;
#endif
#if !defined(PNG_pCAL_SUPPORTED)
          PNG_pCAL;
#endif
#if !defined(PNG_sCAL_SUPPORTED)
          PNG_sCAL;
#endif
#if !defined(PNG_sPLT_SUPPORTED)
          PNG_sPLT;
#endif
#if !defined(PNG_tIME_SUPPORTED)
          PNG_tIME;
#endif
#endif /* PNG_USE_LOCAL_ARRAYS */

          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_IF_SAFE,
            (png_bytep)NULL, 0);
#if !defined(PNG_cHRM_SUPPORTED)
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS, 
            (png_bytep)png_cHRM, 1);
#endif
#if !defined(PNG_hIST_SUPPORTED)
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS, 
            (png_bytep)png_hIST, 1);
#endif
#if !defined(PNG_iCCP_SUPPORTED)
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS, 
            (png_bytep)png_iCCP, 1);
#endif
#if !defined(PNG_sCAL_SUPPORTED)
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS,
            (png_bytep)png_sCAL, 1);
#endif
#if !defined(PNG_pCAL_SUPPORTED)
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS,
            (png_bytep)png_pCAL, 1);
#endif
#if !defined(PNG_sPLT_SUPPORTED)
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS,
            (png_bytep)png_sPLT, 1);
#endif
#if !defined(PNG_tIME_SUPPORTED)
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS,
            (png_bytep)png_tIME, 1);
#endif

#else   /* PNG_UINT_IHDR */

#if !defined(PNG_cHRM_SUPPORTED) || !defined(PNG_hIST_SUPPORTED) || \
    !defined(PNG_iCCP_SUPPORTED) || !defined(PNG_sCAL_SUPPORTED) || \
    !defined(PNG_pCAL_SUPPORTED) || !defined(PNG_sPLT_SUPPORTED) || \
    !defined(PNG_tIME_SUPPORTED)
          png_byte chunk_name[5];
          chunk_name[4]='\0';
#endif

          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_IF_SAFE,
            NULL, 0);
#if !defined(PNG_cHRM_SUPPORTED)
          png_save_uint_32(chunk_name, PNG_UINT_cHRM);
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS, 
            chunk_name, 1);
#endif
#if !defined(PNG_hIST_SUPPORTED)
          png_save_uint_32(chunk_name, PNG_UINT_hIST);
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS, 
            chunk_name, 1);
#endif
#if !defined(PNG_iCCP_SUPPORTED)
          png_save_uint_32(chunk_name, PNG_UINT_iCCP);
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS, 
            chunk_name, 1);
#endif
#if !defined(PNG_sCAL_SUPPORTED)
          png_save_uint_32(chunk_name, PNG_UINT_sCAL);
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS,
            chunk_name, 1);
#endif
#if !defined(PNG_pCAL_SUPPORTED)
          png_save_uint_32(chunk_name, PNG_UINT_pCAL);
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS,
            chunk_name, 1);
#endif
#if !defined(PNG_sPLT_SUPPORTED)
          png_save_uint_32(chunk_name, PNG_UINT_sPLT);
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS,
            chunk_name, 1);
#endif
#if !defined(PNG_tIME_SUPPORTED)
          png_save_uint_32(chunk_name, PNG_UINT_tIME);
          png_set_keep_unknown_chunks(write_ptr, HANDLE_CHUNK_ALWAYS,
            chunk_name, 1);
#endif
#endif  /* PNG_UINT_IHDR */
          }
        }
#endif

      png_debug(0, "Reading info struct\n");
      png_read_info(read_ptr, read_info_ptr);

      png_debug(0, "Transferring info struct\n");
      {
         int interlace_type, compression_type, filter_type;

         if (png_get_IHDR(read_ptr, read_info_ptr, &width, &height, &bit_depth,
             &color_type, &interlace_type, &compression_type, &filter_type))
         {
            int compression_window;
            int need_expand = 0;
            input_color_type=color_type;
            input_bit_depth=bit_depth;
            if(verbose > 1 && first_trial)
            {
               fprintf(STDERR, "   IHDR chunk data:\n");
               fprintf(STDERR, "      Width=%ld, height=%ld\n", width, height);
               fprintf(STDERR, "      Bit depth =%d\n", bit_depth);
               fprintf(STDERR, "      Color type=%d\n", color_type);
               fprintf(STDERR, "      Interlace =%d\n", interlace_type);
            }

            if(output_color_type > 7)
            {
               output_color_type=input_color_type;
            }

#ifndef PNG_WRITE_PACK_SUPPORTED
            if(output_bit_depth == 0)
#endif
            {
               output_bit_depth=input_bit_depth;
            }
            if(output_bit_depth != input_bit_depth)
               need_expand = 1;

            if((color_type == 2 || color_type == 6 || color_type == 3) &&
              (output_color_type == 0 || output_color_type == 4))
            {
#if defined(PNG_READ_RGB_TO_GRAY_SUPPORTED)
               png_set_rgb_to_gray_fixed(read_ptr, 1, -1, -1);
               if(output_bit_depth < 8)output_bit_depth=8;
               if(color_type == 3) need_expand = 1;
#endif
            }
           
            if(color_type != 3 && output_color_type == 3)
            {
               printf("  Cannot change to indexed color (color_type 3)\n");
               output_color_type=input_color_type;
            }

            if((color_type == 0 || color_type == 4) &&
               (output_color_type == 2 || output_color_type == 6))
            {
               png_set_gray_to_rgb(read_ptr);
            }

            if((color_type == 4 || color_type == 6) &&
               (output_color_type != 4 && output_color_type != 6))
            {
                if(verbose > 0 && first_trial)
                   fprintf(STDERR, "   Stripping existing alpha channel.\n");
#ifdef PNG_READ_STRIP_ALPHA_SUPPORTED
                png_set_strip_alpha(read_ptr);
#endif
            }

            if((output_color_type == 4 || output_color_type == 6) &&
               (color_type != 4 && color_type != 6))
            {
                if(verbose > 0 && first_trial)
                   fprintf(STDERR, "   Adding an alpha channel.\n");
#ifdef PNG_READ_FILLER_SUPPORTED
                png_set_filler(read_ptr, (png_uint_32)65535L, PNG_FILLER_AFTER);
#endif
                need_expand = 1;
            }

            if(output_color_type != 0 && output_color_type != 3 &&
               output_bit_depth < 8) output_bit_depth = 8;

            if((output_color_type == 2 || output_color_type == 6) &&
               color_type == 3)
            {
                if(verbose > 0 && first_trial)
                   fprintf(STDERR, "   Expanding indexed color file.\n");
                need_expand = 1;
            }

#ifdef PNG_READ_EXPAND_SUPPORTED
            if (need_expand == 1)
                png_set_expand(read_ptr);
#endif

            if(nosave == 0)
            {
               int required_window;
               int channels=0;

               write_ptr->flags |= PNG_FLAG_ZLIB_CUSTOM_STRATEGY;
               write_ptr->zlib_strategy = z_strategy;

               if (output_color_type == 0)channels=1;
               if (output_color_type == 2)channels=3;
               if (output_color_type == 3)channels=1;
               if (output_color_type == 4)channels=2;
               if (output_color_type == 6)channels=4;

               required_window=(int)(height*((width*channels*bit_depth+15)>>3));

#ifdef WBITS_8_OK
               if     (required_window <=   256)compression_window =  8;
               else if(required_window <=   512)compression_window =  9;
#else
               if     (required_window <=   512)compression_window =  9;
#endif
               else if(required_window <=  1024)compression_window = 10;
               else if(required_window <=  2048)compression_window = 11;
               else if(required_window <=  4096)compression_window = 12;
               else if(required_window <=  8192)compression_window = 13;
               else if(required_window <= 16386)compression_window = 14;
               else compression_window = 15;
               if(compression_window > default_compression_window ||
                   force_compression_window)
                 compression_window = default_compression_window;

               if(verbose > 1 && first_trial && (compression_window != 15 ||
                     force_compression_window))
                  fprintf(STDERR, "   Compression window for output= %d\n",
                     1 << compression_window);

               png_set_compression_window_bits(write_ptr, compression_window);
            }

            if(verbose > 1)
                fprintf(STDERR, "   Setting IHDR\n");

            png_set_IHDR(write_ptr, write_info_ptr, width, height,
              output_bit_depth, output_color_type, interlace_type,
              compression_type, filter_type);

            if(output_color_type != input_color_type) things_have_changed++;
         }
      }
#if defined(PNG_READ_bKGD_SUPPORTED) && defined(PNG_WRITE_bKGD_SUPPORTED)
      {
         png_color_16p background;

         if (!have_bkgd && png_get_bKGD(read_ptr, read_info_ptr, &background))
         {
            if(keep_chunk("bKGD",argv))
            {
               if((input_color_type == 2 || input_color_type == 6 ) &&
                  (output_color_type == 0 || output_color_type == 4))
                    background->gray = background->green;
               png_set_bKGD(write_ptr, write_info_ptr, background);
            }
         }
         if (have_bkgd)
         {
            png_color_16 backgd;
            png_color_16p background = &backgd;
            background->red=bkgd_red;
            background->green=bkgd_green;
            background->blue=bkgd_blue;
            background->gray = background->green;
            png_set_bKGD(write_ptr, write_info_ptr, background);
         }
      }
#endif
#if defined(PNG_READ_cHRM_SUPPORTED) && defined(PNG_WRITE_cHRM_SUPPORTED)
      {
         png_fixed_point white_x, white_y, red_x, red_y, green_x, green_y,
            blue_x, blue_y;

         if (png_get_cHRM_fixed(read_ptr, read_info_ptr, &white_x, &white_y,
            &red_x, &red_y, &green_x, &green_y, &blue_x, &blue_y))
         {
            if(keep_chunk("cHRM",argv))
            png_set_cHRM_fixed(write_ptr, write_info_ptr, white_x, white_y,
               red_x, red_y, green_x, green_y, blue_x, blue_y);
         }
      }
#endif
#if defined(PNG_READ_gAMA_SUPPORTED) && defined(PNG_WRITE_gAMA_SUPPORTED)
      {
         if(force_specified_gamma > 0)
         {
            if(first_trial)
            {
               things_have_changed=1;
               if(verbose > 0)
                 fprintf(STDERR,
                "   Inserting gAMA chunk with gamma=(%d/100000)\n",
                    force_specified_gamma);
            }
            png_set_gAMA_fixed(write_ptr, write_info_ptr, 
               (png_fixed_point)force_specified_gamma);
            file_gamma=(png_fixed_point)force_specified_gamma;
         }
         else if (png_get_gAMA_fixed(read_ptr, read_info_ptr, &file_gamma))
         {
            if(keep_chunk("gAMA",argv))
            {
               if(verbose > 1 && first_trial)
                 fprintf(STDERR, "   gamma=(%d/100000)\n", (int)file_gamma);
               if(double_gamma)
                 file_gamma+=file_gamma;
               png_set_gAMA_fixed(write_ptr, write_info_ptr, file_gamma);
            }
         }
         else if(specified_gamma > 0)
         {
            if(first_trial)
            {
               things_have_changed=1;
               if(verbose > 0)
                 fprintf(STDERR,
                 "   Inserting gAMA chunk with gamma=(%d/100000)\n",
                    specified_gamma);
            }
               png_set_gAMA_fixed(write_ptr, write_info_ptr, specified_gamma);
            file_gamma=(png_fixed_point)specified_gamma;
         }
      }
#endif
#if defined(PNG_READ_sRGB_SUPPORTED) && defined(PNG_WRITE_sRGB_SUPPORTED)
      {
         int file_intent;

         if (png_get_sRGB(read_ptr, read_info_ptr, &file_intent))
         {
            if(keep_chunk("sRGB",argv))
            png_set_sRGB(write_ptr, write_info_ptr, file_intent);
         }
         else if(intent >= 0)
         {
#ifdef PNG_gAMA_SUPPORTED
            if(file_gamma >= 45000L && file_gamma <= 46000L)
            {
               things_have_changed=1;
               if(first_trial)
               fprintf(STDERR, "   Inserting sRGB chunk with intent=%d\n",intent);
               png_set_sRGB(write_ptr, write_info_ptr, intent);
            }
            else if(file_gamma == 0)
            {
               things_have_changed=1;
               png_set_sRGB_gAMA_and_cHRM(write_ptr, write_info_ptr, intent);
            }
            else
            {
               if(first_trial)
               {
                  fprintf(STDERR,
          "   Ignoring sRGB request; gamma=(%lu/100000) is not approx. 0.455\n",
                   file_gamma);
               }
            }
#endif
         }
      }
#endif
#if defined(PNG_READ_iCCP_SUPPORTED) && defined(PNG_WRITE_iCCP_SUPPORTED)
   {
      png_charp name;
      png_charp profile;
      png_uint_32 proflen;
      int compression_type;

      if (png_get_iCCP(read_ptr, read_info_ptr, &name, &compression_type, 
                      &profile, &proflen))
      {
         if(keep_chunk("iCCP",argv))
            png_set_iCCP(write_ptr, write_info_ptr, name, compression_type, 
                      profile, proflen);
      }
   }
#endif
#if defined(PNG_READ_oFFs_SUPPORTED) && defined(PNG_WRITE_oFFs_SUPPORTED)
      {
         png_int_32 offset_x, offset_y;
         int unit_type;

         if (png_get_oFFs(read_ptr, read_info_ptr,&offset_x,&offset_y,&unit_type))
         {
            if(offset_x == 0 && offset_y == 0)
            {
               if(verbose > 0 && first_trial)
                  fprintf(STDERR, "   Deleting useless oFFs 0 0 chunk\n");
            }
            else
            {
            if(keep_chunk("oFFs",argv))
            png_set_oFFs(write_ptr, write_info_ptr, offset_x, offset_y,
               unit_type);
            }
         }
      }
#endif
#if defined(PNG_READ_pCAL_SUPPORTED) && defined(PNG_WRITE_pCAL_SUPPORTED)
      {
         png_charp purpose, units;
         png_charpp params;
         png_int_32 X0, X1;
         int type, nparams;

         if (png_get_pCAL(read_ptr, read_info_ptr, &purpose, &X0, &X1, &type,
            &nparams, &units, &params))
         {
            if(keep_chunk("pCAL",argv))
            png_set_pCAL(write_ptr, write_info_ptr, purpose, X0, X1, type,
               nparams, units, params);
         }
      }
#endif
#if defined(PNG_READ_pHYs_SUPPORTED) && defined(PNG_WRITE_pHYs_SUPPORTED)
      {
         png_uint_32 res_x, res_y;
         int unit_type;

         if(resolution == 0)
         {
            if (png_get_pHYs(read_ptr, read_info_ptr, &res_x, &res_y,
                &unit_type))
            {
               if(keep_chunk("pHYs",argv))
            png_set_pHYs(write_ptr, write_info_ptr, res_x, res_y, unit_type);
            }
         }
         else
         {
            unit_type=1;
            res_x = res_y = (png_uint_32)((resolution/.0254 + 0.5));
            png_set_pHYs(write_ptr, write_info_ptr, res_x, res_y, unit_type);
            if(verbose > 0 && first_trial)
               fprintf(STDERR, "   Added pHYs %lu %lu 1 chunk\n",res_x,res_y);
         }
      }
#endif

     if (png_get_PLTE(read_ptr, read_info_ptr, &palette, &num_palette))
     {
        if (plte_len > 0)
           num_palette=plte_len;
        if (do_pplt != 0)
        {
           printf("PPLT: %s\n",pplt_string);
        }
        if(output_color_type == 3)
           png_set_PLTE(write_ptr, write_info_ptr, palette, num_palette);
        else if(keep_chunk("PLTE",argv))
           png_set_PLTE(write_ptr, write_info_ptr, palette, num_palette);
        if(verbose > 1 && first_trial)
        {
           int i;
           png_colorp p = palette;
           fprintf(STDERR, "   Palette:\n");
           fprintf(STDERR, "      I    R    G    B ( color )    A\n");
           for (i=0; i<num_palette; i++)
           {
              fprintf(STDERR, "   %4d %4d %4d %4d (#%2.2x%2.2x%2.2x) %4d\n",
                  i, p->red, p->green, p->blue,
                     p->red, p->green, p->blue,
                     trns_array[i]);
              p++;
           }
        }
     }

#if defined(PNG_READ_hIST_SUPPORTED) && defined(PNG_WRITE_hIST_SUPPORTED)
      {
         png_uint_16p hist;

         if (png_get_hIST(read_ptr, read_info_ptr, &hist))
         {
            if(keep_chunk("hIST",argv))
            png_set_hIST(write_ptr, write_info_ptr, hist);
         }
      }
#endif
#if defined(PNG_READ_tRNS_SUPPORTED) && defined(PNG_WRITE_tRNS_SUPPORTED)
      {
         png_bytep trans;
         int num_trans;
         png_color_16p trans_values;

         if (png_get_tRNS(read_ptr, read_info_ptr, &trans, &num_trans,
            &trans_values))
         {
            if(verbose > 1)
               fprintf(STDERR,"   Got tRNS chunk.\n");
            if(keep_chunk("tRNS",argv))
            {
                int last_nonmax = -1;
                trns_red = trans_values->red;
                trns_green = trans_values->green;
                trns_blue = trans_values->blue;
                trns_gray = trans_values->gray;
                if(output_color_type == 3)
                  {
                    if(verbose > 2)
                       fprintf(STDERR,"   Filling trns_array\n");
                    for (ia=0;ia<num_trans;ia++)
                       trns_array[ia]=trans[ia];
                    if(verbose > 2)
                       fprintf(STDERR,"   Extending trns_array\n");
                    for ( ; ia<256; ia++)
                       trns_array[ia]=255;
                    if(verbose > 2)
                       fprintf(STDERR,"   Done filling trns_array\n");
                    for (ia=0; ia<256; ia++)
                      {
                       if(trns_array[ia] != 255)
                          last_nonmax=ia;
                      }
                    num_trans = last_nonmax+1;
                    if(num_trans == 0 && verbose > 0)
                       fprintf(STDERR,"   Deleting all-opaque tRNS chunk.\n");
                  }
                if(verbose > 1)
                   fprintf(STDERR,"   png_set_tRNS, num_trans=%d\n",num_trans);
                if (output_color_type != 3 || num_trans)
                   png_set_tRNS(write_ptr, write_info_ptr, trans, num_trans,
                      trans_values);
            }
         }
         else if (have_trns == 1) /* will not overwrite existing trns data */
         {
            png_color_16 trans_data;
            png_byte index_data = (png_byte)trns_index;
            num_trans = index_data+1;
            if(verbose > 1)
              fprintf(STDERR,"Have_tRNS, num_trans=%d\n",num_trans);
            for (ia=0;ia<256;ia++)
                trns_array[ia]=255;
            trns_array[index_data]=0;

            trans_data.index = index_data;
            trans_data.red   = trns_red;
            trans_data.green = trns_green;
            trans_data.blue  = trns_blue;
            trans_data.gray  = trns_gray;
            trans_values = &trans_data;

         if(verbose > 1)
            fprintf(STDERR,"png_set_tRNS\n");
            png_set_tRNS(write_ptr, write_info_ptr, trns_array, num_trans,
               trans_values);

            things_have_changed=1;
         }
         else
         {
            for (ia=0 ; ia<256; ia++)
               trns_array[ia]=255;
         }
         if (verbose > 1 && first_trial)
         {
            int last=-1;
            for (i=0 ; ia<num_palette; ia++)
               if(trns_array[ia] != 255) last = ia;
            if(last >= 0)
            {
               fprintf(STDERR, "   Transparency:\n");
               if(output_color_type == 3)
                  for (i=0 ; ia<num_palette; ia++)
                     fprintf(STDERR, "      %4d %4d\n",ia,trns_array[ia]);
               else if(output_color_type == 0)
                     fprintf(STDERR, "      %d\n",trns_gray);
               else if(output_color_type == 2)
                     fprintf(STDERR, "      %d %d %d\n",
                        trns_red, trns_green, trns_blue);
            }
         }
      }
#endif

#if defined(PNG_READ_sBIT_SUPPORTED) && defined(PNG_WRITE_sBIT_SUPPORTED)
      {
         png_color_8p sig_bit;

         if (png_get_sBIT(read_ptr, read_info_ptr, &sig_bit))
         {
            if(keep_chunk("sBIT",argv))
            {
               if((input_color_type == 0 || input_color_type == 4) &&
                  (output_color_type == 2 || output_color_type == 6 ||
                   output_color_type == 3))
                    sig_bit->red = sig_bit->green = sig_bit->blue
                                 = sig_bit->gray;
               if((input_color_type == 2 || input_color_type == 6 ||
                   output_color_type == 3) &&
                  (output_color_type == 0 || output_color_type == 4))
                    sig_bit->gray = sig_bit->green;

               png_set_sBIT(write_ptr, write_info_ptr, sig_bit);
            }
         }
      }
#endif
#if defined(PNG_sCAL_SUPPORTED)
#ifdef PNG_FLOATING_POINT_SUPPORTED
   {
      int unit;
      double width, height;

      if (png_get_sCAL(read_ptr, read_info_ptr, &unit, &width, &height))
      {
         png_set_sCAL(write_ptr, write_info_ptr, unit, width, height);
      }
   }
#else
#ifdef PNG_FIXED_POINT_SUPPORTED
   {
      int unit;
      png_charp width, height;

      if (png_get_sCAL_s(read_ptr, read_info_ptr, &unit, &width, &height))
      {
         if(keep_chunk("sCAL",argv))
            png_set_sCAL_s(write_ptr, write_info_ptr, unit, width, height);
      }
   }
#endif
#endif
#endif
#if defined(PNG_sPLT_SUPPORTED)
   {
      png_sPLT_tp entries;
      int num_entries;

      num_entries = (int)png_get_sPLT(read_ptr, read_info_ptr, &entries);
      if (num_entries)
      {
         if(keep_chunk("sPLT",argv))
            png_set_sPLT(write_ptr, write_info_ptr, entries, num_entries);
         png_free_data(read_ptr, read_info_ptr, PNG_FREE_SPLT, num_entries);
      }
   }
#endif

#if defined(PNG_TEXT_SUPPORTED)
      {
         png_textp text_ptr;
         int num_text=0;

         if (png_get_text(read_ptr, read_info_ptr, &text_ptr, &num_text) > 0 ||
             text_inputs != 0)
         {
            int ntext;
            png_debug1(0, "Handling %d tEXt/zTXt chunks\n", num_text);

            if (verbose > 1 && first_trial && num_text > 0)
            {
               for (ntext = 0; ntext < num_text; ntext++)
               {
                  fprintf(STDERR,"%d  %s",ntext,text_ptr[ntext].key);
                  if(text_ptr[ntext].text_length != 0)
                     fprintf(STDERR,": %s\n",text_ptr[ntext].text);
                  else if (text_ptr[ntext].itxt_length != 0)
                  {
                     fprintf(STDERR," (%s: %s): \n",
                          text_ptr[ntext].lang,
                          text_ptr[ntext].lang_key);
                     fprintf(STDERR,"%s\n",text_ptr[ntext].text);
                  }
                  else
                     fprintf(STDERR,"\n");
               }
            }

            if(num_text > 0)
            {
               if(keep_chunk("text",argv))
                 {
                   int num_to_write=num_text;
                   for (ntext = 0; ntext < num_text; ntext++)
                   {
                     if (first_trial)
                       P2("Text chunk before IDAT, compression=%d\n",
                         text_ptr[ntext].compression);
                     if(text_ptr[ntext].compression==PNG_TEXT_COMPRESSION_NONE)
                       {
                         if(!keep_chunk("tEXt",argv))
                           {
                             text_ptr[ntext].key[0]='\0';
                             num_to_write--;
                           }
                       }
                     if(text_ptr[ntext].compression==PNG_TEXT_COMPRESSION_zTXt)
                       {
                         if(!keep_chunk("zTXt",argv))
                           {
                             text_ptr[ntext].key[0]='\0';
                             num_to_write--;
                           }
                       }
                     if(text_ptr[ntext].compression==PNG_ITXT_COMPRESSION_NONE
                       ||text_ptr[ntext].compression==PNG_ITXT_COMPRESSION_zTXt)
                       {
                         if(!keep_chunk("iTXt",argv))
                           {
                             text_ptr[ntext].key[0]='\0';
                             num_to_write--;
                           }
                       }
                   }
                   if (num_to_write > 0)
                      png_set_text(write_ptr, write_info_ptr, text_ptr,
                         num_text);
                 }
            }
            for (ntext=0; ntext<text_inputs; ntext++)
              {
                if(text_where[ntext] == 1)
                  {
                    png_textp added_text;
                    added_text = (png_textp)
                       png_malloc(write_ptr, (png_uint_32)sizeof(png_text));
                    added_text[0].key = &text_keyword[ntext*80];
                    added_text[0].lang = &text_lang[ntext*80];
                    added_text[0].lang_key = &text_lang_key[ntext*80];
                    added_text[0].text = &text_text[ntext*2048];
                    added_text[0].compression = text_compression[ntext];
                    png_set_text(write_ptr, write_info_ptr, added_text, 1);
                    if(added_text[0].compression < 0)
                       printf("   Added a tEXt chunk.\n");
                    else if(added_text[0].compression == 0)
                       printf("   Added a zTXt chunk.\n");
                    else if(added_text[0].compression == 1)
                       printf("   Added an uncompressed iTXt chunk.\n");
                    else
                       printf("   Added a compressed iTXt chunk.\n");
                    png_free(write_ptr,added_text);
                  }
              }
         }
      }
#endif
#if defined(PNG_READ_tIME_SUPPORTED) && defined(PNG_WRITE_tIME_SUPPORTED)
      {
         png_timep mod_time;

         if (png_get_tIME(read_ptr, read_info_ptr, &mod_time))
         {
            if(keep_chunk("tIME",argv))
            png_set_tIME(write_ptr, write_info_ptr, mod_time);
         }
      }
#endif

      png_read_transform_info(read_ptr, read_info_ptr);

      if(nosave == 0)
      {
      png_set_compression_level(write_ptr, zlib_level);

      if     (filter_method == 0)png_set_filter(write_ptr,0,PNG_FILTER_NONE);
      else if(filter_method == 1)png_set_filter(write_ptr,0,PNG_FILTER_SUB);
      else if(filter_method == 2)png_set_filter(write_ptr,0,PNG_FILTER_UP);
      else if(filter_method == 3)png_set_filter(write_ptr,0,PNG_FILTER_AVG);
      else if(filter_method == 4)png_set_filter(write_ptr,0,PNG_FILTER_PAETH);
      else if(filter_method == 5)png_set_filter(write_ptr,0,PNG_ALL_FILTERS);
      else                       png_set_filter(write_ptr,0,PNG_FILTER_NONE);


#if defined(PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED)
      {
         png_unknown_chunkp unknowns;
         int num_unknowns = (int)png_get_unknown_chunks(read_ptr, read_info_ptr,
            &unknowns);
         if (num_unknowns)
         {
            png_size_t i;
            png_set_unknown_chunks(write_ptr, write_info_ptr, unknowns,
              num_unknowns);
            for (i = 0; i < read_info_ptr->unknown_chunks_num; i++)
              write_info_ptr->unknown_chunks[i].location =
                 unknowns[i].location;
         }
      }
#endif

      P2("writing info structure.\n");
      png_crush_pause();
      png_debug(0, "\nWriting info struct\n");
      png_write_info(write_ptr, write_info_ptr);
      png_debug(0, "\nWrote info struct\n");

      P2("wrote info structure.\n");
      png_crush_pause();

#ifdef PNG_WRITE_PACK_SUPPORTED
      if(output_bit_depth < input_bit_depth)
      {
          png_color_8 true_bits;
          write_ptr->bit_depth=(png_byte)output_bit_depth;
          true_bits.gray = (png_byte)(8 - (input_bit_depth - output_bit_depth));
          png_set_shift(read_ptr, &true_bits);
          png_set_packing(write_ptr);
      }
#endif
      }  /* no save */

#define LARGE_PNGCRUSH

#ifndef LARGE_PNGCRUSH
      {
         png_uint_32 rowbytes_s;
         png_uint_32 rowbytes;

         rowbytes = png_get_rowbytes(read_ptr, read_info_ptr);

         rowbytes_s = (png_size_t)rowbytes;
         if(rowbytes == (png_uint_32)rowbytes_s)
            row_buf = png_malloc(read_ptr, rowbytes+2 );
         else
         {
            fprintf(STDERR, "rowbytes= %d\n",rowbytes);
            row_buf = NULL;
         }
      }
#else
      {
      png_uint_32 read_row_length, write_row_length, row_length;
      read_row_length=
         (png_uint_32)(png_get_rowbytes(read_ptr, read_info_ptr));
      write_row_length=
         (png_uint_32)(png_get_rowbytes(write_ptr, write_info_ptr));
      row_length = read_row_length > write_row_length ?
          read_row_length : write_row_length;
      row_buf = (png_bytep)png_malloc(read_ptr,row_length+16);
      }
#endif

      if (row_buf == NULL)
      {
         fprintf(STDERR, "Insufficient memory to allocate row buffer\n");
         png_destroy_read_struct(&read_ptr, &read_info_ptr, (png_infopp)NULL);
         png_destroy_write_struct(&write_ptr, &write_info_ptr);
         if(png_row_filters != NULL)
         {
            free(png_row_filters); png_row_filters=NULL;
         }
         FCLOSE(fpin);
         FCLOSE(fpout);
         return 1;
      }

      {
      /* check for sufficient memory: we need 2*zlib_window
         and, if filter_method == 5, 4*rowbytes in separate allocations.
         If it's not enough we can drop the "average" filter and
         we can reduce the zlib_window for writing.  We can't change
         the input zlib_window because the input file might have
         used the full 32K sliding window.
       */
      }

      P2("allocated rowbuf.\n");
      png_crush_pause();

      num_pass = png_set_interlace_handling(read_ptr);
      if(nosave == 0)
        png_set_interlace_handling(write_ptr);

      t_stop = (float)clock();
      t_misc += (t_stop - t_start);
      t_start = t_stop;
      for (pass = 0; pass < num_pass; pass++)
      {
         png_debug(0, "\nBegin Pass\n");
         for (y = 0; y < height; y++)
         {
            png_read_row(read_ptr, row_buf, (png_bytep)NULL);
            if(nosave == 0)
            {
               t_stop = (float)clock();
               t_decode += (t_stop - t_start);
               t_start = t_stop;
               png_write_row(write_ptr, row_buf);
               t_stop = (float)clock();
               t_encode += (t_stop - t_start);
               t_start = t_stop;
            }
         }
         png_debug(0, "\nEnd Pass\n");
      }
      if(nosave)
      {
          t_stop = (float)clock();
          t_decode += (t_stop - t_start);
          t_start = t_stop;
      }

   
#if defined(PNG_READ_RGB_TO_GRAY_SUPPORTED) && \
    defined(PNG_FLOATING_POINT_SUPPORTED)
      if((color_type == 2 || color_type == 6 || color_type == 3) &&
          (output_color_type == 0 || output_color_type == 4))
      {
          png_byte rgb_error = png_get_rgb_to_gray_status(read_ptr);
          if((first_trial) && rgb_error)
            printf("   **** Converted non-gray image to gray. **** \n");
      }
#endif

#ifdef PNG_FREE_UNKN 
#  if defined(PNG_READ_UNKNOWN_CHUNKS_SUPPORTED)
   png_free_data(read_ptr, read_info_ptr, PNG_FREE_UNKN, -1);
#  endif
#  if defined(PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED)
   png_free_data(write_ptr, write_info_ptr, PNG_FREE_UNKN, -1);
#  endif
#else
#  if defined(PNG_READ_UNKNOWN_CHUNKS_SUPPORTED)
   png_free_unknown_chunks(read_ptr, read_info_ptr, -1);
#  endif
#  if defined(PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED)
   png_free_unknown_chunks(write_ptr, write_info_ptr, -1);
#  endif
#endif

      png_debug(0, "Reading and writing end_info data\n");
      png_read_end(read_ptr, end_info_ptr);

#if (defined(PNG_READ_tEXt_SUPPORTED) && defined(PNG_WRITE_tEXt_SUPPORTED)) || \
       (defined(PNG_READ_zTXt_SUPPORTED) && defined(PNG_WRITE_zTXt_SUPPORTED))
      {
         png_textp text_ptr;
         int num_text=0;

         if (png_get_text(read_ptr, end_info_ptr, &text_ptr, &num_text) > 0 ||
             text_inputs != 0)
         {
            int ntext;
            png_debug1(0, "Handling %d tEXt/zTXt chunks\n", num_text);

            if (verbose > 1 && first_trial && num_text > 0)
            {
               for (ntext = 0; ntext < num_text; ntext++)
               {
                  fprintf(STDERR,"%d  %s",ntext,text_ptr[ntext].key);
                  if(text_ptr[ntext].text_length != 0)
                     fprintf(STDERR,": %s\n",text_ptr[ntext].text);
                  else if (text_ptr[ntext].itxt_length != 0)
                  {
                     fprintf(STDERR," (%s: %s): \n",
                          text_ptr[ntext].lang,
                          text_ptr[ntext].lang_key);
                     fprintf(STDERR,"%s\n",text_ptr[ntext].text);
                  }
                  else
                     fprintf(STDERR,"\n");
               }
            }

            if(num_text > 0)
            {
               if(keep_chunk("text",argv))
                 {
                   int num_to_write=num_text;
                   for (ntext = 0; ntext < num_text; ntext++)
                   {
                     if (first_trial)
                       P2("Text chunk after IDAT, compression=%d\n",
                          text_ptr[ntext].compression);
                     if(text_ptr[ntext].compression==PNG_TEXT_COMPRESSION_NONE)
                       {
                         if(!keep_chunk("tEXt",argv))
                           {
                             text_ptr[ntext].key[0]='\0';
                             num_to_write--;
                           }
                       }
                     if(text_ptr[ntext].compression==PNG_TEXT_COMPRESSION_zTXt)
                       {
                         if(!keep_chunk("zTXt",argv))
                           {
                             text_ptr[ntext].key[0]='\0';
                             num_to_write--;
                           }
                       }
                     if(text_ptr[ntext].compression==PNG_ITXT_COMPRESSION_NONE
                       ||text_ptr[ntext].compression==PNG_ITXT_COMPRESSION_zTXt)
                       {
                         if(!keep_chunk("iTXt",argv))
                           {
                             text_ptr[ntext].key[0]='\0';
                             num_to_write--;
                           }
                       }
                   }
                   if (num_to_write > 0)
                      png_set_text(write_ptr, write_end_info_ptr, text_ptr,
                          num_text);
                 }
            }
            for (ntext=0; ntext<text_inputs; ntext++)
              {
                if(text_where[ntext] == 2)
                  {
                    png_textp added_text;
                    added_text = (png_textp)
                       png_malloc(write_ptr, (png_uint_32)sizeof(png_text));
                    added_text[0].key = &text_keyword[ntext*80];
                    added_text[0].lang = &text_lang[ntext*80];
                    added_text[0].lang_key = &text_lang_key[ntext*80];
                    added_text[0].text = &text_text[ntext*2048];
                    added_text[0].compression = text_compression[ntext];
                    png_set_text(write_ptr, write_end_info_ptr, added_text, 1);
                    if(added_text[0].compression < 0)
                       printf("   Added a tEXt chunk.\n");
                    else if(added_text[0].compression == 0)
                       printf("   Added a zTXt chunk.\n");
                    else if(added_text[0].compression == 1)
                       printf("   Added an uncompressed iTXt chunk.\n");
                    else
                       printf("   Added a compressed iTXt chunk.\n");
                    png_free(write_ptr,added_text);
                  }
              }
         }
      }
#endif
#if defined(PNG_READ_tIME_SUPPORTED) && defined(PNG_WRITE_tIME_SUPPORTED)
      {
         png_timep mod_time;

         if (png_get_tIME(read_ptr, end_info_ptr, &mod_time))
         {
            if(keep_chunk("tIME",argv))
            png_set_tIME(write_ptr, write_end_info_ptr, mod_time);
         }
      }
#endif

#if defined(PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED)
   {
      png_unknown_chunkp unknowns;
      int num_unknowns = (int)png_get_unknown_chunks(read_ptr, read_info_ptr,
         &unknowns);
      if (num_unknowns && nosave == 0)
      {
         png_size_t i;
         printf("setting %d unknown chunks after IDAT\n",num_unknowns);
         png_set_unknown_chunks(write_ptr, write_end_info_ptr, unknowns,
           num_unknowns);
         for (i = 0; i < read_info_ptr->unknown_chunks_num; i++)
           write_end_info_ptr->unknown_chunks[i].location =
              unknowns[i].location;
      }
   }
#endif

      if(nosave == 0)
         png_write_end(write_ptr, write_end_info_ptr);

      png_debug(0, "Destroying data structs\n");
      if(row_buf != (png_bytep)NULL)
         png_free(read_ptr, row_buf);
      row_buf = (png_bytep)NULL;
      png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
      if(nosave == 0)
      {
         png_destroy_info_struct(write_ptr, &write_end_info_ptr);
         png_destroy_write_struct(&write_ptr, &write_info_ptr);
      }
      read_ptr=NULL;
      write_ptr=NULL;

      FCLOSE(fpin);
      if(nosave == 0)
         FCLOSE(fpout);

      if(nosave != 0)
         break;

      if (nosave == 0)
      {
         png_debug(0, "Opening file for length measurement\n");
         if ((fpin = FOPEN(outname, "rb")) == NULL)
         {
            fprintf(STDERR, "Could not find file %s\n", outname);
            if(png_row_filters != NULL)
            {
               free(png_row_filters); png_row_filters=NULL;
            }
            return 1;
         }
         number_of_open_files++;

         idat_length[trial]=measure_idats(fpin);

         FCLOSE(fpin);
      }

      if(verbose  > 0 && trial != MAX_METHODS)
         {
         fprintf(STDERR,
         "   IDAT length with method %d (fm %d zl %d zs %d)= %8lu\n",
             trial,filter_method,zlib_level,z_strategy,idat_length[trial]);
         fflush(STDERR);
         }

         first_trial=0;
      } /* end of trial-loop */

      if (fpin)
      {
         FCLOSE(fpin);
      }
      if (nosave == 0 && fpout)
      {
         FCLOSE(fpout);
      }

      if(verbose > 0 && nosave == 0)
      {
         png_uint_32 input_length, output_length;

         struct stat stat_buf;

         stat(inname, &stat_buf);
         input_length = (unsigned long)stat_buf.st_size;
         total_input_length += input_length;
         stat(outname, &stat_buf);
         output_length = (unsigned long)stat_buf.st_size;
         total_output_length += output_length;
         if(input_length == output_length)
            fprintf(STDERR,
               "   Best %s method = %d for %s (no change)\n\n",
                progname, best, outname);
         else if(input_length > output_length)
            fprintf(STDERR,
               "   Best %s method = %d for %s (%4.2f%% reduction)\n\n",
                progname, best, outname,
               (100.0 - (100.0*output_length)/input_length));
         else
            fprintf(STDERR,
               "   Best %s method = %d for %s (%4.2f%% increase)\n\n",
                progname, best, outname,
               -(100.0 - (100.0*output_length)/input_length));
         if(verbose > 2)
            fprintf(STDERR, "   Number of open files=%d\n",number_of_open_files);

      }
      if(pngcrush_mode == DEFAULT_MODE)
      {
         if(png_row_filters != NULL)
         {
            free(png_row_filters); png_row_filters=NULL;
         }
         if(verbose > 0) show_result();
         return 0;
      }
   }  /* end of loop on input files */
}

png_uint_32
measure_idats(FILE *fpin)
{
   png_uint_32 measured_idat_length;
   P2("measure_idats:\n");
   png_debug(0, "Allocating read structure\n");
   read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL,
      (png_error_ptr)NULL, (png_error_ptr)NULL);
#if defined(PNG_NO_STDIO)
   png_set_error_fn(read_ptr, (png_voidp)inname, png_default_error, png_default_warning);
#endif
   png_debug(0, "Allocating read_info,  end_info structures\n");
   read_info_ptr = png_create_info_struct(read_ptr);
   end_info_ptr = png_create_info_struct(read_ptr);
   png_debug(0, "Setting jmpbuf for read struct\n");

#ifndef PNG_SETJMP_NOT_SUPPORTED
#  ifdef USE_FAR_KEYWORD
   if (setjmp(jmpbuf))
#  else
#    ifdef PNG_JMPBUF_SUPPORTED
   /* New setjmp interface */
   if (setjmp(png_jmpbuf(read_ptr)))
#    else
   /* old interface */
   if (setjmp(read_ptr->jmpbuf))
#    endif
#  endif
      {
          PNG_CRUSH_CLEANUP
          P2("returning from measure_idats after longjump\n");
          return 0;
      }

#  if defined(USE_FAR_KEYWORD)
   png_memcpy(png_jmpbuf(read_ptr),jmpbuf,sizeof(jmp_buf));
#  endif
#endif

#if !defined(PNG_NO_STDIO)
   png_init_io(read_ptr, fpin);
#else
   png_set_read_fn(read_ptr, (png_voidp)fpin, png_default_read_data);
#endif

   read_ptr->sig_bytes=0;
   measured_idat_length=png_measure_idat(read_ptr, read_info_ptr);
   P2("measure_idats: IDAT length=%lu\n",measured_idat_length);
   png_debug(0, "Destroying data structs\n");
   png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
   return measured_idat_length;
}


png_uint_32
png_measure_idat(png_structp png_ptr, png_infop info_ptr)
{
   png_uint_32 sum_idat_length=0;
   png_debug(1, "in png_read_info\n");

   /* If we haven't checked all of the PNG signature bytes, do so now. */
   if (png_ptr->sig_bytes < 8)
   {
      png_size_t num_checked = png_ptr->sig_bytes,
                 num_to_check = 8 - num_checked;

      png_read_data(png_ptr, &(info_ptr->signature[num_checked]), num_to_check);
      png_ptr->sig_bytes = 8;

      if (png_sig_cmp(info_ptr->signature, num_checked, num_to_check))
      {
         if (num_checked < 4 &&
             png_sig_cmp(info_ptr->signature, num_checked, num_to_check - 4))
            png_error(png_ptr, "Not a PNG file");
         else
            png_error(png_ptr, "PNG file corrupted by ASCII conversion");
      }
   }

   for(;;)
   {
#ifndef PNG_UINT_IDAT
#ifdef PNG_USE_LOCAL_ARRAYS
      PNG_IDAT;
      PNG_IEND;
#endif
#endif
      png_byte chunk_name[5];
      png_byte chunk_length[4];
      png_uint_32 length;

      png_read_data(png_ptr, chunk_length, 4);
      length = png_get_uint_32(chunk_length);

      png_reset_crc(png_ptr);
      png_crc_read(png_ptr, chunk_name, 4);


#ifdef PNG_UINT_IDAT
      if (png_get_uint_32(chunk_name) == PNG_UINT_IDAT)
#else
      if (!png_memcmp(chunk_name, png_IDAT, 4))
#endif
         sum_idat_length += length;

      if(verbose > 1)
      {
         chunk_name[4]='\0';
         printf( "Reading %s chunk, length = %ld.\n", chunk_name, length);
      }
      png_crc_finish(png_ptr, length);

#ifdef PNG_UINT_IEND
      if (png_get_uint_32(chunk_name) == PNG_UINT_IEND)
#else
      if (!png_memcmp(chunk_name, png_IEND, 4))
#endif
         return sum_idat_length;
   }
}

