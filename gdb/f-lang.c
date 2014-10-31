/* Fortran language support routines for GDB, the GNU debugger.

   Copyright (C) 1993-1996, 1998-2005, 2007-2012 Free Software
   Foundation, Inc.

   Contributed by Motorola.  Adapted from the C parser by Farooq Butt
   (fmbutt@engage.sps.mot.com).

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "gdb_string.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "parser-defs.h"
#include "language.h"
#include "f-lang.h"
#include "valprint.h"
#include "value.h"
#include "cp-support.h"
#include "charset.h"


/* Following is dubious stuff that had been in the xcoff reader.  */

struct saved_fcn
  {
    long line_offset;		/* Line offset for function.  */
    struct saved_fcn *next;
  };


struct saved_bf_symnum
  {
    long symnum_fcn;		/* Symnum of function (i.e. .function
				   directive).  */
    long symnum_bf;		/* Symnum of .bf for this function.  */
    struct saved_bf_symnum *next;
  };

typedef struct saved_fcn SAVED_FUNCTION, *SAVED_FUNCTION_PTR;
typedef struct saved_bf_symnum SAVED_BF, *SAVED_BF_PTR;

/* Local functions */

extern void _initialize_f_language (void);
#if 0
static void clear_function_list (void);
static long get_bf_for_fcn (long);
static void clear_bf_list (void);
static void patch_all_commons_by_name (char *, CORE_ADDR, int);
static SAVED_F77_COMMON_PTR find_first_common_named (char *);
static void add_common_entry (struct symbol *);
static void add_common_block (char *, CORE_ADDR, int, char *);
static SAVED_FUNCTION *allocate_saved_function_node (void);
static SAVED_BF_PTR allocate_saved_bf_node (void);
static COMMON_ENTRY_PTR allocate_common_entry_node (void);
static SAVED_F77_COMMON_PTR allocate_saved_f77_common_node (void);
static void patch_common_entries (SAVED_F77_COMMON_PTR, CORE_ADDR, int);
#endif

static void f_printchar (int c, struct type *type, struct ui_file * stream);
static void f_emit_char (int c, struct type *type,
			 struct ui_file * stream, int quoter);

/* Return the encoding that should be used for the character type
   TYPE.  */

static const char *
f_get_encoding (struct type *type)
{
  const char *encoding;

  switch (TYPE_LENGTH (type))
    {
    case 1:
      encoding = target_charset (get_type_arch (type));
      break;
    case 4:
      if (gdbarch_byte_order (get_type_arch (type)) == BFD_ENDIAN_BIG)
	encoding = "UTF-32BE";
      else
	encoding = "UTF-32LE";
      break;

    default:
      error (_("unrecognized character type"));
    }

  return encoding;
}

/* Print the character C on STREAM as part of the contents of a literal
   string whose delimiter is QUOTER.  Note that that format for printing
   characters and strings is language specific.
   FIXME:  This is a copy of the same function from c-exp.y.  It should
   be replaced with a true F77 version.  */

static void
f_emit_char (int c, struct type *type, struct ui_file *stream, int quoter)
{
  const char *encoding = f_get_encoding (type);

  generic_emit_char (c, type, stream, quoter, encoding);
}

/* Implementation of la_printchar.  */

static void
f_printchar (int c, struct type *type, struct ui_file *stream)
{
  fputs_filtered ("'", stream);
  LA_EMIT_CHAR (c, type, stream, '\'');
  fputs_filtered ("'", stream);
}

/* Print the character string STRING, printing at most LENGTH characters.
   Printing stops early if the number hits print_max; repeat counts
   are printed as appropriate.  Print ellipses at the end if we
   had to stop before printing LENGTH characters, or if FORCE_ELLIPSES.
   FIXME:  This is a copy of the same function from c-exp.y.  It should
   be replaced with a true F77 version.  */

