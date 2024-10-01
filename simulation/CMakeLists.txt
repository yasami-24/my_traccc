# TRACCC library, part of the ACTS project (R&D line)
#
# (c) 2023 CERN for the benefit of the ACTS project
#
# Mozilla Public License Version 2.0

# Set up the "build" of the traccc::io library.
traccc_add_library( traccc_simulation simulation TYPE INTERFACE
  # Public headers
  "include/traccc/simulation/measurement_smearer.hpp"
  "include/traccc/simulation/simulator.hpp"
  "include/traccc/simulation/smearing_writer.hpp" 
  # 2024/07/17 asami
  "include/traccc/simulation/multi_vtx_simulator.hpp")
target_link_libraries( traccc_simulation
  INTERFACE traccc::core traccc::io detray::core detray::io
            detray::utils dfelibs::dfelibs )
  # up to here
