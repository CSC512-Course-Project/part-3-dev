
/*--------------------------------------------------------------------*/
/*--- Nulgrind: The minimal Valgrind tool.               nl_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Nulgrind, the minimal Valgrind tool,
   which does no instrumentation or analysis.

   Copyright (C) 2002-2017 Nicholas Nethercote
      njn@valgrind.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

#include "pub_tool_basics.h"
#include "pub_tool_clientstate.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcproc.h"
#include "pub_tool_machine.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_options.h"
#include "pub_tool_redir.h"
#include "pub_tool_replacemalloc.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_vki.h"
#include "pub_tool_xarray.h"
#include "pub_tool_xtree.h"
#include "valgrind.h"

// Global counter for executed instructions
static ULong total_instructions = 0;

static VG_REGPARM(0) void inc_instruction_count(void) { total_instructions++; }

static IRSB *br_instrument(VgCallbackClosure *callbackClosure, IRSB *inputBlock,
                           const VexGuestLayout *guestLayout,
                           const VexGuestExtents *guestExtents,
                           const VexArchInfo *archInfoHost, IRType guestWordType,
                           IRType hostWordType)
{
  IRSB *outputBlock = deepCopyIRSBExceptStmts(inputBlock);
  Int statementIndex;

  for (statementIndex = 0; statementIndex < inputBlock->stmts_used; statementIndex++)
  {
    IRStmt *currentStmt = inputBlock->stmts[statementIndex];
    if (!currentStmt)
      continue;

    // Copy the current statement to the output block
    addStmtToIRSB(outputBlock, currentStmt);

    // Instrumentation: increment the instruction counter at Ist_IMark
    if (currentStmt->tag == Ist_IMark)
    {
      IRDirty *dirtyStmt = unsafeIRDirty_0_N(
          0, "inc_instruction_count",
          VG_(fnptr_to_fnentry)(&inc_instruction_count), mkIRExprVec_0());
      addStmtToIRSB(outputBlock, IRStmt_Dirty(dirtyStmt));
    }
  }

  return outputBlock;
}

static void br_post_clo_init(void)
{
  // Any initialization that depends on command-line options goes here.
  // If you don't have any such initialization, you can leave this empty.
}

// Finalization function
static void br_fini(Int exitcode)
{
  VG_(umsg)
  ("Total number of executed instructions: %llu\n", total_instructions);
}

// Initialization function
static void br_pre_clo_init(void)
{
  VG_(details_name)("Branch");
  VG_(details_version)(NULL);
  VG_(details_description)("a binary profiling tool to count instructions");
  VG_(details_copyright_author)
  ("Copyright (C) 2023, and GNU GPL'd, by Surya Sukumar.");
  VG_(details_bug_reports_to)(VG_BUGS_TO);
  VG_(details_avg_translation_sizeB)(275);

  VG_(basic_tool_funcs)(br_post_clo_init, br_instrument, br_fini);
}

VG_DETERMINE_INTERFACE_VERSION(br_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