static void
f_printstr (struct ui_file *stream, struct type *type, const gdb_byte *string,
	    unsigned int length, const char *encoding, int force_ellipses,
	    const struct value_print_options *options)
{
  const char *type_encoding = f_get_encoding (type);

  if (TYPE_LENGTH (type) == 4)
    fputs_filtered ("4_", stream);

  if (!encoding || !*encoding)
    encoding = type_encoding;

  generic_printstr (stream, type, string, length, encoding,
		    force_ellipses, '\'', 0, options);
}


/* Table of operators and their precedences for printing expressions.  */

static const struct op_print f_op_print_tab[] =
{
  {"+", BINOP_ADD, PREC_ADD, 0},
  {"+", UNOP_PLUS, PREC_PREFIX, 0},
  {"-", BINOP_SUB, PREC_ADD, 0},
  {"-", UNOP_NEG, PREC_PREFIX, 0},
  {"*", BINOP_MUL, PREC_MUL, 0},
  {"/", BINOP_DIV, PREC_MUL, 0},
  {"DIV", BINOP_INTDIV, PREC_MUL, 0},
  {"MOD", BINOP_REM, PREC_MUL, 0},
  {"=", BINOP_ASSIGN, PREC_ASSIGN, 1},
  {".OR.", BINOP_LOGICAL_OR, PREC_LOGICAL_OR, 0},
  {".AND.", BINOP_LOGICAL_AND, PREC_LOGICAL_AND, 0},
  {".NOT.", UNOP_LOGICAL_NOT, PREC_PREFIX, 0},
  {".EQ.", BINOP_EQUAL, PREC_EQUAL, 0},
  {".NE.", BINOP_NOTEQUAL, PREC_EQUAL, 0},
  {".LE.", BINOP_LEQ, PREC_ORDER, 0},
  {".GE.", BINOP_GEQ, PREC_ORDER, 0},
  {".GT.", BINOP_GTR, PREC_ORDER, 0},
  {".LT.", BINOP_LESS, PREC_ORDER, 0},
  {"**", UNOP_IND, PREC_PREFIX, 0},
  {"@", BINOP_REPEAT, PREC_REPEAT, 0},
  {NULL, 0, 0, 0}
};

enum f_primitive_types {
  f_primitive_type_character,
  f_primitive_type_logical,
  f_primitive_type_logical_s1,
  f_primitive_type_logical_s2,
  f_primitive_type_logical_s8,
  f_primitive_type_integer,
  f_primitive_type_integer_s2,
  f_primitive_type_real,
  f_primitive_type_real_s8,
  f_primitive_type_real_s16,
  f_primitive_type_complex_s8,
  f_primitive_type_complex_s16,
  f_primitive_type_void,
  nr_f_primitive_types
};

static void
f_language_arch_info (struct gdbarch *gdbarch,
		      struct language_arch_info *lai)
{
  const struct builtin_f_type *builtin = builtin_f_type (gdbarch);

  lai->string_char_type = builtin->builtin_character;
  lai->primitive_type_vector
    = GDBARCH_OBSTACK_CALLOC (gdbarch, nr_f_primitive_types + 1,
                              struct type *);

  lai->primitive_type_vector [f_primitive_type_character]
    = builtin->builtin_character;
  lai->primitive_type_vector [f_primitive_type_logical]
    = builtin->builtin_logical;
  lai->primitive_type_vector [f_primitive_type_logical_s1]
    = builtin->builtin_logical_s1;
  lai->primitive_type_vector [f_primitive_type_logical_s2]
    = builtin->builtin_logical_s2;
  lai->primitive_type_vector [f_primitive_type_logical_s8]
    = builtin->builtin_logical_s8;
  lai->primitive_type_vector [f_primitive_type_real]
    = builtin->builtin_real;
  lai->primitive_type_vector [f_primitive_type_real_s8]
    = builtin->builtin_real_s8;
  lai->primitive_type_vector [f_primitive_type_real_s16]
    = builtin->builtin_real_s16;
  lai->primitive_type_vector [f_primitive_type_complex_s8]
    = builtin->builtin_complex_s8;
  lai->primitive_type_vector [f_primitive_type_complex_s16]
    = builtin->builtin_complex_s16;
  lai->primitive_type_vector [f_primitive_type_void]
    = builtin->builtin_void;

