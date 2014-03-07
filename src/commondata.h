/***************************************************************************
*      Copyright (C) 2008 by Norwegian Computing Center and Statoil        *
***************************************************************************/


#ifndef COMMONDATA_H
#define COMMONDATA_H

#include "src/simbox.h"
#include "nrlib/well/well.hpp"
#include "nrlib/segy/segy.hpp"
#include "lib/utils.h"
#include "src/blockedlogscommon.h"
#include "src/tasklist.h"
#include "src/seismicstorage.h"
#include "src/background.h"
#include "src/gridmapping.h"
#include "src/analyzelog.h"
#include "src/inputfiles.h"
#include "src/modelsettings.h"
#include "src/timeline.h"

class MultiIntervalGrid;
class CravaTrend;
class BlockedLogsCommon;

class CommonData{
public:
  CommonData(ModelSettings                            * model_settings,
             InputFiles                               * input_files);

  ~ CommonData();

  enum                 gridTypes{CTMISSING, DATA, PARAMETER};

  //GET FUNCTIONS

  const Simbox                            & GetEstimationSimbox()               const { return estimation_simbox_      ;}
  const NRLib::Volume                     & GetFullInversionVolume()            const { return full_inversion_volume_  ;}
  const std::vector<NRLib::Well>          & GetWells()                          const { return wells_                  ;}
        std::vector<NRLib::Well>          & GetWells()                                { return wells_                  ;}
  const MultiIntervalGrid                 * GetMultipleIntervalGrid()           const { return multiple_interval_grid_ ;}
  MultiIntervalGrid                       * GetMultipleIntervalGrid(void)             { return multiple_interval_grid_ ;}
  //const std::vector<NRLib::Grid<double> > & GetCovParametersInterval(int i_interval);
  //const std::vector<NRLib::Grid<double> > & GetCorrParametersInterval(int i_interval);
  //const NRLib::Matrix                     & GetPriorVar0(int i_interval);

  const std::map<std::string, std::vector<DistributionsRock *> >     & GetDistributionsRock()  const { return rock_distributions_  ;}
  const std::map<std::string, std::vector<DistributionWithTrend *> > & GetReservoirVariables() const { return reservoir_variables_ ;}

  TimeLine                               * GetTimeLine()                        { return time_line_       ;}
  const std::vector<std::string>         & GetFaciesNames()               const { return facies_names_    ;}
  const std::vector<int>                 & GetFaciesNr()                  const { return facies_nr_       ;}
  const std::vector<std::vector<float> > & GetPriorFacies()               const { return prior_facies_    ;}
  const std::vector<float>               & GetPriorFaciesInterval(int i)  const { return prior_facies_[i] ;}

  const std::map<std::string, BlockedLogsCommon *> GetBlockedLogs()       const { return mapped_blocked_logs_ ;}

  std::vector<Surface *>                 & GetFaciesEstimInterval()             { return facies_estim_interval_ ;}

  std::vector<float>       & GetGravityObservationUtmxTimeLapse(int time_lapse)  { return observation_location_utmx_[time_lapse]  ;}
  std::vector<float>       & GetGravityObservationUtmyTimeLapse(int time_lapse)  { return observation_location_utmy_[time_lapse]  ;}
  std::vector<float>       & GetGravityObservationDepthTimeLapse(int time_lapse) { return observation_location_depth_[time_lapse] ;}
  std::vector<float>       & GetGravityResponseTimeLapse(int time_lapse)         { return gravity_response_[time_lapse]           ;}
  std::vector<float>       & GetGravityStdDevTimeLapse(int time_lapse)           { return gravity_std_dev_[time_lapse]            ;}

  GridMapping              * GetTimeDepthMapping()                             { return time_depth_mapping_            ;}
  bool                       GetVelocityFromInversion()                        { return velocity_from_inversion_       ;}

  bool                                    GetUseLocalNoise()                            { return use_local_noise_                            ;}
  std::map<int, std::vector<Grid2D *> > & GetLocalNoiseScale()                          { return local_noise_scale_                          ;}
  std::vector<Grid2D *>                 & GetLocalNoiseScaleTimeLapse(int time_lapse)   { return local_noise_scale_.find(time_lapse)->second ;}
  std::vector<SeismicStorage>           & GetSeismicDataTimeLapse(int time_lapse)       { return seismic_data_.find(time_lapse)->second      ;}
  std::map<int, std::vector<float> >    & GetSNRatio()                                  { return sn_ratio_                                   ;}
  std::vector<float>                    & GetSNRatioTimeLapse(int time_lapse)           { return sn_ratio_.find(time_lapse)->second          ;}

