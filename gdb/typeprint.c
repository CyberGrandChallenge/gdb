/* Language independent support for printing types for GDB, the GNU debugger.

   Copyright (C) 1986, 1988-1989, 1991-1995, 1998-2001, 2003, 2006-2012
   Free Software Foundation, Inc.

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
#include "gdb_obstack.h"
#include "bfd.h"		/* Binary File Description */
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "gdbcore.h"
#include "command.h"
#include "gdbcmd.h"
#include "target.h"
#include "language.h"
#include "cp-abi.h"
#include "typeprint.h"
#include "gdb_string.h"
#include "exceptions.h"
#include "valprint.h"
#include <errno.h>

extern void _initialize_typeprint (void);

static void ptype_command (char *, int);

static void whatis_command (char *, int);

static void whatis_exp (char *, int);


/* Print a description of a type in the format of a 
   typedef for the current language.
   NEW is the new name for a type TYPE.  */

void
typedef_print (struct type *type, struct symbol *new, struct ui_file *stream)
{
  LA_PRINT_TYPEDEF (type, new, stream);
}

/* The default way to print a typedef.  */

void
default_print_typedef (struct type *type, struct symbol *new_symbol,
		       struct ui_file *stream)
{
  error (_("Language not supported."));
}

/* Print a description of a type TYPE in the form of a declaration of a
   variable named VARSTRING.  (VARSTRING is demangled if necessary.)
   Output goes to STREAM (via stdio).
   If SHOW is positive, we show the contents of the outermost level
   of structure even if there is a type name that could be used instead.
   If SHOW is negative, we never show the details of elements' types.  */

void
type_print (struct type *type, char *varstring, struct ui_file *stream,
	    int show)
{
  LA_PRINT_TYPE (type, varstring, stream, show, 0);
}

/* Print TYPE to a string, returning it.  The caller is responsible for
   freeing the string.  */

char *
type_to_string (struct type *type)
{
  char *s = NULL;
  struct ui_file *stb;
  struct cleanup *old_chain;
  volatile struct gdb_exception except;

  stb = mem_fileopen ();
  old_chain = make_cleanup_ui_file_delete (stb);

  TRY_CATCH (except, RETURN_MASK_ALL)
    {
      type_print (type, "", stb, -1);
      s = ui_file_xstrdup (stb, NULL);
    }
  if (except.reason < 0)
    s = NULL;

  do_cleanups (old_chain);

  return s;
}

/* Print type of EXP, or last thing in value history if EXP == NULL.
   show is passed to type_print.  */

static void
whatis_exp (char *exp, int show)
{
  struct expression *expr;
  struct value *val;
  struct cleanup *old_chain = NULL;
  struct type *real_type = NULL;
  struct type *type;
  int full = 0;
  int top = -1;
  int using_enc = 0;
  struct value_print_options opts;

  if (exp)
    {
      expr = parse_expression (exp);
      old_chain = make_cleanup (free_current_contents, &expr);
      val = evaluate_type (expr);
    }
  else
    val = access_value_history (0);

  type = value_type (val);

  get_user_print_options (&opts);
  if (opts.objectprint)
    {
      if (((TYPE_CODE (type) == TYPE_CODE_PTR)
	   || (TYPE_CODE (type) == TYPE_CODE_REF))
	  && (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_CLASS))
        {
          real_type = value_rtti_target_type (val, &full, &top, &using_enc);
          if (real_type)
            {
              if (TYPE_CODE (type) == TYPE_CODE_PTR)
                real_type = lookup_pointer_type (real_type);
              else
                real_type = lookup_reference_type (real_type);
            }
        }
      else if (TYPE_CODE (type) == TYPE_CODE_CLASS)
	real_type = value_rtti_type (val, &full, &top, &using_enc);
    }

  printf_filtered ("type = ");

  if (real_type)
    {
      printf_filtered ("/* real type = ");
      type_print (real_type, "", gdb_stdout, -1);
      if (! full)
        printf_filtered (" (incomplete object)");
      printf_filtered (" */\n");    
    }

  type_print (type, "", gdb_stdout, show);
  printf_filtered ("\n");

  if (exp)
    do_cleanups (old_chain);
}

static void
whatis_command (char *exp, int from_tty)
{
  /* Most of the time users do not want to see all the fields
     in a structure.  If they do they can use the "ptype" command.
     Hence the "-1" below.  */
  whatis_exp (exp, -1);
}