  lai->bool_type_symbol = "logical";
  lai->bool_type_default = builtin->builtin_logical_s2;
}

/* Remove the modules separator :: from the default break list.  */

static char *
f_word_break_characters (void)
{
  static char *retval;

  if (!retval)
    {
      char *s;

      retval = xstrdup (default_word_break_characters ());
      s = strchr (retval, ':');
      if (s)
	{
	  char *last_char = &s[strlen (s) - 1];

	  *s = *last_char;
	  *last_char = 0;
	}
    }
  return retval;
}

/* Consider the modules separator :: as a valid symbol name character
   class.  */

static char **
f_make_symbol_completion_list (char *text, char *word)
{
  return default_make_symbol_completion_list_break_on (text, word, ":");
}

/* This is declared in c-lang.h but it is silly to import that file for what
   is already just a hack.  */
extern int c_value_print (struct value *, struct ui_file *,
			  const struct value_print_options *);

const struct language_defn f_language_defn =
{
  "fortran",
  language_fortran,
  range_check_on,
  type_check_on,
  case_sensitive_off,
  array_column_major,
  macro_expansion_no,
  &exp_descriptor_standard,
  f_parse,			/* parser */
  f_error,			/* parser error function */
  null_post_parser,
  f_printchar,			/* Print character constant */
  f_printstr,			/* function to print string constant */
  f_emit_char,			/* Function to print a single character */
  f_print_type,			/* Print a type using appropriate syntax */
  default_print_typedef,	/* Print a typedef using appropriate syntax */
  f_val_print,			/* Print a value using appropriate syntax */
  c_value_print,		/* FIXME */
  NULL,				/* Language specific skip_trampoline */
  NULL,                    	/* name_of_this */
  cp_lookup_symbol_nonlocal,	/* lookup_symbol_nonlocal */
  basic_lookup_transparent_type,/* lookup_transparent_type */
  NULL,				/* Language specific symbol demangler */
  NULL,				/* Language specific
				   class_name_from_physname */
  f_op_print_tab,		/* expression operators for printing */
  0,				/* arrays are first-class (not c-style) */
  1,				/* String lower bound */
  f_word_break_characters,
  f_make_symbol_completion_list,
  f_language_arch_info,
  default_print_array_index,
  default_pass_by_reference,
  default_get_string,
  strcmp_iw_ordered,
  iterate_over_symbols,
  LANG_MAGIC
};

static void *
build_fortran_types (struct gdbarch *gdbarch)
{
  struct builtin_f_type *builtin_f_type
    = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct builtin_f_type);

  builtin_f_type->builtin_void
    = arch_type (gdbarch, TYPE_CODE_VOID, 1, "VOID");

  builtin_f_type->builtin_character
    = arch_integer_type (gdbarch, TARGET_CHAR_BIT, 0, "character");

  builtin_f_type->builtin_logical_s1
    = arch_boolean_type (gdbarch, TARGET_CHAR_BIT, 1, "logical*1");

  builtin_f_type->builtin_integer_s2
    = arch_integer_type (gdbarch, gdbarch_short_bit (gdbarch), 0,
			 "integer*2");

  builtin_f_type->builtin_logical_s2
    = arch_boolean_type (gdbarch, gdbarch_short_bit (gdbarch), 1,
			 "logical*2");

  builtin_f_type->builtin_logical_s8
    = arch_boolean_type (gdbarch, gdbarch_long_long_bit (gdbarch), 1,
			 "logical*8");

  builtin_f_type->builtin_integer
    = arch_integer_type (gdbarch, gdbarch_int_bit (gdbarch), 0,
			 "integer");

  builtin_f_type->builtin_logical
    = arch_boolean_type (gdbarch, gdbarch_int_bit (gdbarch), 1,
			 "logical*4");

  builtin_f_type->builtin_real
    = arch_float_type (gdbarch, gdbarch_float_bit (gdbarch),
		       "real", NULL);
  builtin_f_type->builtin_real_s8
    = arch_float_type (gdbarch, gdbarch_double_bit (gdbarch),
		       "real*8", NULL);
  builtin_f_type->builtin_real_s16
    = arch_float_type (gdbarch, gdbarch_long_double_bit (gdbarch),
		       "real*16", NULL);

  builtin_f_type->builtin_complex_s8
    = arch_complex_type (gdbarch, "complex*8",
			 builtin_f_type->builtin_real);
  builtin_f_type->builtin_complex_s16
    = arch_complex_type (gdbarch, "complex*16",
			 builtin_f_type->builtin_real_s8);
  builtin_f_type->builtin_complex_s32
    = arch_complex_type (gdbarch, "complex*32",
			 builtin_f_type->builtin_real_s16);

  return builtin_f_type;
}