  bool                                    GetRefMatFromFileGlobalVpVs()                 { return refmat_from_file_global_vpvs_               ;}
  float **                                GetReflectionMatrixTimeLapse(int time_lapse)  { return reflection_matrix_.find(time_lapse)->second ;}

  std::vector<std::vector<float> >      & GetAngularCorrelation(int time_lapse)         { return angular_correlations_[time_lapse]           ;}

  std::vector<Wavelet *>                  & GetWavelet(int time_lapse)                  { return wavelets_.find(time_lapse)->second          ;}
  const std::vector<std::vector<double> > & GetSyntSeis(int time_lapse)                 { return synt_seis_.find(time_lapse)->second         ;}

  const std::vector<std::vector<double> > & GetTGradX()                           const { return t_grad_x_                                   ;}
  const std::vector<std::vector<double> > & GetTGradY()                           const { return t_grad_y_                                   ;}
  const NRLib::Grid2D<float>              & GetRefTimeGradX()                     const { return ref_time_grad_x_                            ;}
  const NRLib::Grid2D<float>              & GetRefTimeGradY()                     const { return ref_time_grad_y_                            ;}

  const Surface                           * GetPriorCorrXY(int i_interval)              { return prior_corr_XY_[i_interval]                  ;}
  const NRLib::Matrix                     & GetPriorParamCov(int i_interval);
  const std::vector<double>               & GetPriorCorrT(int i_interval)               {return prior_corr_T_[i_interval]                    ;}


  void  SetupDefaultReflectionMatrix(float              **& reflection_matrix,
                                     double                 vsvp,
                                     const ModelSettings  * model_settings,
                                     int                    number_of_angles,
                                     int                    this_time_lapse);

  void FillInData(NRLib::Grid<double> * grid,
                  FFTGrid             * fft_grid_new,
                  const Simbox        & simbox,
                  StormContGrid       * storm_grid,
                  const SegY          * segy,
                  FFTGrid             * fft_grid_old,
                  float                 smooth_length,
                  int                 & missing_traces_simbox,
                  int                 & missing_traces_padding,
                  int                 & dead_traces_simbox,
                  int                   grid_type,
                  bool                  scale    = false,
                  bool                  is_segy  = true,
                  bool                  is_storm = false);

  void            GetCorrGradIJ(float         & corr_grad_I,
                                float         & corr_grad_J,
                                const Simbox  * simbox) const;

  static   void ApplyFilter(std::vector<double> & log_filtered,
                   std::vector<double> & log_interpolated,
                   int                   n_time_samples,
                   double                dt_milliseconds,
                   float                 max_hz);

private:

  void LoadWellMoveInterval(const InputFiles             * input_files,
                            const Simbox                 * estimation_simbox,
                            std::vector<Surface *>       & well_move_interval,
                            std::string                  & err_text);

  bool  OptimizeWellLocations(ModelSettings                                 * model_settings,
                              InputFiles                                    * input_files,
                              const Simbox                                  * estimation_simbox,
                              std::vector<NRLib::Well>                      & wells,
                              std::map<std::string, BlockedLogsCommon *>    & mapped_blocked_logs,
                              std::map<int, std::vector<SeismicStorage> >   & seismic_data,
                              std::map<int, float **>                       & reflection_matrix,
                              std::string                                   & err_text);

  void MoveWell( NRLib::Well & well,
                const Simbox * simbox,
                double         delta_X,
                double         delta_Y,
                double         k_move);

  void  CalculateDeviation(NRLib::Well         & new_well,
                           const ModelSettings * const model_settings,
                           float               & dev_angle,
                           Simbox              * simbox);

  void CountFaciesInWell(NRLib::Well            & well,
                         Simbox                 * simbox,
                         int                      n_facies,
                         const std::vector<int> & facies_nr,
                         std::vector<int>       & facies_count);

  void GetGeometryFromGridOnFile(const std::string         grid_file,
                                 const TraceHeaderFormat * thf,
                                 SegyGeometry           *& geometry,
                                 std::string             & err_text);

  SegyGeometry * GetGeometryFromCravaFile(const std::string & file_name);

