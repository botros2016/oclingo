// Copyright (c) 2009, Roland Kaminski <kaminski@cs.uni-potsdam.de>
//
// This file is part of gringo.
//
// gringo is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// gringo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with gringo.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <gringo/gringo.h>
#include <gringo/lit.h>
#include <gringo/index.h>
#include <gringo/predlit.h>
#include <gringo/printer.h>
#include <gringo/groundable.h>
#include <gringo/valvecset.h>

class AggrLit : public Lit
{
public:
	class Printer : public ::Printer
	{
	public:
		virtual void weight(const Val &v) = 0;
		virtual ~Printer();
	};
	class AggrState
	{
		typedef std::map<uint32_t,ValVecSet> Sets;
	public:
		bool accumulate(const ValVec &set, bool fact);
		bool matches() const;
		bool fact() const;
	private:
		Sets sets_;
	protected:
		bool match_;
		bool fact_;
	};
	typedef boost::ptr_vector<AggrState> AggrStates;
public:
	AggrLit(const Loc &loc, CondLitVec &conds);

	void lower(Term *l);
	void upper(Term *u);
	void assign(Term *a);
	Term *assign() const;
	void sign(bool s);

	bool hasNew() const;
	void enqueue(bool enqueue);
	void enqueueParent(Grounder *g);
	AggrStates &states();
	void bind(Grounder *g, uint32_t offset);
	void unbind(Grounder *g);

	void add(CondLit *cond);
	const CondLitVec &conds();

	~AggrLit();

	// Lit interface
	virtual void normalize(Grounder *g, Expander *expander);

	void doHead(bool head);

	bool match(Grounder *grounder);
	bool fact() const;

	bool complete() const;

	virtual Monotonicity monotonicity();

	void grounded(Grounder *grounder);
	void addDomain(Grounder *g, bool fact);
	void finish(Grounder *g);

	virtual void index(Grounder *g, Groundable *gr, VarSet &bound) = 0;
	Score score(Grounder *g, VarSet &bound);

	void visit(PrgVisitor *visitor);

protected:
	bool            sign_;
	bool            assign_;
	clone_ptr<Term> lower_;
	clone_ptr<Term> upper_;
	Groundable     *parent_;
	CondLitVec      conds_;
	VarVec          bound_;
	AggrStates      aggrStates_;
	AggrState      *last_;
	uint32_t        enqueued_;
	ValVecSet       index_;
};

class SetLit : public Lit
{
public:
	SetLit(const Loc &loc, TermPtrVec &terms);

	TermPtrVec *terms() { return &terms_; }

	bool match(Grounder *) { return true; }
	bool fact() const { return true; }

	void addDomain(Grounder *, bool) { assert(false); }
	void index(Grounder *, Groundable *, VarSet &) { }
	bool edbFact() const { return false; }
	void normalize(Grounder *g, Expander *e);
	void visit(PrgVisitor *visitor);
	void print(Storage *sto, std::ostream &out) const;
	void accept(Printer *v) { (void)v; }
	Lit *clone() const;
	~SetLit();
private:
	TermPtrVec terms_; //! determines the uniqueness + holds the weight
};

SetLit* new_clone(const SetLit& a);

class CondLit : public Groundable, public Locateable
{
public:
	enum Style { STYLE_DLV, STYLE_LPARSE, STYLE_LPARSE_SET };
	friend class AggrLit;
public:
	CondLit(const Loc &loc, TermPtrVec &terms, LitPtrVec &lits, Style style);
	AggrLit *aggr();
	TermPtrVec *terms();
	LitPtrVec *lits();
	Style style();
	bool complete() const;
	void add(Lit *lit) { lits_.push_back(lit); }
	void head(bool head);

	void doEnqueue(bool enqueue);
	void aggrEnqueue(Grounder *g);
	void ground(Grounder *g);
	bool grounded(Grounder *g);

	void normalize(Grounder *g, uint32_t number);
	void visit(PrgVisitor *visitor);
	void print(Storage *sto, std::ostream &out) const;
	void accept(AggrLit::Printer *v);
	void addDomain(Grounder *g, bool fact) ;
	~CondLit();
private:
	Style      style_;
	SetLit     set_;
	LitPtrVec  lits_;
	AggrLit   *aggr_;
	uint32_t   offset_;
};

/////////////////////////////// AggrLit::Printer ///////////////////////////////

inline AggrLit::Printer::~Printer() { }

/////////////////////////////// AggrLit::AggrState ///////////////////////////////

inline bool AggrLit::AggrState::matches() const { return match_; }

inline bool AggrLit::AggrState::fact() const { return fact_; }

/////////////////////////////// AggrLit ///////////////////////////////

inline Term *AggrLit::assign() const
{
	assert(assign_);
	return lower_.get();
}
inline void AggrLit::sign(bool s) { sign_ = s; }

inline const CondLitVec &AggrLit::conds() { return conds_; }

inline void AggrLit::enqueue(bool enqueue)
{
	if(enqueue) { enqueued_++; }
	else        { enqueued_--; }
}

inline AggrLit::AggrStates &AggrLit::states() { return aggrStates_; }

inline Lit::Monotonicity AggrLit::monotonicity() { return NONMONOTONE; }

inline void AggrLit::grounded(Grounder *) { }

inline Lit::Score AggrLit::score(Grounder *, VarSet &) { return Score(LOWEST, std::numeric_limits<double>::max()); }

/////////////////////////////// CondLit ///////////////////////////////

inline AggrLit *CondLit::aggr() { return aggr_; }

inline TermPtrVec *CondLit::terms() { return set_.terms(); }

inline LitPtrVec *CondLit::lits() { return &lits_; }

inline CondLit::Style CondLit::style() { return style_;  }

