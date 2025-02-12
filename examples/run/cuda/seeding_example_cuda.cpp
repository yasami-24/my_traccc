/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2021-2024 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Project include(s).
#include "traccc/cuda/finding/finding_algorithm.hpp"
#include "traccc/cuda/fitting/fitting_algorithm.hpp"
#include "traccc/cuda/seeding/seeding_algorithm.hpp"
#include "traccc/cuda/seeding/track_params_estimation.hpp"
#include "traccc/definitions/common.hpp"
#include "traccc/device/container_d2h_copy_alg.hpp"
#include "traccc/device/container_h2d_copy_alg.hpp"
#include "traccc/efficiency/finding_performance_writer.hpp"
#include "traccc/efficiency/nseed_performance_writer.hpp"
#include "traccc/efficiency/seeding_performance_writer.hpp"
#include "traccc/efficiency/track_filter.hpp"
#include "traccc/finding/finding_algorithm.hpp"
#include "traccc/fitting/fitting_algorithm.hpp"
#include "traccc/io/read_geometry.hpp"
#include "traccc/io/read_measurements.hpp"
#include "traccc/io/read_spacepoints.hpp"
#include "traccc/io/utils.hpp"
#include "traccc/options/accelerator.hpp"
#include "traccc/options/detector.hpp"
#include "traccc/options/input_data.hpp"
#include "traccc/options/performance.hpp"
#include "traccc/options/program_options.hpp"
#include "traccc/options/track_finding.hpp"
#include "traccc/options/track_propagation.hpp"
#include "traccc/options/track_seeding.hpp"
#include "traccc/performance/collection_comparator.hpp"
#include "traccc/performance/timer.hpp"
#include "traccc/resolution/fitting_performance_writer.hpp"
#include "traccc/seeding/seeding_algorithm.hpp"
#include "traccc/seeding/track_params_estimation.hpp"

// Detray include(s).
#include "detray/core/detector.hpp"
#include "detray/core/detector_metadata.hpp"
#include "detray/detectors/bfield.hpp"
#include "detray/io/frontend/detector_reader.hpp"
#include "detray/navigation/navigator.hpp"
#include "detray/propagator/propagator.hpp"
#include "detray/propagator/rk_stepper.hpp"

// VecMem include(s).
#include <vecmem/memory/cuda/device_memory_resource.hpp>
#include <vecmem/memory/cuda/host_memory_resource.hpp>
#include <vecmem/memory/cuda/managed_memory_resource.hpp>
#include <vecmem/memory/host_memory_resource.hpp>
#include <vecmem/utils/cuda/async_copy.hpp>
#include <vecmem/utils/cuda/copy.hpp>

// System include(s).
#include <exception>
#include <iomanip>
#include <iostream>

// 2024/07/11 asami
#include <fstream>
// up to here

// asami 240911
#include <regex>
// up to here

// 2024/08/29 asami
// My include(s).
#include "traccc/io/mywrite.hpp"
#include "traccc/options/output_data.hpp"
// up to here


using namespace traccc;