  SegyGeometry * GetGeometryFromStormFile(const std::string    & file_name,
                                          std::string          & err_text,
                                          bool                   scale = false);

  bool CreateOuterTemporarySimbox(ModelSettings           * model_settings,
                                  InputFiles              * input_files,
                                  Simbox                  & estimation_simbox,
                                  NRLib::Volume           & full_inversion_interval,
                                  std::string             & err_text);

  void WriteAreas(const SegyGeometry  * area_params,
                  Simbox              * time_simbox,
                  std::string         & err_text);

  void FindSmallestSurfaceGeometry(const double   x0,
                                   const double   y0,
                                   const double   lx,
                                   const double   ly,
                                   const double   rot,
                                   double       & x_min,
                                   double       & y_min,
                                   double       & x_max,
                                   double       & y_max);

  int GetNzFromGridOnFile(ModelSettings     * model_settings,
                          const std::string & grid_file,
                          std::string       & err_text);

  void SetSurfacesMultipleIntervals(const ModelSettings            * const model_settings,
                                    NRLib::Volume                  & full_inversion_volume,
                                    Simbox                         & estimation_simbox,
                                    int                              nz,
                                    const InputFiles               * input_files,
                                    std::string                    & err_text);

  void SetSurfacesSingleInterval(const ModelSettings              * const model_settings,
                                 NRLib::Volume                    & full_inversion_volume,
                                 Simbox                           & estimation_simbox,
                                 int                                nz,
                                 const std::vector<std::string>   & surf_file,
                                 std::string                      & err_text);

  bool ReadSeismicData(ModelSettings  * modelSettings,
                       InputFiles     * inputFiles,
                       std::string    & err_text);

  bool ReadWellData(ModelSettings                   * model_settings,
                    Simbox                          * estimation_simbox,
                    InputFiles                      * input_files,
                    std::vector<std::string>          & log_names,
                    const std::vector<std::string>  & log_names_from_user,
                    const std::vector<bool>         & inverse_velocity,
                    bool                              facies_log_given,
                    std::string                     & err_text);

  bool BlockWellsForEstimation(const ModelSettings                            * const model_settings,
                               const Simbox                                   & estimation_simbox,
                               const MultiIntervalGrid                        * multiple_interval_grid,
                               std::vector<NRLib::Well>                       & wells,
                               std::map<std::string, BlockedLogsCommon *>     & mapped_blocked_logs_common,
                               std::map<std::string, BlockedLogsCommon *>     & mapped_blocked_logs_for_correlation,
                               std::string                                    & err_text);

  bool RemoveDuplicateLogEntriesFromWell(NRLib::Well   & well,
                                         ModelSettings * model_settings,
                                         const Simbox  * simbox,
                                         int           & n_merges);

  void MergeCells(const std::string         & name,
                  std::vector<double>       & pos_resampled,
                  const std::vector<double> & pos,
                  int                         ii,
                  int                         istart,
                  int                         iend,
                  bool                        print_to_screen);

  void MergeCellsDiscrete(const std::string      & name,
                          std::vector<int>       & log_resampled,
                          const std::vector<int> & log,
                          int                      ii,
                          int                      istart,
                          int                      iend,
                          bool                     print_to_screen);

  void SetWrongLogEntriesInWellUndefined(NRLib::Well   & well,
                                         ModelSettings * model_settings,
                                         int           & count_vp,
                                         int           & count_vs,
                                         int           & count_rho);

  void LookForSyntheticVsLog(NRLib::Well   & well,
                             ModelSettings * model_settings,
                             float         & rank_correlation);

  void FilterLogs(NRLib::Well   & well,
                  ModelSettings * model_settings);

  bool ResampleTime(std::vector<double>       & time_resampled,
                    const std::vector<double> & z_pos,
                    int                         nd,
                    double                    & dt);

  void ResampleLog(std::vector<double>       & log_resampled,
                   const std::vector<double> & log,
                   const std::vector<double> & time,
                   const std::vector<double> & time_resampled,
                   int                         nd,
                   double                      dt);

  void InterpolateLog(std::vector<double>       & log_interpolated,
                      const std::vector<double> & log_resampled,
                      int                         nd);

  void CutWell(std::string           well_file_name,
               NRLib::Well         & well,
               const NRLib::Volume & full_inversion_volume);

