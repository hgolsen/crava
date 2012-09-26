#include "nrlib/trend/trendstorage.hpp"
#include "nrlib/trend/trend.hpp"
#include "nrlib/grid/grid2d.hpp"

#include "rplib/distributionwithtrend.h"
#include "rplib/deltadistributionwithtrend.h"
#include "rplib/distributionssolid.h"
#include "rplib/distributionssolidstorage.h"
#include "rplib/distributionssolidtabulatedvelocity.h"
#include "rplib/distributionssoliddem.h"
#include "rplib/distributionssolidmix.h"
#include "rplib/distributionwithtrendstorage.h"

#include "rplib/distributionsstoragekit.h"


#include "rplib/distributionsmineral.h"
#include "distributionsmineralevolution.h"


DistributionsSolidStorage::DistributionsSolidStorage()
{
}

DistributionsSolidStorage::~DistributionsSolidStorage()
{
}

DistributionsSolid         *
DistributionsSolidStorage::CreateDistributionsSolidMix(const std::string                                        & path,
                                                       const std::vector<std::string>                           & trend_cube_parameters,
                                                       const std::vector<std::vector<double> >                  & trend_cube_sampling,
                                                       const std::map<std::string, DistributionsSolidStorage *> & model_solid_storage,
                                                       const std::vector<std::string>                           & constituent_label,
                                                       const std::vector<DistributionWithTrendStorage *>        & constituent_volume_fraction,
                                                       std::map<std::string, DistributionsSolid *>              & solid_distribution,
                                                       DEMTools::MixMethod                                        mix_method,
                                                       std::string                                              & errTxt) const {
  CheckVolumeConsistency(constituent_volume_fraction, errTxt);

  DistributionsSolid * solid = NULL;

  std::vector<DistributionsSolid*> final_distr_solid;
  std::vector<DistributionWithTrend*> final_distr_vol_frac;
  unsigned int error_count = 0;
  size_t s;
  for (s = 0; s != constituent_label.size(); ++s) {
    std::map<std::string, DistributionsSolid *>::iterator m = solid_distribution.find(constituent_label[s]);
    if (m == solid_distribution.end()) { // label not found in solid_distribution map
      std::map<std::string, DistributionsSolidStorage *>::const_iterator m_all = model_solid_storage.find(constituent_label[s]);
      if (m_all == model_solid_storage.end()) { // fatal error
        error_count++;
        errTxt += "Failed to find solid label " + constituent_label[s] + "\n";
      }
      else { //label found
        DistributionsSolidStorage     * storage     = m_all->second;
        solid                                       = storage->GenerateDistributionsSolid(path, trend_cube_parameters, trend_cube_sampling, model_solid_storage, solid_distribution, errTxt);
        solid_distribution[m_all->first]            = solid;
        final_distr_solid.push_back(solid);
      }

    }
    else { // label found
      final_distr_solid.push_back(m->second);
    }

    if (!error_count && constituent_volume_fraction[s]) {
        const DistributionWithTrend * vol_frac = constituent_volume_fraction[s]->GenerateDistributionWithTrend(path,trend_cube_parameters,trend_cube_sampling,errTxt);
        final_distr_vol_frac.push_back(const_cast<DistributionWithTrend *>(vol_frac));
    }
    else
      final_distr_vol_frac.push_back(NULL);
  } // end outer loop

  if (!error_count) {
    DistributionsSolidMixEvolution * distr_solidmix_evolution = NULL;
    solid = new DistributionsSolidMix(final_distr_solid, final_distr_vol_frac, mix_method, distr_solidmix_evolution);
  }

  return(solid);


}

//----------------------------------------------------------------------------------//
TabulatedVelocitySolidStorage::TabulatedVelocitySolidStorage(DistributionWithTrendStorage * vp,
                                                             DistributionWithTrendStorage * vs,
                                                             DistributionWithTrendStorage * density,
                                                             double                         correlation_vp_vs,
                                                             double                         correlation_vp_density,
                                                             double                         correlation_vs_density)
: vp_(vp),
  vs_(vs),
  density_(density),
  correlation_vp_vs_(correlation_vp_vs),
  correlation_vp_density_(correlation_vp_density),
  correlation_vs_density_(correlation_vs_density)
{
}

TabulatedVelocitySolidStorage::~TabulatedVelocitySolidStorage()
{
  delete vp_;
  delete vs_;
  delete density_;
}

