/* Target-dependent code for GNU/Linux Super-H.

   Copyright (C) 2005, 2007-2012 Free Software Foundation, Inc.

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
#include "osabi.h"

#include "solib-svr4.h"
#include "symtab.h"

#include "glibc-tdep.h"
#include "sh-tdep.h"
#include "linux-tdep.h"

#define REGSx16(base) \
  {(base),      0}, \
  {(base) +  1, 4}, \
  {(base) +  2, 8}, \
  {(base) +  3, 12}, \
  {(base) +  4, 16}, \
  {(base) +  5, 20}, \
  {(base) +  6, 24}, \
  {(base) +  7, 28}, \
  {(base) +  8, 32}, \
  {(base) +  9, 36}, \
  {(base) + 10, 40}, \
  {(base) + 11, 44}, \
  {(base) + 12, 48}, \
  {(base) + 13, 52}, \
  {(base) + 14, 56}, \
  {(base) + 15, 60}

/* Describe the contents of the .reg section of the core file.  */

static const struct sh_corefile_regmap gregs_table[] =
{
  REGSx16 (R0_REGNUM),
  {PC_REGNUM,   64},
  {PR_REGNUM,   68},
  {SR_REGNUM,   72},
  {GBR_REGNUM,  76},
  {MACH_REGNUM, 80},
  {MACL_REGNUM, 84},
  {-1 /* Terminator.  */, 0}
};

/* Describe the contents of the .reg2 section of the core file.  */

static const struct sh_corefile_regmap fpregs_table[] =
{
  REGSx16 (FR0_REGNUM),
  /* REGSx16 xfp_regs omitted.  */
  {FPSCR_REGNUM, 128},
  {FPUL_REGNUM,  132},
  {-1 /* Terminator.  */, 0}
};

static void
sh_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  linux_init_abi (info, gdbarch);

  /* GNU/Linux uses SVR4-style shared libraries.  */
  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
  set_gdbarch_skip_solib_resolver (gdbarch, glibc_skip_solib_resolver);

  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);

  /* Core files are supported for 32-bit SH only, at present.  */
  if (info.bfd_arch_info->mach != bfd_mach_sh5)
    {
      struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

      tdep->core_gregmap = (struct sh_corefile_regmap *)gregs_table;
      tdep->core_fpregmap = (struct sh_corefile_regmap *)fpregs_table;
    }
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
extern void _initialize_sh_linux_tdep (void);

void
_initialize_sh_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_sh, 0, GDB_OSABI_LINUX, sh_linux_init_abi);
}