  void ProcessLogsNorsarWell(NRLib::Well                      & new_well,
                             std::vector<std::string>         & log_names_from_user,
                             const std::vector<bool>          & inverse_velocity,
                             bool                               facies_log_given,
                             std::string                      & error_text);

  void ProcessLogsRMSWell(NRLib::Well                     & new_well,
                          std::vector<std::string>        & log_names_from_user,
                          const std::vector<bool>         & inverse_velocity,
                          bool                              facies_log_given,
                          std::string                     & error_text);

  bool  SetupReflectionMatrix(ModelSettings  * model_settings,
                              InputFiles     * input_files,
                              std::string    & err_text);

  bool  SetupTemporaryWavelet(ModelSettings * model_settings,
                              std::string   & err_text);

  bool  WaveletHandling(ModelSettings * model_settings,
                        InputFiles    * input_files,
                        std::string   & err_text);

  bool       CheckThatDataCoverGrid(const SegY   * segy,
                                    float          offset,
                                    const Simbox & simbox,
                                    float          guard_zone,
                                    std::string &  err_text) const;

  bool       CheckThatDataCoverGrid(StormContGrid * stormgrid,
                                    const Simbox  & simbox,
                                    float           guard_zone,
                                    std::string   & err_text,
                                    bool            scale = false) const;

  void ProcessLogsNorsarWell(NRLib::Well                  & new_well,
                             std::string                  & error_text,
                             bool                         & failed);

  void ProcessLogsRMSWell(NRLib::Well                     & new_well,
                          std::string                     & error_text,
                          bool                            & failed);

  void ReadFaciesNamesFromWellLogs(NRLib::Well              & well,
                                   std::vector<int>         & facies_nr,
                                   std::vector<std::string> & facies_names);

  void SetFaciesNamesFromWells(ModelSettings            *& model_settings,
                               std::string               & err_text);

  void GetMinMaxFnr(int            & min,
                    int            & max,
                    const int        n_facies,
                    std::vector<int> facies_nr);


  float  ** ReadMatrix(const std::string                  & file_name,
                       int                                  n1,
                       int                                  n2,
                       const std::string                  & read_reason,
                       std::string                        & err_text);

  int Process1DWavelet(const ModelSettings                      * modelSettings,
                       const InputFiles                         * inputFiles,
                       const SeismicStorage                     * seismic_data,
                       std::map<std::string, BlockedLogsCommon *> mapped_blocked_logs,
                       const std::vector<Surface *>             & waveletEstimInterval,
                       const float                              * reflection_matrix,
                       std::vector<double>                      & synt_seic,
                       std::string                              & err_text,
                       Wavelet                                 *& wavelet,
                       Grid2D                                  *& local_noise_scale,
                       Grid2D                                  *& local_noise_shift,
                       Grid2D                                  *& local_noise_estimate,
                       unsigned int                               i_timelapse,
                       unsigned int                               j_angle,
                       const float                                angle,
                       float                                    & sn_ratio,
                       bool                                       estimate_wavlet,
                       bool                                       use_ricker_wavelet,
                       bool                                       use_local_noise);

  int Process3DWavelet(const ModelSettings                      * model_settings,
                       const InputFiles                         * input_files,
                       const SeismicStorage                     * seismic_data,
                       std::map<std::string, BlockedLogsCommon *> mapped_blocked_logs,
                       const std::vector<Surface *>             & wavelet_estim_interval,
                       const float                              * reflection_matrix,
                       std::string                              & err_text,
                       Wavelet                                 *& wavelet,
                       unsigned int                               i_timelapse,
                       unsigned int                               j_angle,
                       float                                      angle,
                       float                                      sn_ratio,
                       const NRLib::Grid2D<float>               & ref_time_grad_x,
                       const NRLib::Grid2D<float>               & ref_time_grad_y,
                       const std::vector<std::vector<double> >  & t_grad_x,
                       const std::vector<std::vector<double> >  & t_grad_y,
                       bool                                       estimate_wavelet);

  void FindWaveletEstimationInterval(InputFiles             * input_files,
                                     std::vector<Surface *> & wavelet_estim_interval,
                                     std::string            & err_text);

  void ComputeStructureDepthGradient(double                 v0,
                                     double                 radius,
                                     const Surface        * t0_surf,
                                     const Surface        * correlation_direction,
                                     NRLib::Grid2D<float> & structure_depth_grad_x,
                                     NRLib::Grid2D<float> & structure_depth_grad_y);

