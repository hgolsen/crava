/***************************************************************************
*      Copyright (C) 2008 by Norwegian Computing Center and Statoil        *
***************************************************************************/


#include "src/multiintervalgrid.h"
#include "src/definitions.h"
#include "src/simbox.h"
#include "nrlib/grid/grid.hpp"

MultiIntervalGrid::MultiIntervalGrid(ModelSettings  * model_settings,
                                     InputFiles     * input_files,
                                     const Simbox   * estimation_simbox,
                                     std::string    & err_text,
                                     bool           & failed) {

  interval_names_                                                 = model_settings->getIntervalNames();
  int erosion_priority_top_surface                                = model_settings->getErosionPriorityTopSurface();
  const std::map<std::string,int> erosion_priority_base_surfaces  = model_settings->getErosionPriorityBaseSurfaces();
  dz_min_                                                         = 10000;

  //H Combined the if(multinterval) below
  if (interval_names_.size() == 0) {
    LogKit::WriteHeader("Setting up grid");
    interval_names_.push_back("");
    multiple_interval_setting_ = false;
  }
  else {
    LogKit::WriteHeader("Setting up multiple interval grid");
    multiple_interval_setting_ = true;
  }
  n_intervals_ = static_cast<int>(interval_names_.size());

  Surface               * top_surface = NULL;
  Surface               * base_surface = NULL;
  std::vector<Surface>    eroded_surfaces(n_intervals_+1);
  std::string             previous_interval_name("");
  std::string             top_surface_file_name_temp("");
  std::string             base_surface_file_name_temp("");
  std::vector<Surface>    surfaces;
  //std::vector<int>        erosion_priorities_;

  desired_grid_resolution_.resize(n_intervals_);
  relative_grid_resolution_.resize(n_intervals_);
  eroded_surfaces.resize(n_intervals_+1);
  surfaces.resize(n_intervals_+1); //Store surfaces.
  erosion_priorities_.resize(n_intervals_+1);
  interval_simboxes_.resize(n_intervals_);

  //if (interval_names_.size() > 0) {
  //  multiple_interval_setting_ = true;
  //  desired_grid_resolution_.resize(n_intervals_);
  //  relative_grid_resolution_.resize(n_intervals_);
  //  n_intervals_ = static_cast<int>(interval_names_.size());
  //  eroded_surfaces.resize(n_intervals_ + 1);
  //  LogKit::WriteHeader("Setting up multiple interval grid");
  //  surfaces.resize(n_intervals_+1); //Store surfaces.
  //  erosion_priorities_.resize(n_intervals_+1);
  //  interval_simboxes_.resize(n_intervals_);
  //  parameters_.resize(n_intervals_);
  //  for (size_t i = 0; i < n_intervals_; i++) {
  //    parameters_[i].resize(3);
  //    for (int j = 0; j < 3; j++)
  //      parameters_[i][j] = new NRLib::Grid<float>();
  //  }

  //  background_vs_vp_ratios_.resize(n_intervals_);
  //  prior_facies_prob_cubes_.resize(n_intervals_);
  //}
  //// if there is only one interval
  //else {
  //  multiple_interval_setting_ = false;
  //  desired_grid_resolution_.resize(1);
  //  relative_grid_resolution_.resize(1);
  //  eroded_surfaces.resize(2);
  //  n_intervals_ = 1;
  //  LogKit::WriteHeader("Setting up grid");
  //  surfaces.resize(1);
  //  interval_simboxes_.resize(1);
  //  interval_names_.push_back("");
  //  background_parameters_.resize(1);
  //  background_parameters_[0].resize(3);
  //  for (size_t i = 0; i < 3; i++)
  //    background_parameters_[0][i] = new NRLib::Grid<float>();
  //  background_vs_vp_ratios_.resize(1);
  //  prior_facies_prob_cubes_.resize(1);
  //}

  // 1. ERODE SURFACES AND SET SURFACES OF SIMBOXES -----------------------------------------

  try{
    // if multiple-intervals keyword is used in model settings
    if (multiple_interval_setting_){
      top_surface_file_name_temp = input_files->getTimeSurfFile(0);
      erosion_priorities_[0] = erosion_priority_top_surface;

      top_surface = MakeSurfaceFromFileName(top_surface_file_name_temp, *estimation_simbox);
      surfaces[0] = *top_surface;

      for (size_t i = 0; i < n_intervals_; i++) {

        std::string interval_name = model_settings->getIntervalName(i);
        base_surface_file_name_temp = input_files->getIntervalBaseTimeSurface(interval_name);
        erosion_priorities_[i+1] = erosion_priority_base_surfaces.find(interval_name)->second;

        base_surface = MakeSurfaceFromFileName(base_surface_file_name_temp, *estimation_simbox);
        surfaces[i+1] =  *base_surface;
      }

      if (!failed){

        for (size_t i = 0; i<n_intervals_; i++){
          desired_grid_resolution_[i] = FindResolution(&surfaces[i], &surfaces[i+1], estimation_simbox,
                                                     model_settings->getTimeNzInterval(interval_names_[i]));
        }

        ErodeAllSurfaces(eroded_surfaces,
                         erosion_priorities_,
                         surfaces,
                         *estimation_simbox,
                         err_text);
      }
      else {
        err_text += "Erosion of surfaces failed because the interval surfaces could not be set up correctly.\n";
      }
    }
    // if multiple-intervals is NOT used in model settings
    else {

      //int nz = model_settings->getTimeNz();
      int nz = model_settings->getTimeNzInterval("");

      if (model_settings->getParallelTimeSurfaces() == false) {
        top_surface_file_name_temp = input_files->getTimeSurfFile(0);
        top_surface = MakeSurfaceFromFileName(top_surface_file_name_temp, *estimation_simbox);
        eroded_surfaces[0] = *top_surface;

        base_surface_file_name_temp = input_files->getTimeSurfFile(1);
        base_surface = MakeSurfaceFromFileName(base_surface_file_name_temp, *estimation_simbox);
        eroded_surfaces[1] = *base_surface;

      }
      else { //H added if only one surface-file is used, similar to setup of estimation_simbox.
        top_surface_file_name_temp = input_files->getTimeSurfFile(0);
        top_surface = MakeSurfaceFromFileName(top_surface_file_name_temp, *estimation_simbox);
        eroded_surfaces[0] = *top_surface;

        base_surface = new Surface(*top_surface);
        double lz = model_settings->getTimeLz();
        base_surface->Add(lz);
        eroded_surfaces[1] = *base_surface;

        double dz = model_settings->getTimeDz();
        if (model_settings->getTimeNzInterval("") == RMISSING) //Taken from simbox->SetDepth without nz
          nz = static_cast<int>(0.5+lz/dz);

      }

      desired_grid_resolution_[0] = FindResolution(top_surface, base_surface, estimation_simbox, nz);

    }
  }
  catch(NRLib::Exception & e){
    failed = true;
    err_text += e.what();
  }

  delete top_surface;
  delete base_surface;

  // 2 SET UP INTERVAL_SIMBOXES ------------------------------------------------------------

  //Set up a vector of simboxes, one per interval.
  if (!failed){
    try{
        SetupIntervalSimboxes(model_settings,
                              estimation_simbox,
                              interval_names_,
                              eroded_surfaces,
                              interval_simboxes_,
                              input_files->getCorrDirIntervalFiles(),
                              input_files->getCorrDirIntervalTopSurfaceFiles(),
                              input_files->getCorrDirIntervalBaseSurfaceFiles(),
                              model_settings->getCorrDirIntervalTopConform(),
                              model_settings->getCorrDirIntervalBaseConform(),
                              desired_grid_resolution_,
                              relative_grid_resolution_,
                              dz_min_,
                              dz_rel_,
                              err_text,
                              failed);
    }
    catch(NRLib::Exception & e){
      failed = true;
      err_text += e.what();
    }
  }

  // Add inn surface files.
  surface_files_.push_back(input_files->getTimeSurfFile(0));

  const std::map<std::string, std::string> & interval_base_time_surfaces = input_files->getIntervalBaseTimeSurfaces();
  for (size_t i = 0; i > n_intervals_; i++) {
    surface_files_.push_back(interval_base_time_surfaces.find(interval_names_[i])->second);
  }

}

