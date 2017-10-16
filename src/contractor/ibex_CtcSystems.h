/*
 * ibex_CtcSystems.h
 *
 *  Created on: 13-10-2017
 *      Author: victor
 */

#ifndef SRC_CONTRACTOR_IBEX_CTCSYSTEMS_H_
#define SRC_CONTRACTOR_IBEX_CTCSYSTEMS_H_
#include <list>
#include <map>
#include <set>

#include "ibex_Ctc.h"
#include "ibex_Linearizer.h"
#include "ibex_CtcPolytopeHull.h"
#include "ibex_CtcDAG.h"
#include "ibex_Conditioners.h"
#include "ibex_ExprNode2Dag.h"


#define CAST(m,n) dynamic_cast<const m*>(n)

using namespace std;

namespace ibex {

/**
 * \brief A contractor using a linear system A.x=b
 *
 * Linear system of the form A.x=b.
 *
 * If P exists, the contractor use it for contracting x by using the system PA.x=Pb
 * and a simple projection based method
 * If P does not exist the contractor perform at most 2n simplex for contracting x
 */

class LinearSystem : public Linearizer, Ctc {
public:


	enum {SIMPLEX, PSEUDOINVERSE, GAUSS_JORDAN, GAUSS_PSEUDOINV, MULT_GAUSS_JORDAN};

	CtcPolytopeHull ctc;
	int ctc_type;


	/**
	 * \brief Create the linear inequalities Ax<=b.
	 */
	LinearSystem(int nb_var, int ctc_type = SIMPLEX) :
		Linearizer(nb_var),
		ctc(*this, LinearSolver::default_max_iter,
		LinearSolver::default_max_time_out, LinearSolver::default_eps, Interval (1e-14, 1e10)),
		ctc_type(ctc_type), Ctc(nb_var), A(1,1), b(1), P(1,1), PA(1,1), Pb(1) {

	}

	/**
	 * \brief Create the linear inequalities Ax<=b.
	 */
	LinearSystem(const IntervalMatrix& A, const IntervalMatrix& P, const IntervalMatrix& PA,  int ctc_type = GAUSS_JORDAN) :
		Linearizer(A.nb_cols()), ctc(*this, LinearSolver::default_max_iter,
		LinearSolver::default_max_time_out, LinearSolver::default_eps, Interval (1e-14, 1e10)),
		ctc_type(ctc_type), Ctc(A.nb_cols()), A(A), b(1), P(P), PA(PA), Pb(1) {

	}


	void contract(IntervalVector& box);

	/**
	 * \brief Add the inequalities in the solver (does not perform a linearization)
	 *
	 * This method is required by the PolytopeHull
	 */
	int linearization(const IntervalVector& x, LinearSolver& lp_solver);




protected:

	IntervalMatrix A;
	IntervalVector b;

	IntervalMatrix P;
	IntervalMatrix PA;
	IntervalVector Pb;

};


class EmbeddedLinearSystem : public LinearSystem {
public:

	/**
	 * All but SIMPLEX
	 */
	EmbeddedLinearSystem(const IntervalMatrix& A, const IntervalMatrix& P, const IntervalMatrix& PA,
					Array<const ExprNode>& xn, Array<const ExprNode>& bn, map<const ExprNode*, int>& node2i,
					ExprDomain& d, bool is_mult=false, bool extended=false) :
						LinearSystem(A, P, PA), xn(xn), bn(bn), node2i(node2i), d(d),
							is_mult(is_mult), x(xn.size()), extended(extended)  {

			b.resize(A.nb_rows());
			if(extended)
				b=Vector::zeros(A.nb_rows());
		}

		EmbeddedLinearSystem(const IntervalMatrix& AA, Array<const ExprNode>& xn,
					Array<const ExprNode>& bn, map<const ExprNode*, int>& node2i,
					ExprDomain& d, bool is_mult=false, int ctc_type = SIMPLEX, bool extended=false) :

