/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Local include(s).
#include "traccc/io/mywrite.hpp"

#include "traccc/io/utils.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>

namespace traccc::io {

void mywrite(std::size_t event, std::string_view directory,
	     traccc::cell_collection_types::const_view cells_view,
	     std::string_view device) {

  std::string filename=data_directory() + directory.data() + device.data() +
    get_event_filename(event, "-cells.txt");

  std::ofstream out_file(filename.data());
  out_file<<"module_link,channel0,channel1,timestamp,value"<<std::endl;
  
  traccc::cell_collection_types::const_device cells(cells_view);
  for(const cell & cl: cells) {
    out_file<<cl.module_link<<","<<cl.channel0<<","<<cl.channel1<<","<<cl.time<<","<<cl.activation
	    <<std::endl;;
  }
  out_file.close();
  
}

void mywrite(std::size_t event, std::string_view directory,
	     traccc::cell_module_collection_types::const_view modules_view,
	     std::string_view device) {

  std::string filename=data_directory() + directory.data() + device.data() + 
    get_event_filename(event, "-modules.txt");
  std::ofstream out_file(filename.data());
  out_file<<"geometry_id,threshold,min_center_x,min_center_y,pitch_x,pitch_y"
	  <<std::endl;
  
  traccc::cell_module_collection_types::const_device modules(modules_view);
  for(const cell_module & md: modules) {
    out_file<<md.surface_link.index()<<","<<md.threshold<<","
	    <<md.pixel.min_corner_x<<","<<md.pixel.min_corner_y<<","   // 2024/08/26 asami "min_center_x"->"min_corner_x", "min_center_y"->"min_corner_y"
	    <<md.pixel.pitch_x<<","<<md.pixel.pitch_y
	    <<std::endl;
  }
  out_file.close();
  
}

void mywrite(std::size_t event, std::string_view directory,
	     traccc::measurement_collection_types::const_view measurements_view,
	     std::string_view device){

  std::string filename=data_directory() + directory.data() + device.data() + 
    get_event_filename(event, "-measurements.txt");
  std::ofstream out_file(filename.data());

  out_file<<"module_link,local0,local1,var0,var1"
	  <<std::endl;

  traccc::measurement_collection_types::const_device measurements(measurements_view);
  for(const measurement &am : measurements){
    out_file<<am.module_link<<","<<am.local[0]<<","<<am.local[1]<<","
	    <<am.variance[0]<<","<<am.variance[1]
	    <<std::endl;
  }
  out_file.close();
}

void mywrite(std::size_t event, std::string_view directory,
	     traccc::spacepoint_collection_types::const_view spacepoints_view,
	     std::string_view device){
    
  std::string filename=data_directory() + directory.data() + device.data() + 
      get_event_filename(event, "-spacepoints.txt");
  std::ofstream out_file(filename.data());

  out_file<<"module_link,x,y,z,r"
	    <<std::endl;

  traccc::spacepoint_collection_types::const_device spacepoints(spacepoints_view);
  for(const spacepoint &sp : spacepoints){
    out_file<<sp.meas.surface_link.index()<<","
	    <<std::setprecision(12)<<sp.x()<<","
	    <<std::setprecision(12)<<sp.y()<<","
	    <<std::setprecision(12)<<sp.z()<<","<<sp.radius()
	    <<std::endl;
  }
  
  out_file.close();
}
    
void mywrite(std::size_t event, std::string_view directory,
	     traccc::seed_collection_types::const_view seeds_view,
	     std::string_view device){
    
  std::string filename=data_directory() + directory.data() + device.data() + 
      get_event_filename(event, "-seeds.txt");
  std::ofstream out_file(filename.data());

  out_file<<"spB_link,spM_link,spT_link,weight,z_vertex"
	    <<std::endl;

  traccc::seed_collection_types::const_device seeds(seeds_view);
  for(const seed sd : seeds){
    out_file<<sd.spB_link<<","<<sd.spM_link<<","<<sd.spT_link<<","
	    <<sd.weight<<","<<sd.z_vertex<<","
	    <<std::endl;
  }
  
  out_file.close();
}
    

void mywrite(std::size_t event, std::string_view directory,
	     traccc::bound_track_parameters_collection_types::const_view params_view,
	     std::string_view device){
    
  std::string filename=data_directory() + directory.data() + device.data() + 
      get_event_filename(event, "-params.txt");
  std::ofstream out_file(filename.data());

  out_file<<"surface_index,loc0,loc1,phi,theta,qoverp,time"
	    <<std::endl;

  traccc::bound_track_parameters_collection_types::const_device params(params_view);
  for(const bound_track_parameters &tp : params){
    out_file<<tp.surface_link().index()
      	    <<","<<tp.bound_local().at(0)<<","<<tp.bound_local().at(1)
	    <<","<<tp.phi()<<","<<tp.theta()<<","<<tp.qop()<<","<<tp.time()
	    <<std::endl;

  }
  
  out_file.close();
}
    
void mywrite(std::size_t event, std::string_view directory,
	     traccc::track_state_container_types::const_view tracks_view,
	     std::string_view device){
    
  std::string filename=data_directory() + directory.data() + device.data() + 
      get_event_filename(event, "-fitted.txt");
  std::ofstream out_file(filename.data());

  out_file<<"surface_index,loc0,loc1,phi,theta,qoverp,time,ndf,chi2"
	    <<std::endl;

  traccc::track_state_container_types::const_device tracks(tracks_view);
  const unsigned int n_tracks = tracks.size();
  for(unsigned int i=0; i<n_tracks ; i++){
    const auto &tf = tracks.at(i).header;
    const auto &tp = tf.fit_params;
    
    out_file<<tp.surface_link().index()
      	    <<","<<tp.bound_local().at(0)<<","<<tp.bound_local().at(1)
	    <<","<<tp.phi()<<","<<tp.theta()<<","<<tp.qop()<<","<<tp.time()
	    <<","<<tf.ndf<<","<<tf.chi2
	    <<std::endl;

  }
  
  out_file.close();
}
    

  
}
