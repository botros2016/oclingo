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
#include <gecode/kernel/wait.hh>

//#define DEBUGTEXT

using namespace Gecode;
using namespace Clasp;
namespace Clingcon {

  class ReifWait : public Propagator {
  public:
      typedef void (GecodeSolver::* MemFun)(int);
  protected:
    /// View to wait for becoming assigned
    Int::BoolView x;
    /// Continuation to execute

    MemFun c;
    //void (*c)(Space&);
    /// Constructor for creation
    ReifWait(Space& home, Int::BoolView x, MemFun c0, int index);
    /// Constructor for cloning \a p
    ReifWait(Space& home, bool shared, ReifWait& p);

    static GecodeSolver* solver;
    // index to the boolvar array, index > 0 means boolvar is true,
    // index < 0 means boolvar is false
    // if true, index-1 is index
    // if false, index+1 is index
    int index_;
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Const function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator that waits until \a x becomes assigned and then executes \a c
    static ExecStatus post(Space& home, GecodeSolver* csps, Int::BoolView x, MemFun c, int index);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  GecodeSolver* ReifWait::solver = 0;

  void reifwait(Home home, GecodeSolver* csps, BoolVar x, ReifWait::MemFun c,
         int index, IntConLevel) {
      if (home.failed()) return;
      GECODE_ES_FAIL(ReifWait::post(home,csps, x,c, index));
    }

    forceinline
    ReifWait::ReifWait(Space& home, Int::BoolView x0, MemFun c0, int index)
        : Propagator(home), x(x0), c(c0), index_(index) {
      x.subscribe(home,*this,PC_GEN_ASSIGNED);
    }
    forceinline
    ReifWait::ReifWait(Space& home, bool shared, ReifWait& p)
    : Propagator(home,shared,p), c(p.c), index_(p.index_) {
      x.update(home,shared,p.x);
    }

    Actor*
    ReifWait::copy(Space& home, bool share) {
      return new (home) ReifWait(home,share,*this);
    }
    PropCost
    ReifWait::cost(const Space&, const ModEventDelta&) const {
      return PropCost::unary(PropCost::LO);
    }
    ExecStatus
    ReifWait::propagate(Space& home, const ModEventDelta&) {
      assert(x.assigned());
      if (x.status() == Int::BoolVarImp::ONE)
        (solver->*c)(index_+1);
      else
        (solver->*c)(-index_-1);
      return home.failed() ? ES_FAILED : home.ES_SUBSUMED(*this);
    }
    ExecStatus
    ReifWait::post(Space& home, GecodeSolver* csps, Int::BoolView x, MemFun c, int index) {
        solver = csps;
      if (x.assigned()) {
          if (x.status() == Int::BoolVarImp::ONE)
            (solver->*c)(index+1);
          else
            (solver->*c)(-index-1);
        return home.failed() ? ES_FAILED : ES_OK;
      } else {
        (void) new (home) ReifWait(home,x,c,index);
        return ES_OK;
      }
    }

