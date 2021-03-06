//============================================================================
//                                  I B E X                                   
// File        : ibex_LinearizerCombo.cpp
// Author      : Ignacio Araya, Bertrand Neveu , Gilles Trombettoni, Jordan Ninin
// Copyright   : IMT Atlantique (France)
// License     : See the LICENSE file
// Created     : Jul 1, 2012
// Last Update : Jul 15, 2017
//============================================================================

#include "ibex_LinearizerCombo.h"

namespace ibex {

LinearizerCombo::LinearizerCombo(const System& sys, linear_mode lmode1) :
			Linearizer(sys.nb_var),
			lmode(lmode1),
			myxnewton(NULL)
#ifdef _IBEX_WITH_AFFINE_
			, myart(NULL)
#endif
			{

	switch (lmode) {
	case XNEWTON: {
		// the default corner of XNewton linear relaxation
		myxnewton = new LinearizerXTaylor(sys, LinearizerXTaylor::RELAX, LinearizerXTaylor::RANDOM_OPP, LinearizerXTaylor::HANSEN);
		break;
	}
	case TAYLOR:{
		// the default corner of XNewton linear relaxation
		myxnewton = new LinearizerXTaylor(sys, LinearizerXTaylor::RELAX, LinearizerXTaylor::RANDOM_OPP, LinearizerXTaylor::TAYLOR);
		break;
	}
	case HANSEN: {
		// the default corner of XNewton linear relaxation
		myxnewton = new LinearizerXTaylor(sys, LinearizerXTaylor::RELAX, LinearizerXTaylor::RANDOM_OPP, LinearizerXTaylor::HANSEN);
		break;
	}

#ifdef _IBEX_WITH_AFFINE_
	case ART:
	case AFFINE2: {
		myart = new LinearizerAffine2(sys);
		break;
	}
	case COMPO: {

		myart = new LinearizerAffine2(sys);
		// the default corner of XNewton linear relaxation
		myxnewton = new LinearizerXTaylor(sys, LinearizerXTaylor::RELAX, LinearizerXTaylor::RANDOM_OPP, LinearizerXTaylor::HANSEN);
		break;
	}
#else
	case ART:
		ibex_error("LinearRelaxCombo: ART mode requires \"--with-affine\" option");
		break;
	case AFFINE2:
		ibex_error("LinearRelaxCombo: AFFINE2 mode requires \"--with-affine\" option");
		break;
	case COMPO:
		ibex_error("LinearRelaxCombo: COMPO mode requires \"--with-affine\" option");
		break;
#endif
	}

}

LinearizerCombo::~LinearizerCombo() {
	if (myxnewton!=NULL) delete myxnewton;
}

/*********generation of the linearized system*********/
int LinearizerCombo::linearize(const IntervalVector& box, LinearSolver& lp_solver) {

	int cont = 0;

	switch (lmode) {
	case XNEWTON:
	case TAYLOR:
	case HANSEN:
		cont = myxnewton->linearize(box,lp_solver);
		break;

#ifdef _IBEX_WITH_AFFINE_
	case ART:
	case AFFINE2:
		cont = myart->linearize(box,lp_solver);
		break;
	case COMPO: {
		cont = myxnewton->linearize(box,lp_solver);
		if (cont!=-1) {
			int cont2 = myart->linearize(box,lp_solver);
			if (cont2==-1) cont=-1;
			else cont+=cont2;
		}
	}
#else
	case ART:
		ibex_error("LinearRelaxCombo: ART mode requires \"--with-affine\" option");
		break;
	case AFFINE2:
		ibex_error("LinearRelaxCombo: AFFINE2 mode requires \"--with-affine\" option");
		break;
	case COMPO:
		ibex_error("LinearRelaxCombo: COMPO mode requires \"--with-affine\" option");
		break;
#endif
	}
	return cont;
}

} // end namespace