//Copy constructor
MultiIntervalGrid::MultiIntervalGrid(const MultiIntervalGrid * multi_interval_grid)
{
  n_intervals_               = multi_interval_grid->n_intervals_;
  multiple_interval_setting_ = multi_interval_grid->multiple_interval_setting_;

  interval_names_            = multi_interval_grid->interval_names_;
  erosion_priorities_        = multi_interval_grid->erosion_priorities_;
  surface_files_             = multi_interval_grid->surface_files_;

  interval_simboxes_         = multi_interval_grid->interval_simboxes_;

  desired_grid_resolution_   = multi_interval_grid->desired_grid_resolution_;
  relative_grid_resolution_  = multi_interval_grid->relative_grid_resolution_;
  dz_rel_                    = multi_interval_grid->dz_rel_;
}

MultiIntervalGrid::~MultiIntervalGrid() {
}

// ---------------------------------------------------------------------------------------------------------------
int   MultiIntervalGrid::WhichSimbox(double x, double y, double z) const
{
  int simbox_num = -1;
  for (size_t i = 0; i<interval_simboxes_.size(); i++){
    if (interval_simboxes_[i].IsPointBetweenOriginalSurfaces(x,y,z)){
      simbox_num = i;
      break;
    }
  }
  return simbox_num;
}