static struct gdbarch_data *f_type_data;

const struct builtin_f_type *
builtin_f_type (struct gdbarch *gdbarch)
{
  return gdbarch_data (gdbarch, f_type_data);
}

void
_initialize_f_language (void)
{
  f_type_data = gdbarch_data_register_post_init (build_fortran_types);

  add_language (&f_language_defn);
}

#if 0
static SAVED_BF_PTR
allocate_saved_bf_node (void)
{
  SAVED_BF_PTR new;

  new = (SAVED_BF_PTR) xmalloc (sizeof (SAVED_BF));
  return (new);
}

static SAVED_FUNCTION *
allocate_saved_function_node (void)
{
  SAVED_FUNCTION *new;

  new = (SAVED_FUNCTION *) xmalloc (sizeof (SAVED_FUNCTION));
  return (new);
}

static SAVED_F77_COMMON_PTR
allocate_saved_f77_common_node (void)
{
  SAVED_F77_COMMON_PTR new;

  new = (SAVED_F77_COMMON_PTR) xmalloc (sizeof (SAVED_F77_COMMON));
  return (new);
}

static COMMON_ENTRY_PTR
allocate_common_entry_node (void)
{
  COMMON_ENTRY_PTR new;

  new = (COMMON_ENTRY_PTR) xmalloc (sizeof (COMMON_ENTRY));
  return (new);
}
#endif

SAVED_F77_COMMON_PTR head_common_list = NULL;	/* Ptr to 1st saved COMMON  */
SAVED_F77_COMMON_PTR tail_common_list = NULL;	/* Ptr to last saved COMMON  */
SAVED_F77_COMMON_PTR current_common = NULL;	/* Ptr to current COMMON */

#if 0
static SAVED_BF_PTR saved_bf_list = NULL;	/* Ptr to (.bf,function) 
						   list */
static SAVED_BF_PTR saved_bf_list_end = NULL;	/* Ptr to above list's end */
static SAVED_BF_PTR current_head_bf_list = NULL;    /* Current head of
						       above list.  */

static SAVED_BF_PTR tmp_bf_ptr;	/* Generic temporary for use 
				   in macros.  */

/* The following function simply enters a given common block onto 
   the global common block chain.  */

static void
add_common_block (char *name, CORE_ADDR offset, int secnum, char *func_stab)
{
  SAVED_F77_COMMON_PTR tmp;
  char *c, *local_copy_func_stab;

  /* If the COMMON block we are trying to add has a blank 
     name (i.e. "#BLNK_COM") then we set it to __BLANK
     because the darn "#" character makes GDB's input 
     parser have fits.  */


  if (strcmp (name, BLANK_COMMON_NAME_ORIGINAL) == 0
      || strcmp (name, BLANK_COMMON_NAME_MF77) == 0)
    {

      xfree (name);
      name = alloca (strlen (BLANK_COMMON_NAME_LOCAL) + 1);
      strcpy (name, BLANK_COMMON_NAME_LOCAL);
    }

  tmp = allocate_saved_f77_common_node ();

  local_copy_func_stab = xmalloc (strlen (func_stab) + 1);
  strcpy (local_copy_func_stab, func_stab);

  tmp->name = xmalloc (strlen (name) + 1);

  /* local_copy_func_stab is a stabstring, let us first extract the 
     function name from the stab by NULLing out the ':' character.  */


  c = NULL;
  c = strchr (local_copy_func_stab, ':');

  if (c)
    *c = '\0';
  else
    error (_("Malformed function STAB found in add_common_block()"));


  tmp->owning_function = xmalloc (strlen (local_copy_func_stab) + 1);

  strcpy (tmp->owning_function, local_copy_func_stab);

  strcpy (tmp->name, name);
  tmp->offset = offset;
  tmp->next = NULL;
  tmp->entries = NULL;
  tmp->secnum = secnum;

  current_common = tmp;

  if (head_common_list == NULL)
    {
      head_common_list = tail_common_list = tmp;
    }
  else
    {
      tail_common_list->next = tmp;
      tail_common_list = tmp;
    }
}
#endif