			LinearSystem(AA.nb_cols()+((extended)? bn.size():0), ctc_type), xn(xn), bn(bn), node2i(node2i), d(d),
			is_mult(is_mult), x(xn.size()), extended(extended) {

			A=AA;
			b.resize(bn.size());
			if(extended){
				Matrix I = Matrix::diag(-Vector::ones(bn.size()));
				A.resize(AA.nb_rows(), AA.nb_cols()+bn.size());
				A.put(0,AA.nb_cols(),I);
				this->xn.add(bn);
				x.resize(this->xn.size());
				b=Vector::zeros(bn.size());
			}
		   if(ctc_type == PSEUDOINVERSE){
			   pseudoinverse(A.mid(), P);
			   PA=P*A;
		   }
		   else if(ctc_type == GAUSS_PSEUDOINV ){
			   //A is the matrix A after performing gauss elimination
			   //A <-- G*A : A is a diagonal matrix
			   //A = G*b
			   bool exist;
			   exist = pseudoinverse(A.mid(), P);
			   if (exist)
					PA=P*A;
			   else{
				   PA=A;
				   P = gauss_jordan(PA);
			   }
			   // after this: PA*x = P*b
		   }else if(ctc_type == GAUSS_JORDAN ){
			   PA=A;
			   P = gauss_jordan(PA);
		   }
		   cout.precision(3);
		}


	void contract(IntervalVector& box);
	int linearize(const IntervalVector& x, LinearSolver& lp_solver);

	bool is_mult;
	bool extended;


private:

	Array<const ExprNode> xn; // size:nb_cols
	Array<const ExprNode> bn; // size:nb_rows
	IntervalVector x;

	map<const ExprNode*, int> node2i;
	ExprDomain& d;

};


class EmbeddedLinearSystemBuilder {

public:

	EmbeddedLinearSystemBuilder(const IntervalMatrix& A, Array<const ExprNode>& xn,
			Array<const ExprNode>& bn) : A(A), xn(xn), bn(bn), PA(NULL), P(NULL), node2i(NULL), d(NULL), is_mult(false), ctc_type(-1), extended(false) { }

	void set_P(Matrix* P2);
	void set_PA(IntervalMatrix* PA2);

	void set_node2i(map<const ExprNode*, int>* n) { node2i=n; }
	void set_domain(ExprDomain* dd) { d=dd; }
	void set_is_mult(bool m) { is_mult=m; }
	void set_ctc_type(int t) { ctc_type=t; }
	void set_extended(bool e) { extended=e; }


	EmbeddedLinearSystem* create(){
		if(PA && P && node2i && d && ctc_type!=-1) return new EmbeddedLinearSystem(A, *P, *PA, xn, bn, *node2i, *d, is_mult, extended);
		else if(node2i && d && ctc_type!=-1) return new EmbeddedLinearSystem(A, xn, bn, *node2i, *d, is_mult, ctc_type, extended);
		return NULL;
	}

	inline Array<const ExprNode>& get_bn() { return bn;}
	inline Array<const ExprNode>& get_xn() { return xn;}


    inline IntervalMatrix& getA() { return A; }


private:
	IntervalMatrix A;
	IntervalMatrix* P;
	IntervalMatrix* PA;

	Array<const ExprNode> xn; // size:nb_cols
	Array<const ExprNode> bn; // size:nb_rows

	map<const ExprNode*, int>* node2i;
	ExprDomain* d;

	bool is_mult;
	int ctc_type;
	bool extended;
};

	void create_subsystems(list<EmbeddedLinearSystemBuilder *> &ls_list, IntervalMatrix& A, Array<const ExprNode> &x, Array<const ExprNode> &b,
		vector<pair <set <int>,set <int> > >& subsets);


	void find_subsystems(list<EmbeddedLinearSystemBuilder *> &ls_list, IntervalMatrix& A, Array<const ExprNode> &x,
		Array<const ExprNode> &b, int& nb_rows, int& nb_cols);



