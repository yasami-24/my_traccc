# TRACCC library, part of the ACTS project (R&D line)
#
# (c) 2022-2023 CERN for the benefit of the ACTS project
#
# Mozilla Public License Version 2.0

# Simulation with the default metadata and json inputs
traccc_add_executable(simulate "simulate.cpp"
    LINK_LIBRARIES vecmem::core traccc::io traccc::core
    traccc::options traccc::simulation detray::core detray::utils covfie::core 
    Boost::filesystem)    

# Simulation with the detectors pre-built in detray
traccc_add_executable(simulate_telescope "simulate_telescope.cpp"
    LINK_LIBRARIES vecmem::core traccc::io traccc::core
    traccc::options traccc::simulation detray::core detray::utils covfie::core 
    Boost::filesystem)

traccc_add_executable(simulate_toy_detector "simulate_toy_detector.cpp"
    LINK_LIBRARIES vecmem::core traccc::io traccc::core
    traccc::options traccc::simulation detray::core detray::utils covfie::core
    Boost::filesystem)

traccc_add_executable(simulate_wire_chamber "simulate_wire_chamber.cpp"
    LINK_LIBRARIES vecmem::core traccc::io traccc::core
    traccc::options traccc::simulation detray::core detray::utils covfie::core 
    Boost::filesystem)

# 2024/07/17 asami 
traccc_add_executable(pu_simulate "pu_simulate.cpp"
    LINK_LIBRARIES vecmem::core traccc::io traccc::core
    traccc::options traccc::simulation detray::core detray::utils covfie::core 
    Boost::filesystem)
# up to here