/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2021-2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#pragma once

// Project include(s).
#include "traccc/edm/cell.hpp"
#include "traccc/edm/spacepoint.hpp"
#include "traccc/edm/seed.hpp"
#include "traccc/edm/track_parameters.hpp"
#include "traccc/edm/track_state.hpp"
#include "traccc/io/data_format.hpp"

// System include(s).
#include <cstddef>
#include <string_view>

namespace traccc::io {

/// Function for cell file writing
///
/// @param event is the event index
/// @param directory is the directory for the output cell file
/// @param xx_view is the xx collection to write
///
void mywrite(std::size_t event, std::string_view directory,
	     traccc::cell_collection_types::const_view cells_view,
	     std::string_view device="");

void mywrite(std::size_t event, std::string_view directory,
	     traccc::cell_module_collection_types::const_view modules_view,
	     std::string_view device="");

void mywrite(std::size_t event, std::string_view directory,
	     traccc::measurement_collection_types::const_view measurements_view,
	     std::string_view device="");

void mywrite(std::size_t event, std::string_view directory,
	     traccc::spacepoint_collection_types::const_view spacepoints_view,
	     std::string_view device="");

void mywrite(std::size_t event, std::string_view directory,
	     traccc::seed_collection_types::const_view seeds_view,
	     std::string_view device="");

void mywrite(std::size_t event, std::string_view directory,
	     traccc::bound_track_parameters_collection_types::const_view params_view,
	     std::string_view device="");

void mywrite(std::size_t event, std::string_view directory,
	     traccc::track_state_container_types::const_view tracks_view,
	     std::string_view device="");


}  // namespace traccc::io