	template<typename T>
	void add_row(IntervalMatrix& A, Array<const ExprNode> &b, const T* bi, map<const ExprNode*, int>& xmap,
		int& nb_rows, int& nb_cols);

	template<typename T>
		Array<Ctc> getEmbeddedLinearSystems(CtcDag& dag_ctc, bool is_mult, int ctc_type, bool extended, int nb_nodes=-1){

			Function& f = dag_ctc.get_f();
			ExprDomain& d  = dag_ctc.getDomains();

			map<const ExprNode*, int> node2i;

			list<EmbeddedLinearSystemBuilder *> ls_list;
			Array<Ctc> linear_systems;

			IntervalMatrix A(1,1);
			Array<const ExprNode> x; // size:nb_cols
			Array<const ExprNode> b; // size:nb_rows
			int nb_rows=0;
			int nb_cols=0;

			map<const ExprNode*, int> xmap;
			set<const T*> bmap;

		    if(nb_nodes==-1) nb_nodes=f.nb_nodes();

			for(int i=0;i<f.nb_nodes(); i++){
				const ExprNode* n = &f.node(i);
				node2i[n]=i;


				if(CAST(T, n) && i<=nb_nodes){

					for(int i=0; i<n->fathers.size(); i++){
						if(CAST(T, &n->fathers[i])) continue;

						const T* bi;
						if(! CAST(ExprConstant, &CAST(T,n)->left) )
							bi=CAST(T,n);
						else if (! CAST(ExprConstant,& CAST(T,n)->right) &&
								CAST(T, & CAST(T,n)->right) )
							bi= CAST(T, & CAST(T,n)->right) ;
						else break;

		                if(bmap.find(bi)==bmap.end()   ){
		                   bmap.insert(bi);
						   add_row(A, b, bi, xmap, nb_rows , nb_cols);
		                }

						break;
					}
				}
			}

			x.resize(xmap.size());

			for(map<const ExprNode*, int>::iterator it =xmap.begin(); it!=xmap.end();it++){
				x.set_ref(it->second,*node2i.find(it->first)->first);
			}

			find_subsystems(ls_list, A, x, b, nb_rows , nb_cols);

			list<EmbeddedLinearSystemBuilder *>::iterator it;
			for(it=ls_list.begin(); it!=ls_list.end(); it++){
				(*it)->set_node2i(&node2i);
				(*it)->set_ctc_type(ctc_type);
				(*it)->set_domain(&d);
				(*it)->set_is_mult(is_mult);
				(*it)->set_extended(extended);


		        if(ctc_type==LinearSystem::MULT_GAUSS_JORDAN){

		    		if(extended){
		    			Vector a= -Vector::ones((*it)->get_bn().size());
		    			Matrix I = Matrix::diag(a);
		    			int nb_rows=(*it)->getA().nb_rows();
		    			int nb_cols=(*it)->getA().nb_cols();
		    			(*it)->getA().resize(nb_rows, nb_cols+(*it)->get_bn().size());
		    			(*it)->getA().put(0,nb_cols,I);
		    			(*it)->get_xn().add((*it)->get_bn());
		    		}

		    		list<Matrix> Ps;
		    		list<IntervalMatrix> PAs;

		    		gauss_jordan_collection((*it)->getA(),Ps, PAs);

		    		list<Matrix>::iterator itP=Ps.begin();
		    		list<IntervalMatrix>::iterator itPA=PAs.begin();

		    		for(;itP!=Ps.end();itP++, itPA++){
		    			(*it)->set_P(&(*itP));
		    			(*it)->set_PA(&(*itPA));
		    			linear_systems.add(*(*it)->create());
		    		}
					(*it)->set_P(NULL);
					(*it)->set_PA(NULL);

		        }else{

		        	try{
		        		linear_systems.add(*(*it)->create());
		        	}catch(SingularMatrixException &e) {cout << "SingularMatrixException" << endl;}
		        }

			}
			return linear_systems;
		}


}


#endif /* SRC_CONTRACTOR_IBEX_CTCSYSTEMS_H_ */
