/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2022-2024 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#pragma once

// Project include(s).
#include "traccc/simulation/smearing_writer.hpp"

// Detray include(s).
#include "detray/navigation/navigator.hpp"
#include "detray/propagator/actor_chain.hpp"
#include "detray/propagator/actors/aborters.hpp"
#include "detray/propagator/actors/parameter_resetter.hpp"
#include "detray/propagator/actors/parameter_transporter.hpp"
#include "detray/propagator/propagator.hpp"
#include "detray/propagator/rk_stepper.hpp"
#include "detray/simulation/random_scatterer.hpp"
#include "detray/simulation/event_generator/track_generators.hpp"

// System include(s).
#include <limits>
#include <memory>
#include <vector>
#include <random>

namespace traccc {

template <typename detector_t, typename bfield_t, typename track_generator_t,
          typename writer_t>
struct multi_vtx_simulator {

    using scalar_type = typename detector_t::scalar_type;

    struct config {
        detray::propagation::config propagation;
    };

    using algebra_type = typename detector_t::algebra_type;
    using bfield_type = bfield_t;

    using actor_chain_type =
        detray::actor_chain<detray::dtuple,
                            detray::parameter_transporter<algebra_type>,
                            detray::random_scatterer<algebra_type>,
                            detray::parameter_resetter<algebra_type>, writer_t>;

    using navigator_type = detray::navigator<detector_t>;
    using stepper_type =
        detray::rk_stepper<typename bfield_type::view_t, algebra_type,
                           detray::constrained_step<>>;
    using propagator_type =
        detray::propagator<stepper_type, navigator_type, actor_chain_type>;
  using uniform_gen_t =
    detray::detail::random_numbers<scalar, std::uniform_real_distribution<scalar>>;

    using generator_type =
        detray::random_track_generator<traccc::free_track_parameters,
                                       uniform_gen_t>;


    multi_vtx_simulator(std::size_t events, const detector_t& det,
              const bfield_type& field, track_generator_t&& track_gen,
              typename writer_t::config&& writer_cfg,
              const std::string directory = "")
        : m_events(events),
          m_directory(directory),
          m_detector(det),
          m_field(field),
          m_track_generator(
              std::make_unique<track_generator_t>(std::move(track_gen))),
          m_writer_cfg(writer_cfg) {}

    config& get_config() { return m_cfg; }

    void setup_pu(int nvtx, int ntrk_per_vtx){
      
      m_pu_track_generators.clear();

      std::random_device seed_gen;
      std::default_random_engine engine(seed_gen());

      std::normal_distribution<> distz(0, 54); //in mm
      std::normal_distribution<> distx(0, 0.04);
      std::normal_distribution<> disty(0, 0.04);
      
      for(int i=0; i<nvtx; i++){
	float zpos = distz(engine);
	float xpos = distx(engine);
	float ypos = disty(engine);
	    generator_type::configuration pu_gen_cfg{};
	    pu_gen_cfg.n_tracks(ntrk_per_vtx);
	    pu_gen_cfg.origin(traccc::point3{xpos, ypos, zpos});
	    pu_gen_cfg.origin_stddev({0.f, 0.f, 0.f});
	    pu_gen_cfg.phi_range(-detray::constant<scalar>::pi,
		detray::constant<scalar>::pi);
	    pu_gen_cfg.eta_range(-4.,4.);
	    pu_gen_cfg.mom_range(1.,10.);
	    pu_gen_cfg.randomize_charge(true);
	    pu_gen_cfg.seed(pu_gen_cfg.seed() + i);
	    //	    generator_type generator(pu_gen_cfg);
	    auto ptr = std::make_unique<generator_type>(pu_gen_cfg);
	    m_pu_track_generators.push_back(std::move(ptr));
      }
    }

  
    void run() {

        for (std::size_t event_id = 0u; event_id < m_events; event_id++) {

            typename writer_t::state writer_state(
                event_id, std::move(m_writer_cfg), m_directory);

            // Set random seed
            m_scatterer.set_seed(event_id);
            writer_state.set_seed(event_id);

            auto actor_states =
                std::tie(m_transporter, m_scatterer, m_resetter, writer_state);

            for (auto track : *m_track_generator.get()) {

                writer_state.write_particle(track);

                typename propagator_type::state propagation(track, m_field,
                                                            m_detector);

                propagator_type p(m_cfg.propagation);

                // Set overstep tolerance and stepper constraint
                propagation._stepping.template set_constraint<
                    detray::step::constraint::e_accuracy>(
                    m_cfg.propagation.stepping.step_constraint);

                p.propagate(propagation, actor_states);

                // Increase the particle id
                writer_state.particle_id++;
            }

	    for (auto &gen: m_pu_track_generators){
	      for (auto track : *gen.get()) {

                writer_state.write_particle(track);

                typename propagator_type::state propagation(track, m_field,
                                                            m_detector);

                propagator_type p(m_cfg.propagation);

                // Set overstep tolerance and stepper constraint
                propagation._stepping.template set_constraint<
                    detray::step::constraint::e_accuracy>(
                    m_cfg.propagation.stepping.step_constraint);

                p.propagate(propagation, actor_states);

                // Increase the particle id
                writer_state.particle_id++;
	      }
	    }
        }
    }

    private:
    config m_cfg;
    std::size_t m_events{0u};
    std::string m_directory = "";
    const detector_t& m_detector;
    const bfield_type& m_field;
    std::unique_ptr<track_generator_t> m_track_generator;
    std::vector<std::unique_ptr<track_generator_t>> m_pu_track_generators;
    typename writer_t::config m_writer_cfg;

    /// Actor states
    typename detray::parameter_transporter<algebra_type>::state m_transporter{};
    typename detray::random_scatterer<algebra_type>::state m_scatterer{};
    typename detray::parameter_resetter<algebra_type>::state m_resetter{};
};

}  // namespace traccc