  void ComputeReferenceTimeGradient(const Surface       * t0_surf,
                                    NRLib::Grid2D<float> &ref_time_grad_x,
                                    NRLib::Grid2D<float> &ref_time_grad_y);

  void CalculateSmoothGrad(const Surface * surf, double x, double y, double radius, double ds,  double& gx, double& gy);


  void ResampleSurfaceToGrid2D(const Surface * surface,
                               Grid2D        * outgrid);

  int  GetWaveletFileFormat(const std::string & fileName, std::string & errText);

  void ReadAndWriteLocalGridsToFile(const std::string   & fileName,
                                    const std::string   & type,
                                    const float           scaleFactor,
                                    const ModelSettings * modelSettings,
                                    const Grid2D        * grid,
                                    const float           angle);

  void ResampleGrid2DToSurface(const Simbox   * simbox,
                               const Grid2D   * grid,
                               Surface       *& surface);

  bool SetupTrendCubes(ModelSettings                     * model_settings,
                       InputFiles                        * input_files,
                       MultiIntervalGrid                 * multiple_interval_grid,
                       std::string                       & error_text);

  bool SetupRockPhysics(const ModelSettings                                 * model_settings,
                        const InputFiles                                    * input_files,
                        const MultiIntervalGrid                             * multiple_interval_grid,
                        const std::vector<CravaTrend>                       & trend_cubes,
                        const std::map<std::string, BlockedLogsCommon *>    & mapped_blocked_logs,
                        int                                                   n_trend_cubes,
                        std::string                                         & error_text);

  void PrintExpectationAndCovariance(const std::vector<double>   & expectation,
                                     const NRLib::Grid2D<double> & covariance,
                                     const bool                  & has_trend) const;

  bool EstimateWaveletShape();

  bool SetupPriorFaciesProb(ModelSettings                               * model_settings,
                            InputFiles                                  * input_files,
                            std::string                                 & err_text);

  void FindFaciesEstimationInterval(InputFiles             * input_files,
                                    std::vector<Surface *> & facies_estim_interval,
                                    std::string            & err_text);

  void CheckFaciesNamesConsistency(ModelSettings     *& model_settings,
                                   const InputFiles   * input_files,
                                   std::string        & tmp_err_text) const;

  void SetFaciesNamesFromRockPhysics();

  void ReadPriorFaciesProbCubes(const InputFiles                                 * input_files,
                                ModelSettings                                    * model_settings,
                                std::vector<std::vector<NRLib::Grid<double> *> > & prior_facies_prob_cubes,
                                const std::vector<Simbox>                        & interval_simboxes,
                                std::string                                      & err_text);

  static FFTGrid  * CreateFFTGrid(int nx,
                                  int ny,
                                  int nz,
                                  int nxp,
                                  int nyp,
                                  int nzp,
                                  bool file_grid);

  void ReadGridFromFile(const std::string                  & file_name,
                        const std::string                  & par_name,
                        const float                          offset,
                        std::vector<NRLib::Grid<double> *> & grid,
                        const SegyGeometry                *& geometry,
                        const TraceHeaderFormat            * format,
                        int                                  grid_type,
                        const std::vector<Simbox>          & interval_simboxes,
                        const Simbox                       & estimation_simbox,
                        const ModelSettings                * model_settings,
                        std::string                        & err_text,
                        bool                                 nopadding = true);

  //void ReadCravaFile(NRLib::Grid<double> & grid,
  //                   const std::string   & file_name,
  //                   std::string         & err_text);

  void GetZPaddingFromCravaFile(const std::string & file_name,
                                std::string       & err_text,
                                int               & nz_pad);

  void ReadSegyFile(const std::string                  & file_name,
                    std::vector<NRLib::Grid<double> *> & interval_grids,
                    const std::vector<Simbox>          & interval_simboxes,
                    const Simbox                       & simbox,
                    const ModelSettings                * model_settings,
                    const SegyGeometry                *& geometry,
                    int                                  grid_type,
                    const std::string                  & par_name,
                    float                                offset,
                    const TraceHeaderFormat            * format,
                    std::string                        & err_text,
                    bool                                 nopadding = true);

  int GetFillNumber(int i, int n, int np);

  int FindClosestFactorableNumber(int leastint);

  void SmoothTraceInGuardZone(std::vector<float> & data_trace,
                              float                dz_data,
                              float                smooth_length);