int seq_run(const traccc::opts::track_seeding& seeding_opts,
            const traccc::opts::track_finding& finding_opts,
            const traccc::opts::track_propagation& propagation_opts,
            const traccc::opts::input_data& input_opts,
            const traccc::opts::detector& detector_opts,
            const traccc::opts::performance& performance_opts,
            const traccc::opts::accelerator& accelerator_opts,
            // 240829 asami
            const traccc::opts::output_data& output_opts) {
            // up to here

    /// Type declarations
    using host_detector_type = detray::detector<>;
    using device_detector_type =
        detray::detector<detray::default_metadata,
                         detray::device_container_types>;

    using b_field_t = covfie::field<detray::bfield::const_bknd_t>;
    using rk_stepper_type =
        detray::rk_stepper<b_field_t::view_t,
                           typename host_detector_type::algebra_type,
                           detray::constrained_step<>>;
    using host_navigator_type = detray::navigator<const host_detector_type>;
    using host_fitter_type =
        traccc::kalman_fitter<rk_stepper_type, host_navigator_type>;
    using device_navigator_type = detray::navigator<const device_detector_type>;
    using device_fitter_type =
        traccc::kalman_fitter<rk_stepper_type, device_navigator_type>;

    // Memory resources used by the application.
    vecmem::host_memory_resource host_mr;
    vecmem::cuda::host_memory_resource cuda_host_mr;
    vecmem::cuda::managed_memory_resource mng_mr;
    vecmem::cuda::device_memory_resource device_mr;
    traccc::memory_resource mr{device_mr, &cuda_host_mr};

    // Performance writer
    traccc::seeding_performance_writer sd_performance_writer(
        traccc::seeding_performance_writer::config{});
    traccc::finding_performance_writer find_performance_writer(
        traccc::finding_performance_writer::config{});
    traccc::fitting_performance_writer fit_performance_writer(
        traccc::fitting_performance_writer::config{});

    traccc::nseed_performance_writer nsd_performance_writer(
        "nseed_performance_",
        std::make_unique<traccc::simple_charged_eta_pt_cut>(
            2.7f, 1.f * traccc::unit<traccc::scalar>::GeV),
        std::make_unique<traccc::stepped_percentage>(0.6f));

    if (performance_opts.run) {
        nsd_performance_writer.initialize();
    }

    // Output stats
    uint64_t n_modules = 0;
    uint64_t n_spacepoints = 0;
    uint64_t n_seeds = 0;
    uint64_t n_seeds_cuda = 0;
    uint64_t n_found_tracks = 0;
    uint64_t n_found_tracks_cuda = 0;
    uint64_t n_fitted_tracks = 0;
    uint64_t n_fitted_tracks_cuda = 0;

    // 2024/07/11 asami

    std::string filename = "track_candidate.txt";
    std::ofstream file_track_candidate;

    // up to here
    
    /*****************************
     * Build a geometry
     *****************************/

    // B field value and its type
    // @TODO: Set B field as argument
    const traccc::vector3 B{0, 0, 2 * detray::unit<traccc::scalar>::T};
    auto field = detray::bfield::create_const_field(B);

    // Read the detector
    detray::io::detector_reader_config reader_cfg{};
    reader_cfg.add_file(traccc::io::data_directory() +
                        detector_opts.detector_file);
    if (!detector_opts.material_file.empty()) {
        reader_cfg.add_file(traccc::io::data_directory() +
                            detector_opts.material_file);
    }
    if (!detector_opts.grid_file.empty()) {
        reader_cfg.add_file(traccc::io::data_directory() +
                            detector_opts.grid_file);
    }
    auto [host_det, names] =
        detray::io::read_detector<host_detector_type>(mng_mr, reader_cfg);

    traccc::geometry surface_transforms =
        traccc::io::alt_read_geometry(host_det);

    // Detector view object
    auto det_view = detray::get_data(host_det);

    // Copy objects
    vecmem::cuda::copy copy;

    traccc::device::container_d2h_copy_alg<
        traccc::track_candidate_container_types>
        track_candidate_d2h{mr, copy};

    traccc::device::container_d2h_copy_alg<traccc::track_state_container_types>
        track_state_d2h{mr, copy};

    // Seeding algorithm
    traccc::seeding_algorithm sa(seeding_opts.seedfinder,
                                 {seeding_opts.seedfinder},
                                 seeding_opts.seedfilter, host_mr);
    traccc::track_params_estimation tp(host_mr);

    traccc::cuda::stream stream;

    vecmem::cuda::async_copy async_copy{stream.cudaStream()};

    traccc::cuda::seeding_algorithm sa_cuda{seeding_opts.seedfinder,
                                            {seeding_opts.seedfinder},
                                            seeding_opts.seedfilter,
                                            mr,
                                            async_copy,
                                            stream};
    traccc::cuda::track_params_estimation tp_cuda{mr, async_copy, stream};

    // Propagation configuration
    detray::propagation::config propagation_config(propagation_opts);

    // Finding algorithm configuration
    typename traccc::cuda::finding_algorithm<
        rk_stepper_type, device_navigator_type>::config_type cfg(finding_opts);
    cfg.propagation = propagation_config;

    // Finding algorithm object
    traccc::finding_algorithm<rk_stepper_type, host_navigator_type>
        host_finding(cfg);
    traccc::cuda::finding_algorithm<rk_stepper_type, device_navigator_type>
        device_finding(cfg, mr, async_copy, stream);

    // Fitting algorithm object
    typename traccc::fitting_algorithm<host_fitter_type>::config_type fit_cfg;
    fit_cfg.propagation = propagation_config;

    traccc::fitting_algorithm<host_fitter_type> host_fitting(fit_cfg);
    traccc::cuda::fitting_algorithm<device_fitter_type> device_fitting(
        fit_cfg, mr, async_copy, stream);

    traccc::performance::timing_info elapsedTimes;

    // Loop over events
    for (std::size_t event = input_opts.skip;
         event < input_opts.events + input_opts.skip; ++event) {

        // Instantiate host containers/collections
        traccc::io::spacepoint_reader_output sp_reader_output(mr.host);
        traccc::io::measurement_reader_output meas_reader_output(mr.host);

        traccc::seeding_algorithm::output_type seeds;
        traccc::track_params_estimation::output_type params;
        traccc::track_candidate_container_types::host track_candidates;
        traccc::track_state_container_types::host track_states;

        traccc::seed_collection_types::buffer seeds_cuda_buffer(0, *(mr.host));
        traccc::bound_track_parameters_collection_types::buffer
            params_cuda_buffer(0, *mr.host);

        traccc::track_candidate_container_types::buffer
            track_candidates_cuda_buffer{{{}, *(mr.host)},
                                         {{}, *(mr.host), mr.host}};

        traccc::track_state_container_types::buffer track_states_cuda_buffer{
            {{}, *(mr.host)}, {{}, *(mr.host), mr.host}};

        {  // Start measuring wall time
            traccc::performance::timer wall_t("Wall time", elapsedTimes);

            /*-----------------
            hit file reading
            -----------------*/
            {
                traccc::performance::timer t("Hit reading  (cpu)",
                                             elapsedTimes);
                // Read the hits from the relevant event file
                traccc::io::read_spacepoints(
                    sp_reader_output, event, input_opts.directory,
                    surface_transforms, input_opts.format);

                // Read measurements
                traccc::io::read_measurements(meas_reader_output, event,
                                              input_opts.directory,
                                              input_opts.format);
            }  // stop measuring hit reading timer

            auto& spacepoints_per_event = sp_reader_output.spacepoints;
            auto& modules_per_event = sp_reader_output.modules;
            auto& measurements_per_event = meas_reader_output.measurements;

            /*----------------------------
                Seeding algorithm
            ----------------------------*/

            /// CUDA

            // Copy the spacepoint and module data to the device.
            traccc::spacepoint_collection_types::buffer spacepoints_cuda_buffer(
                spacepoints_per_event.size(), mr.main);
            async_copy(vecmem::get_data(spacepoints_per_event),
                       spacepoints_cuda_buffer);
            traccc::cell_module_collection_types::buffer modules_buffer(
                modules_per_event.size(), mr.main);
            async_copy(vecmem::get_data(modules_per_event), modules_buffer);

            traccc::measurement_collection_types::buffer
                measurements_cuda_buffer(measurements_per_event.size(),
                                         mr.main);
            async_copy(vecmem::get_data(measurements_per_event),
                       measurements_cuda_buffer);

            {
                traccc::performance::timer t("Seeding (cuda)", elapsedTimes);
                // Reconstruct the spacepoints into seeds.
                seeds_cuda_buffer = sa_cuda(spacepoints_cuda_buffer);
                stream.synchronize();
            }  // stop measuring seeding cuda timer

            // CPU

            if (accelerator_opts.compare_with_cpu) {
                {
                    traccc::performance::timer t("Seeding  (cpu)",
                                                 elapsedTimes);
                    seeds = sa(spacepoints_per_event);
                }
            }  // stop measuring seeding cpu timer

            /*----------------------------
               Track params estimation
            ----------------------------*/

            // CUDA
            {
                traccc::performance::timer t("Track params (cuda)",
                                             elapsedTimes);
                params_cuda_buffer =
                    tp_cuda(spacepoints_cuda_buffer, seeds_cuda_buffer,
                            {0.f, 0.f, seeding_opts.seedfinder.bFieldInZ});
                stream.synchronize();
            }  // stop measuring track params cuda timer

            // CPU
            if (accelerator_opts.compare_with_cpu) {
                traccc::performance::timer t("Track params  (cpu)",
                                             elapsedTimes);
                params = tp(spacepoints_per_event, seeds,
                            {0.f, 0.f, seeding_opts.seedfinder.bFieldInZ});
            }  // stop measuring track params cpu timer

            // Navigation buffer
            auto navigation_buffer = detray::create_candidates_buffer(
                host_det,
                device_finding.get_config().navigation_buffer_size_scaler *
                    copy.get_size(seeds_cuda_buffer),
                mr.main, mr.host);

            /*------------------------
               Track Finding with CKF
              ------------------------*/

            {
                traccc::performance::timer t("Track finding with CKF (cuda)",
                                             elapsedTimes);
                track_candidates_cuda_buffer = device_finding(
                    det_view, field, navigation_buffer,
                    measurements_cuda_buffer, params_cuda_buffer);
            }

            if (accelerator_opts.compare_with_cpu) {
                traccc::performance::timer t("Track finding with CKF (cpu)",
                                             elapsedTimes);
                track_candidates = host_finding(host_det, field,
                                                measurements_per_event, params);
            }

            /*------------------------
               Track Fitting with KF
              ------------------------*/

            {
                traccc::performance::timer t("Track fitting with KF (cuda)",
                                             elapsedTimes);

                track_states_cuda_buffer =
                    device_fitting(det_view, field, navigation_buffer,
                                   track_candidates_cuda_buffer);
            }

            if (accelerator_opts.compare_with_cpu) {
                traccc::performance::timer t("Track fitting with KF (cpu)",
                                             elapsedTimes);
                track_states = host_fitting(host_det, field, track_candidates);
            }

        }  // Stop measuring wall time

        /*----------------------------------
          compare seeds from cpu and cuda
          ----------------------------------*/

        // Copy the seeds to the host for comparisons
        traccc::seed_collection_types::host seeds_cuda;
        traccc::bound_track_parameters_collection_types::host params_cuda;
        async_copy(seeds_cuda_buffer, seeds_cuda)->wait();
        async_copy(params_cuda_buffer, params_cuda)->wait();

        // Copy track candidates from device to host
        traccc::track_candidate_container_types::host track_candidates_cuda =
            track_candidate_d2h(track_candidates_cuda_buffer);

        // Copy track states from device to host
        traccc::track_state_container_types::host track_states_cuda =
            track_state_d2h(track_states_cuda_buffer);

        if (accelerator_opts.compare_with_cpu) {
            // Show which event we are currently presenting the results for.
            std::cout << "===>>> Event " << event << " <<<===" << std::endl;

            // Compare the seeds made on the host and on the device
            traccc::collection_comparator<traccc::seed> compare_seeds{
                "seeds", traccc::details::comparator_factory<traccc::seed>{
                             vecmem::get_data(sp_reader_output.spacepoints),
                             vecmem::get_data(sp_reader_output.spacepoints)}};
            compare_seeds(vecmem::get_data(seeds),
                          vecmem::get_data(seeds_cuda));

            // Compare the track parameters made on the host and on the device.
            traccc::collection_comparator<traccc::bound_track_parameters>
                compare_track_parameters{"track parameters"};
            compare_track_parameters(vecmem::get_data(params),
                                     vecmem::get_data(params_cuda));

            // Compare the track candidates made on the host and on the
            // device
            unsigned int n_matches = 0;
            for (unsigned int i = 0; i < track_candidates.size(); i++) {
                auto iso = traccc::details::is_same_object(
                    track_candidates.at(i).items);

                for (unsigned int j = 0; j < track_candidates_cuda.size();
                     j++) {
                    if (iso(track_candidates_cuda.at(j).items)) {
                        n_matches++;
                        break;
                    }
                }
            }
            
            std::cout << "Track candidate matching Rate: "
                      << float(n_matches) / track_candidates.size()
                      << std::endl;

            // 2024/07/11 asami

	        std::cout << "track candidate size: "
                      << track_candidates.size()
                      << std::endl;

            std::cout << "track candidate cuda size: "
                      << track_candidates_cuda.size()
                      << std::endl;

            file_track_candidate.open(filename, std::ios::app);
            if (file_track_candidate.is_open()) {
            
                file_track_candidate << float(n_matches) / track_candidates.size() << " " 
                                     << track_candidates.size() << " " << track_candidates_cuda.size() << std::endl;
                                        // Track candidate matching rate  << track candidate size << track candidate cuda size

                file_track_candidate.close();
            
            } else {
            
                std::cerr << "Unable to open file: " << std::endl;
            
            }


            // up to here

        }

        /*----------------
             Statistics
          ---------------*/

        n_spacepoints += sp_reader_output.spacepoints.size();
        n_modules += sp_reader_output.modules.size();
        n_seeds_cuda += seeds_cuda.size();
        n_seeds += seeds.size();
        n_found_tracks_cuda += track_candidates_cuda.size();
        n_found_tracks += track_candidates.size();
        n_fitted_tracks_cuda += track_states_cuda.size();
        n_fitted_tracks += track_states.size();

        /*------------
          Writer
          ------------*/

        // 24/08/29 asami
        traccc::io::mywrite(event,output_opts.directory+"cpu/",vecmem::get_data(seeds));
        traccc::io::mywrite(event,output_opts.directory+"cuda/",vecmem::get_data(seeds_cuda));
        // up to here

        if (performance_opts.run) {
            traccc::event_map2 evt_map(event, input_opts.directory,
                                       input_opts.directory,
                                       input_opts.directory);
            sd_performance_writer.write(
                vecmem::get_data(seeds_cuda),
                vecmem::get_data(sp_reader_output.spacepoints), evt_map);

            find_performance_writer.write(
                traccc::get_data(track_candidates_cuda), evt_map);                

            for (unsigned int i = 0; i < track_states_cuda.size(); i++) {         
                const auto& trk_states_per_track =
                    track_states_cuda.at(i).items;                                

                const auto& fit_res = track_states_cuda[i].header;                

                fit_performance_writer.write(trk_states_per_track, fit_res,
                                             host_det, evt_map);
            }
        }

        // if (performance_opts.run) {
        //     traccc::event_map2 evt_map(event, input_opts.directory,
        //                                input_opts.directory,
        //                                input_opts.directory);
        //     sd_performance_writer.write(
        //         vecmem::get_data(seeds),                                        // 2024/08/26 asami "seeds_cuda"->"seeds"
        //         vecmem::get_data(sp_reader_output.spacepoints), evt_map);

        //     find_performance_writer.write(
        //         traccc::get_data(track_candidates), evt_map);                 // 2024/08/26 asami "track_candidates_cuda"->"track_candidates"

        //     for (unsigned int i = 0; i < track_states.size(); i++) {          // 2024/08/26 asami "track_states_cuda.size()"->"track_states.size()"
        //         const auto& trk_states_per_track =
        //             track_states.at(i).items;                                 // 2024/08/26 asami "track_states_cuda.at(i).items"->"track_states.at(i).items;"

        //         const auto& fit_res = track_states[i].header;                 // 2024/08/26 asami "track_states_cuda[i].header"->"track_states[i].header"

        //         fit_performance_writer.write(trk_states_per_track, fit_res,
        //                                      host_det, evt_map);
        //     }
        // }
    }

    if (performance_opts.run) {
        sd_performance_writer.finalize();
        nsd_performance_writer.finalize();
        find_performance_writer.finalize();
        fit_performance_writer.finalize();
        std::cout << nsd_performance_writer.generate_report_str();
    }

    std::cout << "==> Statistics ... " << std::endl;
    std::cout << "- read    " << n_spacepoints << " spacepoints from "
              << n_modules << " modules" << std::endl;
    std::cout << "- created  (cpu)  " << n_seeds << " seeds" << std::endl;
    std::cout << "- created (cuda)  " << n_seeds_cuda << " seeds" << std::endl;
    std::cout << "- created  (cpu) " << n_found_tracks << " found tracks"
              << std::endl;
    std::cout << "- created (cuda) " << n_found_tracks_cuda << " found tracks"
              << std::endl;
    std::cout << "- created  (cpu) " << n_fitted_tracks << " fitted tracks"
              << std::endl;
    std::cout << "- created (cuda) " << n_fitted_tracks_cuda << " fitted tracks"
              << std::endl;
    std::cout << "==>Elapsed times...\n" << elapsedTimes << std::endl;


    // asami 240911

    // create a file to store elapsed times temporarily
    std::string filename_tempo = "tempo_time.txt";
    std::ofstream file_tempo_time;

    file_tempo_time.open(filename_tempo, std::ios::app);
    if (file_tempo_time.is_open()) {
    
        file_tempo_time << elapsedTimes << std::endl;

        file_tempo_time.close();
    
    } else {
    
        std::cerr << "Unable to open file: " << std::endl;
    
    }

    // input from the temporary file and output only values to processing_time.txt
    std::string filename_time = "processing_time.txt";

    std::ifstream inputFile(filename_tempo);

    if (!inputFile.is_open()) {
        std::cerr << "unable to open input file" << std::endl;
        return 1;
    }

    std::ofstream outputFile("processing_time.txt");

    if (!outputFile.is_open()) {
        std::cerr << "unable to open processing_time.txt" << std::endl;
        return 1;
    }

    std::string line;
    std::regex numberRegex(R"(\d+)");
    std::smatch match;

    while (std::getline(inputFile, line)) {

        while (std::regex_search(line, match, numberRegex)) {

            outputFile << match.str() << std::endl;

            line = match.suffix().str();
        }
    }

    inputFile.close();
    outputFile.close();

    // up to here

    return 0;
}

// The main routine
//
int main(int argc, char* argv[]) {

    // Program options.
    traccc::opts::detector detector_opts;
    traccc::opts::input_data input_opts;
    traccc::opts::track_seeding seeding_opts;
    traccc::opts::track_finding finding_opts;
    traccc::opts::track_propagation propagation_opts;
    traccc::opts::performance performance_opts;
    traccc::opts::accelerator accelerator_opts;
    // 240829 asami
    traccc::opts::output_data output_opts;
    // up to here
    traccc::opts::program_options program_opts{
        "Full Tracking Chain Using CUDA (without clusterization)",
        {detector_opts, input_opts, seeding_opts, finding_opts,
         propagation_opts, performance_opts, accelerator_opts,
        // 240829 asami
        output_opts},
        // up to here
        argc,
        argv};

    // Run the application.
    return seq_run(seeding_opts, finding_opts, propagation_opts, input_opts,
                   detector_opts, performance_opts, accelerator_opts,
                   // 240829 asami
                    output_opts
                   // up to here
                   );
}