    size_t
    ReifWait::dispose(Space& home) {
      x.cancel(home,*this,PC_GEN_ASSIGNED);
      (void) Propagator::dispose(home);
      return sizeof(*this);
    }


Gecode::IntConLevel ICL;
Gecode::IntVarBranch branchVar; //integer branch variable
Gecode::IntValBranch branchVal; //integer branch value

std::string GecodeSolver::num2name( unsigned int var)
{
    Clasp::AtomIndex& index = *s_->strategies().symTab;
    for (Clasp::AtomIndex::const_iterator it = index.begin(); it != index.end(); ++it) {
        if ( it->second.lit.var()==var) {
            if (!it->second.name.empty()) {
                {
                return it->second.name.c_str();
                }
            }else
                return "leer!";
        }
    }
    std::stringstream s;
    s<< var;
    return s.str();
}


std::vector<int> GecodeSolver::optValues;

GecodeSolver::GecodeSolver(bool lazyLearn, bool useCDG, bool weakAS, int numAS,
                           std::string ICLString, std::string branchVarString,
                           std::string branchValString) :
    currentSpace_(0), lazyLearn_(lazyLearn),
    useCDG_(useCDG), weakAS_(weakAS), numAS_(numAS),
    dummyReason_(this), updateOpt_(false)

{
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

#pragma message "check if current is on stack"
    if (currentSpace_)
    {
        for (std::vector<SearchSpace*>::iterator i = spaces_.begin(); i != spaces_.end(); ++i)
            delete *i;
    }

    // if we have not done initialization
    for (std::map<int, Constraint*>::iterator i = constraints_.begin(); i != constraints_.end(); ++i)
        delete i->second;
    constraints_.clear();
    globalConstraints_.clear();
}


void GecodeSolver::propagateLiteral(const Clasp::Literal& l, int)
{

    propQueue_.push_back(l);

}

void GecodeSolver::reset()
{
    propQueue_.clear();
}

void GecodeSolver::addVarToIndex(unsigned int var, unsigned int index)
{
    varToIndex_[var] = index;
    indexToVar_[index] = var;
}

bool GecodeSolver::initialize()
{

    currentDL_ = 0;
    dfsSearchEngine_ = 0;
    babSearchEngine_ = 0;
    enumerator_ = 0;
    if (optimize_)
    {
        weakAS_=false;
        numAS_=0;
    }

    //if (domain_.left == std::numeric_limits<int>::min())
    if (domain_.left < Int::Limits::min)
    {
        domain_.left = Int::Limits::min;
        std::cerr << "Warning: Raised lower domain to " << domain_.left;
    }

    if (domain_.right > Int::Limits::max+1)
    {
        domain_.right = Int::Limits::max+1;
        std::cerr << "Warning: Reduced upper domain to " << domain_.right-1;
    }



    /////////
    // Register all variables in the Constraints
    // Convert uids to solver literal ids
    // Guess initial domains of level0 constraints
    std::map<int, Constraint*> newConstraints;
    for (std::map<int, Constraint*>::iterator i = constraints_.begin(); i != constraints_.end(); ++i)
    {
        int newVar = s_->strategies().symTab->find(i->first)->lit.var();
        // convert uids to solver literal ids
        newConstraints.insert(std::make_pair(newVar, i->second));
        i->second->registerAllVariables(this);

        //guess domains of already decided constraints
        if (s_->isTrue(posLit(newVar)))
        {
            guessDomains(i->second, true);
        }
        else
        if (s_->isFalse(posLit(newVar)))
        {
            guessDomains(i->second, false);
        }
        else
        {
            s_->addWatch(posLit(newVar), clingconPropagator_,0);
            s_->addWatch(negLit(newVar), clingconPropagator_,0);
        }
    }
    constraints_ = newConstraints;

    for (LParseGlobalConstraintPrinter::GCvec::iterator i = globalConstraints_.begin(); i != globalConstraints_.end(); ++i)
        for (boost::ptr_vector<IndexedGroundConstraintVec>::iterator j = i->heads_.begin(); j != i->heads_.end(); ++j)
            for (IndexedGroundConstraintVec::iterator k = j->begin(); k != j->end(); ++k)
            {
                k->a_->registerAllVariables(this);
                k->b_.registerAllVariables(this);
            }

    // End
    /////////


    // propagate empty constraint set, maybe some trivial constraints can be fullfilled
    currentSpace_ = new SearchSpace(this, variables_.size(), constraints_, globalConstraints_, &varToIndex_);

    //TODO: 1. check if already failed through initialitation
    for (std::map<int, Constraint*>::iterator i = constraints_.begin(); i != constraints_.end(); ++i)
        delete i->second;
    constraints_.clear();
    globalConstraints_.clear();

    if(!currentSpace_->failed() && currentSpace_->status() != SS_FAILED)
    {
        //Clasp::LitVec lv(currentSpace_->getAssignment(Clasp::LitVec()));
#pragma message "Check with benni the return value, i do waste it here"
        return propagateNewLiteralsToClasp();
    }
    else
        return false;
}


void GecodeSolver::newlyDerived(int index)
{
    if (index>0)
    {
        derivedLits_.push_back(Literal(indexToVar(index-1),false));
    }
    else
    {
        derivedLits_.push_back(Literal(indexToVar(-(index+1)),true));
    }
}


GecodeSolver::DomainMap intersect(const GecodeSolver::DomainMap& a,  const GecodeSolver::DomainMap& b);
GecodeSolver::DomainMap unite(const GecodeSolver::DomainMap& a,  const GecodeSolver::DomainMap& b);
GecodeSolver::DomainMap xorit(const GecodeSolver::DomainMap& a,  const GecodeSolver::DomainMap& b);
GecodeSolver::DomainMap eq(const GecodeSolver::DomainMap& a,  const GecodeSolver::DomainMap& b);

void GecodeSolver::guessDomains(const Constraint* c, bool val)
{
    DomainMap dom(guessDomainsImpl(c));
    if (!val)
        for (DomainMap::iterator i = dom.begin(); i != dom.end(); ++i)
            i->second.complement(Interval<int>(Int::Limits::min, Int::Limits::max+1));

    domains_ = intersect(dom, domains_);
}

GecodeSolver::DomainMap GecodeSolver::guessDomainsImpl(const Constraint* c)
{
    DomainMap m;
    switch(c->getType())
    {
    case CSPLit::AND:
    {
        Constraint const* a;
        Constraint const* b;
        c->getConstraints(a,b); 
        m = intersect(guessDomainsImpl(a), guessDomainsImpl(b));
        return m;
    }
    case CSPLit::OR:
    {
        Constraint const* a;
        Constraint const* b;
        c->getConstraints(a,b);
        m = unite(guessDomainsImpl(a), guessDomainsImpl(b));
        return m;
    }
    case CSPLit::XOR:
    {
        Constraint const* a;
        Constraint const* b;
        c->getConstraints(a,b);;
        m = xorit(guessDomainsImpl(a), guessDomainsImpl(b));
        return m;
    }
    case CSPLit::EQ:
    {
        Constraint const* a;
        Constraint const* b;
        c->getConstraints(a,b);
        m = eq(guessDomainsImpl(a), guessDomainsImpl(b));
        return m;
    }
    }

    //else
    std::vector<unsigned int> vec;
    c->getAllVariables(vec, this);
    //unary constraint
    if (vec.size()==1)
    {
        const GroundConstraint* a(0);
        const GroundConstraint* b(0);
        CSPLit::Type comp = c->getRelations(a,b);
        unsigned int var = -1;
        int value = 0;
        if (a->isVariable())
        {
            assert(b->isInteger());
            var = getVariable(a->getString());
            value = b->getInteger();
        }
        else
        {
            assert(b->isVariable());
            assert(a->isInteger());
            switch(comp)
            {
            case CSPLit::GREATER:  comp = CSPLit::LOWER;   break;
            case CSPLit::LOWER:    comp = CSPLit::GREATER; break;
            case CSPLit::EQUAL:    comp = CSPLit::EQUAL; break;
            case CSPLit::GEQUAL:   comp = CSPLit::LEQUAL;  break;
            case CSPLit::LEQUAL:   comp = CSPLit::GEQUAL;  break;
            case CSPLit::INEQUAL:  comp = CSPLit::INEQUAL;   break;
            }
            var = getVariable(b->getString());
            value = a->getInteger();
        }
        m[var] = getSet(var, comp, value);
        return m;
    }
    return m;
}

GecodeSolver::Domain GecodeSolver::getSet(unsigned int var, CSPLit::Type t, int x) const
{
    Domain ret;
    switch(t)
    {
    case CSPLit::GREATER: ret.unite(Interval<int>(x+1,Int::Limits::max+1));  break;
    case CSPLit::LOWER:   ret.unite(Interval<int>(Int::Limits::min, x-1+1)); break;
    case CSPLit::GEQUAL:  ret.unite(Interval<int>(x,Int::Limits::max+1));    break;
    case CSPLit::LEQUAL:  ret.unite(Interval<int>(Int::Limits::min, x+1));   break;
    case CSPLit::EQUAL:   ret.unite(Interval<int>(x,x+1));                   break;
    case CSPLit::INEQUAL: ret.unite(Interval<int>(x,x+1)); ret.complement(Interval<int>(Int::Limits::min, Int::Limits::max+1)); break;
    }
    return ret;
}

GecodeSolver::DomainMap intersect(const GecodeSolver::DomainMap& a,  const GecodeSolver::DomainMap& b)
{
    GecodeSolver::DomainMap ret;
    for (GecodeSolver::DomainMap::const_iterator i = a.begin(); i != a.end(); ++i)
    {
        GecodeSolver::DomainMap::const_iterator found = b.find(i->first);

        if (found == b.end())
        {
            ret[i->first] = i->second;
        }
        else
        {
            GecodeSolver::Domain inter = i->second;
            inter.intersect(found->second);
            ret[i->first]=inter;
        }
    }

    // can optimize this
    // all variables that we have only in b but not in a
    for (GecodeSolver::DomainMap::const_iterator i = b.begin(); i != b.end(); ++i)
    {
        GecodeSolver::DomainMap::const_iterator found = a.find(i->first);

        if (found == a.end())
        {
            ret[i->first] = i->second;
        }
    }
    return ret;
}

GecodeSolver::DomainMap unite(const GecodeSolver::DomainMap& a,  const GecodeSolver::DomainMap& b)
{
    GecodeSolver::DomainMap ret;
    for (GecodeSolver::DomainMap::const_iterator i = a.begin(); i != a.end(); ++i)
    {
        GecodeSolver::DomainMap::const_iterator found = b.find(i->first);

        if (found == b.end())
        {
        }
        else
        {
            GecodeSolver::Domain inter = i->second;
            inter.unite(found->second);
            ret[i->first]=inter;
        }
    }
    return ret;
}

GecodeSolver::DomainMap xorit(const GecodeSolver::DomainMap& a,  const GecodeSolver::DomainMap& b)
{
    // (a \/ b) /\ (-a \/ -b)
    GecodeSolver::DomainMap ret;
    for (GecodeSolver::DomainMap::const_iterator i = a.begin(); i != a.end(); ++i)
    {
        GecodeSolver::DomainMap::const_iterator found = b.find(i->first);

        if (found == b.end())
        {
        }
        else
        {
            GecodeSolver::Domain l(i->second);
            l.unite(found->second);
            GecodeSolver::Domain nega = i->second;
            nega.complement(GecodeSolver::IntInterval(Int::Limits::min, Int::Limits::max+1));
            GecodeSolver::Domain negb = found->second;
            negb.complement(GecodeSolver::IntInterval(Int::Limits::min, Int::Limits::max+1));
            nega.unite(negb);
            l.intersect(nega);
            ret[i->first] = l;
        }
    }
    return ret;
}

GecodeSolver::DomainMap eq(const GecodeSolver::DomainMap& a,  const GecodeSolver::DomainMap& b)
{
    // (a /\ b) \/ (-a /\ -b)
    GecodeSolver::DomainMap ret;
    for (GecodeSolver::DomainMap::const_iterator i = a.begin(); i != a.end(); ++i)
    {
        GecodeSolver::DomainMap::const_iterator found = b.find(i->first);

        if (found == b.end())
        {
        }
        else
        {
            GecodeSolver::Domain l(i->second);
            l.intersect(found->second);
            GecodeSolver::Domain nega = i->second;
            nega.complement(GecodeSolver::IntInterval(Int::Limits::min, Int::Limits::max+1));
            GecodeSolver::Domain negb = found->second;
            negb.complement(GecodeSolver::IntInterval(Int::Limits::min, Int::Limits::max+1));
            nega.intersect(negb);
            l.unite(nega);
            ret[i->first]=l;
        }
    }
    return ret;
}


bool GecodeSolver::hasAnswer()
{
    // first weak answer? of solver assignment A
    delete dfsSearchEngine_;
    delete babSearchEngine_;
    dfsSearchEngine_ = 0;
    babSearchEngine_ = 0;
    asCounter_ = 0;
    // currently using a depth first search, this could be changed by options
    if (!optimize_)
        dfsSearchEngine_ = new DFS<GecodeSolver::SearchSpace>(currentSpace_, searchOptions_);
    else
        babSearchEngine_ = new BAB<GecodeSolver::SearchSpace>(currentSpace_, searchOptions_);
#ifdef DEBUGTEXT
    //std::cout << "created new SearchEngine_:" << searchEngine_ << std::endl;
    currentSpace_->print(variables_);std::cout << std::endl;
#endif
    if (enumerator_)
    {
        delete enumerator_;
        enumerator_ = 0;
    }
    if (!optimize_)
        enumerator_ = dfsSearchEngine_->next();
    else
        enumerator_ = babSearchEngine_->next();
    if (enumerator_ != NULL)
    {
        if (optimize_)
        {
            GecodeSolver::optValues.clear();
            for (int i = 0; i < enumerator_->opts_.size(); ++i)
                GecodeSolver::optValues.push_back(enumerator_->opts_[i].val());

            updateOpt_=true;
        }
        asCounter_++;
        return true;
    }
    else
        setConflict(assignment_);
    return false;
}

bool GecodeSolver::nextAnswer()
{
    assert(enumerator_);
    if (weakAS_) return false;
    if (numAS_ && asCounter_ >= numAS_) return false;

    GecodeSolver::SearchSpace* oldEnum = enumerator_;

    ++asCounter_;
    if (!optimize_)
        enumerator_ = dfsSearchEngine_->next();
    else
        enumerator_ = babSearchEngine_->next();
    delete oldEnum;

    if (enumerator_)
    {
        GecodeSolver::optValues.clear();
        for (int i = 0; i < enumerator_->opts_.size(); ++i)
            GecodeSolver::optValues.push_back(enumerator_->opts_[i].val());

    }
    return (enumerator_ != NULL);
}


void GecodeSolver::setConflict(Clasp::LitVec conflict)
{
    unsigned int maxDL = 0;
    for (Clasp::LitVec::const_iterator j = conflict.begin(); j != conflict.end(); ++j)
        maxDL = std::max(s_->level(j->var()), maxDL);
    s_->setBacktrackLevel(std::min(maxDL,s_->backtrackLevel()));
    if (maxDL < s_->decisionLevel())
        s_->undoUntil(maxDL);
    if (conflict.size()==0) // if root level conflict due to global constraints
        conflict.push_back(posLit(0));
    s_->setConflict(conflict);
    return;

    /* test new stuff IIS (Irreducible Infeasible Set)

    if (conflict.size()==0)
    {
        conflict.push_back(posLit(0));
        s_->setConflict(conflict);
        return;
    }

    // copy the very first searchspace
    SearchSpace* tester;

    std::cout << "Reduced from " << conflict.size() << " to ";
    unsigned int counter = 1;
    while(conflict.size()>counter)
    {
        if (spaces_.size())
            tester = static_cast<GecodeSolver::SearchSpace*>(spaces_[0]->clone());
        else
            tester = static_cast<GecodeSolver::SearchSpace*>(currentSpace_->clone());

        tester->propagate(conflict.begin(), conflict.end()-counter);

        if (tester->failed() || tester->status()==SS_FAILED)
        {
            // still failing, throw it away, it did not contribute to the conflict
            conflict.erase(conflict.end()-counter);
        }
        else
        {
            counter++;
        }
        delete tester;
    }
    std::cout << conflict.size() << std::endl;
    unsigned int maxDL = 0;
    for (Clasp::LitVec::const_iterator j = conflict.begin(); j != conflict.end(); ++j)
        maxDL = std::max(s_->level(j->var()), maxDL);
    s_->setBacktrackLevel(std::min(maxDL,s_->backtrackLevel()));
    if (maxDL < s_->decisionLevel())
        s_->undoUntil(maxDL);
    if (conflict.size()==0) // if root level conflict due to global constraints
        conflict.push_back(posLit(0));
    s_->setConflict(conflict);
    */
}


unsigned int GecodeSolver::varToIndex(unsigned int var)
{
    //if (varToIndex_.find(var) == varToIndex_.end())
    //    int error=5;
    return varToIndex_[var];
}

unsigned int GecodeSolver::indexToVar(unsigned int index)
{
    //if (indexToVar_.find(index) == indexToVar_.end())
    //    int error=5;
    return indexToVar_[index];
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


    // if already failed, create conflict, this may be on a lower level
    if (currentSpace_->failed())
    {
        setConflict(assignment_);
        return false;
    }
    else // everything is fine
    {
        // if we need to update the minimize function, do this
        if (updateOpt_ && GecodeSolver::optValues.size()>0)
        { 
            // this is still the old space, no new knowledge has been integrated
            derivedLits_.clear();
            currentSpace_->updateOptValues();
            updateOpt_=false;

            if (!currentSpace_->failed() && currentSpace_->status() != SS_FAILED)
            {
                // there was a problem due to the update function
                if (derivedLits_.size()>0)
                {
                    uint32 dl = s_->decisionLevel();
                    if (!propagateNewLiteralsToClasp())
                        return false;
                    // if we backtracked during assigning, clear propQueue
                    if (dl>s_->decisionLevel())
                        propQueue_.clear();
                }
            }
            else
            {
                setConflict(assignment_);
                Clasp::LitVec clits(assignment_);
                return false;
            }
        }

        // no updateOpt or no problem with this

        // remove already assigned literals, this happens if we have propagated a new literal to clasp
        Clasp::LitVec clits;
        for (Clasp::LitVec::const_iterator i = propQueue_.begin(); i != propQueue_.end(); ++i)
        {
            //assert(s_->isTrue(*i));
            if (!s_->isTrue(*i))
            {
                //this literal was once propagated but then backtracked without any notice of me, so we ignore it
                continue;
            }
            // add if free
            // conflict if contradicting
            GecodeSolver::SearchSpace::Value constr = currentSpace_->getValueOfConstraint(i->var());
            if ( constr == SearchSpace::BFREE)
                clits.push_back(*i);
            if (( constr == SearchSpace::BFALSE && i->sign()==false ) ||
                ( constr == SearchSpace::BTRUE  && i->sign()==true  )
                )
            {
                //std::cout << "Conflicting due to wrong assignment of clasp variable" << std::endl;
                setConflict(assignment_);
                return false;
            }
        }
        propQueue_.clear();

        // we have something to propagate
        if (clits.size())
        {
            // if we have a new decision level, create a new space and stack the old one
            if (s_->decisionLevel() > currentDL_)
            {
                //register in solver for undo event
                assert(s_->decisionLevel()!=0);
                s_->addUndoWatch(s_->decisionLevel(),clingconPropagator_);

                //std::cout << "store a new space" << std::endl;
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

            derivedLits_.clear();
            currentSpace_->propagate(clits.begin(),clits.end()); // can lead to failed space

            if (!currentSpace_->failed() && currentSpace_->status() != SS_FAILED)
            {
                assignment_.insert(assignment_.end(), clits.begin(), clits.end());

                // this function avoids propagating already decided literals
                if(!propagateNewLiteralsToClasp())
                    return false;
            }
            // currentSpace_->status() == FAILED
            else
            {
                // could save a bit of conflict size if conflict resultet from propagate function, which does singular propagation

                clits.insert(clits.end(), assignment_.begin(), assignment_.end());
                setConflict(clits);
                return false;
            }
            return true;
        }
    return true;
    }
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
//    std::cout << "backtrack over level" << level << std::endl;

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
    propQueue_.clear();
    if (optimize_ && GecodeSolver::optValues.size()>0)
        updateOpt_=true;

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

/*
void GecodeSolver::generateReason(Clasp::LitVec& lits, unsigned int upToAssPos)
{
    // add only atoms from solver assignment
    // go only over atoms that are assigned in the gecode assignment
    // TODO: test if switching the presettings is the solution (go only over assigned gecode, even new, add only old assigned plus new derived lits from clasp== assignment_)
    // TODO: Do get the new learnt literals immediatly true ? If so, ignoring them could lead to smaller clauses
    {

        lits.insert(lits.end(), assignment_.begin(), assignment_.begin() + upToAssPos);
    }

}
*/
/*
void GecodeSolver::generateConflict(Clasp::LitVec& lits, unsigned int upToAssPos)
{
    // very naive appraoch, all assigned constraints so far
    generateReason(lits, upToAssPos);
}*/



bool GecodeSolver::propagateNewLiteralsToClasp()
{
    unsigned int size = assignment_.size();

    if (lazyLearn_)
    {
        for (Clasp::LitVec::const_iterator i = derivedLits_.begin(); i != derivedLits_.end(); ++i)
        {

            // if not already decided
            //if (s_->value(i->var())==Clasp::value_free)
            if (!s_->isTrue(*i))  // if the literal is not already true (undecided or false)          
            {
                litToAssPosition_[(*i).var()] = size;
                assignment_.push_back(*i);

    #pragma message "Ask Benni if i register for -a will i get calls for a?"
                if (!s_->force(*i, &dummyReason_))
                {

                    derivedLits_.clear();
                    return false;
                }

            }
        }
        derivedLits_.clear();
        return true;
    }
    else
    {
#pragma message "Could optimize here as a big part of the reason is the same, first create this part, maybe this can be done with the ClauseCreator"

        //std::cout << "Propagate to clasp" << std::endl;
        ClauseCreator gc(s_);
        for (Clasp::LitVec::const_iterator i = derivedLits_.begin(); i != derivedLits_.end(); ++i)
        {

            /*
            Clasp::LitVec reason;
            reason.push_back(*i);
            generateReason(reason, size);
            gc.startAsserting(Constraint_t::learnt_conflict, *reason.begin());
            for (Clasp::LitVec::const_iterator j = reason.begin() + 1; j != reason.end(); ++j)
            {
                gc.add(~(*j));
            }
            if(!gc.end())
                return false;
            */
            //if (s_->value(i->var())==Clasp::value_free)

            if (!s_->isTrue(*i))
            {
                /*
                   std::cout << "reason for: ";
                   if (s_->isFalse(*i)) std::cout << "f!!!!!!!!!!!!!!!!!!!!!!!!!";
                   else
                   if (s_->isTrue(*i)) std::cout << "t!!!!!!!!!!!!!!!!!!!!!!!!";
                   else
                       std::cout << "u";
                   if (i->sign())
                        std::cout << "neg ";
                    else
                        std::cout << "pos ";
                    std::cout << num2name(i->var());
                    if (s_->isFalse(*i))
                        std::cout << "WAS FALSE!!!!!!!!!!!!!!!!!!!!" << std::endl;
                    std::cout << std::endl;
                    */

                uint32 dl = s_->decisionLevel();
                assignment_.push_back(*i);
                gc.startAsserting(Constraint_t::learnt_conflict, *i);
                for (Clasp::LitVec::const_iterator j = assignment_.begin(); j != assignment_.begin()+size; ++j)
                {
                    /*
                    if (s_->isFalse(*j)) std::cout << "f!!!!!!!!!!!!!!!!!!!!!!";
                    else
                    if (s_->isTrue(*j)) std::cout << "t";
                    else
                        std::cout << "UNKNOWN !!!!!!!!!!!!!!!!!!!!!";

                    if (j->sign())
                         std::cout << "neg ";
                     else
                         std::cout << "pos";
                     std::cout << num2name(j->var());
                     std::cout << " on level " << s_->level(j->var());

                     std::cout << std::endl;
                     */
                    gc.add(~(*j));
                }
                if(!gc.end())
                {
                    derivedLits_.clear();
                    return false;
                }
                if (dl>s_->decisionLevel())
                    assert(false && "Shouldn't occur here");
            }
            else
            {
                /*
                std::cout << "Skip: ";
                   if (i->sign())
                        std::cout << "neg ";
                    else
                        std::cout << "pos";
                    std::cout << num2name(i->var());
                    */

            }
           // std::cout << std::endl;
        }

        derivedLits_.clear();
        return true;
    }
    assert(false);
}

Clasp::ConstraintType GecodeSolver::CSPDummy::reason(const Literal& l, Clasp::LitVec& reason)
{
    reason.insert(reason.end(), gecode_->assignment_.begin(), gecode_->assignment_.begin()+gecode_->litToAssPosition_[l.var()]);
   // for (Clasp::LitVec::const_iterator i = gecode_->assignment_.begin(); i != gecode_->assignment_.begin()+; ++i)
    {

    }

    //Clasp::LitVec temp;
    //temp.push_back(l);
   // gecode_->generateReason(reason, gecode_->litToAssPosition_[l.var()]);
    //remove first literal from reason
    //reason[0] = reason.back();
    //reason.pop_back();
    return Clasp::Constraint_t::learnt_other;
}




GecodeSolver::SearchSpace::SearchSpace(GecodeSolver* csps, unsigned int numVar, std::map<int, Constraint*>& constraints,
                                       LParseGlobalConstraintPrinter::GCvec& gcvec,
                                       std::map<unsigned int, unsigned int>* litToVar) : Space(),
    x_(*this, numVar),
    //b_(*this, constraints.size(), 0, 1),
    b_(),
    csps_(csps)
{

    //initialize all variables with their domain
    for(size_t i = 0; i < csps_->getVariables().size(); ++i)
    {
        GecodeSolver::Domain def;
        def.unite(csps_->domain_);
        if (csps_->domains_.find(i)==csps_->domains_.end())
        {
            csps_->domains_[i]=def;
        }
        else
            csps_->domains_[i].intersect(def);
        //std::cout << csps_->domains_[i] << " " << std::endl;

        typedef int Range[2];
        Range* array = new Range[csps_->domains_[i].size()];
        unsigned int count=0;
        for (GecodeSolver::Domain::ConstIterator j = csps_->domains_[i].begin(); j != csps_->domains_[i].end(); ++j)
        {
            array[count][0]=j->left;
            array[count++][1]=j->right-1;
        }

        IntSet is(array, csps_->domains_[i].size());
        delete[] array;

        x_[i] = IntVar(*this, is);
    }

    // set static constraints
    int numReified = 0;
    for(std::map<int, Constraint*>::iterator i = constraints.begin(); i != constraints.end(); ++i)
    {
        if (csps_->getSolver()->value(i->first) == value_free)
            ++numReified;
        else
        {
            //std::cout << "Constraint " << csps_->num2name(i->first) << " is static" << std::endl;
            generateConstraint(i->second,csps_->getSolver()->value(i->first) == value_true);
        }
    }

    b_ = BoolVarArray(*this,numReified,0,1);
    unsigned int counter = 0;
    for(std::map<int, Constraint*>::iterator i = constraints.begin(); i != constraints.end(); ++i)
    {
         if (csps_->getSolver()->value(i->first) == value_free)
         {
             //std::cout << "add to map " << i->first << " with " << counter << std::endl;
             csps_->addVarToIndex(i->first, counter);
             //std::cout << csps_->num2name(i->first) << " will be generated" << std::endl;
             generateConstraint(i->second, counter);
             reifwait(*this,csps_,b_[counter],&GecodeSolver::newlyDerived, counter, ICL);
             //Gecode::wait(*this, b_[counter],boost::bind(&(GecodeSolver::newlyDerived),csps_,counter,_1),ICL);
             ++counter;
         }
    }


    std::map<unsigned int,std::vector<std::pair<GroundConstraint*,bool> > > optimize; // true means maximize, false means minimize
    for (size_t i = 0; i < gcvec.size(); ++i)
    {
        if (!(gcvec[i].type_ == MINIMIZE || gcvec[i].type_ == MINIMIZE_SET || gcvec[i].type_ == MAXIMIZE || gcvec[i].type_ == MAXIMIZE_SET))
            generateGlobalConstraint(gcvec[i]);
        else
        {
            LParseGlobalConstraintPrinter::GC& gc = gcvec[i];
            IndexedGroundConstraintVec& igcv = gc.heads_[0];
            for (size_t i = 0; i < igcv.size(); ++i)
            {
                bool max = false;
                if (gc.type_ == MAXIMIZE_SET || gc.type_ == MAXIMIZE)
                    max=true;
                optimize[igcv[i].b_.getInteger()].push_back(std::pair<GroundConstraint*,bool>(igcv[i].a_.get(), max));
                csps_->setOptimize(true);
            }
        }
    }

    opts_ = IntVarArray(*this, optimize.size(), Int::Limits::min, Int::Limits::max);


    size_t index = 0;
    for (std::map<unsigned int,std::vector<std::pair<GroundConstraint*,bool> > >::iterator i = optimize.begin(); i != optimize.end(); ++i)
    {
        LinExpr expr(generateSum(i->second));
        //    if (i->second.second)
        //        rel(*this, LinRel(opts_[index],IRT_EQ,-expr), ICL); // maximize == minimize the opposite
        //    else
        rel(*this, LinRel(opts_[index],IRT_EQ,expr), ICL);

        ++index;
    }

    iva_ << opts_;
    iva_ << x_;

    std::sort(iva_.begin(), iva_.end(), boost::bind(&IntVar::before,_1,_2));
    IntVarArgs::iterator newEnd = std::unique(iva_.begin(), iva_.end(), boost::bind(&IntVar::same,_1,_2));

    IntVarArgs temp;
    if (iva_.size())
        temp << iva_.slice(0,1,std::distance(iva_.begin(),newEnd));
    branch(*this, temp, branchVar, branchVal);
    iva_ = IntVarArgs();
}

GecodeSolver::SearchSpace::SearchSpace(bool share, SearchSpace& sp) : Space(share, sp), csps_(sp.csps_)
{
    x_.update(*this, share, sp.x_);
    b_.update(*this, share, sp.b_);
    opts_.update(*this,share,sp.opts_);
}

GecodeSolver::SearchSpace* GecodeSolver::SearchSpace::copy(bool share)
{
    return new GecodeSolver::SearchSpace(share, *this);
}

//optimize function
void GecodeSolver::SearchSpace::constrain(const Space& _b)
{
    updateOptValues();
}

void GecodeSolver::SearchSpace::updateOptValues()
{
    if (opts_.size()>1)
    {
        rel(*this, opts_[0] <= GecodeSolver::optValues[0], ICL);
    }

    for (int i = 0; i < opts_.size()-1; ++i)
    {
        BoolVar lhs(*this,0,1);
        BoolVar rhs(*this,0,1);
        rel(*this, opts_[i], IRT_EQ, GecodeSolver::optValues[i], lhs, ICL);
        if (i+1==opts_.size()-1)
            rel(*this, opts_[i+1], IRT_LE, GecodeSolver::optValues[i+1], rhs, ICL);
        else
            rel(*this, opts_[i+1], IRT_LQ, GecodeSolver::optValues[i+1], rhs, ICL);
        rel(*this, lhs, BOT_IMP, rhs, 1, ICL);
    }

    if (opts_.size()==1)
    {
        rel(*this, opts_[0] < GecodeSolver::optValues[0],ICL);
    }

}

void GecodeSolver::SearchSpace::propagate(const Clasp::LitVec::const_iterator& lvstart, const Clasp::LitVec::const_iterator& lvend)
{
    for (Clasp::LitVec::const_iterator i = lvstart; i != lvend; ++i)
    {
        Int::BoolView bv(b_[csps_->varToIndex(i->var())]);
        if (!i->sign())
        {
            if(Gecode::Int::ME_BOOL_FAILED == bv.one(*this))
            {
                fail();
                return;
            }
        }
        else
        {
            if(Gecode::Int::ME_BOOL_FAILED == bv.zero(*this))
            {
                fail();
                return;
            }
        }
    }
}

/*
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
}*/


void GecodeSolver::SearchSpace::print(std::vector<std::string>& variables) const
{
    // i use ::operator due to gecode namespace bug
    for (int i = 0; i < x_.size(); ++i)
    {

        Int::IntView v(x_[i]);
        std::cout << variables[i] << "=";
        std::cout << v;
        //::operator<<(std::cout, v);
        std::cout << " ";
        //for (IntVarRanges i(x); i(); ++i)
        // std::cout << i.min() << ".." << i.max() << ’ ’;

    }
}

GecodeSolver::SearchSpace::Value GecodeSolver::SearchSpace::getValueOfConstraint(const Clasp::Var& i)
{
    Int::BoolView bv(b_[csps_->varToIndex(i)]);
    //Int::BoolView bv(b_[(*litToVar_)[i.var()]]);
    if (bv.status() == Int::BoolVarImp::NONE)
        return BFREE;
    else
	if (bv.status() == Int::BoolVarImp::ONE)
            return BTRUE;
	else
            return BFALSE;
}

void GecodeSolver::SearchSpace::generateConstraint(Constraint* c, bool val)
{
    if (c->isSimple())
    {
        generateLinearRelation(c).post(*this,val,ICL);
    }
    else
    {
        Gecode::rel(*this, !generateBooleanExpression(c),ICL);
    }
}

void GecodeSolver::SearchSpace::generateConstraint(Constraint* c, unsigned int boolvar)
{
    if (c->isSimple())
    {
        Gecode::rel(*this, generateLinearRelation(c) == b_[boolvar],ICL);
    }
    else
    {
        Gecode::rel(*this, generateBooleanExpression(c) == b_[boolvar],ICL);
    }
}

Gecode::LinRel GecodeSolver::SearchSpace::generateLinearRelation(const Constraint* c) const
{
    assert(c);
    assert(c->isSimple());
    const GroundConstraint* x;
    const GroundConstraint* y;
    CSPLit::Type r = c->getRelations(x,y);
    Gecode::IntRelType ir;

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
    case CSPLit::LEQUAL:
        ir = IRT_LQ;
        break;
    case CSPLit::GEQUAL:
        ir = IRT_GQ;
        break;
    case CSPLit::GREATER:
        ir = IRT_GR;
        break;
    case CSPLit::ASSIGN:
    default:
        assert(false);
    }
    return LinRel(generateLinearExpr(x), ir, generateLinearExpr(y));
}

Gecode::BoolExpr GecodeSolver::SearchSpace::generateBooleanExpression(const Constraint* c)
{
    assert(c);
    if(c->isSimple())
    {
        return generateLinearRelation(c);
    }
    //else

    const Constraint* x;
    const Constraint* y;
    CSPLit::Type r = c->getConstraints(x,y);

    BoolExpr a = generateBooleanExpression(x);
    BoolExpr b = generateBooleanExpression(y);

    switch(r)
    {
    case CSPLit::AND: return a && b;
    case CSPLit::OR:  return a || b;
    case CSPLit::XOR: return a ^ b;
    case CSPLit::EQ:  return a == b;
    default: assert(false);
    }
}


Gecode::LinExpr GecodeSolver::SearchSpace::generateSum(std::vector<std::pair<GroundConstraint*,bool> >& vec) const
{
    return generateSum(vec,0);
}

Gecode::LinExpr GecodeSolver::SearchSpace::generateSum( std::vector<std::pair<GroundConstraint*,bool> >& vec, size_t i) const
{
    if (i==vec.size()-1)
    {
        if (vec[i].second)
            return generateLinearExpr(vec[i].first)*-1;
        else
            return generateLinearExpr(vec[i].first);
    }
    else
        if (vec[i].second)
            return generateLinearExpr(vec[i].first)*-1 + generateSum(vec,i+1);
        else
            return generateLinearExpr(vec[i].first) + generateSum(vec,i+1);


}

Gecode::LinExpr GecodeSolver::SearchSpace::generateLinearExpr(const GroundConstraint* c) const
{
    if (c->isInteger())
    {
        return LinExpr(c->getInteger());
    }
    else
        if (c->isVariable())
        {
            return LinExpr(x_[csps_->getVariable(c->getString())]);
        }

    const GroundConstraint* a = c->a_;
    const GroundConstraint* b = c->b_;
    GroundConstraint::Operator op = c->getOperator();

    switch(op)
    {
    case GroundConstraint::TIMES:
        return generateLinearExpr(a)*generateLinearExpr(b);
    case GroundConstraint::MINUS:
        return generateLinearExpr(a)-generateLinearExpr(b);
    case GroundConstraint::PLUS:
        return generateLinearExpr(a)+generateLinearExpr(b);
    case GroundConstraint::DIVIDE:
        return generateLinearExpr(a)/generateLinearExpr(b);
    case GroundConstraint::ABS:
        return Gecode::abs(generateLinearExpr(a));
    }
    assert(false);

}


void GecodeSolver::SearchSpace::generateGlobalConstraint(LParseGlobalConstraintPrinter::GC& gc)
{

    if (gc.type_==DISTINCT)
    {
        IntVarArgs z(gc.heads_[0].size());

        for (size_t i = 0; i < gc.heads_[0].size(); ++i)
        {
            z[i] = expr(*this,generateLinearExpr(gc.heads_[0][i].a_.get()),ICL);
        }

        iva_ << z;

        Gecode::distinct(*this,z,ICL);
        return;

    }
    if (gc.type_==BINPACK)
    {
        IntVarArgs l(gc.heads_[0].size());
        for (size_t i = 0; i < gc.heads_[0].size(); ++i)
        {
            l[i] = expr(*this,generateLinearExpr(gc.heads_[0][i].a_.get()),ICL);
        }

        IntVarArgs b(gc.heads_[1].size());
        for (size_t i = 0; i < gc.heads_[1].size(); ++i)
        {
            b[i] = expr(*this,generateLinearExpr(gc.heads_[1][i].a_.get()),ICL);
        }

        IntArgs  s(gc.heads_[2].size());
        for (size_t i = 0; i < gc.heads_[2].size(); ++i)
        {


            if (!gc.heads_[2][i].a_->isInteger())
                throw ASPmCSPException("Third argument of binpacking constraint must be a list of integers.");
            s[i] = gc.heads_[2][i].a_->getInteger();
        }
        iva_ << l;
        iva_ << b;

        Gecode::binpacking(*this,l,b,s,ICL);
        return;

    }
    if (gc.type_==COUNT)
    {
        IntVarArgs a(gc.heads_[0].size());
        IntArgs c(gc.heads_[0].size());

        for (size_t i = 0; i < gc.heads_[0].size(); ++i)
        {
            a[i] = expr(*this,generateLinearExpr(gc.heads_[0][i].a_.get()),ICL);

            assert(gc.heads_[0][i].b_.isInteger());
            c[i] = gc.heads_[0][i].b_.getInteger();

        }

        IntRelType cmp;
        switch(gc.cmp_)
        {
        case CSPLit::ASSIGN:assert(false);
        case CSPLit::GREATER:cmp=IRT_GR; break;
        case CSPLit::LOWER:cmp=IRT_LE; break;
        case CSPLit::EQUAL:cmp=IRT_EQ; break;
        case CSPLit::GEQUAL:cmp=IRT_GQ; break;
        case CSPLit::LEQUAL:cmp=IRT_LQ; break;
        case CSPLit::INEQUAL:cmp=IRT_NQ; break;
        default: assert(false);
        }

        iva_ << a;
        IntVar temp(expr(*this,generateLinearExpr(gc.heads_[1][0].a_.get()),ICL));
        iva_ << temp;

        Gecode::count(*this,a,c,cmp,temp,ICL);
        return;

    }
    if (gc.type_==COUNT_UNIQUE)
    {
        IntVarArgs a(gc.heads_[0].size());

        for (size_t i = 0; i < gc.heads_[0].size(); ++i)
        {
            a[i] = expr(*this, generateLinearExpr(gc.heads_[0][i].a_.get()),ICL);
        }

        IntRelType cmp;
        switch(gc.cmp_)
        {
        case CSPLit::ASSIGN:assert(false);
        case CSPLit::GREATER:cmp=IRT_GR; break;
        case CSPLit::LOWER:cmp=IRT_LE; break;
        case CSPLit::EQUAL:cmp=IRT_EQ; break;
        case CSPLit::GEQUAL:cmp=IRT_GQ; break;
        case CSPLit::LEQUAL:cmp=IRT_LQ; break;
        case CSPLit::INEQUAL:cmp=IRT_NQ; break;
        default: assert(false);
        }

        iva_ << a;
        IntVar temp1(expr(*this, generateLinearExpr(&gc.heads_[0][0].b_),ICL));
        iva_ << temp1;
        IntVar temp2(expr(*this, generateLinearExpr(gc.heads_[1][0].a_.get()),ICL));
        iva_ << temp2;
        Gecode::count(*this,a,temp1,cmp,temp2,ICL);
        return;
    }
    if (gc.type_==COUNT_GLOBAL)
    {
        IntVarArgs a(gc.heads_[0].size());
        for (size_t i = 0; i < gc.heads_[0].size(); ++i)
        {
            a[i] = expr(*this, generateLinearExpr(gc.heads_[0][i].a_.get()),ICL);
        }

        IntVarArgs b(gc.heads_[1].size());
        IntArgs c(gc.heads_[0].size());
        for (size_t i = 0; i < gc.heads_[1].size(); ++i)
        {
            b[i] = expr(*this, generateLinearExpr(gc.heads_[1][i].a_.get()),ICL);

            assert(gc.heads_[1][i].b_.isInteger());
            c[i] = gc.heads_[1][i].b_.getInteger();
        }

        iva_ << a;
        iva_ << b;
        Gecode::count(*this,a,b,c,ICL);

        return;
    }
    if (gc.type_==MINIMIZE_SET || gc.type_==MINIMIZE)
    {
        IntVarArgs a(gc.heads_[0].size());
        for (size_t i = 0; i < gc.heads_[0].size(); ++i)
        {
            a[i] = expr(*this, generateLinearExpr(gc.heads_[0][i].a_.get()),ICL);
        }

        IntVarArgs b(gc.heads_[1].size());
        IntArgs c(gc.heads_[0].size());
        for (size_t i = 0; i < gc.heads_[1].size(); ++i)
        {
            b[i] = expr(*this, generateLinearExpr(gc.heads_[1][i].a_.get()),ICL);

            assert(gc.heads_[1][i].b_.isInteger());
            c[i] = gc.heads_[1][i].b_.getInteger();
        }

        iva_ << a;
        iva_ << b;
        Gecode::count(*this,a,b,c,ICL);

        return;
    }
    assert(false);

}



}//namespace

