//////////////////////////////////////////////////////////////////////////
//
// This file is part of the BWEM Library.
// BWEM is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2015, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "bwem.h"

void BWEM::detail::onAssertThrowFailed(const std::string & file, int line, const std::string & condition, const std::string & message)
{
  std::ignore = file;
  std::ignore = line;
  std::ignore = condition;
  std::ignore = message;

	bwem_assert_debug_only(false);
	throw Exception(file + ", line " + std::to_string(line) + " - " + message);
}