// ---------------------------------------------------------------------------------------------------------------
/*
void  MultiIntervalGrid::SetupIntervalSimbox(ModelSettings                               * model_settings,
                                             const Simbox                                * estimation_simbox,
                                             Simbox                                      & interval_simbox,
                                             const std::vector<Surface>                  & eroded_surfaces,
                                             const std::string                           & corr_dir_single_surf,
                                             const std::string                           & corr_dir_top_surf,
                                             const std::string                           & corr_dir_base_surf,
                                             bool                                          corr_dir_top_conform,
                                             bool                                          corr_dir_base_conform,
                                             double                                      & desired_grid_resolution,
                                             double                                      & relative_grid_resolution,
                                             std::string                                 & err_text,
                                             bool                                        & failed) const{


  bool                   corr_dir                               = false;
  Surface                top_surface                            = eroded_surfaces[0];
  Surface                base_surface                           = eroded_surfaces[1];
  int                    n_layers                               = model_settings->getTimeNz();

  if (n_layers == RMISSING)
    n_layers = static_cast<int>(0.5+model_settings->getTimeLz()/model_settings->getTimeDz());

  // Case 1: Single correlation surface
  if (corr_dir_single_surf != "") {
    Surface * corr_surf = MakeSurfaceFromFileName(corr_dir_single_surf,  estimation_simbox);
    interval_simbox = Simbox(estimation_simbox, "", n_layers, top_surface, base_surface, corr_surf, err_text, failed);
    const SegyGeometry * area_params = model_settings->getAreaParameters();
    failed = interval_simbox.setArea(area_params, err_text);

    //interval_simbox.SetTopBotErodedSurfaces(top_surface, base_surface);
    interval_simbox.SetErodedSurfaces(top_surface, base_surface);
  }
  else {
    interval_simbox = Simbox(estimation_simbox, "", n_layers, top_surface, base_surface, err_text, failed);
    const SegyGeometry * area_params = model_settings->getAreaParameters();
    failed = interval_simbox.setArea(area_params, err_text);

    //interval_simbox.SetTopBotErodedSurfaces(top_surface, base_surface);
    interval_simbox.SetErodedSurfaces(top_surface, base_surface);
  }

  interval_simbox.SetNXpad(estimation_simbox->GetNXpad());
  interval_simbox.SetNYpad(estimation_simbox->GetNYpad());
  interval_simbox.SetXPadFactor(estimation_simbox->GetXPadFactor());
  interval_simbox.SetYPadFactor(estimation_simbox->GetYPadFactor());


  // Calculate Z padding ----------------------------------------------------------------
  interval_simbox.calculateDz(model_settings->getLzLimit(),err_text);
  EstimateZPaddingSize(&interval_simbox, model_settings);
}
*/