DistributionsSolid *
TabulatedVelocitySolidStorage::GenerateDistributionsSolid(const std::string                                        & path,
                                                          const std::vector<std::string>                           & trend_cube_parameters,
                                                          const std::vector<std::vector<double> >                  & trend_cube_sampling,
                                                          const std::map<std::string, DistributionsSolidStorage *> & /*model_solid_storage*/,
                                                          std::map<std::string, DistributionsSolid *>              & /*solid_distribution*/,
                                                          std::string                                              & errTxt) const
{
  const DistributionWithTrend * vp_dist_with_trend              = vp_                    ->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt);
  const DistributionWithTrend * vs_dist_with_trend              = vs_                    ->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt);
  const DistributionWithTrend * density_dist_with_trend         = density_               ->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt);

  DistributionsSolid * solid = new DistributionsSolidTabulatedVelocity(vp_dist_with_trend,
                                                                       vs_dist_with_trend,
                                                                       density_dist_with_trend,
                                                                       correlation_vp_vs_,
                                                                       correlation_vp_density_,
                                                                       correlation_vs_density_);

  if(solid == NULL)
    errTxt += "The tabulated model has not been implemented yet for solids\n"; //Marit: Denne feilmeldingen fjernes n�r modellen er implementert

  return(solid);
}

//----------------------------------------------------------------------------------//
TabulatedModulusSolidStorage::TabulatedModulusSolidStorage(DistributionWithTrendStorage * bulk_modulus,
                                                           DistributionWithTrendStorage * shear_modulus,
                                                           DistributionWithTrendStorage * density,
                                                           double                         correlation_bulk_shear,
                                                           double                         correlation_bulk_density,
                                                           double                         correlation_shear_density)
: bulk_modulus_(bulk_modulus),
  shear_modulus_(shear_modulus),
  density_(density),
  correlation_bulk_shear_(correlation_bulk_shear),
  correlation_bulk_density_(correlation_bulk_density),
  correlation_shear_density_(correlation_shear_density)
{
}

TabulatedModulusSolidStorage::~TabulatedModulusSolidStorage()
{
  delete bulk_modulus_;
  delete shear_modulus_;
  delete density_;
}

DistributionsSolid *
TabulatedModulusSolidStorage::GenerateDistributionsSolid(const std::string                                        & path,
                                                         const std::vector<std::string>                           & trend_cube_parameters,
                                                         const std::vector<std::vector<double> >                  & trend_cube_sampling,
                                                         const std::map<std::string, DistributionsSolidStorage *> & /*model_solid_storage*/,
                                                         std::map<std::string, DistributionsSolid *>              & /*solid_distribution*/,
                                                         std::string                                              & errTxt) const
{
  const DistributionWithTrend * bulk_dist_with_trend               = bulk_modulus_              ->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt);
  const DistributionWithTrend * shear_dist_with_trend              = shear_modulus_             ->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt);
  const DistributionWithTrend * density_dist_with_trend            = density_                   ->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt);

  DistributionsMineralEvolution* p_mineral_distr_evolve = NULL;

  DistributionsSolid * solid = new DistributionsMineral(bulk_dist_with_trend,
                                                        shear_dist_with_trend,
                                                        density_dist_with_trend,
                                                        correlation_bulk_shear_,
                                                        correlation_bulk_density_,
                                                        correlation_shear_density_,
                                                        p_mineral_distr_evolve);

  return(solid);
}

//----------------------------------------------------------------------------------//
ReussSolidStorage::ReussSolidStorage(std::vector<std::string>                    constituent_label,
                                     std::vector<DistributionWithTrendStorage *> constituent_volume_fraction)
: constituent_label_(constituent_label),
  constituent_volume_fraction_(constituent_volume_fraction)
{
}

ReussSolidStorage::~ReussSolidStorage()
{
  for(int i=0; i<static_cast<int>(constituent_volume_fraction_.size()); i++) {
    if(constituent_volume_fraction_[i] && constituent_volume_fraction_[i]->GetIsShared() == false)
      delete constituent_volume_fraction_[i];
  }
}

DistributionsSolid *
ReussSolidStorage::GenerateDistributionsSolid(const std::string                                        & path,
                                              const std::vector<std::string>                           & trend_cube_parameters,
                                              const std::vector<std::vector<double> >                  & trend_cube_sampling,
                                              const std::map<std::string, DistributionsSolidStorage *> & model_solid_storage,
                                              std::map<std::string, DistributionsSolid *>              & solid_distribution,
                                              std::string                                              & errTxt) const
{
  DistributionsSolid * solid = CreateDistributionsSolidMix(path,
                                                           trend_cube_parameters,
                                                           trend_cube_sampling,
                                                           model_solid_storage,
                                                           constituent_label_,
                                                           constituent_volume_fraction_,
                                                           solid_distribution,
                                                           DEMTools::Reuss,
                                                           errTxt);
  return(solid);
}