  void ResampleTrace(const std::vector<float> & data_trace,
                     const rfftwnd_plan       & fftplan1,
                     const rfftwnd_plan       & fftplan2,
                     fftw_real                * rAmpData,
                     fftw_real                * rAmpFine,
                     int                        cnt,
                     int                        rnt,
                     int                        cmt,
                     int                        rmt);

  void InterpolateGridValues(std::vector<float> & grid_trace,
                             float                z0_grid,
                             float                dz_grid,
                             fftw_real          * rAmpFine,
                             float                z0_data,
                             float                dz_fine,
                             int                  n_fine,
                             int                  nz,
                             int                  nzp);

  void InterpolateAndShiftTrend(std::vector<float>       & interpolated_trend,
                                float                      z0_grid,
                                float                      dz_grid,
                                const std::vector<float> & trend_long,
                                float                      z0_data,
                                float                      dz_fine,
                                int                        n_fine,
                                int                        nz,
                                int                        nzp);

  int GetZSimboxIndex(int k,
                      int nz,
                      int nzp);

  void SetTrace(const std::vector<float> & trace,
                NRLib::Grid<double>      * grid,
                size_t                     i,
                size_t                     j);

  void SetTrace(float                 value,
                NRLib::Grid<double> * grid,
                size_t                i,
                size_t                j);

  void SetTrace(const std::vector<float> & trace,
                FFTGrid                  * grid,
                size_t                     i,
                size_t                     j);

  void SetTrace(float     value,
                FFTGrid * grid,
                size_t    i,
                size_t    j);

  void ReadStormFile(const std::string                  & file_name,
                     std::vector<NRLib::Grid<double> *> & interval_grids,
                     const int                            grid_type,
                     const std::string                  & par_name,
                     const std::vector<Simbox>          & interval_simboxes,
                     const ModelSettings                * model_settings,
                     std::string                        & err_text,
                     bool                                 is_storm = true,
                     bool                                 nopadding = true);

  bool SetupDepthConversion(ModelSettings  * model_settings,
                            InputFiles     * input_files,
                            std::string    & err_text);

  bool SetupBackgroundModel(ModelSettings  * model_settings,
                            InputFiles     * input_files,
                            std::string    & err_text);

  double FindMeanVsVp(const NRLib::Grid<double> * vp,
                      const NRLib::Grid<double> * vs);

  void SetUndefinedCellsToGlobalAverageGrid(NRLib::Grid<double> * grid,
                                            const double          avg);

  void SubtractGrid(NRLib::Grid<double>       * to_grid,
                    const NRLib::Grid<double> * from_grid);

  void ChangeSignGrid(NRLib::Grid<double> * grid);

  void LoadVelocity(NRLib::Grid<double>  * velocity,
                    const Simbox         & interval_simbox,
                    const ModelSettings  * model_settings,
                    const std::string    & velocity_field,
                    bool                 & velocity_from_inversion,
                    std::string          & err_text);

  std::map<std::string, DistributionsRock *> GetRockDistributionTime0() const;

  void GenerateRockPhysics3DBackground(const std::vector<DistributionsRock *> & rock_distribution,
                                       const std::vector<float>               & probability,
                                       NRLib::Grid<double>                    * vp,
                                       NRLib::Grid<double>                    * vs,
                                       NRLib::Grid<double>                    * rho,
                                       Simbox                                 & simbox,
                                       const CravaTrend                       & trend_cube);

  void SetupExtendedBackgroundSimbox(Simbox   * simbox,
                                     Surface  * corr_surf,
                                     Simbox  *& bg_simbox,
                                     int        output_format,
                                     int        output_domain,
                                     int        other_output);

  void SetupExtendedBackgroundSimbox(Simbox   * simbox,
                                     Surface  * top_corr_surf,
                                     Surface  * base_corr_surf,
                                     Simbox  *& bg_simbox,
                                     int        output_format,
                                     int        output_domain,
                                     int        other_output);

  bool SetupPriorCorrelation(const ModelSettings                                          * model_settings,
                             const InputFiles                                             * input_files,
                             const std::vector<NRLib::Well>                               & wells,
                             const std::map<std::string, BlockedLogsCommon *>             & mapped_blocked_logs_for_correlation,
                             const std::vector<Simbox>                                    & interval_simboxes,
                             const std::vector<std::vector<float> >                       & prior_facies_prob,
                             const std::vector<std::string>                               & facies_names,
                             const std::vector<CravaTrend>                                & trend_cubes,
                             const std::map<int, std::vector<SeismicStorage> >            & seismic_data,
                             const std::vector<std::vector<NRLib::Grid<double> *> >       & background,
                             std::string                                                  & err_text);