// ---------------------------------------------------------------------------------------------------------------
void   MultiIntervalGrid::SetupIntervalSimboxes(ModelSettings                             * model_settings,
                                                const Simbox                              * estimation_simbox,
                                                const std::vector<std::string>            & interval_names,
                                                const std::vector<Surface>                & eroded_surfaces,
                                                std::vector<Simbox>                       & interval_simboxes,
                                                const std::map<std::string, std::string>  & corr_dir_single_surfaces,
                                                const std::map<std::string, std::string>  & corr_dir_top_surfaces,
                                                const std::map<std::string, std::string>  & corr_dir_base_surfaces,
                                                const std::map<std::string, bool>         & corr_dir_top_conform,
                                                const std::map<std::string, bool>         & corr_dir_base_conform,
                                                std::vector<double>                       & desired_grid_resolution,
                                                std::vector<double>                       & relative_grid_resolution,
                                                double                                    & dz_min,
                                                std::vector<double>                       & dz_rel,
                                                std::string                               & err_text,
                                                bool                                      & failed) const
{
  for (size_t i = 0; i< interval_names.size(); i++){

    bool                   corr_dir                               = false;
    Surface                top_surface                            = eroded_surfaces[i];
    Surface                base_surface                           = eroded_surfaces[i+1];
    std::string            interval_name                          = interval_names[i];
    int                    n_layers                               = model_settings->getTimeNzInterval(interval_name);
    std::map<std::string, std::string>::const_iterator it_single  = corr_dir_single_surfaces.find(interval_name);
    std::map<std::string, std::string>::const_iterator it_top     = corr_dir_top_surfaces.find(interval_name);
    std::map<std::string, std::string>::const_iterator it_base    = corr_dir_base_surfaces.find(interval_name);
    std::map<std::string, bool>::const_iterator it_top_conform    = corr_dir_top_conform.find(interval_name);
    std::map<std::string, bool>::const_iterator it_base_conform   = corr_dir_base_conform.find(interval_name);

    // Make a simbox for the original interval --------------------------------------------
    //SegyGeometry * geometry = model_settings->getAreaParameters();

    float min_samp_dens = model_settings->getMinSamplingDensity();

    // Make extended interval_simbox for the inversion interval ---------------------------

    if (interval_simboxes[i].status() == Simbox::EMPTY){

      // Case 1: Single correlation surface
      if (it_single != corr_dir_single_surfaces.end() && it_top == corr_dir_top_surfaces.end() && it_base == corr_dir_base_surfaces.end()){
        corr_dir = true;
        Surface * corr_surf     = MakeSurfaceFromFileName(it_single->second,  estimation_simbox);
        interval_simboxes[i]    = Simbox(estimation_simbox, interval_names[i], n_layers, top_surface, base_surface, corr_surf, err_text, failed);
      }
      // Case 2: Top and base correlation surfaces
      else if (it_single == corr_dir_single_surfaces.end() && it_top != corr_dir_top_surfaces.end() && it_base != corr_dir_base_surfaces.end()){
        corr_dir = true;
        Surface * corr_surf_top = MakeSurfaceFromFileName(it_top->second,  estimation_simbox);
        Surface * corr_surf_base = MakeSurfaceFromFileName(it_base->second,  estimation_simbox);
        interval_simboxes[i] = Simbox(estimation_simbox, interval_names[i], n_layers, top_surface, base_surface, err_text, failed,
                                                corr_surf_top, corr_surf_base);
      }
      // Case 3: Top conform and base conform if (i) both are set conform or (ii) if no other corr surfaces have been defined
      else if ((it_top_conform->second == true && it_base_conform->second == true) ||
               (it_single == corr_dir_single_surfaces.end() && it_top == corr_dir_top_surfaces.end() && it_base == corr_dir_base_surfaces.end())){
        interval_simboxes[i] = Simbox(estimation_simbox, interval_names[i], n_layers, top_surface, base_surface, err_text, failed);
      }
      // Case 4: Top correlation surface and base conform
      else if (it_top != corr_dir_top_surfaces.end() && it_base_conform->second == true){
        corr_dir = true;
        Surface * corr_surf_top = MakeSurfaceFromFileName(it_top->second,  estimation_simbox);
        interval_simboxes[i] = Simbox(estimation_simbox, interval_names[i], n_layers, top_surface, base_surface, err_text, failed,
                                                corr_surf_top, NULL);
      }
      // Case 5: Top conform and base correlation surface
      else if (it_top_conform == corr_dir_top_conform.end() && it_base_conform != corr_dir_base_conform.end()){
        corr_dir = true;
        Surface * corr_surf_base = MakeSurfaceFromFileName(it_base->second,  estimation_simbox);
        interval_simboxes[i] = Simbox(estimation_simbox, interval_names[i], n_layers, top_surface, base_surface, err_text, failed,
                                                NULL, corr_surf_base);
      }
      // else something is wrong
      else{
        err_text += "\nCorrelation directions are not set correctly for interval " + interval_name[i];
        err_text += ".\n";
        failed = true;
      }


    }
    // Calculate Z padding ----------------------------------------------------------------

    if (!failed){
      // calculated dz should be the same as the desired grid resolution?
      interval_simboxes[i].calculateDz(model_settings->getLzLimit(),err_text);
      relative_grid_resolution[i] = interval_simboxes[i].getdz() / desired_grid_resolution[i];
      EstimateZPaddingSize(&interval_simboxes[i], model_settings);

      if (interval_simboxes[i].getdz()*interval_simboxes[i].getMinRelThick() < min_samp_dens){
        failed   = true;
        err_text += "We normally discourage denser sampling than "+NRLib::ToString(min_samp_dens);
        err_text += "ms in the time grid. If you really need\nthis, please use ";
        err_text += "<project-settings><advanced-settings><minimum-sampling-density>\n";
      }

      if(interval_simboxes[i].status() == Simbox::BOXOK){
        if(corr_dir){
          LogIntervalInformation(interval_simboxes[i], interval_names[i],
            "Time inversion interval (extended relative to output interval due to correlation):","Two-way-time");
        }
        else{
          LogIntervalInformation(interval_simboxes[i], interval_names[i], "Time output interval:","Two-way-time");
        }
      }
      else {
        err_text += "Could not make the time simulation grid.\n";
        failed = true;
      }
    }

    // Calculate XY padding ---------------------------------------------------------------

    if (!failed) {

      //EstimateXYPaddingSizes(&interval_simboxes[i], model_settings);
      interval_simboxes[i].SetNXpad(estimation_simbox->GetNXpad());
      interval_simboxes[i].SetNYpad(estimation_simbox->GetNYpad());
      interval_simboxes[i].SetXPadFactor(estimation_simbox->GetXPadFactor());
      interval_simboxes[i].SetYPadFactor(estimation_simbox->GetYPadFactor());

      unsigned long long int grid_size = static_cast<unsigned long long int>(interval_simboxes[i].GetNXpad())*interval_simboxes[i].GetNYpad()*interval_simboxes[i].GetNZpad();

      if(grid_size > std::numeric_limits<unsigned int>::max()) {
        float fsize = 4.0f*static_cast<float>(grid_size)/static_cast<float>(1024*1024*1024);
        float fmax  = 4.0f*static_cast<float>(std::numeric_limits<unsigned int>::max()/static_cast<float>(1024*1024*1024));
        err_text += "Grids as large as "+NRLib::ToString(fsize,1)+"GB cannot be handled. The largest accepted grid size\n";
        err_text += "is "+NRLib::ToString(fmax)+"GB. Please reduce the number of layers or the lateral resolution.\n";
        failed = true;
      }

      if (interval_names.size() == 1)
        LogKit::LogFormatted(LogKit::Low,"\nTime simulation grids: \n");
      else
        LogKit::LogFormatted(LogKit::Low,"\nTime simulation grids for interval \'"+interval_names[i]+"\':\n");
      LogKit::LogFormatted(LogKit::Low,"  Output grid        %4i * %4i * %4i   : %10llu\n",
                            interval_simboxes[i].getnx(),interval_simboxes[i].getny(),interval_simboxes[i].getnz(),
                            static_cast<unsigned long long int>(interval_simboxes[i].getnx())*interval_simboxes[i].getny()*interval_simboxes[i].getnz());
      LogKit::LogFormatted(LogKit::Low,"  FFT grid           %4i * %4i * %4i   :%11llu\n",
                            interval_simboxes[i].GetNXpad(),interval_simboxes[i].GetNYpad(),interval_simboxes[i].GetNZpad(),
                            static_cast<unsigned long long int>(interval_simboxes[i].GetNXpad())*interval_simboxes[i].GetNYpad()*interval_simboxes[i].GetNZpad());
    }

    // Check consistency ------------------------------------------------------------------
    if (interval_simboxes[i].getdz() >= 10.0 && model_settings->getFaciesProbFromRockPhysics() == true) {
      err_text += "dz for interval \'" + interval_names[i] + "\' is too large to generate synthetic well data when estimating facies probabilities using rock physics models. Need dz < 10.";
      failed = true;
    }
  } // end for loop over intervals
  dz_rel.resize(interval_names.size());

  // Pick the simbox with the finest vertical resolution and set dz_rel relative to this dz
  dz_min = 10000;
  for (size_t m = 0; m < interval_simboxes.size(); m++){
    if (interval_simboxes[m].getdz() < dz_min)
      dz_min = interval_simboxes[m].getdz();
  }

  for(size_t m = 0; m < interval_simboxes.size(); m++){
    dz_rel[m] = interval_simboxes[m].getdz()/dz_min;
  }
}