//----------------------------------------------------------------------------------//
VoigtSolidStorage::VoigtSolidStorage(std::vector<std::string>                    constituent_label,
                                     std::vector<DistributionWithTrendStorage *> constituent_volume_fraction)
: constituent_label_(constituent_label),
  constituent_volume_fraction_(constituent_volume_fraction)
{
}

VoigtSolidStorage::~VoigtSolidStorage()
{
  for(int i=0; i<static_cast<int>(constituent_volume_fraction_.size()); i++) {
    if(constituent_volume_fraction_[i] && constituent_volume_fraction_[i]->GetIsShared() == false)
      delete constituent_volume_fraction_[i];
  }
}

DistributionsSolid *
VoigtSolidStorage::GenerateDistributionsSolid(const std::string                                        & path,
                                              const std::vector<std::string>                           & trend_cube_parameters,
                                              const std::vector<std::vector<double> >                  & trend_cube_sampling,
                                              const std::map<std::string, DistributionsSolidStorage *> & model_solid_storage,
                                              std::map<std::string, DistributionsSolid *>              & solid_distribution,
                                              std::string                                              & errTxt) const
{
  DistributionsSolid * solid = CreateDistributionsSolidMix(path,
                                                           trend_cube_parameters,
                                                           trend_cube_sampling,
                                                           model_solid_storage,
                                                           constituent_label_,
                                                           constituent_volume_fraction_,
                                                           solid_distribution,
                                                           DEMTools::Voigt,
                                                           errTxt);
  return(solid);
}

//----------------------------------------------------------------------------------//
HillSolidStorage::HillSolidStorage(std::vector<std::string>                    constituent_label,
                                   std::vector<DistributionWithTrendStorage *> constituent_volume_fraction)
: constituent_label_(constituent_label),
  constituent_volume_fraction_(constituent_volume_fraction)
{
}

HillSolidStorage::~HillSolidStorage()
{
  for(int i=0; i<static_cast<int>(constituent_volume_fraction_.size()); i++) {
    if(constituent_volume_fraction_[i] && constituent_volume_fraction_[i]->GetIsShared() == false)
      delete constituent_volume_fraction_[i];
  }
}

DistributionsSolid *
HillSolidStorage::GenerateDistributionsSolid(const std::string                                        & path,
                                             const std::vector<std::string>                           & trend_cube_parameters,
                                             const std::vector<std::vector<double> >                  & trend_cube_sampling,
                                             const std::map<std::string, DistributionsSolidStorage *> & model_solid_storage,
                                             std::map<std::string, DistributionsSolid *>              & solid_distribution,
                                             std::string                                              & errTxt) const
{
  DistributionsSolid * solid = CreateDistributionsSolidMix(path,
                                                           trend_cube_parameters,
                                                           trend_cube_sampling,
                                                           model_solid_storage,
                                                           constituent_label_,
                                                           constituent_volume_fraction_,
                                                           solid_distribution,
                                                           DEMTools::Hill,
                                                           errTxt);
  return(solid);
}

//----------------------------------------------------------------------------------//

DEMSolidStorage::DEMSolidStorage(std::string                                 host_label,
                                 DistributionWithTrendStorage *              host_volume_fraction,
                                 DistributionWithTrendStorage *              host_aspect_ratio,
                                 std::vector<std::string>                    inclusion_label,
                                 std::vector<DistributionWithTrendStorage *> inclusion_volume_fraction,
                                 std::vector<DistributionWithTrendStorage *> inclusion_aspect_ratio)
: host_label_(host_label),
  host_volume_fraction_(host_volume_fraction),
  host_aspect_ratio_(host_aspect_ratio),
  inclusion_label_(inclusion_label),
  inclusion_volume_fraction_(inclusion_volume_fraction),
  inclusion_aspect_ratio_(inclusion_aspect_ratio)
{
}

DEMSolidStorage::~DEMSolidStorage()
{
  if(host_volume_fraction_->GetIsShared() == false)
    delete host_volume_fraction_;

  if(host_aspect_ratio_->GetIsShared() == false)
    delete host_aspect_ratio_;

  for(int i=0; i<static_cast<int>(inclusion_volume_fraction_.size()); i++) {
    if(inclusion_volume_fraction_[i] && inclusion_volume_fraction_[i]->GetIsShared() == false)
      delete inclusion_volume_fraction_[i];
  }

  for(int i=0; i<static_cast<int>(inclusion_aspect_ratio_.size()); i++) {
    if(inclusion_aspect_ratio_[i] && inclusion_aspect_ratio_[i]->GetIsShared() == false)
      delete inclusion_aspect_ratio_[i];
  }
}

