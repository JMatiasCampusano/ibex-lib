/*
 * ibex_PSOSwarm.cpp
 *
 *  Created on: 21-12-2017
 *      Author: cduartes
 */

#include "ibex_PSOSwarm.h"

using namespace std;
namespace ibex{
	bool PSOSwarm::trace = false;

	/*
	 * Initialize swarm
	 * Instance particles and set parameters
	 */
	PSOSwarm::PSOSwarm(System* orig_sys, double c1, double c2, int nParticles, int limit) : c1(c1), c2(c2), gBest(Vector::zeros(orig_sys->box.size())){
		this->nParticles = nParticles;
		particlesArray = new PSOParticle*[nParticles];
		this->limit = limit;
		this->orig_sys = orig_sys;
	}

	/*
	 * Execute PSO algorithm
	 */
	Vector PSOSwarm::executePSO(double p){
		double fitness;
		int iterations = 0;
		bool exit = false;
		if (trace) cout << "Particles: " << nParticles << endl;
		if (trace) cout << "Iterations limit: " << limit << endl;
		if (trace) cout << "Box's diameter ponderator (p): " << p << endl;
		if (trace) cout << "Cognitive parameter (c1): " << c1 << endl;
		if (trace) cout << "Social parameter (c2): " << c2 << endl;

		// ** Initialize particles on random places**
		if (trace) cout << "\033[0;33m# Initialize population of particles with random position and velocity" << endl;
		for(int i=0; i<nParticles; i++){
			particlesArray[i] = new PSOParticle(orig_sys, c1, c2);
			particlesArray[i]->calculateFitness(orig_sys);
			if( gBest != (Vector::zeros(orig_sys->box.size())) ){
				selectParticle(particlesArray[i]);
			}else{
				gBest = particlesArray[i]->getPosition();
				if (trace) cout << "\033[0;32mgBest fitness["<< getGBestCountViolations() <<"]:" << getGBestValue() << "\t First" << endl;
				if (trace) cout << "\033[0;31mAt: " << gBest << endl;
			}
		}

		// ** Iterations **
		if (trace) cout << "\033[0;33m# ITERATIONS" << endl;
		if (trace) startPlot();				// create file to plot with python
		do{
			if (trace) iterationPlot();		//dump iteration to file
			iterations++;
			for(int i=0; i<nParticles; i++){
				// # Update velocity and position of every particle.
				particlesArray[i]->updateVelocityAndPosition(orig_sys,gBest,c1,c2, p);

				// # Evaluate objective (fitness) of every particle.
				particlesArray[i]->calculateFitness(orig_sys);
				// - Select best position of particle and save it internally.
				particlesArray[i]->selectBestInternal();
				// # Select the particle with best fitness (minimize) and save as gBest.
				selectParticle(particlesArray[i]);

			}
		}while(exit == false && iterations < limit);
		if (trace) cout << "\033[0;33m# END ITERATIONS" << endl;
		if (trace) cout << "\033[0mvBest["<< getGBestValue() <<"] violBest["<< getGBestPenalty() <<"]" << endl;
		if (trace) closePlot();				// close file
		return getGBestPosition();
	}

	/*
	 * Select best particle of the swarm based on Feasibility rule (K. Deb - 2000)
	 */
	void PSOSwarm::selectParticle(PSOParticle* particle){
		// both feasible: select better fitness.
		if(isGBestFeasible() == true && particle->isFeasible() == true){
			if(particle->getValue() < getGBestValue()){
				gBest = particle->getPosition();	//update globalBest particle
				if (trace) cout << "\033[0;32mgBest fitness["<< getGBestPenalty() <<"]:" << getGBestValue() << "\t Criteria[1]" << endl;
				if (trace) cout << "\033[0;31mAt: " << getGBestPosition() << endl;
			}
		}else{
			// only one is feasible: select feasible.
			if(isGBestFeasible() == false && particle->isFeasible() == true){
				gBest = particle->getPosition();	//update globalBest particle
				if (trace) cout << "\033[0;32mgBest fitness["<< getGBestPenalty() <<"]:" << getGBestValue() << "\t Criteria[2]" << endl;
				if (trace) cout << "\033[0;31mAt: " << getGBestPosition() << endl;
			}else{
				// both infeasible: select better fitness.
				if(isGBestFeasible() == false && particle->isFeasible() == false){
					if(particle->getPenalty() < getGBestPenalty()){
						gBest = particle->getPosition();	//update globalBest particle
						if (trace) cout << "\033[0;32mgBest fitness["<< getGBestPenalty() <<"]:" << getGBestValue() << "\t Criteria[3]" << endl;
						if (trace) cout << "\033[0;31mAt: " << getGBestPosition() << endl;
					}
				}
			}
		}
	}

	void PSOSwarm::startPlot(){
		output.open("output.txt");
	}

	void PSOSwarm::iterationPlot(){
		for(int i=0; i < nParticles ; i++){
			output << particlesArray[i]->getPosition()[0] << "," << particlesArray[i]->getPosition()[1] << ";";
		}
		output << getGBestPosition()[0] << "," << getGBestPosition()[1] << endl;
	}

	void PSOSwarm::closePlot(){
		output.close();
		//system("python3 plot.py");
	}

	/*
	 * GETTER AND SETTER
	 */
	Vector PSOSwarm::getGBestPosition(){
		return gBest;
	}

	double PSOSwarm::getGBestValue(){
		 Interval eval = orig_sys->goal->eval(gBest);
		 return eval.ub();
	}

	double PSOSwarm::getGBestPenalty(){
		double sum = 0;
		for(int i=0; i<orig_sys->ctrs.size();i++){
			Interval eval = orig_sys->ctrs[i].f.eval(gBest);
			if((orig_sys->ctrs[i].op == LEQ || orig_sys->ctrs[i].op == LT) && eval.ub()>0.0){
				sum += eval.ub();
			}
			else if((orig_sys->ctrs[i].op == GEQ || orig_sys->ctrs[i].op == GT) && eval.lb()<0.0){
				sum += -eval.ub();
			}
			else if(orig_sys->ctrs[i].op == EQ ){
				sum += abs(eval).ub();
			}
		}

		return sum;
	}

	int PSOSwarm::getGBestCountViolations(){
		double countViolations = 0;
		for(int i=0; i<orig_sys->ctrs.size();i++){
			Interval eval = orig_sys->ctrs[i].f.eval(gBest);
			if((orig_sys->ctrs[i].op == LEQ || orig_sys->ctrs[i].op == LT) && eval.ub()>0.0){
				countViolations++;
			}
			else if((orig_sys->ctrs[i].op == GEQ || orig_sys->ctrs[i].op == GT) && eval.lb()<0.0){
				countViolations++;
			}
			else if(orig_sys->ctrs[i].op == EQ ){
				countViolations++;
			}
		}
		return countViolations;
	}

	bool PSOSwarm::isGBestFeasible(){
		if (getGBestCountViolations() > 0)
			return false;
		else
			return true;
	}

	PSOSwarm::~PSOSwarm() {
		for(int i=0; i<nParticles; i++){
			delete particlesArray[i];
		}
		delete this;
	}
}