// --------------------------------------------------------------------------------
Surface * MultiIntervalGrid::MakeSurfaceFromFileName(const std::string    & file_name,
                                                     const Simbox         & estimation_simbox) const{

  Surface * new_surface = NULL;

  if (!NRLib::IsNumber(file_name)) { // If the file name is a string
    new_surface = new Surface(file_name);
  }
  else { //If the file name is a value

    double x_min, x_max, y_min, y_max;

    FindSmallestSurfaceGeometry(estimation_simbox.getx0(), estimation_simbox.gety0(),
                                estimation_simbox.getlx(), estimation_simbox.getly(),
                                estimation_simbox.getAngle(), x_min, y_min, x_max, y_max);

    new_surface = new Surface(x_min-100, y_min-100, x_max-x_min+200, y_max-y_min+200, 2, 2, atof(file_name.c_str()));
  }

  return new_surface;
}

// --------------------------------------------------------------------------------
void MultiIntervalGrid::FindSmallestSurfaceGeometry(const double   x0,
                                                    const double   y0,
                                                    const double   lx,
                                                    const double   ly,
                                                    const double   rot,
                                                    double       & x_min,
                                                    double       & y_min,
                                                    double       & x_max,
                                                    double       & y_max) const
{
  x_min = x0 - ly*sin(rot);
  x_max = x0 + lx*cos(rot);
  y_min = y0;
  y_max = y0 + lx*sin(rot) + ly*cos(rot);
  if (rot < 0) {
    x_min = x0;
    x_max = x0 + lx*cos(rot) - ly*sin(rot);
    y_min = y0 + lx*sin(rot);
    y_max = y0 + ly*cos(rot);
  }
}

