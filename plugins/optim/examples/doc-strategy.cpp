//============================================================================
//                                  I B E X
// File        : doc-strategy.cpp
// Author      : Gilles Chabert
// Copyright   : Ecole des Mines de Nantes (France)
// License     : See the LICENSE file
// Created     : Apr 25, 2014
// Last Update : Apr 25, 2014
//============================================================================


#include "ibex.h"

#ifndef _IBEX_WITH_OPTIM_
#error "You need the plugin Optim to run this example."
#endif

#ifndef IBEX_BENCHS_DIR
  #define IBEX_BENCHS_DIR "../../../benchs"
#endif

using namespace std;
using namespace ibex;

/**
 * This file contains examples of the documentation.
 */

int main() {

	{
	//! [bsc-different-prec]
	double _prec[]={1e-8,1e-8,1e-4,1};

	Vector prec(4,_prec);

	RoundRobin rr(prec);
	//! [bsc-different-prec]
	}

	{
	//! [solver-parallel-C-1]

	// Get the system
	System sys1(IBEX_BENCHS_DIR "/benchs-satisfaction/benchs-IBB/ponts-geo.bch");

	// Create a copy for the second solver
	System sys2(sys1,System::COPY);

	// Precision of the solution boxes
	double prec=1e-08;

	// Create two solvers
	DefaultSolver solver1(sys1,prec);
	DefaultSolver solver2(sys2,prec);

	// Create a partition of the initial box into two subboxes,
	// by bisecting any variable (here, n°4)
	pair<IntervalVector,IntervalVector> pair=sys1.box.bisect(4);

	// =======================================================
	// Run the solvers in parallel
	// =======================================================
#pragma omp parallel sections
	{
		solver1.solve(pair.first);
#pragma omp section
		solver2.solve(pair.second);
	}
	// =======================================================

	cout << "solver #1 found " << solver1.get_manifold().size() << endl;
	cout << "solver #2 found " << solver2.get_manifold().size() << endl;
	//! [solver-parallel-C-1]

	{
	//! [solver-parallel-C-2]
	Vector eps_min(sys1.nb_var,prec);
	Vector eps_max(sys1.nb_var,POS_INFINITY);
	Solver solver1(sys1,*new CtcCompo(*new CtcHC4(sys1),*new CtcNewton(sys1.f_ctrs)), *new RoundRobin(prec), *new CellStack(), eps_min, eps_max);
	Solver solver2(sys2,*new CtcCompo(*new CtcHC4(sys2),*new CtcNewton(sys2.f_ctrs)), *new RoundRobin(prec), *new CellStack(), eps_min, eps_max);
	//! [solver-parallel-C-2]
	}


	}


}