  void  CalculateCovarianceFromRockPhysics(const std::vector<DistributionsRock *>           & rock_distribution,
                                             const std::map<std::string, float>               & probability,
                                             const std::vector<std::string>                   & facies_names,
                                             const CravaTrend                                 & trend_cubes,
                                             NRLib::Matrix                                    & param_cov,
                                             std::string                                      & err_txt);

  void  CalculateCovarianceInTrendPosition(const std::vector<DistributionsRock *> & rock_distribution,
                                           const std::vector<float>               & probability,
                                           const std::vector<double>              & trend_position,
                                           NRLib::Grid2D<double>                  & sigma_sum) const;

  void EstimateXYPaddingSizes(Simbox          * interval_simbox,
                              ModelSettings   * model_settings) const;

  void ValidateCovarianceMatrix(float               ** C,
                                 const ModelSettings *  model_settings,
                                 std::string         &  err_txt);

  Surface * FindCorrXYGrid(const Simbox           * time_simbox,
                           const ModelSettings    * model_settings) const;

  bool  SetupTimeLine(ModelSettings * model_settings,
                      InputFiles    * input_files,
                      std::string   & err_text_common);

  bool  SetupGravityInversion(ModelSettings * model_settings,
                              InputFiles    * input_files,
                              std::string   & err_text_common);

  void  ReadGravityDataFile(const std::string   & file_name,
                            const std::string   & read_reason,
                            int                   n_obs,
                            int                   n_columns,
                            std::vector <float> & obs_loc_utmx,
                            std::vector <float> & obs_loc_utmy,
                            std::vector <float> & obs_loc_depth,
                            std::vector <float> & gravity_response,
                            std::vector <float> & gravity_std_dev,
                            std::string         & err_text);

  //void  SetUpscaledPaddingSize(ModelSettings * model_settings);

  //int   SetPaddingSize(int original_nxp, int upscaling_factor);

  bool  SetupTravelTimeInversion(ModelSettings * model_settings,
                                 InputFiles    * input_files,
                                 std::string   & err_text_common);

  void  ProcessHorizons(std::vector<Surface>   & horizons,
                        const InputFiles       * input_files,
                        std::string            & err_text,
                        bool                   & failed,
                        int                      i_timelapse);

  void CheckCovarianceParameters(NRLib::Matrix            & param_cov);

  void  WriteFilePriorVariances(const ModelSettings      * model_settings,
                               const std::vector<double> & prior_corr_T,
                               const Surface            * prior_corr_XY,
                               const float              & dt) const;

  void  PrintPriorVariances() const;

  void ReadAngularCorrelations(ModelSettings * model_settings,
                               std::string   & err_text);

  bool optimizeWellLocations();

  int ComputeTime(int year, int month, int day) const;

  // CLASS VARIABLES ---------------------------------------------------

  // Bool variables indicating whether the corresponding data processing
  // succeeded
  bool outer_temp_simbox_;
  bool read_seismic_;
  bool read_wells_;
  bool block_wells_;
  bool setup_reflection_matrix_;
  bool temporary_wavelet_;
  bool optimize_well_location_;
  bool wavelet_handling_;
  bool setup_multigrid_;
  bool setup_trend_cubes_;
  bool setup_estimation_rock_physics_;
  bool setup_prior_facies_probabilities_;
  bool setup_depth_conversion_;
  bool setup_background_model_;
  bool setup_prior_correlation_;
  bool setup_timeline_;
  //bool prior_corr_estimation_;
  bool setup_gravity_inversion_;
  bool setup_traveltime_inversion_;

  MultiIntervalGrid                           * multiple_interval_grid_;
  Simbox                                        estimation_simbox_;
  NRLib::Volume                                 full_inversion_volume_;

  std::map<int, std::vector<SeismicStorage> >   seismic_data_; //Map timelapse

  // Well logs
  std::vector<std::string>                      log_names_;
  std::vector<NRLib::Well>                      wells_;