// --------------------------------------------------------------------------------
void  MultiIntervalGrid::ErodeAllSurfaces(std::vector<Surface>       & eroded_surfaces,
                                          const std::vector<int>     & erosion_priorities,
                                          const std::vector<Surface> & surfaces,
                                          const Simbox               & simbox,
                                          std::string                & err_text) const{
  double    min_product     = 1000000000;
  bool      simbox_covered  = true;
  int       surf_max_res    = 0;
  int       n_surf          = static_cast<int>(eroded_surfaces.size());

  for(int i = 0; i < n_surf; i++){
    if(!simbox.CheckSurface(surfaces[i])){
      simbox_covered = false;
      err_text += "Surface " + surfaces[i].GetName() + " does not cover the inversion volume.";
    }
  }

  if(simbox_covered){
    // Find the surface with the highest resolution
    for (int i = 0; i < n_surf; i++){
      if (surfaces[i].GetDX()*surfaces[i].GetDY() < min_product) {
        min_product     = surfaces[i].GetDX()*surfaces[i].GetDY();
        surf_max_res    = i;
      }
    }

    for (int i=0; i<n_surf; i++) {
      int l=0;
      //For surface[0] find the priority 1 surface, for surface[1] find the priority 2 surface etc.
      //All priority 1 to i surfaces have been found at this point
      while(i+1 != erosion_priorities[l])
        l++;

      Surface temp_surface = Surface(surfaces[l]);

      //Find closest eroded surface downward
      for (int k=l+1; k<n_surf; k++) {
        if (eroded_surfaces[k].GetN() > 0) {
          ErodeSurface(temp_surface, eroded_surfaces[k], surfaces[surf_max_res], false);
          break;
        }
      }
      //Find closest eroded surface upward
      for (int k=l-1; k>=0; k--) {
        if (eroded_surfaces[k].GetN() > 0) {
          ErodeSurface(temp_surface, eroded_surfaces[k], surfaces[surf_max_res], true);
          break;
        }
      }
      eroded_surfaces[l] = temp_surface;
    }
  }
  else{
    err_text += "Could not process surface erosion since one or more of the surfaces does not cover the inversion area.";
  }
}