/* The following function simply enters a given common entry onto 
   the "current_common" block that has been saved away.  */

#if 0
static void
add_common_entry (struct symbol *entry_sym_ptr)
{
  COMMON_ENTRY_PTR tmp;



  /* The order of this list is important, since 
     we expect the entries to appear in decl.
     order when we later issue "info common" calls.  */

  tmp = allocate_common_entry_node ();

  tmp->next = NULL;
  tmp->symbol = entry_sym_ptr;

  if (current_common == NULL)
    error (_("Attempt to add COMMON entry with no block open!"));
  else
    {
      if (current_common->entries == NULL)
	{
	  current_common->entries = tmp;
	  current_common->end_of_entries = tmp;
	}
      else
	{
	  current_common->end_of_entries->next = tmp;
	  current_common->end_of_entries = tmp;
	}
    }
}
#endif

/* This routine finds the first encountred COMMON block named "name".  */

#if 0
static SAVED_F77_COMMON_PTR
find_first_common_named (char *name)
{

  SAVED_F77_COMMON_PTR tmp;

  tmp = head_common_list;

  while (tmp != NULL)
    {
      if (strcmp (tmp->name, name) == 0)
	return (tmp);
      else
	tmp = tmp->next;
    }
  return (NULL);
}
#endif

/* This routine finds the first encountred COMMON block named "name" 
   that belongs to function funcname.  */

SAVED_F77_COMMON_PTR
find_common_for_function (char *name, char *funcname)
{

  SAVED_F77_COMMON_PTR tmp;

  tmp = head_common_list;

  while (tmp != NULL)
    {
      if (strcmp (tmp->name, name) == 0
	  && strcmp (tmp->owning_function, funcname) == 0)
	return (tmp);
      else
	tmp = tmp->next;
    }
  return (NULL);
}


#if 0

/* The following function is called to patch up the offsets 
   for the statics contained in the COMMON block named
   "name."  */

static void
patch_common_entries (SAVED_F77_COMMON_PTR blk, CORE_ADDR offset, int secnum)
{
  COMMON_ENTRY_PTR entry;

  blk->offset = offset;		/* Keep this around for future use.  */

  entry = blk->entries;

  while (entry != NULL)
    {
      SYMBOL_VALUE (entry->symbol) += offset;
      SYMBOL_SECTION (entry->symbol) = secnum;

      entry = entry->next;
    }
  blk->secnum = secnum;
}

/* Patch all commons named "name" that need patching.Since COMMON
   blocks occur with relative infrequency, we simply do a linear scan on
   the name.  Eventually, the best way to do this will be a
   hashed-lookup.  Secnum is the section number for the .bss section
   (which is where common data lives).  */

static void
patch_all_commons_by_name (char *name, CORE_ADDR offset, int secnum)
{

  SAVED_F77_COMMON_PTR tmp;

  /* For blank common blocks, change the canonical reprsentation 
     of a blank name */

  if (strcmp (name, BLANK_COMMON_NAME_ORIGINAL) == 0
      || strcmp (name, BLANK_COMMON_NAME_MF77) == 0)
    {
      xfree (name);
      name = alloca (strlen (BLANK_COMMON_NAME_LOCAL) + 1);
      strcpy (name, BLANK_COMMON_NAME_LOCAL);
    }

  tmp = head_common_list;

  while (tmp != NULL)
    {
      if (COMMON_NEEDS_PATCHING (tmp))
	if (strcmp (tmp->name, name) == 0)
	  patch_common_entries (tmp, offset, secnum);

      tmp = tmp->next;
    }
}
#endif

