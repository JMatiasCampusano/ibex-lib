//                                  I B E X                                   
// File        : ibex_Optimizer.cpp
// Author      : Gilles Chabert, Bertrand Neveu
// Copyright   : Ecole des Mines de Nantes (France)
// License     : See the LICENSE file
// Created     : May 14, 2012
// Last Update : December 24, 2012
//============================================================================

#include "ibex_Optimizer-BS.h"
#include "ibex_EmptyBoxException.h"
#include "ibex_EmptySystemException.h"
#include "ibex_Timer.h"
//#include "ibex_OptimProbing.cpp_"
//#include "ibex_OptimSimplex.cpp_"
#include "ibex_CtcFwdBwd.h"
#include "ibex_CtcOptimShaving.h"
#include "ibex_CtcHC4.h"

#include "ibex_ExprCopy.h"
#include "ibex_Function.h"
#include "ibex_NoBisectableVariableException.h"
//#include "ibex_Multipliers.h"
#include "ibex_PdcFirstOrder.h"

#include <float.h>
#include <stdlib.h>

#include <tr1/unordered_set>

using namespace std;
using namespace std::tr1;
namespace ibex {

//~ const double OptimizerBS::default_prec = 1e-07;
//~ const double OptimizerBS::default_goal_rel_prec = 1e-07;
//~ const double OptimizerBS::default_goal_abs_prec = 1e-07;
//~ const int    OptimizerBS::default_sample_size = 10;
//~ const double OptimizerBS::default_equ_eps = 1e-08;
//~ const double OptimizerBS::default_loup_tolerance = 0.1;
//~ template class CellSet<minLB>;


OptimizerBS::~OptimizerBS() {
	if (is_inside) {
		for (int i=0; i<m; i++) {
			delete &(is_inside->list[i]);
		}

		delete is_inside;
	}
	bufferset.flush();
	bufferset_dfs.flush();

	delete mylp;
	//	delete &(objshaver->ctc);
	//	delete objshaver;
}


void OptimizerBS::update_uplo() {
	double new_uplo=POS_INFINITY;

	if (! bufferset.empty()) {
		new_uplo= bufferset.top()->box[ext_sys.goal_var()].lb();
		if (new_uplo > loup) {
			cout << " loup = " << loup << " new_uplo=" << new_uplo << endl;
			ibex_error("optimizer: new_uplo>loup (please report bug)");
		}
		if (new_uplo < uplo_of_epsboxes) uplo = new_uplo;
		else uplo= uplo_of_epsboxes;
	}
	else if (bufferset.empty() && loup != POS_INFINITY) {
		// empty buffer : new uplo is set to ymax (loup - precision) if a loup has been found
		new_uplo=compute_ymax(); // not new_uplo=loup, because constraint y <= ymax was enforced
		//    cout << " new uplo buffer empty " << new_uplo << " uplo " << uplo << endl;

		double m = minimum(new_uplo, uplo_of_epsboxes);
		if (uplo < m) uplo = m; // warning: hides the field "m" of the class
		// note: we always have uplo <= uplo_of_epsboxes but we may have uplo > new_uplo, because
		// ymax is strictly lower than the loup.
	}

}

void OptimizerBS::report() {

	if (timeout >0 &&  time >=timeout ) {
		cout << "time limit " << timeout << "s. reached " << endl;
	}
	// No solution found and optimization stopped with empty buffer  before the required precision is reached => means infeasible problem
	if (bufferset.empty() && uplo_of_epsboxes == POS_INFINITY && (loup==POS_INFINITY || (loup==initial_loup && goal_abs_prec==0 && goal_rel_prec==0))) {
		cout << " infeasible problem " << endl;
		cout << " cpu time used " << time << "s." << endl;
		cout << " number of cells " << nb_cells << endl;
	}

	else {
		cout << " best bound in: [" << uplo << "," << loup << "]" << endl;

		double rel_prec;

		if (loup==POS_INFINITY)
			rel_prec= POS_INFINITY;
		else
			rel_prec=(loup-uplo)/(fabs (loup))-1.e-15;

		double abs_prec=loup-uplo-1.e-15;

		cout << " Relative precision obtained on objective function: " << rel_prec << " " <<
				(rel_prec <= goal_rel_prec? " [passed]" : " [failed]") << "  " << goal_rel_prec <<  endl;

		cout << " Absolute precision obtained on objective function: " << abs_prec << " " <<
				(abs_prec <= goal_abs_prec? " [passed]" : " [failed]") << "  " << goal_abs_prec << endl;
		if (uplo_of_epsboxes != NEG_INFINITY && uplo_of_epsboxes != POS_INFINITY)
			cout << " precision on variable domains obtained " << prec << " "   << " uplo_of_epsboxes " << uplo_of_epsboxes << endl;
		else if (uplo_of_epsboxes == NEG_INFINITY)
			cout << " small boxes with negative infinity objective :  objective not bound " << endl;
		if (loup==initial_loup)
			cout << " no feasible point found " << endl;
		else
			cout << " best feasible point " << loup_point << endl;


		cout << " cpu time used " << time << "s." << endl;
		cout << " number of cells " << nb_cells << endl;
	}
	/*   // statistics on upper bounding
    if (trace) {
      cout << " nbrand " << nb_rand << " nb_inhc4 " << nb_inhc4 << " nb simplex " << nb_simplex << endl;
      cout << " diam_rand " << diam_rand << " diam_inhc4 " << diam_inhc4 << " diam_simplex " << diam_simplex << endl;
    }
	 */
}

void OptimizerBS::report_perf() {

	double rel_prec;
	if (loup==POS_INFINITY)
		rel_prec= POS_INFINITY;
	else
		rel_prec=(loup-uplo)/(fabs(loup))-1.e-15;

	double abs_prec=loup-uplo-1.e-15;

	cout << (	((rel_prec <= goal_rel_prec)||
			(abs_prec <= goal_abs_prec)||
			((bufferset.empty() && uplo_of_epsboxes == POS_INFINITY && loup==POS_INFINITY))||
			(uplo<-1.e300)
	)? " T & " : " F & " );

	cout << uplo << " & " << loup << " & ";
	cout <<  time << "  "<< endl ;
}


OptimizerBS::OptimizerBS(System& user_sys, Ctc& ctc, Bsc& bsc, double prec,
					 double goal_rel_prec, double goal_abs_prec, int sample_size, double equ_eps,
					 bool rigor, double N, int max_deadends) : Optimizer(user_sys, ctc, bsc, prec,
					 goal_rel_prec, goal_abs_prec, sample_size, equ_eps,
					 rigor,0), N(N), max_deadends(max_deadends), bufferset(), 	bufferset_dfs()
				 {
					if(N==0.0) 
						SELNODE_STRATEGY=RESTART_DFS_PLUS;
					else
						SELNODE_STRATEGY=BEAM_SEARCH;
}

void OptimizerBS::contract_and_bound(Cell& c, const IntervalVector& init_box) {

	/*======================== contract y with y<=loup ========================*/
	Interval& y=c.box[ext_sys.goal_var()];

	//	IntervalVector tmp_box(n);
	//	read_ext_box(c.box,tmp_box);

	double ymax;
	if (loup==POS_INFINITY) ymax=POS_INFINITY;
	else ymax= compute_ymax();
	//	else ymax= compute_ymax();
	/*
	if (!(y.ub() == ymax))
		  y &= Interval(NEG_INFINITY,ymax+1.e-15);
		else if (y.ub()==ymax && !(tmp_box.contains(loup_point)))
		  y = Interval(y.lb(), ymax+1.e-15);
	 */
	y &= Interval(NEG_INFINITY,ymax);
	if (y.is_empty()) {
		c.box.set_empty();
		throw EmptyBoxException();
	}

	/*================ contract x with f(x)=y and g(x)<=0 ================*/
	//cout << " [contract]  x before=" << c.box << endl;
	//cout << " [contract]  y before=" << y << endl;

	contract(c.box, init_box);

	//cout << " [contract]  x after=" << c.box << endl;
	//cout << " [contract]  y after=" << y << endl;
	// TODO: no more cell in argument here (just a box). Does it matter?
	/*====================================================================*/


	/*========================= update loup =============================*/

	IntervalVector tmp_box(n);
	read_ext_box(c.box,tmp_box);

	entailed = &c.get<EntailedCtr>();
	update_entailed_ctr(tmp_box);

	bool loup_ch=update_loup(tmp_box);

	// update of the upper bound of y in case of a new loup found
	if (loup_ch) y &= Interval(NEG_INFINITY,compute_ymax());

	loup_changed |= loup_ch;

	if (y.is_empty()) { // fix issue #44
		throw EmptyBoxException();
	}

	/*====================================================================*/
	// [gch] TODO: the case (!c.box.is_bisectable()) seems redundant
	// with the case of a NoBisectableVariableException in
	// optimize(). Is update_uplo_of_epsboxes called twice in this case?
	// (bn] NO , the NoBisectableVariableException is raised by the bisector, there are 2 different cases of a non bisected box that may cause an update of uplo_of_epsboxes
	if ((tmp_box.max_diam()<=prec && y.diam() <=goal_abs_prec) || !c.box.is_bisectable()) {
		// rem1: tmp_box and not c.box because y is handled with goal_rel_prec and goal_abs_prec
		// rem2: do not use a precision contractor here since it would make the box empty (and y==(-inf,-inf)!!)
		// rem 3 : the extended  boxes with no bisectable  domains  should be catched for avoiding infinite bisections
		update_uplo_of_epsboxes(y.lb());
		throw EmptyBoxException();
	}

	//gradient=0 contraction for unconstrained optimization ; 
	//first order test for constrained optimization (useful only when there are no equations replaced by inequalities) 
	//works with the box without the objective (tmp_box)
	firstorder_contract(tmp_box,init_box);
	// the current extended box in the cell is updated
	write_ext_box(tmp_box,c.box);
}

bool OptimizerBS::handle_cell_nopush(Cell& c, const IntervalVector& init_box ){
	try {
		    
			contract_and_bound(c, init_box);
	        c.get<CellBS>().lb=c.box[c.box.size()-1].lb();
	        c.get<CellBS>().id=nb_cells;
	        nb_cells++;
	}
	catch(EmptyBoxException&) {
		delete &c;
		return false;
	}
	return true;
}

Optimizer::Status OptimizerBS::optimize(const IntervalVector& init_box, double obj_init_bound) {

	loup=obj_init_bound;
	double old_loup=loup;
	pseudo_loup=obj_init_bound;

	uplo=NEG_INFINITY;
	uplo_of_epsboxes=POS_INFINITY;

	nb_cells=0;
	nb_simplex=0;
	diam_simplex=0;
	nb_rand=0;
	diam_rand=0;

    bufferset.flush();
	bufferset_dfs.flush();
	
	Cell* root=new Cell(IntervalVector(n+1));

	write_ext_box(init_box,root->box);

	// add data required by the bisector
	bsc.add_backtrackable(*root);

	// add data required by optimizer + Fritz John contractor
	root->add<EntailedCtr>();
	//root->add<Multipliers>();
	entailed=&root->get<EntailedCtr>();
	entailed->init_root(user_sys,sys);

    //data for the OptimizerBS
    root->add<CellBS>();

	loup_changed=false;
	loup_point=init_box.mid();
	time=0;
	Timer::start();
	
	if(handle_cell_nopush(*root,init_box)) {
		bufferset.push(root);
		if(SELNODE_STRATEGY==RESTART_DFS || SELNODE_STRATEGY==RESTART_DFS_PLUS)
			bufferset_dfs.push(root);
	}
	
	double ymax= POS_INFINITY;
	
	try {	
		while (!bufferset.empty()) {

		  //~ update_uplo();

		  if (bufferset.empty()) 
			  break;
		 
		  
			loup_changed=false;
			
			set<Cell*,minLB> SS;
			bool exploitation=true;
			
			switch(SELNODE_STRATEGY){
				case RESTART_DFS:
				case RESTART_DFS_PLUS:
				if(iter>=max_deadends){
					iter=0;
					Cell* c = bufferset.pop();
					bufferset_dfs.erase(c);
					SS.insert(c);
				    exploitation=false;
				}else{
					Cell* c = bufferset_dfs.pop();
					bufferset.erase(c);
					SS.insert(c);
					exploitation=false;
				}
				break;
				
				case BEAM_SEARCH:
				Cell* c = bufferset.pop();
				//bufferset_dfs.erase(c);
				SS.insert(c);
				exploitation=((double)rand()/(double)RAND_MAX < N);
			}


            set<Cell*,minLB> S;
            while(!SS.empty()){
			S=SS;
			SS.clear();
	 

			int deadends=0;		 
			while(!S.empty()){
				Cell *c=(*S.begin());
				if (c->box[c->box.size()-1].lb() > compute_ymax()) {S.erase(S.begin()); delete c; continue;};  

				try {

					pair<IntervalVector,IntervalVector> boxes=bsc.bisect(*c);
					pair<Cell*,Cell*> new_cells=c->bisect(boxes.first,boxes.second);			
					S.erase(S.begin());

					delete c; 
			    
					if(!exploitation){
						if(handle_cell_nopush(*new_cells.second,init_box)){
							bufferset.push(new_cells.second);
						}else{
							new_cells.second=NULL;
							iter++;
						}
					
						if(handle_cell_nopush(*new_cells.first,init_box)){
							bufferset.push(new_cells.first);
						}else{
							new_cells.first=NULL;
							iter++;
						}
						
						if(SELNODE_STRATEGY==RESTART_DFS || SELNODE_STRATEGY==RESTART_DFS_PLUS){
							//se ordenan por minLB los ultimos 2 nodos agregados a la pila
							if(SELNODE_STRATEGY==RESTART_DFS_PLUS && new_cells.first && new_cells.second )
								if(new_cells.first->get<CellBS>().lb > new_cells.second->get<CellBS>().lb){
								new_cells.first->get<CellBS>().id --;
								new_cells.second->get<CellBS>().id ++;
							}
							if( new_cells.first) bufferset_dfs.push(new_cells.first);
							if( new_cells.second) bufferset_dfs.push(new_cells.second);
						}
							
					}else{
						if(handle_cell_nopush(*new_cells.first,init_box)) SS.insert(new_cells.first); 
						else deadends++;
						if(handle_cell_nopush(*new_cells.second,init_box)) SS.insert(new_cells.second); 
						else deadends++;
					}
						  
				if (uplo_of_epsboxes == NEG_INFINITY) {
					cout << " possible infinite minimum " << endl;
					goto end;
				}
	
				time_limit_check();
			}
			
			catch (NoBisectableVariableException& ) {
				update_uplo_of_epsboxes ((c->box)[ext_sys.goal_var()].lb());
                S.erase(S.begin());
				delete c;
			}

			
		}


        //cout << SS.size() << endl;
        if(exploitation){
		  std::set<Cell *>::iterator it=SS.begin();
		  
		  if(max_deadends > deadends)
			for(int i=0;i<N && it!=SS.end();i++,it++); 
		  
		  
          for (;it!=SS.end();){
			bufferset.push(*it);
			if(SELNODE_STRATEGY==RESTART_DFS || SELNODE_STRATEGY==RESTART_DFS_PLUS)
				bufferset_dfs.push(*it);
			SS.erase(it++);
		  }
	    }


		}
		
        if (loup<old_loup) {

					// In case of a new upper bound (loup_changed == true), all the boxes
					// with a lower bound greater than (loup - goal_prec) are removed and deleted.
					// Note: if contraction was before bisection, we could have the problem
					// that the current cell is removed by contract_heap. See comments in
					// older version of the code (before revision 284).
					ymax= compute_ymax();

                   
                    if(SELNODE_STRATEGY==RESTART_DFS || SELNODE_STRATEGY==RESTART_DFS_PLUS)
						bufferset_dfs.contract_heap(ymax,ext_sys.goal_var());
					bufferset.contract_heap(ymax,ext_sys.goal_var(), true);

                    
					if (ymax <=NEG_INFINITY) {
					    cout << " infinite value for the minimum " << endl;
						goto end;
						break;
					}
					if (trace) cout << setprecision(12) << "ymax=" << ymax << " uplo= " <<  uplo<< endl;

					old_loup=loup; 
		}	

		
		if (uplo_of_epsboxes == NEG_INFINITY || ymax <=NEG_INFINITY) {cout << "uplo = neginfinity"<< endl; break;}
		update_uplo();

	  }
	}
	catch (TimeOutException& ) {
		return TIME_OUT;
	}
	
	end:

	Timer::stop();
	time+= Timer::VIRTUAL_TIMELAPSE();


	if (uplo_of_epsboxes == POS_INFINITY && (loup==POS_INFINITY || (loup==initial_loup && goal_abs_prec==0 && goal_rel_prec==0)))
		return INFEASIBLE;
	else if (loup==initial_loup)
		return NO_FEASIBLE_FOUND;
	else if (uplo_of_epsboxes == NEG_INFINITY)
		return UNBOUNDED_OBJ;
	else
		return SUCCESS;

}

		  


} // end namespace ibex