// --------------------------------------------------------------------------------
void  MultiIntervalGrid::ErodeSurface(Surface       &  surface,
                                      const Surface &  priority_surface,
                                      const Surface &  resolution_surface,
                                      const bool    &  compare_upward) const{
  /*
  int nx    = simbox.getnx();
  int ny    = simbox.getny();
  double x0 = simbox.GetXMin();
  double y0 = simbox.GetYMin();
  double lx = simbox.GetLX();
  double ly = simbox.GetLY();
  */

  int nx    = static_cast<int>(resolution_surface.GetNI());
  int ny    = static_cast<int>(resolution_surface.GetNJ());
  double x0 = resolution_surface.GetXMin();
  double y0 = resolution_surface.GetYMin();
  double lx = resolution_surface.GetLengthX();
  double ly = resolution_surface.GetLengthY();

  NRLib::Grid2D<double> eroded_surface(nx,ny,0);
  double x;
  double y;
  double z;
  double z_priority;

  double missing = surface.GetMissingValue();
  for (int i=0; i<nx; i++) {
    for (int j=0; j<ny; j++) {
      x = resolution_surface.GetX(i);
      y = resolution_surface.GetY(j);
      //simbox.getXYCoord(i,j,x,y);

      z_priority = priority_surface.GetZ(x,y);
      z          = surface.GetZ(x,y);

      if (compare_upward) {
        if (z < z_priority && z != missing)
          eroded_surface(i,j) = z_priority;
        else
          eroded_surface(i,j) = z;
      }

      else {
        if (z > z_priority && z_priority != missing)
          eroded_surface(i,j) = z_priority;
        else
          eroded_surface(i,j) = z;
      }
    }
  }


  surface = Surface(x0, y0, lx, ly, eroded_surface);
}

// --------------------------------------------------------------------------------
void MultiIntervalGrid::EstimateZPaddingSize(Simbox          * simbox,
                                             ModelSettings   * model_settings) const {
  int    nz             = simbox->getnz();
  double min_lz         = simbox->getlz()*simbox->getMinRelThick();
  double z_pad_fac      = model_settings->getZPadFac();
  double z_pad          = z_pad_fac*min_lz;

  if (model_settings->getEstimateZPadding())
  {
    double w_length    = static_cast<double>(model_settings->getDefaultWaveletLength());
    double p_fac      = 1.0;
    z_pad             = w_length/p_fac;                               // Use half a wavelet as padding
    z_pad_fac         = std::min(1.0, z_pad/min_lz);                  // More than 100% padding is not sensible
  }
  int nz_pad          = FindPaddingSize(nz, z_pad_fac);
  z_pad_fac           = static_cast<double>(nz_pad - nz)/static_cast<double>(nz);

  simbox->SetNZpad(nz_pad);
  simbox->SetZPadFactor(z_pad_fac);
}

// --------------------------------------------------------------------------------
int MultiIntervalGrid::FindPaddingSize(int     nx,
                                      double  px){

  int leastint    = static_cast<int>(ceil(nx*(1.0f+px)));
  int closestprod = FindClosestFactorableNumber(leastint);
  return(closestprod);
}

// --------------------------------------------------------------------------------
int MultiIntervalGrid::FindClosestFactorableNumber(int leastint){
  int i,j,k,l,m,n;
  int factor   =       1;

  int maxant2    = static_cast<int>(ceil(static_cast<double>(log(static_cast<float>(leastint))) / log(2.0f) ));
  int maxant3    = static_cast<int>(ceil(static_cast<double>(log(static_cast<float>(leastint))) / log(3.0f) ));
  int maxant5    = static_cast<int>(ceil(static_cast<double>(log(static_cast<float>(leastint))) / log(5.0f) ));
  int maxant7    = static_cast<int>(ceil(static_cast<double>(log(static_cast<float>(leastint))) / log(7.0f) ));
  int maxant11   = 0;
  int maxant13   = 0;
  int closestprod= static_cast<int>(pow(2.0f,maxant2));

  /* kan forbedres ved aa trekke fra i endepunktene.i for lokkene*/
  for (i=0;i<maxant2+1;i++)
    for (j=0;j<maxant3+1;j++)
      for (k=0;k<maxant5+1;k++)
        for (l=0;l<maxant7+1;l++)
          for (m=0;m<maxant11+1;m++)
            for (n=maxant11;n<maxant13+1;n++)
            {
              factor = static_cast<int>(pow(2.0f,i)*pow(3.0f,j)*pow(5.0f,k)*
                pow(7.0f,l)*pow(11.0f,m)*pow(13.0f,n));
              if ((factor >=  leastint) &&  (factor <  closestprod))
              {
                closestprod=factor;
              }
            }
            return closestprod;
}