/* TYPENAME is either the name of a type, or an expression.  */

static void
ptype_command (char *typename, int from_tty)
{
  whatis_exp (typename, 1);
}

/* Print integral scalar data VAL, of type TYPE, onto stdio stream STREAM.
   Used to print data from type structures in a specified type.  For example,
   array bounds may be characters or booleans in some languages, and this
   allows the ranges to be printed in their "natural" form rather than as
   decimal integer values.

   FIXME:  This is here simply because only the type printing routines
   currently use it, and it wasn't clear if it really belonged somewhere
   else (like printcmd.c).  There are a lot of other gdb routines that do
   something similar, but they are generally concerned with printing values
   that come from the inferior in target byte order and target size.  */

void
print_type_scalar (struct type *type, LONGEST val, struct ui_file *stream)
{
  unsigned int i;
  unsigned len;

  CHECK_TYPEDEF (type);

  switch (TYPE_CODE (type))
    {

    case TYPE_CODE_ENUM:
      len = TYPE_NFIELDS (type);
      for (i = 0; i < len; i++)
	{
	  if (TYPE_FIELD_BITPOS (type, i) == val)
	    {
	      break;
	    }
	}
      if (i < len)
	{
	  fputs_filtered (TYPE_FIELD_NAME (type, i), stream);
	}
      else
	{
	  print_longest (stream, 'd', 0, val);
	}
      break;

    case TYPE_CODE_INT:
      print_longest (stream, TYPE_UNSIGNED (type) ? 'u' : 'd', 0, val);
      break;

    case TYPE_CODE_CHAR:
      LA_PRINT_CHAR ((unsigned char) val, type, stream);
      break;

    case TYPE_CODE_BOOL:
      fprintf_filtered (stream, val ? "TRUE" : "FALSE");
      break;

    case TYPE_CODE_RANGE:
      print_type_scalar (TYPE_TARGET_TYPE (type), val, stream);
      return;

    case TYPE_CODE_UNDEF:
    case TYPE_CODE_PTR:
    case TYPE_CODE_ARRAY:
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
    case TYPE_CODE_FUNC:
    case TYPE_CODE_FLT:
    case TYPE_CODE_VOID:
    case TYPE_CODE_SET:
    case TYPE_CODE_STRING:
    case TYPE_CODE_ERROR:
    case TYPE_CODE_MEMBERPTR:
    case TYPE_CODE_METHODPTR:
    case TYPE_CODE_METHOD:
    case TYPE_CODE_REF:
    case TYPE_CODE_NAMESPACE:
      error (_("internal error: unhandled type in print_type_scalar"));
      break;

    default:
      error (_("Invalid type code in symbol table."));
    }
  gdb_flush (stream);
}

/* Dump details of a type specified either directly or indirectly.
   Uses the same sort of type lookup mechanism as ptype_command()
   and whatis_command().  */

void
maintenance_print_type (char *typename, int from_tty)
{
  struct value *val;
  struct type *type;
  struct cleanup *old_chain;
  struct expression *expr;

  if (typename != NULL)
    {
      expr = parse_expression (typename);
      old_chain = make_cleanup (free_current_contents, &expr);
      if (expr->elts[0].opcode == OP_TYPE)
	{
	  /* The user expression names a type directly, just use that type.  */
	  type = expr->elts[1].type;
	}
      else
	{
	  /* The user expression may name a type indirectly by naming an
	     object of that type.  Find that indirectly named type.  */
	  val = evaluate_type (expr);
	  type = value_type (val);
	}
      if (type != NULL)
	{
	  recursive_dump_type (type, 0);
	}
      do_cleanups (old_chain);
    }
}


void
_initialize_typeprint (void)
{
  add_com ("ptype", class_vars, ptype_command, _("\
Print definition of type TYPE.\n\
Argument may be a type name defined by typedef, or \"struct STRUCT-TAG\"\n\
or \"class CLASS-NAME\" or \"union UNION-TAG\" or \"enum ENUM-TAG\".\n\
The selected stack frame's lexical context is used to look up the name.\n\
Contrary to \"whatis\", \"ptype\" always unrolls any typedefs."));

  add_com ("whatis", class_vars, whatis_command,
	   _("Print data type of expression EXP.\n\
Only one level of typedefs is unrolled.  See also \"ptype\"."));
}