/* This macro adds the symbol-number for the start of the function 
   (the symbol number of the .bf) referenced by symnum_fcn to a 
   list.  This list, in reality should be a FIFO queue but since 
   #line pragmas sometimes cause line ranges to get messed up 
   we simply create a linear list.  This list can then be searched 
   first by a queueing algorithm and upon failure fall back to 
   a linear scan.  */

#if 0
#define ADD_BF_SYMNUM(bf_sym,fcn_sym) \
  \
  if (saved_bf_list == NULL) \
{ \
    tmp_bf_ptr = allocate_saved_bf_node(); \
      \
	tmp_bf_ptr->symnum_bf = (bf_sym); \
	  tmp_bf_ptr->symnum_fcn = (fcn_sym);  \
	    tmp_bf_ptr->next = NULL; \
	      \
		current_head_bf_list = saved_bf_list = tmp_bf_ptr; \
		  saved_bf_list_end = tmp_bf_ptr; \
		  } \
else \
{  \
     tmp_bf_ptr = allocate_saved_bf_node(); \
       \
         tmp_bf_ptr->symnum_bf = (bf_sym);  \
	   tmp_bf_ptr->symnum_fcn = (fcn_sym);  \
	     tmp_bf_ptr->next = NULL;  \
	       \
		 saved_bf_list_end->next = tmp_bf_ptr;  \
		   saved_bf_list_end = tmp_bf_ptr; \
		   }
#endif

/* This function frees the entire (.bf,function) list.  */

#if 0
static void
clear_bf_list (void)
{

  SAVED_BF_PTR tmp = saved_bf_list;
  SAVED_BF_PTR next = NULL;

  while (tmp != NULL)
    {
      next = tmp->next;
      xfree (tmp);
      tmp = next;
    }
  saved_bf_list = NULL;
}
#endif

int global_remote_debug;

#if 0

static long
get_bf_for_fcn (long the_function)
{
  SAVED_BF_PTR tmp;
  int nprobes = 0;

  /* First use a simple queuing algorithm (i.e. look and see if the 
     item at the head of the queue is the one you want).  */

  if (saved_bf_list == NULL)
    internal_error (__FILE__, __LINE__,
		    _("cannot get .bf node off empty list"));

  if (current_head_bf_list != NULL)
    if (current_head_bf_list->symnum_fcn == the_function)
      {
	if (global_remote_debug)
	  fprintf_unfiltered (gdb_stderr, "*");

	tmp = current_head_bf_list;
	current_head_bf_list = current_head_bf_list->next;
	return (tmp->symnum_bf);
      }

  /* If the above did not work (probably because #line directives were 
     used in the sourcefile and they messed up our internal tables) we now do
     the ugly linear scan.  */

  if (global_remote_debug)
    fprintf_unfiltered (gdb_stderr, "\ndefaulting to linear scan\n");

  nprobes = 0;
  tmp = saved_bf_list;
  while (tmp != NULL)
    {
      nprobes++;
      if (tmp->symnum_fcn == the_function)
	{
	  if (global_remote_debug)
	    fprintf_unfiltered (gdb_stderr, "Found in %d probes\n", nprobes);
	  current_head_bf_list = tmp->next;
	  return (tmp->symnum_bf);
	}
      tmp = tmp->next;
    }

  return (-1);
}

static SAVED_FUNCTION_PTR saved_function_list = NULL;
static SAVED_FUNCTION_PTR saved_function_list_end = NULL;

static void
clear_function_list (void)
{
  SAVED_FUNCTION_PTR tmp = saved_function_list;
  SAVED_FUNCTION_PTR next = NULL;

  while (tmp != NULL)
    {
      next = tmp->next;
      xfree (tmp);
      tmp = next;
    }

  saved_function_list = NULL;
}
#endif
