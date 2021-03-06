//============================================================================
//                                  I B E X                                   
// File        : ibex_DefaultOptimizer.h
// Author      : Gilles Chabert, Bertrand Neveu
// Copyright   : IMT Atlantique (France)
// License     : See the LICENSE file
// Created     : Aug 27, 2012
// Last Update : Jul 25, 2017
//============================================================================

#ifndef __IBEX_DEFAULT_OPTIMIZER_H__
#define __IBEX_DEFAULT_OPTIMIZER_H__

#include "ibex_Optimizer.h"
#include "ibex_CtcCompo.h"

namespace ibex {

/**
 * \ingroup optim
 *
 * \brief Default optimizer.
 */
class DefaultOptimizer : public Optimizer {
public:
	/**
	 * \brief Create a default optimizer.
	 *
	 * \param sys         - The system to optimize.
	 * \param eps_x       - Stopping criterion for box splitting (absolute precision).
	 * \param rel_eps_f   - Relative precision on the objective.
	 * \param abs_eps_f   - Absolute precision on the objective.
	 * \param eps_h       - Equality thickness.
	 * \param rigor       - If true, feasibility of equalities is certified. By default:
	 *                      false.
	 * \param inHC4       - If true, feasibility is also tried with LoupFinderInHC4.
	 * \param random_seed - The sequence of random numbers is reinitialized with
	 *                      this seed before calling optimize(..) (useful for
	 *                      reproducibility). Set by default to #default_random_seed.
	 */
    DefaultOptimizer(const System& sys,
    		double eps_x=Optimizer::default_eps_x,
    		double rel_eps_f=Optimizer::default_rel_eps_f,
			double abs_eps_f=Optimizer::default_abs_eps_f,
			double eps_h=NormalizedSystem::default_eps_h,
			bool rigor=false, bool inHC4=true,
			double random_seed=default_random_seed);

	/**
	 * \brief Delete *this.
	 */
    ~DefaultOptimizer();

	/** Default random seed: 1.0. */
	static const double default_random_seed;

private:

    /**
     * The contractor: HC4 + acid(HC4) + X-Newton
     */
	Ctc& ctc(const System& ext_sys);

	void* data; // keep track of data, for memory cleanup
};

} // end namespace ibex

#endif // __IBEX_DEFAULT_OPTIMIZER_H__
