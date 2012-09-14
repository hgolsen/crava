#ifndef DISTRIBUTIONSMINERAL_H
#define DISTRIBUTIONSMINERAL_H

#include "rplib/distributionssolid.h"
#include "rplib/mineral.h"

class DistributionWithTrend;
class DistributionsBrineEvolution;
class DistributionsMineralEvolution;

class DistributionsMineral : public DistributionsSolid {
public:

  DistributionsMineral(const DistributionWithTrend         * distr_k,
                       const DistributionWithTrend         * distr_mu,
                       const DistributionWithTrend         * distr_rho,
                       const DistributionWithTrend         * corr_k_mu,
                       const DistributionWithTrend         * corr_k_rho,
                       const DistributionWithTrend         * corr_mu_rho,
                             DistributionsMineralEvolution * distr_evolution = NULL);

  virtual ~DistributionsMineral();

  virtual Solid * GenerateSample(const std::vector<double> & trend_params) const;

  virtual bool                  HasDistribution() const;

  virtual std::vector<bool>     HasTrend() const;

private:
  const DistributionWithTrend         * distr_k_;         // Pointer to external object.
  const DistributionWithTrend         * distr_mu_;        // Pointer to external object.
  const DistributionWithTrend         * distr_rho_;       // Pointer to external object.
  const DistributionWithTrend         * corr_k_mu_;       // Pointer to external object.
  const DistributionWithTrend         * corr_k_rho_;      // Pointer to external object.
  const DistributionWithTrend         * corr_mu_rho_;     // Pointer to external object.
        DistributionsMineralEvolution * distr_evolution_; // Pointer to external object.
};

#endif
