//
// Copyright (c) 2006-2007, Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
//
// Clasp is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Clasp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Clasp; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

#include <iostream>
#include <clingcon/gecodesolver.h>
#include <clasp/program_builder.h>
#include <clasp/clause.h>
#include <clasp/solver.h>
#include <clingcon/propagator.h>
#include <gecode/search.hh>
#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <exception>
#include <sstream>
#include <clingcon/cspconstraint.h>
#include <gringo/litdep.h>

//#define DEBUGTEXT

using namespace Gecode;
using namespace Clasp;
namespace Clingcon {

Gecode::IntConLevel ICL;
Gecode::IntVarBranch branchVar; //integer branch variable
Gecode::IntValBranch branchVal; //integer branch value

std::string GecodeSolver::num2name( unsigned int var)
{
    Clasp::AtomIndex& index = *s_->strategies().symTab;
    for (Clasp::AtomIndex::const_iterator it = index.begin(); it != index.end(); ++it) {
        if ( it->second.lit.var()==var) {
            if (!it->second.name.empty()) {
            return it->second.name.c_str();
        }else
            return "leer!";
            }
    }
    std::stringstream s;
    s<< var;
    return s.str();
}

GecodeSolver::GecodeSolver(bool lazyLearn, bool useCDG, bool weakAS, int numAS,
                           std::string ICLString, std::string branchVarString,
                           std::string branchValString) :
                                currentSpace_(0), lazyLearn_(lazyLearn),
                                useCDG_(useCDG), weakAS_(weakAS), numAS_(numAS),
                                dummyReason_(this)

{
        //domain_.first=INT_MIN/2+1;
        //domain_.second=INT_MAX/2-1;
        //addedDomain_ = false;

        if(ICLString == "bound") ICL = ICL_BND;
        if(ICLString == "domain") ICL = ICL_DOM;
        if(ICLString == "default") ICL = ICL_DEF;
        if(ICLString == "value") ICL = ICL_VAL;

        if(branchVarString == "NONE") branchVar = INT_VAR_NONE;
        if(branchVarString == "RND") branchVar = INT_VAR_RND;
        if(branchVarString == "DEGREE_MIN") branchVar = INT_VAR_DEGREE_MIN;
        if(branchVarString == "DEGREE_MAX") branchVar = INT_VAR_DEGREE_MAX;
        if(branchVarString == "AFC_MIN") branchVar = INT_VAR_AFC_MIN;
        if(branchVarString == "AFC_MAX") branchVar = INT_VAR_AFC_MAX;
        if(branchVarString == "MIN_MIN") branchVar = INT_VAR_MIN_MIN;
        if(branchVarString == "MIN_MAX") branchVar = INT_VAR_MIN_MAX;
        if(branchVarString == "MAX_MIN") branchVar = INT_VAR_MAX_MIN;
        if(branchVarString == "MAX_MAX") branchVar = INT_VAR_MAX_MAX;
        if(branchVarString == "SIZE_MIN") branchVar = INT_VAR_SIZE_MIN;
        if(branchVarString == "SIZE_MAX") branchVar = INT_VAR_SIZE_MAX;
        if(branchVarString == "DEGREE_MIN") branchVar = INT_VAR_SIZE_DEGREE_MIN;
        if(branchVarString == "DEGREE_MAX") branchVar = INT_VAR_SIZE_DEGREE_MAX;
        if(branchVarString == "SIZE_AFC_MIN") branchVar = INT_VAR_SIZE_AFC_MIN;
        if(branchVarString == "SIZE_AFC_MAX") branchVar = INT_VAR_SIZE_AFC_MAX;
        if(branchVarString == "REGRET_MIN_MIN") branchVar = INT_VAR_REGRET_MIN_MIN;
        if(branchVarString == "REGRET_MIN_MAX") branchVar = INT_VAR_REGRET_MIN_MAX;
        if(branchVarString == "REGRET_MAX_MIN") branchVar = INT_VAR_REGRET_MAX_MIN;
        if(branchVarString == "REGRET_MAX_MAX") branchVar = INT_VAR_REGRET_MAX_MAX;

        if(branchValString == "MIN") branchVal = INT_VAL_MIN;
        if(branchValString == "MED") branchVal = INT_VAL_MED;
        if(branchValString == "MAX") branchVal = INT_VAL_MAX;
        if(branchValString == "RND") branchVal = INT_VAL_RND;
        if(branchValString == "SPLIT_MIN") branchVal = INT_VAL_SPLIT_MIN;
        if(branchValString == "SPLIT_MAX") branchVal = INT_VAL_SPLIT_MAX;
        if(branchValString == "RANGE_MIN") branchVal = INT_VAL_RANGE_MIN;
        if(branchValString == "RANGE_MAX") branchVal = INT_VAL_RANGE_MAX;
        if(branchValString == "VALUES_MIN") branchVal = INT_VALUES_MIN;
        if(branchValString == "VALUES_MAX") branchVal = INT_VALUES_MAX;

}

GecodeSolver::~GecodeSolver()
{
	//delete litToVar from first space
	if (currentSpace_)
	{
		currentSpace_->cleanAll();
		for (std::vector<SearchSpace*>::iterator i = spaces_.begin(); i != spaces_.end(); ++i)
			delete *i;
	}

        for (std::map<int, Constraint*>::iterator i = constraints_.begin(); i != constraints_.end(); ++i)
            delete i->second;

}






const Clasp::LitVec& GecodeSolver::getAssignment() const
{
	return assignment_;
}


void GecodeSolver::propagateLiteral(const Clasp::Literal& l, int)
{
	propQueue_.push_back(l);
}

void GecodeSolver::reset()
{
	propQueue_.clear();
}

bool GecodeSolver::initialize()
{
	// convert uids to solver literal ids
	currentDL_ = 0;
	searchEngine_ = 0;
	enumerator_ = 0;

        //crashed the gdb
        //if(domain_.size())
        //{
        //    IntSet ar(3,5);
        //}


	// convert constraints to literal variables
        // create all csp variables using getAllVariables
        std::map<int, Constraint*> newConstraints;
        for (std::map<int, Constraint*>::iterator i = constraints_.begin(); i != constraints_.end(); ++i)
	{
                newConstraints.insert(std::make_pair(s_->strategies().symTab->find(i->first)->lit.var(), i->second));
                i->second->getAllVariables(this);
	}
	constraints_ = newConstraints;

        std::vector<unsigned int> vars;
        for (LParseGlobalConstraintPrinter::GCvec::iterator i = globalConstraints_.begin(); i != globalConstraints_.end(); ++i)
            for (boost::ptr_vector<GroundConstraintVec>::iterator j = i->heads_.begin(); j != i->heads_.end(); ++j)
                for (GroundConstraintVec::iterator k = j->begin(); k != j->end(); ++k)
                    k->getAllVariables(vars,this);


        //adding the default domains to the specialized domains
        combineWithDefaultDomain();
        //setting a default domain for all variables without domain
        for(std::vector<std::string>::const_iterator i = variables_.begin(); i != variables_.end(); ++i)
        {
            //unsigned int var = csps->getVariable(*i);

            if (!hasDomain(*i))
            {
                std::cerr << "Warning: No domain is given for variable " << *i << std::endl;
                domains_[*i].push_back(INT_MIN/2+1);
                domains_[*i].push_back(INT_MAX/2-1);
            }
        }




	// register all interesting variables for propagation
        for(std::map<int, Constraint*>::iterator i = constraints_.begin(); i != constraints_.end(); ++i)
	{
#ifdef DEBUGTEXT
                std::cout << "adding variable " << num2name(i->first) << std::endl;
#endif
		s_->addWatch(posLit(i->first), clingconPropagator_,0);	
                s_->addWatch(negLit(i->first), clingconPropagator_,0);


	}



	// mark used variables, to not enumerate unconstrained variables, this can be removed in the future if
	// the gringo part is fixed
//	IntToSet constraintToVariables;
//	IntToSet variableToConstraints;

//	// calculate biggest constraint number
//	int highestConstraint = 0;
//	// fill variables to constraint and constraint to variables map
//        for (std::map<int, Constraint*>::iterator i = constraints_.begin(); i != constraints_.end(); ++i)
//	{
//		highestConstraint = (i->first > highestConstraint ? i->first : highestConstraint);
//                std::vector<unsigned int> variables(i->second->getAllVariables(this));
//		for (std::vector<unsigned int>::iterator j = variables.begin(); j != variables.end(); ++j)
//		{
//			constraintToVariables[i->first].insert(*j);
//			variableToConstraints[*j].insert(i->first);
//		}
//	}

	// create CDG
	// for all constraints
        /*
	if (useCDG_)
	{
		// initialize visited map, for depth first search
		visitedConstraints_.assign(highestConstraint+1, false);
		for (IntToSet::const_iterator i = constraintToVariables.begin(); i != constraintToVariables.end(); ++i)
		{
			// for all variables of the constraint
			for (std::set<unsigned int>::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				std::set<unsigned int> constr = variableToConstraints[*j];
				for (std::set<unsigned int>::const_iterator k = constr.begin(); k != constr.end(); ++k)
				{
					if (*k != i->first)
					{
						dependencyGraph_[i->first].node_.insert(*k);
						dependencyGraph_[*k].node_.insert(i->first);
					}
				}
			}
		}
		//TODO: switch off CDG if all variable comletly depend on each other
		std::vector<unsigned int> deps;
		// find dependencies of some constraint, does not matter
		findAllDependencies(highestConstraint, deps);
		// if one constraint depends on all others, all other depend on each other
		if(deps.size() == constraints_.size())
		{
			//TODO: should not be useless, but can be seen as heuristic
		//	std::cerr << "Warning: Using CDG is useless, all constraint variables depend on each other" << std::endl;
			//TODO: after initial benchmarking for thesis has been done, switch off cdg calculation and delete dependencyGraph_
			//
			//useCDG_ = false;
			//dependencyGraph_.clear();
		}
	}
        */

	// debug output
#ifdef DEBUGTEXT
        /*
	for (std::map<unsigned int, Node>::const_iterator i = dependencyGraph_.begin(); i != dependencyGraph_.end(); ++i)
	{
		std::cout << "Constraint " << i->first << " depends on ";
		ConstraintSet x = i->second.node_;
		for (ConstraintSet::const_iterator j = x.begin(); j != x.end(); ++j)
		{
			std::cout << *j << " ";
		}
		std::cout << std::endl;
	}
        */
#endif

        std::map<unsigned int, unsigned int>* litToVar = new std::map<unsigned int, unsigned int>();
	// propagate empty constraint set, maybe some trivial constraints can be fullfilled
        currentSpace_ = new SearchSpace(this, variables_.size(), constraints_, globalConstraints_, litToVar);
        for(std::map<int, Constraint*>::iterator i = constraints_.begin(); i != constraints_.end(); ++i)
        {
            if (s_->value(i->first) != value_free)
            {
                if (s_->value(i->first) == value_true)
                {
                    assignment_.push_back(posLit(i->first));
                }
                else
                {
                    assignment_.push_back(negLit(i->first));
                }

            }
        }
        constraints_.clear();
        if(currentSpace_->status() != SS_FAILED)
	{
                Clasp::LitVec lv(currentSpace_->getAssignment(Clasp::LitVec()));
                return propagateNewLiteralsToClasp(lv);
	}
	else
		return false;
}

bool GecodeSolver::hasAnswer()
{
	// first weak answer? of solver assignment A
	delete searchEngine_;
	searchEngine_ = 0;
	asCounter_ = 0;
	// currently using a depth first search, this could be changed by options
//	searchEngine_ = new LDS<GecodeSolver::SearchSpace>(currentSpace_, sizeof(*currentSpace_), searchOptions_);
	searchEngine_ = new DFS<GecodeSolver::SearchSpace>(currentSpace_, searchOptions_);
#ifdef DEBUGTEXT
        std::cout << "created new SearchEngine_:" << searchEngine_ << std::endl;
        currentSpace_->print(variables_);std::cout << std::endl;
#endif
	if (enumerator_)
	{
		delete enumerator_;
		enumerator_ = 0;
	}
	enumerator_ = searchEngine_->next();
	if (enumerator_ != NULL)
	{
		asCounter_++;
		return true;
	}
	else
		finalConflict();
	return false;
}
/*
void GecodeSolver::findAllDependencies(unsigned int l, std::vector<unsigned int>& result)
{
	std::vector<unsigned int> stack;

	visitedConstraints_.assign(visitedConstraints_.size(), false);
	visitedConstraints_[l] = true;
	stack.push_back(l);
	// start depth first search from all literals
	while(stack.size())
	{
		unsigned int constraintVar = stack.back();
		stack.pop_back();
		// does not matter which literal (true or false)
		//SearchSpace::Value v = currentSpace_->getValueOfConstraint(Clasp::Literal(constraintVar, true));

		result.push_back(constraintVar);
		// push all children to the stack and mark them as visited
		ConstraintSet cs = dependencyGraph_[constraintVar].node_;
		for (ConstraintSet::const_iterator j = cs.begin(); j != cs.end(); ++j)
		{
			if (!visitedConstraints_[*j])
			{
				visitedConstraints_[*j] = true;
				stack.push_back(*j);
			}
		}
	}
}
*/
bool GecodeSolver::nextAnswer()
{
	assert(enumerator_);
	assert(searchEngine_);
	if (weakAS_) return false;
	if (numAS_ && asCounter_ >= numAS_) return false;

        delete enumerator_;
        enumerator_ = NULL;

	++asCounter_;
	enumerator_ = searchEngine_->next();

	return (enumerator_ != NULL);
}

void GecodeSolver::finalConflict()
{
#ifdef DEBUGTEXT
			std::cout << "final conflict detected: " << std::endl;
#endif
                        Clasp::LitVec conflict(assignment_);
#ifdef DEBUGTEXT
			std::cout << "Conflict: " << std::endl;
                        for (Clasp::LitVec::const_iterator j = conflict.begin(); j != conflict.end(); ++j)
                                std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << ", ";
			std::cout << std::endl;
#endif
			unsigned int maxDL = 0;
                        for (Clasp::LitVec::const_iterator j = conflict.begin(); j != conflict.end(); ++j)
				maxDL = std::max(s_->level(j->var()), maxDL);
			if (maxDL < s_->decisionLevel())
				s_->undoUntil(maxDL);
                        if (conflict.size()==0) // if root level conflict due to global constraints
                            conflict.push_back(posLit(0));
			s_->setConflict(conflict);
			//TODO: resolveConflict ? never experienced that case, totally untested
}

void GecodeSolver::printAnswer()
{
	assert(enumerator_);
	assert(currentSpace_);
	if (weakAS_)
                currentSpace_->print(variables_);
	else
                enumerator_->print(variables_);
}

bool GecodeSolver::propagate()
{
	//moved from solver.propagate()
#ifdef DEBUGTEXT
        std::cout << "Solver assignment size is " << s_->assignment().size() << std::endl;
        std::cout << "SOLVER DL: " << s_->decisionLevel() << std::endl;
	std::cout << "Solver Assignment before propagation: " << std::endl;
        for (Clasp::LitVec::const_iterator j = s_->assignment().begin(); j != s_->assignment().end(); ++j)
        	std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << "(" << s_->level(j->var()) << ")" << ", ";
	std::cout << std::endl;
#endif

	// remove already assigned literals, this happens if we have propagated a new literal to clasp
        Clasp::LitVec clits;
        for (Clasp::LitVec::const_iterator i = propQueue_.begin(); i != propQueue_.end(); ++i)
        	if (currentSpace_->getValueOfConstraint(*i) == SearchSpace::BFREE)
                {
                	clits.push_back(*i);
                }
	propQueue_.clear();
	if (clits.size())
	{
		// if we have a new decision level
		if (s_->decisionLevel() > currentDL_)
		{
			//register in solver for undo event
			assert(s_->decisionLevel()!=0);
                        s_->addUndoWatch(s_->decisionLevel(),clingconPropagator_);
#ifdef DEBUGTEXT
                        std::cout << "Adding UndoWatch for Level " << s_->decisionLevel() << " while in current Lvl " << currentDL_ << std::endl;
			std::cout << "Store old space under level " << currentDL_ << std::endl;
			std::cout << "Prefetched Assignment at the moment: " << std::endl;
                        for (Clasp::LitVec::const_iterator j = assignment_.begin(); j != assignment_.end(); ++j)
        	                std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << ", ";
			std::cout << std::endl;
#endif
			// store old space
			if (dl_.size() && currentDL_ == dl_.back())
			{
				delete spaces_.back();
				spaces_.back() = static_cast<SearchSpace*>(currentSpace_->clone());
				assLength_.back() = assignment_.size();
			}
			else
			{
				spaces_.push_back(static_cast<SearchSpace*>(currentSpace_->clone()));
				dl_.push_back(currentDL_);
				assLength_.push_back(assignment_.size());
			}
			currentDL_ = s_->decisionLevel();
		}

#ifdef DEBUGTEXT
		std::cout << "Cleaned up queue: " << std::endl;
                for (Clasp::LitVec::const_iterator j = clits.begin(); j != clits.end(); ++j)
	                std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << ", ";
		std::cout << std::endl;

		std::cout << "Assignment before: " << std::endl;
                for (Clasp::LitVec::const_iterator j = assignment_.begin(); j != assignment_.end(); ++j)
        	        std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << ", ";
		std::cout << std::endl;
#endif
#ifdef DEBUGTEXT
                currentSpace_->print(variables_);std::cout << std::endl;
#endif
		currentSpace_->propagate(clits, s_);

		if (currentSpace_->status() != SS_FAILED)
                {
#ifdef DEBUGTEXT
                        currentSpace_->print(variables_);std::cout << std::endl;
                        std::cout << "sucessfully propagated" << std::endl;
#endif
			// add literals from clasp to the assignment
			assignment_.insert(assignment_.end(), clits.begin(), clits.end());
                        Clasp::LitVec newAss = currentSpace_->getAssignment(assignment_);
#ifdef DEBUGTEXT
			std::cout << "Assignment after prop: " << std::endl;
                        for (Clasp::LitVec::const_iterator j = assignment_.begin(); j != assignment_.end(); ++j)
	                        std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << ", ";
			std::cout << std::endl;
			// get all propagated clits
			std::cout << "NewAssignment after prop: " << std::endl;
                        for (Clasp::LitVec::const_iterator j = newAss.begin(); j != newAss.end(); ++j)
        	                std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << ", ";
			std::cout << std::endl;
			std::cout << "Solver Assignmentafter prop(): " << std::endl;
                        for (Clasp::LitVec::const_iterator j = s_->assignment().begin(); j != s_->assignment().end(); ++j)
	                        std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << "(" << s_->level(j->var()) << ")" << ", ";
			std::cout << std::endl;
#endif

			// if we have propagated new values
			if (newAss.size() > assignment_.size())
			{
				// the new derived literals
                                Clasp::LitVec newLits(newAss.begin() +  assignment_.size(), newAss.end());
#ifdef DEBUGTEXT
				std::cout << "New derived lits : " << std::endl;
                                for (Clasp::LitVec::const_iterator j = newLits.begin(); j != newLits.end(); ++j)
        	                        std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << ", ";
				std::cout << std::endl;
#endif
				// propagate new literals to clasp
				if (!propagateNewLiteralsToClasp(newLits))
				{
#ifdef DEBUGTEXT
					std::cout << "conflict detected while propagating to clasp" << std::endl;
#endif					
					return false;
				}
			}
		}
		// currentSpace_->status() == FAILED
		else
		{
#ifdef DEBUGTEXT
			std::cout << "Solver Assignment before conflict learning: " << std::endl;
                        for (Clasp::LitVec::const_iterator j = s_->assignment().begin(); j != s_->assignment().end(); ++j)
                	        std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << "(" << s_->level(j->var()) << ")" << ", ";
			std::cout << std::endl;
			std::cout << "conflict detected: " << std::endl;
#endif
			// generate conflict with all literals newly propagated from clasp
                        generateConflict(clits, assignment_.size());
                        Clasp::LitVec conflict(clits);
#ifdef DEBUGTEXT
			std::cout << "Conflict: " << std::endl;
                        for (Clasp::LitVec::const_iterator j = conflict.begin(); j != conflict.end(); ++j)
                        	std::cout << (j->sign() ? "not " : "") << num2name(j->var()) << ", ";
			std::cout << std::endl;
#endif
			unsigned int maxDL = 0;
                        for (Clasp::LitVec::const_iterator j = conflict.begin(); j != conflict.end(); ++j)
				maxDL = std::max(s_->level(j->var()), maxDL);
			if (maxDL < s_->decisionLevel())
				s_->undoUntil(maxDL);
			s_->setConflict(conflict);
			return false;
		}
		return true;
	}
	return true;
}


/* backjump to decision level: unsigned int level
   for undoLevel one has to call with: undoUntil(s_-decisionLevel() - 1)
*/
void GecodeSolver::undoUntil(unsigned int level)
{
#ifdef DEBUGTEXT
	std::cout << "Backjump to level(+1?) " << level << " from level solver: " << s_->decisionLevel() << " and intern: " << currentDL_ << std::endl;
        currentSpace_->print(variables_);std::cout << std::endl;
#endif
	level++;
#ifdef DEBUGTEXT
	std::cout << "the following dl levels are stored" << std::endl;
	for (unsigned int i = 0; i < dl_.size(); ++i)
		std::cout << dl_[i] << " ";
	std::cout << std::endl;
#endif
	if (level > currentDL_)
	{
		assert(false);
#ifdef DEBUGTEXT
		std::cout << "broken backjump, nothing to do" << std::endl;
#endif
		return;
	}

	unsigned int index;
        //bool found = false;
	for (index = 0; index < dl_.size(); ++index)
	{
		if (dl_[index]>=level)
		{
                        //found = true;
			break;
		}
	}

	// if not found, go one less
//	if (!found) // only if not 0
	if (index)
		--index;

	if (currentSpace_)
		delete currentSpace_;
	currentSpace_ = static_cast<GecodeSolver::SearchSpace*>(spaces_[index]->clone());

	//erase all spaces above
	++index;
	for (unsigned int j = index; j != spaces_.size(); ++j)
	{
		delete spaces_[j];
	}
	spaces_.resize(index);
	dl_.resize(index);
	assLength_.resize(index);

	assignment_.resize(assLength_.back());

        //currentDL_ = level-1;
        currentDL_ = dl_[index-1];
#ifdef DEBUGTEXT
	std::cout << "The decision level after backjump is " << currentDL_ << std::endl;
	std::cout << "the following dl levels are stored" << std::endl;
	for (unsigned int i = 0; i < dl_.size(); ++i)
		std::cout << dl_[i] << " ";
	std::cout << std::endl;

	std::cout << "Assignment after undo: " << std::endl;
        for (Clasp::LitVec::const_iterator i = assignment_.begin(); i != assignment_.end(); ++i)
                std::cout << (i->sign() ? "not " : "") << num2name(i->var()) << ", ";
	std::cout << std::endl;
        currentSpace_->print(variables_);std::cout << std::endl;
#endif

	return;
}

void GecodeSolver::generateReason(Clasp::LitVec& lits, const std::vector<unsigned int>& /*variables*/, unsigned int upToAssPos)
{
	// add only atoms from solver assignment
	// go only over atoms that are assigned in the gecode assignment
	// TODO: test if switching the presettings is the solution (go only over assigned gecode, even new, add only old assigned plus new derived lits from clasp== assignment_)
	// TODO: Do get the new learnt literals immediatly true ? If so, ignoring them could lead to smaller clauses
/*
	if (useCDG_)
	{
                Clasp::LitVec ret(lits);
		std::vector<unsigned int> stack;

		visitedConstraints_.assign(visitedConstraints_.size(), false);
		// mark all lits as visited and add them to the stack
                for (Clasp::LitVec::const_iterator i = lits.begin(); i != lits.end(); ++i)
		{
			visitedConstraints_[i->var()] = true;

			ConstraintSet cs = dependencyGraph_[i->var()].node_;
			for (ConstraintSet::const_iterator j = cs.begin(); j != cs.end(); ++j)
			{
				if (!visitedConstraints_[*j])
				{
					visitedConstraints_[*j] = true;
					stack.push_back(*j);
				}
			}
		}

		// start depth first search from all literals
		while(stack.size())
		{
			unsigned int constraintVar = stack.back();
			stack.pop_back();
			// does not matter which literal (true or false)
			SearchSpace::Value v = currentSpace_->getValueOfConstraint(Clasp::Literal(constraintVar, true));


			// if unassigned, test if it is one of the variables, if yes, dont add it but visit the children
			if(v==SearchSpace::BFREE)
			{
				bool found = false;
				for (std::vector<unsigned int>::const_iterator i = variables.begin(); i != variables.end(); ++i)
				{
					if (*i == constraintVar)
						found = true;
				}

				if (!found)
					continue;
			}

			// if assigned in gecode, and in assignment so far, add it
			//TODO: SearchSpace::BTRUE muss imho nicht getestet weden
			if(v==SearchSpace::BTRUE)
			{
				bool found = false;
				for (unsigned int i = 0; i < upToAssPos; ++i)
				{
					if (assignment_[i].var() == constraintVar)
					{
						found = true;
						break;
					}
				}
				if (found)
					ret.push_back(Literal(constraintVar, false));
			}
			else
			if(v==SearchSpace::BFALSE)
			{
				bool found = false;
				for (unsigned int i = 0; i < upToAssPos; ++i)
				{
					if (assignment_[i].var() == constraintVar)
					{
						found = true;
						break;
					}
				}
				if (found)
					ret.push_back(Literal(constraintVar, true));
			}

			// push all children to the stack and mark them as visited
			ConstraintSet cs = dependencyGraph_[constraintVar].node_;
			for (ConstraintSet::const_iterator j = cs.begin(); j != cs.end(); ++j)
			{
				if (!visitedConstraints_[*j])
				{
					visitedConstraints_[*j] = true;
					stack.push_back(*j);
				}
			}
		}

		return ret;
        }
        else*/
	{
   // works faster
        //Clasp::LitVec temp(lits);
//	temp.push_back(i);
        //temp.insert(temp.end(), assignment_.begin(), assignment_.begin() + upToAssPos);
        lits.insert(lits.end(), assignment_.begin(), assignment_.begin() + upToAssPos);
        //return temp;

	}

}

void GecodeSolver::generateConflict(Clasp::LitVec& lits, unsigned int upToAssPos)
{
	// very naive appraoch, all assigned constraints so far
	//return currentSpace_->getAssignment(assignment_);
        //Clasp::LitVec ret = currentSpace_->getAssignment(assignment_);
	//ret.push_back(i);
	//return ret;
	//
//	Clasp::LitVec ret(assignment_);
//	ret.push_back(i);
//	return ret;
        generateReason(lits, std::vector<unsigned int >(), upToAssPos);
}



bool GecodeSolver::propagateNewLiteralsToClasp(const Clasp::LitVec& newLits)
{
#ifdef DEBUGTEXT
	std::cout << "before propagateToClasp: " << std::endl;
	std::cout << "LitToAssignmentPosition:" << std::endl;
	for (LitToAssPosition::const_iterator i = litToAssPosition_.begin(); i != litToAssPosition_.end(); ++i)
	{
                std::cout << num2name(i->first) << " -> ";
                std::cout << i->second << std::endl;
	}
	std::cout << "assPositionToSize: " << std::endl;
	for (AssPosToSize::const_iterator i = assPosToSize_.begin(); i != assPosToSize_.end(); ++i)
	{
                std::cout << "Postion " << i->first << " lasts " << i->second << " steps." << std::endl;
	}
#endif

	unsigned int size = assignment_.size();
#ifdef DEBUGTEXT
        currentSpace_->print(variables_);std::cout << std::endl;
#endif
	if (lazyLearn_)
        {
		int counter = 1;
                for (Clasp::LitVec::const_iterator i = newLits.begin(); i != newLits.end(); ++i)
		{
			litToAssPosition_[(*i).var()] = size;
			assignment_.push_back(*i);
			assPosToSize_[size] = counter;
			++counter;
			if (!s_->force(*i, &dummyReason_))
			{
#ifdef DEBUGTEXT
                                currentSpace_->print(variables_);std::cout << std::endl;
#endif
				// add all already reasoned literals
                        /*	for (Clasp::LitVec::const_iterator x = newLits.begin(); x != i; ++x)
				{
					assignment_.push_back(*x);
					++counter;
				}
				//TODO: is this necessary, add the failed literal
				assignment_.push_back(*i);
				assPosToSize_[size] = counter;
			*/
#ifdef DEBUGTEXT
	std::cout << "After propagate to Clasp failed: " << std::endl;
	std::cout << "LitToAssignmentPosition:" << std::endl;
	for (LitToAssPosition::const_iterator i = litToAssPosition_.begin(); i != litToAssPosition_.end(); ++i)
	{
                std::cout << num2name(i->first) << " -> ";
                std::cout << i->second << std::endl;
	}
	std::cout << "assPositionToSize: " << std::endl;
	for (AssPosToSize::const_iterator i = assPosToSize_.begin(); i != assPosToSize_.end(); ++i)
	{
                std::cout << "Postion " << i->first << " lasts " << i->second << " steps." << std::endl;
	}
#endif


				return false;
			}
#ifdef DEBUGTEXT
                        currentSpace_->print(variables_);std::cout << std::endl;
#endif
		}
                //for (Clasp::LitVec::const_iterator i = newLits.begin(); i != newLits.end(); ++i)
		//	assignment_.push_back(*i);
		//assPosToSize_[size] = newLits.size();
#ifdef DEBUGTEXT
	std::cout << "After propagate to Clasp: " << std::endl;
	std::cout << "LitToAssignmentPosition:" << std::endl;
	for (LitToAssPosition::const_iterator i = litToAssPosition_.begin(); i != litToAssPosition_.end(); ++i)
	{
                std::cout << num2name(i->first) << " -> ";
                std::cout << i->second << std::endl;
	}
	std::cout << "assPositionToSize: " << std::endl;
	for (AssPosToSize::const_iterator i = assPosToSize_.begin(); i != assPosToSize_.end(); ++i)
	{
                std::cout << "Postion " << i->first << " lasts " << i->second << " steps." << std::endl;
	}
#endif

		return true;
	}
	else
	{
		ClauseCreator gc(s_);
                for (Clasp::LitVec::const_iterator i = newLits.begin(); i != newLits.end(); ++i)
		{
			std::vector<unsigned int> variables;
                        for (Clasp::LitVec::const_iterator u = newLits.begin(); u != newLits.end(); ++u)
			{
				variables.push_back(u->var());
			}
                        Clasp::LitVec reason;
                        reason.push_back(*i);
#ifdef DEBUGTEXT
                        std::cout << "GenerateReason for " << num2name(i->var()) << std::endl;
#endif
                        generateReason(reason, variables, size);
#ifdef DEBUGTEXT
                        for (Clasp::LitVec::const_iterator j = reason.begin(); j != reason.end(); ++j)
                                std::cout << num2name(j->var()) << std::endl;
		        std::cout << std::endl;
			for (unsigned int i = 0; i < getAssignment().size(); ++i)
		        {
                                std::cout << "ass[" << i << "] = " << num2name(getAssignment()[i].var()) << std::endl;
		        }
#endif
			gc.startAsserting(Constraint_t::learnt_conflict, *reason.begin());
                        for (Clasp::LitVec::const_iterator j = reason.begin() + 1; j != reason.end(); ++j)
			{
				gc.add(~(*j));
			}
			if(!gc.end())
				return false;
		}
                for (Clasp::LitVec::const_iterator i = newLits.begin(); i != newLits.end(); ++i)
			assignment_.push_back(*i);
		return true;
	}
	assert(false);
	return true;
/*
#ifdef VARIATION
	ClauseCreator gc(s_);
#endif
	bool ret = true;
        for (Clasp::LitVec::const_iterator i = newLits.begin(); i != newLits.end(); ++i)
	{
//		litToAssPosition_.insert(std::make_pair(*i, size));
		litToAssPosition_[*i] = size;
#ifdef DEBUGTEXT
		std::cout << "Should have learnt the following reason: " << std::endl;
		std::vector<unsigned int> variables;
                for (Clasp::LitVec::const_iterator u = newLits.begin(); u != newLits.end(); ++u)
		{
			variables.push_back(u->var());
		}
                Clasp::LitVec reason = generateReason(*i, variables, size);
                for (Clasp::LitVec::const_iterator j = reason.begin(); j != reason.end(); ++j)
			std::cout << (j->sign() ? "not " : "") << j->var() << ", ";
		std::cout << std::endl;
#endif

#ifdef VARIATION
	#ifndef DEBUGTEXT
		std::vector<unsigned int> variables;
                for (Clasp::LitVec::const_iterator u = newLits.begin(); u != newLits.end(); ++u)
		{
			variables.push_back(u->var());
		}
                Clasp::LitVec reason = generateReason(*i, variables, size);
	#endif
		gc.startAsserting(Constraint_t::learnt_conflict, *reason.begin());
                for (Clasp::LitVec::const_iterator j = reason.begin() + 1; j != reason.end(); ++j)
		{
			gc.add(~(*j));
		}
		if(!gc.end())
			return false;
#endif
#ifndef VARIATION
		if (!s_->force(*i, &dummyReason_))
		{
			int counter = 1;
			// add all already reasoned literals
                        for (Clasp::LitVec::const_iterator x = newLits.begin(); x != i; ++x)
			{
				assignment_.push_back(*x);
				++counter;
			}
			//TODO: is this necessary, add the failed literal
			assignment_.push_back(*i);
			assPosToSize_[size] = counter;
			return false;
		}
#endif
//		assignment_.push_back(*i); // i do need this as checker in generateReason
	}
        for (Clasp::LitVec::const_iterator i = newLits.begin(); i != newLits.end(); ++i)
		assignment_.push_back(*i);
	assPosToSize_[size] = newLits.size();

#ifdef DEBUGTEXT
	std::cout << "after propagateToClasp: " << std::endl;
	std::cout << "LitToAssignmentPosition:" << std::endl;
	for (LitToAssPosition::const_iterator i = litToAssPosition_.begin(); i != litToAssPosition_.end(); ++i)
	{
		std::cout << i->first << " -> ";
		std::cout << i->second << std::endl;
	}
	std::cout << "assPositionToSize: " << std::endl;
	for (AssPosToSize::const_iterator i = assPosToSize_.begin(); i != assPosToSize_.end(); ++i)
	{
		std::cout << "Postion " << i->first << " lasts " << i->second << " steps." << std::endl;
	}
#endif
	return true; //huh ?
*/

/*	ClauseCreator gc(s_);
        for (Clasp::LitVec::const_iterator i = newLits.begin(); i != newLits.end(); ++i)
	{
                Clasp::LitVec reason = generateReason(*i);

#ifdef DEBUGTEXT
		std::cout << "Reason: " << std::endl;
                for (Clasp::LitVec::const_iterator j = reason.begin(); j != reason.end(); ++j)
			std::cout << (j->sign() ? "not " : "") << j->var() << ", ";
		std::cout << std::endl;

//		std::cout << "Learning new constraint, and assigning first literal: ";
#endif
		gc.startAsserting(Constraint_t::learnt_conflict, *reason.begin());
#ifdef DEBUGTEXT
//		std::cout << (i->sign() ? "not " : "") << i->var() << ", ";
#endif
                for (Clasp::LitVec::const_iterator j = reason.begin() + 1; j != reason.end(); ++j)
		{
#ifdef DEBUGTEXT
//			std::cout << ((~(*j)).sign() ? "not " : "") << (~(*j)).var() << ", ";
#endif
			gc.add(~(*j));
		}
	//	gc.simplify(); // test without!!!
#ifdef DEBUGTEXT
//		std::cout << std::endl;
#endif
		if(!gc.end())
			return false;
	}

	return true;
*/
}

Clasp::ConstraintType GecodeSolver::CSPDummy::reason(const Literal& l, Clasp::LitVec& reason)
{
#ifdef DEBUGTEXT
	std::cout << "Generating Reason from Dummy" << std::endl;
	std::cout << "LitToAssignmentPosition:" << std::endl;
	for (LitToAssPosition::const_iterator i = gecode_->litToAssPosition_.begin(); i != gecode_->litToAssPosition_.end(); ++i)
	{
                std::cout << i->first << " -> ";
		std::cout << i->second << std::endl;
	}
	std::cout << "assPositionToSize: " << std::endl;
	for (AssPosToSize::const_iterator i = gecode_->assPosToSize_.begin(); i != gecode_->assPosToSize_.end(); ++i)
	{
		std::cout << "Postion " << i->first << " lasts " << i->second << " steps." << std::endl;
	}
        std::cout << "Using literal: " << l.var() << std::endl;
	std::cout << "on assignmend: " << std::endl;
	for (unsigned int i = 0; i < gecode_->getAssignment().size(); ++i)
	{
                std::cout << "ass[" << i << "] = " << gecode_->getAssignment()[i].var() << std::endl;

	}
	std::cout << std::endl;
#endif

	std::vector<unsigned int> variables;
        for (Clasp::LitVec::const_iterator i = gecode_->getAssignment().begin() + gecode_->litToAssPosition_[l.var()];
			i != gecode_->getAssignment().begin() + gecode_->litToAssPosition_[l.var()] + gecode_->assPosToSize_[gecode_->litToAssPosition_[l.var()]]; ++i)
	{
		variables.push_back(i->var());
	}
        //Clasp::LitVec temp;
        //temp.push_back(l);
        gecode_->generateReason(reason, variables, gecode_->litToAssPosition_[l.var()]);
	//remove first literal from reason
        //reason[0] = reason.back();
        //reason.pop_back();
#ifdef DEBUGTEXT
        for (Clasp::LitVec::const_iterator j = reason.begin(); j != reason.end(); ++j)
	{
                std::cout << ((*j).sign() ? "not " : "") << (*j).var() << ", ";
	}
	std::cout << std::endl;
#endif
        return Clasp::Constraint_t::learnt_other;
}


GecodeSolver::SearchSpace::SearchSpace(CSPSolver* csps, unsigned int numVar, std::map<int, Constraint*>& constraints,
                                        LParseGlobalConstraintPrinter::GCvec& gcvec,
                                        std::map<unsigned int, unsigned int>* litToVar) : Space(),
					//x_(this, numVar, Gecode::Int::Limits::min,  Gecode::Int::Limits::max),
                                        //x_(*this, numVar, domMin,  domMax),
                                        x_(*this, numVar),
                                        b_(*this, constraints.size(), 0, 1),
                                        litToVar_(litToVar)
{

    //initialize all variables with their domain
    const std::vector<std::string>& vars = csps->getVariables();
    for(std::vector<std::string>::const_iterator i = vars.begin(); i != vars.end(); ++i)
    {
        unsigned int var = csps->getVariable(*i);

        if (csps->hasDomain(*i))
        {
            IntSet is(reinterpret_cast<const int (*)[2]>(&csps->getDomain(*i)[0]), csps->getDomain(*i).size()/2);
            x_[var] = IntVar(*this, is);
        }
        else
        {
            IntSet is(reinterpret_cast<const int (*)[2]>(&csps->getDomain()[0]), csps->getDomain().size()/2);
            x_[var] = IntVar(*this, is);
        }
    }

    unsigned int counter = 0;
    for(std::map<int, Constraint*>::iterator i = constraints.begin(); i != constraints.end(); ++i)
    {
        (*litToVar_)[i->first] = counter;
        generateConstraint(csps,i->second, counter++);

        if (csps->getSolver()->value(i->first) != value_free)
        {
            Int::BoolView bv(b_[(*litToVar_)[i->first]]);
            if (csps->getSolver()->value(i->first) == value_true)
                //das geht net so ohne weiteres, kann fehlschlagen
                bv.one(*this);
            else
                bv.zero(*this);
        }


    }
    //        branch(this, x_, branchVar, branchVal);

    for (LParseGlobalConstraintPrinter::GCvec::iterator i = gcvec.begin(); i != gcvec.end(); ++i)
    {
        generateGlobalConstraint(csps, *i);
    }


    //	// set branching
    IntVarArgs iva(x_.size());


    for (int i = 0; i < x_.size(); ++i)
    {
        iva[i] = x_[i];

    }

    branch(*this, iva, branchVar, branchVal);
}

GecodeSolver::SearchSpace::SearchSpace(bool share, SearchSpace& sp) : Space(share, sp)
{
        x_.update(*this, share, sp.x_);
        b_.update(*this, share, sp.b_);
	litToVar_ = sp.litToVar_;
}

GecodeSolver::SearchSpace* GecodeSolver::SearchSpace::copy(bool share)
{
	return new GecodeSolver::SearchSpace(share, *this);
}


void GecodeSolver::SearchSpace::propagate(const Clasp::LitVec& lv, Solver* )
{
        for (Clasp::LitVec::const_iterator i = lv.begin(); i != lv.end(); ++i)
	{
		Int::BoolView bv(b_[(*litToVar_)[i->var()]]);
		if (!i->sign())
                        bv.one(*this);
		else
		{
                        bv.zero(*this);
		}
	}
}

Clasp::LitVec GecodeSolver::SearchSpace::getAssignment(const Clasp::LitVec& as)
{
	// only add new literals that are not in the as vector (some kind of assignment)
        Clasp::LitVec ret(as);
	for (std::map<unsigned int, unsigned int>::iterator i = litToVar_->begin(); i != litToVar_->end(); ++i)
	{
		Int::BoolView bv(b_[i->second]);
		if (bv.assigned())
		{
			if (bv.zero())
			{
					if (std::find(as.begin(), as.end(), Literal(i->first, true)) == as.end())
						ret.push_back(Literal(i->first, true));
			}
			else
			{
					if (std::find(as.begin(), as.end(), Literal(i->first, false)) == as.end())
						ret.push_back(Literal(i->first, false));
			}
		}
	}
	return ret;
}


void GecodeSolver::SearchSpace::cleanAll()
{
	delete litToVar_;
}


void GecodeSolver::SearchSpace::print(std::vector<std::string>& variables) const
{
	// i use ::operator due to gecode namespace bug
	for (int i = 0; i < x_.size(); ++i)
	{

                Int::IntView v(x_[i]);
                std::cout << variables[i] << "=";
                ::operator<<(std::cout, v);
                std::cout << " ";

	}
#ifdef DEBUGTEXT
	for (std::map<unsigned int, unsigned int>::iterator i = litToVar_->begin(); i != litToVar_->end(); ++i)
	{
                //std::cout << globalVar->num2name(i->first) << " ";
		Int::BoolView bv(b_[i->second]);
		if (bv.status() == Int::BoolVarImp::NONE)
	                std::cout << "NONE";
	        else
	        if (bv.status() == Int::BoolVarImp::ONE)
	                std::cout << "true";
	        else
	                std::cout << "false";
		std::cout << std::endl;

	}
#endif
}


GecodeSolver::SearchSpace::Value GecodeSolver::SearchSpace::getValueOfConstraint(const Literal& i)
{
	Int::BoolView bv(b_[(*litToVar_)[i.var()]]);
	if (bv.status() == Int::BoolVarImp::NONE)
		return BFREE;
	else
	if (bv.status() == Int::BoolVarImp::ONE)
		return BTRUE;
	else
		return BFALSE;
}


void GecodeSolver::SearchSpace::generateConstraint(CSPSolver* csps,Constraint* c, unsigned int boolvar)
{
    const GroundConstraint* x;
    const GroundConstraint* y;
    CSPLit::Type r = c->getRelation(x,y);
    Gecode::IntRelType ir;
    if (!x->isOperator())
    {
        std::swap(x,y);
        r = CSPLit::switchRel(r);
    }
    switch(r)
    {
    case CSPLit::EQUAL:
        ir = IRT_EQ;
        break;
    case CSPLit::INEQUAL:
        ir = IRT_NQ;
        break;
    case CSPLit::LOWER:
        ir = IRT_LE;
        break;
    case CSPLit::LTHAN:
        ir = IRT_LQ;
        break;
    case CSPLit::GTHAN:
        ir = IRT_GQ;
        break;
    case CSPLit::GREATER:
        ir = IRT_GR;
        break;
    case CSPLit::ASSIGN:
    default:
        assert(false);
    }

    // transform into linear relation Y*3+Z*Y >= X+3  ==>> Y*3 + Z*Y - X -3 >= 0
    if (!y->isOperator())
    {
        IntVarArgs array(x->getLinearSize() + 1);
        IntArgs args(x->getLinearSize() + 1);
        generateLinearConstraint(csps, x, args, array, 1);
        if (y->isInteger())
            array[array.size()-1] = IntVar(*this, y->getInteger(), y->getInteger());
        else
            array[array.size()-1] = x_[csps->getVariable(y->getString())];
        args[args.size()-1] = -1;
        Gecode::linear(*this, args, array, ir, 0, b_[boolvar], ICL);
        return;
    }
    else
    {
        IntVarArgs array(y->getLinearSize());
        IntArgs args(y->getLinearSize());
        IntVarArgs array2(x->getLinearSize() + array.size());
        IntArgs args2(x->getLinearSize() + args.size());
        generateLinearConstraint(csps, y, args, array, 0);
        generateLinearConstraint(csps, x, args2, array2, array.size());
        for (int i = 0; i < array.size(); ++i)
        {
            array2[i+x->getLinearSize()] = array[i];
            args2[i+x->getLinearSize()] = args[i]*-1;
        }
        Gecode::linear(*this, args2, array2, ir, 0, b_[boolvar], ICL);
        return;
    }

}

void GecodeSolver::SearchSpace::generateLinearConstraint(CSPSolver* csps, const GroundConstraint* c, IntArgs& args, IntVarArgs& array, unsigned int num)
{

        if (c->isInteger())
        {
           array[0] = IntVar(*this, c->getInteger(), c->getInteger());
           args[0] = 1;
           return;
        }
        else
        if (c->isVariable())
        {
            array[0] = x_[csps->getVariable(c->getString())];
            args[0] = 1;
            return;
        }

        const GroundConstraint* a = c->a_.get();
        const GroundConstraint* b = c->b_.get();
        GroundConstraint::Operator op = c->getOperator();

        if(op == GroundConstraint::TIMES)
        {
            if (b->isInteger())
                std::swap(a,b);
            if (a->isInteger())
            {
                if (b->isVariable())
                {
                    array[0] = x_[csps->getVariable(b->getString())];
                    args[0]  = a->getInteger();
                    return;
                }
                else // INTEGER should not be allowed and already optimized away
                    //if (b->getOperator() == GroundConstraint::OPERATOR)
                {
                    generateLinearConstraint(csps, b, args, array, num);
                    // multiply everything in brackets by integer
                    // 3 * (x+3*Y+Z) becomes 3*X + 9*Y + 3*Z
                    for (unsigned int i = 0; i < args.size() - num; ++i)
                    {
                        args[i] = args[i] * a->getInteger();
                    }
                    return;
                }
                // INTEGER should not be allowed and already optimized away
                assert(false);
            }
            else
            {
                if (b->isOperator())
                    std::swap(a,b);
                if (!a->isOperator())
                {
                    if (!b->isOperator())
                    {
                        IntVar erg(*this, Gecode::Int::Limits::min, Gecode::Int::Limits::max);
                        mult(*this, x_[csps->getVariable(b->getString())],x_[csps->getVariable(a->getString())], erg, ICL);
                        array[0] = erg;
                        args[0] = 1;
                        return;
                    }
                }
                else
                    //if (a->getType() == GroundConstraint::OPERATOR)
                {
                    if (!b->isOperator())
                    {
                        // X * (A+B*4+Y) ==> A+B*4+Y - Z == 0, Erg =  X*Z
                        IntArgs subArgs(a->getLinearSize() + 1);
                        IntVarArgs subArray(a->getLinearSize() + 1);
                        generateLinearConstraint(csps, a, subArgs, subArray, 1);
                        IntVar z(*this,  Gecode::Int::Limits::min, Gecode::Int::Limits::max);
                        subArray[subArray.size() - 1] = z;
                        subArgs[subArgs.size() - 1] = -1;
                        Gecode::linear(*this, subArgs, subArray, IRT_EQ, 0, ICL);
                        mult(*this, z,x_[csps->getVariable(b->getString())],array[0], ICL);
                        args[0] = 1;
                        return;
                    }
                    else
                        //if (b->getOperator() == GroundConstraint::OPERATOR)
                    {
                        IntArgs subArgs1(a->getLinearSize() + 1);
                        IntVarArgs subArray1(a->getLinearSize() + 1);
                        generateLinearConstraint(csps, a, subArgs1, subArray1, 1);
                        IntVar z1(*this,  Gecode::Int::Limits::min, Gecode::Int::Limits::max);
                        subArray1[subArray1.size() - 1] = z1;
                        subArgs1[subArgs1.size() - 1] = -1;
                        Gecode::linear(*this, subArgs1, subArray1, IRT_EQ, 0, ICL);

                        IntArgs subArgs2(b->getLinearSize() + 1);
                        IntVarArgs subArray2(b->getLinearSize() + 1);
                        generateLinearConstraint(csps, b, subArgs2, subArray2, 1);
                        IntVar z2(*this,  Gecode::Int::Limits::min, Gecode::Int::Limits::max);
                        subArray2[subArray2.size() - 1] = z2;
                        subArgs2[subArgs2.size() - 1] = -1;
                        Gecode::linear(*this, subArgs2, subArray2, IRT_EQ, 0, ICL);

                        mult(*this, z1,z2, array[0], ICL);
                        args[0] = 1;
                        return;

                    }
                }
            }
        }


        int minus = 1;
        if (op == GroundConstraint::MINUS)
            minus = -1;
        if (op == GroundConstraint::PLUS || op == GroundConstraint::MINUS)
        {
            if ((!a->isOperator()) && (!b->isOperator()))
            {
                if (a->isVariable())
                {
                    array[0] = x_[csps->getVariable(a->getString())];
                    args[0] = 1;
                }
                else
                    //if (a->getType() == GroundConstraint::INTEGER)
                {
                    array[0] = IntVar(*this, a->getInteger(), a->getInteger());
                    args[0] = 1;
                }

                if (b->isVariable())
                {
                    array[1] = x_[csps->getVariable(b->getString())];
                    args[1] = 1 * minus;
                }
                else
                    //if (b->getType() == GroundConstraint::INTEGER)
                {
                    array[1] = IntVar(*this, b->getInteger(), b->getInteger());
                    args[1] = 1 * minus;
                }
                return;
            }
            else
                if (!b->isOperator())
                {
                    // a is operator
                    generateLinearConstraint(csps, a, args, array, num+1);

                    if (b->isVariable())
                    {
                        array[array.size() - num - 1] = x_[csps->getVariable(b->getString())];
                        args[args.size() - num - 1] = 1 * minus;
                    }
                    else
                        //if (b->getOperator() == GroundConstraint::INTEGER)
                    {
                        array[array.size() - num - 1] = IntVar(*this, b->getInteger(), b->getInteger());
                        args[args.size() - num - 1] = 1 * minus;
                    }
                    return;
                }
                else
                    if (!a->isOperator())
                    {
                        //b is operator
                        IntArgs subArgs(b->getLinearSize());
                        IntVarArgs subArray(b->getLinearSize());
                        generateLinearConstraint(csps, b,subArgs, subArray, 0);
                        if (a->isVariable())
                        {
                            array[0] = x_[csps->getVariable(a->getString())];
                            args[0] = 1;
                        }
                        else
                            //if (a->getOperator() == GroundConstraint::INTEGER)
                        {
                            array[0] = IntVar(*this, a->getInteger(), a->getInteger());
                            args[0] = 1;
                        }
                        for (int i = 0; i < subArgs.size(); ++i)
                        {
                            array[i+1] = subArray[i];
                            args[i+1] = subArgs[i]*minus;
                        }

                    }
                    else
                        //both are operators
                    {
                        IntArgs subArgs(a->getLinearSize());
                        IntVarArgs subArray(a->getLinearSize());
                        generateLinearConstraint(csps, a, subArgs, subArray, 0);
                        generateLinearConstraint(csps, b, args, array, num+subArray.size());
                        for (int i = 0; i < subArray.size(); ++i)
                        {
                            array[array.size()- subArray.size() - num + i] = subArray[i];
                            args[args.size()- subArgs.size() - num + i] = subArgs[i] * minus;
                        }
                        return;
                    }

        }
        else
            if (op == GroundConstraint::ABS)
            {
                IntVar z(*this, 0, Gecode::Int::Limits::max);
                if (a->isOperator())
                {
                    IntArgs subArgs(a->getLinearSize() + 1);
                    IntVarArgs subArray(a->getLinearSize() + 1);
                    generateLinearConstraint(csps, a, subArgs, subArray, 1);
                    subArray[subArray.size() -1] = z;
                    subArgs[subArgs.size() -1] = -1;
                    Gecode::linear(*this, subArgs, subArray, IRT_EQ, 0, ICL);
                }
                else
                    if (a->isVariable())
                    {
                        z = x_[csps->getVariable(a->getString())];
                    }
                Gecode::abs(*this, z,array[0],ICL);
                args[0] = 1;
            }
            else
                if (op == GroundConstraint::DIVIDE)
		{
                    //Z / W
                    IntVar z(*this, Gecode::Int::Limits::min, Gecode::Int::Limits::max);
                    IntVar w(*this, Gecode::Int::Limits::min, Gecode::Int::Limits::max);
                    if (a->isOperator())
                    {
                        IntArgs subArgs(a->getLinearSize() + 1);
                        IntVarArgs subArray(a->getLinearSize() + 1);
                        generateLinearConstraint(csps, a, subArgs, subArray, 1);
                        subArray[subArray.size() -1] = z;
                        subArgs[subArgs.size() -1] = -1;
                        Gecode::linear(*this, subArgs, subArray, IRT_EQ, 0, ICL);
                    }
                    else
                        //if (a->getOperator() == GroundConstraint::VARIABLE)
                        if (a->isVariable())
			{
                            z = x_[csps->getVariable(a->getString())];
			}
			else
                            //if (a->getOperator() == GroundConstraint::INTEGER)
			{
                            z = IntVar(*this, a->getInteger(), a->getInteger());
			}

                    if (b->isOperator())
                    {
                        IntArgs subArgs(b->getLinearSize() + 1);
                        IntVarArgs subArray(b->getLinearSize() + 1);
                        generateLinearConstraint(csps, b, subArgs, subArray, 1);
                        subArray[subArray.size() -1] = w;
                        subArgs[subArgs.size() -1] = -1;
                        Gecode::linear(*this, subArgs, subArray, IRT_EQ, 0, ICL);
                    }
                    else
                        //if (b->getOperator() == GroundConstraint::VARIABLE)
                        if (b->isVariable())
			{
                            w = x_[csps->getVariable(b->getString())];
			}
			else
                            //if (b->getOperator() == GroundConstraint::INTEGER)
			{
                            w = IntVar(*this, b->getInteger(), b->getInteger());
			}

                    IntVar erg(*this,  Gecode::Int::Limits::min, Gecode::Int::Limits::max);
                    div(*this, z,w,erg, ICL);
                    array[0] = erg;
                    args[0] = 1;
                    return;
		}
}

void GecodeSolver::SearchSpace::generateGlobalConstraint(CSPSolver* csps, LParseGlobalConstraintPrinter::GC& gc)
{

    if (gc.type_==DISTINCT)
    {
        IntVarArray z(*this, gc.heads_[0].size(), Gecode::Int::Limits::min, Gecode::Int::Limits::max);

        for (size_t i = 0; i < gc.heads_[0].size(); ++i)
        {
            IntVarArgs array(gc.heads_[0][i].getLinearSize());
            IntArgs args(gc.heads_[0][i].getLinearSize());
            //TODO: special case for variables, do not need to create linear constraint
            generateLinearConstraint(csps, &gc.heads_[0][i], args, array, 0);

            Gecode::linear(*this, args, array, IRT_EQ, z[i], ICL);
        }

        Gecode::distinct(*this,z,ICL);
        return;

    }
    if (gc.type_==BINPACK)
    {
        IntVarArray l(*this, gc.heads_[0].size(), Gecode::Int::Limits::min, Gecode::Int::Limits::max);
        for (size_t i = 0; i < gc.heads_[0].size(); ++i)
        {
            IntVarArgs array(gc.heads_[0][i].getLinearSize());
            IntArgs args(gc.heads_[0][i].getLinearSize());
            //TODO: special case for variables, do not need to create linear constraint
            generateLinearConstraint(csps, &gc.heads_[0][i], args, array, 0);

            Gecode::linear(*this, args, array, IRT_EQ, l[i], ICL);
        }

        IntVarArray b(*this, gc.heads_[1].size(), Gecode::Int::Limits::min, Gecode::Int::Limits::max);
        for (size_t i = 0; i < gc.heads_[1].size(); ++i)
        {
            IntVarArgs array(gc.heads_[1][i].getLinearSize());
            IntArgs args(gc.heads_[1][i].getLinearSize());
            //TODO: special case for variables, do not need to create linear constraint
            generateLinearConstraint(csps, &gc.heads_[1][i], args, array, 0);

            Gecode::linear(*this, args, array, IRT_EQ, b[i], ICL);
        }

        IntArgs  s(gc.heads_[2].size());
        for (size_t i = 0; i < gc.heads_[2].size(); ++i)
        {
            //IntVarArgs array(gc.heads_[2][i].getLinearSize());
           // IntArgs args(gc.heads_[2][i].getLinearSize());
            //TODO: special case for variables, do not need to create linear constraint
            //generateLinearConstraint(csps, &gc.heads_[2][i], args, array, 0);

            if (!gc.heads_[2][i].isInteger())
                throw ASPmCSPException("Third argument of binpacking constraint must be a list of integers.");
            //Gecode::linear(*this, args, array, IRT_EQ, s[i], ICL);
            s[i] = gc.heads_[2][i].getInteger();
        }

        Gecode::binpacking(*this,l,b,s,ICL);
        return;

    }
    assert(false);

}

/*
IntVar GecodeSolver::SearchSpace::generateVariable(Constraint c)
{
        if (c.getType() == Constraint::VARIABLE)
	{
		return x_[c.getVar()];
	}
	else
        if (c.getType() == Constraint::OPERATOR)
	{
                Constraint* a,*b;
		IntVar v1,v2;
                Constraint::Operator op = c.getOperator(a,b);

                if (a->getType() == Constraint::VARIABLE || a->getType() == Constraint::OPERATOR)
		{
			v1 = generateVariable(*a);
		}
		else
                if (a->getType() == Constraint::INTEGER)
		{
                        v1 = IntVar(*this, a->getInteger(), a->getInteger());
		}
                if (op != Constraint::ABS)
		{
                if (b->getType() == Constraint::VARIABLE || b->getType() == Constraint::OPERATOR)
		{
			v2 = generateVariable(*b);
		}
		else
                if (b->getType() == Constraint::INTEGER)
		{
                        v2 = IntVar(*this, b->getInteger(), b->getInteger());
		}
		}


		switch(op)
		{
			// this is linear for plus/minus, but not for times, as gecode can not see if v2 is a constant or not.
                        case Constraint::PLUS:
				// does  linear(home, xy, IRT_EQ, z, icl, pk);
                                return plus(*this, v1,v2);
                        case Constraint::MINUS:
				// does  IntVarArgs xy(2); IntArgs a(2, 1,-1);
				//      xy[0]=x; xy[1]=y;
				//      linear(home, a, xy, IRT_EQ, z, icl, pk);
                                return minus(*this, v1,v2, ICL);
                        case Constraint::TIMES:
                                return mult(*this, v1,v2, ICL);
                        case Constraint::DIVIDE:
				{
                                        IntVar ret(*this, Gecode::Int::Limits::min, Gecode::Int::Limits::max);
                                        div(*this, v1,v2, ret, ICL);
					return ret;
				}
                        case Constraint::ABS:
                                return abs(*this, v1, ICL);


		}
	}
	assert(false);
}
*/

}//namespace

