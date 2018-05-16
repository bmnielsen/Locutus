//////////////////////////////////////////////////////////////////////////
//
// This file is part of the BWEM Library.
// BWEM is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2015, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#pragma once

#include <cstdint>
#include <string>
#if BWEM_TRACE
#include <iostream>
#endif

namespace BWEM
{

namespace detail
{
	void onAssertThrowFailed(const std::string & file, int line, const std::string & condition, const std::string & message);

} // namespace details

#ifdef BWEM_ASSERTS
#include <cassert>
#define bwem_assert_debug_only(expr)			assert(expr)
#define bwem_assert_plus(expr, message)			assert(expr)
#else
#if BWEM_TRACE
#define bwem_assert_plus(expr, message)			if(!(expr)) { std::cout << __FILE__ << ", line " << std::to_string(__LINE__) << " " << #expr << " - " << (message) << std::endl; }
#define bwem_assert_debug_only(expr)			bwem_assert_plus(expr, "")
#else
#define bwem_assert_debug_only(expr)
#define bwem_assert_plus(expr, message)
#endif
#define bwem_assert(expr)						bwem_assert_plus(expr, "")
#define bwem_assert_throw_plus(expr, message)   ((expr)?(void)0:detail::onAssertThrowFailed(__FILE__,__LINE__, #expr, message))
#define bwem_assert_throw(expr)					bwem_assert_throw_plus(expr, "")
#endif

class Exception : public std::runtime_error
{
public:
	explicit                Exception(const char * message) : std::runtime_error(message) {}
	explicit                Exception(const std::string & message) : Exception(message.c_str()) {}
};







typedef int16_t altitude_t;		// type of the altitudes, in pixels




namespace utils
{

enum class check_t {no_check, check};

} // namespace utils


namespace detail
{

// These constants control how to decide between Seas and Lakes.
const int lake_max_miniTiles = 300;
const int lake_max_width_in_miniTiles = 8*4;

// At least area_min_miniTiles connected MiniTiles are necessary for an Area to be created.
const int area_min_miniTiles = 64;

const int max_tiles_between_CommandCenter_and_ressources = 10;
const int min_tiles_between_Bases = 10;

const int max_tiles_between_StartingLocation_and_its_AssignedBase = 3;

} // namespace detail


} // namespace BWEM