DistributionsSolid *
DEMSolidStorage::GenerateDistributionsSolid(const std::string                                        & path,
                                            const std::vector<std::string>                           & trend_cube_parameters,
                                            const std::vector<std::vector<double> >                  & trend_cube_sampling,
                                            const std::map<std::string, DistributionsSolidStorage *> & model_solid_storage,
                                            std::map<std::string, DistributionsSolid *>              & solid_distribution,
                                            std::string                                              & errTxt) const
{
  // Remember: Host info is included first in inclusion vectors
  int n_inclusions = static_cast<int>(inclusion_volume_fraction_.size());

  std::vector<DistributionWithTrendStorage *> volume_fractions(n_inclusions + 1);
  volume_fractions[0] = host_volume_fraction_;

  for(int i=1; i<n_inclusions+1; i++)
    volume_fractions[i] = inclusion_volume_fraction_[i-1];

  CheckVolumeConsistency(volume_fractions, errTxt);

  DistributionsSolid * solid = NULL;

  std::vector< DistributionWithTrend *> inclusion_volume_fraction_distr(inclusion_volume_fraction_.size()+1, NULL);
  std::vector< DistributionWithTrend *> inclusion_aspect_ratio_distr(inclusion_aspect_ratio_.size()+1, NULL);

  inclusion_volume_fraction_distr[0]  = host_volume_fraction_->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt);
  inclusion_aspect_ratio_distr[0]     = host_aspect_ratio_   ->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt); //NBNB fjellvoll host har alts� aspect ratio.

  for (size_t i = 1; i < inclusion_volume_fraction_.size(); ++i)
    inclusion_volume_fraction_distr[i] = inclusion_volume_fraction_[i]->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt);

  for (size_t i = 1; i < inclusion_aspect_ratio_.size(); ++i)
    inclusion_aspect_ratio_distr[i] = inclusion_aspect_ratio_[i]->GenerateDistributionWithTrend(path, trend_cube_parameters, trend_cube_sampling, errTxt);

  //Read host label
  unsigned int          error_count       = 0;
  DistributionsSolid  * final_distr_solid = NULL;
  std::map<std::string, DistributionsSolid *>::iterator m = solid_distribution.find(host_label_);
  if (m == solid_distribution.end()) { // label not found in solid_distribution map
    std::map<std::string, DistributionsSolidStorage *>::const_iterator m_all = model_solid_storage.find(host_label_);
    if (m_all == model_solid_storage.end()) { // fatal error
      error_count++;
      errTxt += "Failed to find solid label " + host_label_ + "\n";
    }
    else { //label found
      DistributionsSolidStorage     * storage     = m_all->second;
      solid                                       = storage->GenerateDistributionsSolid(path, trend_cube_parameters, trend_cube_sampling, model_solid_storage, solid_distribution, errTxt);
      solid_distribution[m_all->first]            = solid;
      final_distr_solid                           = solid;
    }
  }
  else // label found
    final_distr_solid = m->second;

  //Read inclusion label
  std::vector< DistributionsSolid* > final_distr_solid_inc;
  size_t s;
  for (s = 0; s != inclusion_label_.size(); ++s) {
    m = solid_distribution.find(inclusion_label_[s]);
    if (m == solid_distribution.end()) { // label not found in solid_distribution map
      std::map<std::string, DistributionsSolidStorage *>::const_iterator m_all = model_solid_storage.find(inclusion_label_[s]);
      if (m_all == model_solid_storage.end()) { // fatal error
        error_count++;
        errTxt += "Failed to find solid label " + inclusion_label_[s] + "\n";
      }
      else { //label found
        DistributionsSolidStorage     * storage     = m_all->second;
        solid                                       = storage->GenerateDistributionsSolid(path, trend_cube_parameters, trend_cube_sampling, model_solid_storage, solid_distribution, errTxt);
        solid_distribution[m_all->first]            = solid;
        final_distr_solid_inc.push_back(solid);
      }
    }
    else // label found
      final_distr_solid_inc.push_back(m->second);
  }

  //Questions //NBNB fjellvoll //NBNB marit
  //1.Porosity taken from where: distr_porosity
  //2.Do we support more than one inclusion?

  //Dummy "porosity"
  NRLib::Trend * trend_porosity          = new NRLib::TrendConstant(1.0);
  DistributionWithTrend * distr_porosity = new DeltaDistributionWithTrend(trend_porosity, false);

  if (!error_count) {
    DistributionsSolidDEMEvolution * distr_evolution = NULL;
    solid = new DistributionsSolidDEM(final_distr_solid,
                                      final_distr_solid_inc[0],         //tmp solution only single inclusion supported
                                      inclusion_aspect_ratio_distr,
                                      inclusion_volume_fraction_distr,
                                      distr_porosity,
                                      distr_evolution);
  }

  return(solid);
}
