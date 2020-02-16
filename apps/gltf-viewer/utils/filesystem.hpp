#pragma once

#ifdef GLMLV_USE_BOOST_FILESYSTEM
#include <boost/filesystem.hpp>
#else
#ifdef _MSC_VER
#if _MSC_VER >= 1923
#define USE_STD_FILESYSTEM 1
#else
#include <experimental/filesystem>
#endif
#else
#include <experimental/filesystem>
#endif
#endif

#ifdef GLMLV_USE_BOOST_FILESYSTEM
namespace fs = boost::filesystem;
#else
#ifdef USE_STD_FILESYSTEM
#include <filesystem>
namespace fs = std::filesystem; // Shorter namespace for experimental
                                // filesystem standard library
#else
namespace fs =
    std::experimental::filesystem; // Shorter namespace for experimental
                                   // filesystem standard library
#endif
#endif