// --------------------------------------------------------------------------------
void  MultiIntervalGrid::LogIntervalInformation(const Simbox      & simbox,
                                                const std::string & interval_name,
                                                const std::string & header_text1,
                                                const std::string & header_text2) const{
  LogKit::LogFormatted(LogKit::Low,"\n"+header_text1+"\n");
  double zmin, zmax;
  simbox.getMinMaxZ(zmin,zmax);
  if (interval_name != "")
    LogKit::LogFormatted(LogKit::Low," Interval name: "+interval_name +"\n");
  LogKit::LogFormatted(LogKit::Low," %13s          avg / min / max    : %7.1f /%7.1f /%7.1f\n",
                       header_text2.c_str(),
                       zmin+simbox.getlz()*simbox.getAvgRelThick()*0.5,
                       zmin,zmax);
  LogKit::LogFormatted(LogKit::Low,"  Interval thickness    avg / min / max    : %7.1f /%7.1f /%7.1f\n",
                       simbox.getlz()*simbox.getAvgRelThick(),
                       simbox.getlz()*simbox.getMinRelThick(),
                       simbox.getlz());
  LogKit::LogFormatted(LogKit::Low,"  Sampling density      avg / min / max    : %7.2f /%7.2f /%7.2f\n",
                       simbox.getdz()*simbox.getAvgRelThick(),
                       simbox.getdz(),
                       simbox.getdz()*simbox.getMinRelThick());
}

void MultiIntervalGrid::LogIntervalInformation(const Simbox      * simbox,
                                               const std::string & header_text1,
                                               const std::string & header_text2) const{
  LogKit::LogFormatted(LogKit::Low,"\n"+header_text1+"\n");
  double zmin, zmax;
  simbox->getMinMaxZ(zmin,zmax);
  LogKit::LogFormatted(LogKit::Low," %13s          avg / min / max    : %7.1f /%7.1f /%7.1f\n",
                       header_text2.c_str(),
                       zmin+simbox->getlz()*simbox->getAvgRelThick()*0.5,
                       zmin,zmax);
  LogKit::LogFormatted(LogKit::Low,"  Interval thickness    avg / min / max    : %7.1f /%7.1f /%7.1f\n",
                       simbox->getlz()*simbox->getAvgRelThick(),
                       simbox->getlz()*simbox->getMinRelThick(),
                       simbox->getlz());
  LogKit::LogFormatted(LogKit::Low,"  Sampling density      avg / min / max    : %7.2f /%7.2f /%7.2f\n",
                       simbox->getdz()*simbox->getAvgRelThick(),
                       simbox->getdz(),
                       simbox->getdz()*simbox->getMinRelThick());
}

// --------------------------------------------------------------------------------
double  MultiIntervalGrid::FindResolution(const Surface * top_surface,
                                          const Surface * base_surface,
                                          const Simbox  * estimation_simbox,
                                          int             n_layers) const{
  size_t nx = top_surface->GetNI();
  size_t ny = base_surface->GetNJ();

  double max_resolution = 0;

  for (size_t i = 0; i<nx; i++){
    for (size_t j = 0; j<ny; j++){
      double x,y;
      estimation_simbox->getXYCoord(i,j,x,y);
      double z_top  = top_surface->GetZ(x,y);
      double z_base = base_surface->GetZ(x,y);
      double resolution = (z_base - z_top) / n_layers;
      if (resolution > max_resolution)
        max_resolution = resolution;
    }
  }

  return max_resolution;
}