  // Blocked well logs
  std::map<std::string, BlockedLogsCommon *>    mapped_blocked_logs_;                 ///< Blocked logs with estimation simbox
  std::map<std::string, BlockedLogsCommon *>    mapped_blocked_logs_for_correlation_; ///< Blocked logs for estimation of vertical corr
  std::vector<std::string>                      continuous_logs_to_be_blocked_;       ///< Continuous logs that should be blocked
  std::vector<std::string>                      discrete_logs_to_be_blocked_;         ///< Discrete logs that should be blocked

  // Trend cubes and rock physics
  int                                           n_trend_cubes_;

  std::map<std::string, std::vector<DistributionsRock *> >     rock_distributions_;     ///< Rocks used in rock physics model
  std::map<std::string, std::vector<DistributionWithTrend *> > reservoir_variables_;    ///< Reservoir variables used in the rock physics model

  // prior facies
  std::vector<std::vector<float> >                   prior_facies_;                  ///< Prior facies probabilities
  std::vector<Surface *>                             facies_estim_interval_;

  // Timeline
  TimeLine                                         * time_line_;

  bool                                               forward_modeling_;

  std::map<int, float **>                            reflection_matrix_; //Map timelapse
  bool                                               refmat_from_file_global_vpvs_;  //True if reflection matrix is from file or set up from global vp/vs value.

  // Wavelet
  std::map<int, std::vector<Wavelet *> >             wavelets_; //Map time_lapse, vector angles
  std::map<int, std::vector<Grid2D *> >              local_noise_scale_;
  std::map<int, std::vector<Grid2D *> >              local_shift_;
  std::map<int, std::vector<Grid2D *> >              local_scale_;
  std::map<int, std::vector<float> >                 global_noise_estimate_;
  std::map<int, std::vector<float> >                 sn_ratio_;
  bool                                               use_local_noise_;
  std::map<int, std::vector<std::vector<double> > >  synt_seis_; //Map time_lapse, vector angles

  std::vector<std::vector<double> >             t_grad_x_;
  std::vector<std::vector<double> >             t_grad_y_;
  NRLib::Grid2D<float>                          ref_time_grad_x_; ///< Time gradient in x-direction for reference time surface (t0)
  NRLib::Grid2D<float>                          ref_time_grad_y_; ///< Time gradient in x-direction for reference time surface (t0)

  std::vector<std::vector<int> >                facies_nr_wells_;               ///< Facies Numbers per well.
  std::vector<std::vector<std::string> >        facies_names_wells_;            ///< Facies Names per well
  std::vector<bool>                             facies_log_wells_;              ///< True if this well has a facies log

  std::vector<std::string>                      facies_names_;                  ///< Facies names combined for wells.
  std::vector<int>                              facies_nr_;

  // Prior correlation
  bool                                          prior_corr_per_interval_;       ///< If there is not enough data to estimate per interval, this is false
  //std::vector<NRLib::Grid<double> >             cov_params_interval_;           ///<
  //std::vector<NRLib::Grid<double> >             corr_params_interval_;
  std::vector<Surface *>                        prior_corr_XY_;
  std::vector<NRLib::Matrix>                    prior_param_cov_;
  std::vector<std::vector<double> >             prior_corr_T_;
  //std::vector<NRLib::Grid<double> >             prior_cov_; //Vp, vs, rho
  //std::vector<std::vector<NRLib::Grid<double> > > prior_corr_; //Vp-vs, Vp-Rho, Vs-Rho

  std::vector<Wavelet*>                         temporary_wavelets_;            ///< Wavelet per angle

  //Gravimetry parameters per timelapse
  std::vector<std::vector<float> >              observation_location_utmx_;     ///< Vectors to store observation location coordinates
  std::vector<std::vector<float> >              observation_location_utmy_;
  std::vector<std::vector<float> >              observation_location_depth_;
  std::vector<std::vector<float> >              gravity_response_;              ///< Vector to store base line gravity response
  std::vector<std::vector<float> >              gravity_std_dev_;               ///< Vector to store base line gravity standard deviation

  //Traveltime parameters per timelapse
  std::vector<std::vector<Surface> >            horizons_;                      ///< Horizons used for horizon inversion
  std::vector<NRLib::Grid<double> *>            rms_data_;                      ///< RMS data U^2

  //Depth conversion
  GridMapping                                 * time_depth_mapping_;
  bool                                          velocity_from_inversion_;

  //Angular correlations
  std::vector<std::vector<std::vector<float> > > angular_correlations_;

};
#endif
