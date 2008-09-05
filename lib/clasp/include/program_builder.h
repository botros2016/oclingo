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

#ifndef CLASP_PROGRAM_BUILDER_H_INCLUDED
#define CLASP_PROGRAM_BUILDER_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/include/solver_types.h>
#include <clasp/include/program_rule.h>
#include <clasp/include/util/misc_types.h>
#include <string>
#include <map>
#include <algorithm>

namespace Clasp {

class Solver;
class ClauseCreator;
class PrgNode;
class PrgBodyNode;
class PrgAtomNode;
class ProgramBuilder;
class Preprocessor;
class DefaultUnfoundedCheck;
class MinimizeConstraint;
typedef PodVector<PrgAtomNode*>::type AtomList;
typedef PodVector<PrgBodyNode*>::type BodyList;
typedef PodVector<PrgNode*>::type NodeList;

/**
 * \defgroup problem Problem specification
 * Classes and functions for defining/processing logic programs
 */
//@{

//! A list of variables and their types
/*!
 * A VarList stores the type of each variable, which allows for easy variable-cloning.
 * It also stores flags for each literal to be used in "mark and test"-algorithms.
 */
class VarList {
public:
	VarList() { add(Var_t::atom_var); }
	//! adds a new var of type t to the list
	Var		add(VarType t) {
		vars_.push_back( static_cast<uint8>(t << 5) );
		return (Var)vars_.size()-1;
	}
	//! marks v as equivalent to both a body and an atom
	void	setAtomBody(Var v) { setFlag(v, eq_f); }
	//! adds to s all variables contained in this var list that are >= startVar 
	void	addTo(Solver& s, Var startVar);
	//! returns true if this var list is empty
	bool empty() const { return vars_.size() == 1; }
	//! returns the number of vars contained in this list
	VarVec::size_type size() const { return vars_.size(); }
	//! clears the var list
	void clear() { Vars().swap(vars_); add(Var_t::atom_var); }
	
	//! removes all vars > startVar
	void shrink(Var startVar) {
		startVar = std::max(Var(1), startVar);
		vars_.resize(startVar);
	}
	
	/*!
	 * \name Mark and Test
	 * Functions supporting O(1) "mark and test" operations on literals
	 */
	//@{
	bool marked(Literal p)			const	{ return hasFlag(p.var(), p.sign()?uint8(bm_f):uint8(bp_f)); }
	bool markedHead(Literal p)	const { return hasFlag(p.var(), p.sign()?uint8(hn_f):uint8(hp_f)); }
	void mark(Literal p)							{ setFlag(p.var(), p.sign()?uint8(bm_f):uint8(bp_f)); }
	void markHead(Literal p)					{ setFlag(p.var(), p.sign()?uint8(hn_f):uint8(hp_f)); }
	void unmark(Var v)								{ clearFlag(v, uint8(bp_f+bm_f)); }
	void unmarkHead(Var v)						{	clearFlag(v, uint8(hp_f+hn_f)); }
	//@}
private:
	typedef PodVector<uint8>::type Vars;
	enum Flags {
		bp_f	= 0x1u,
		bm_f	= 0x2u,
		hp_f	= 0x4u,
		hn_f	= 0x8u,
		eq_f	= 0x10u
	};
	void setFlag(Var v, uint8 f)			{ assert(v < vars_.size()); vars_[v] |= f; }
	void clearFlag(Var v, uint8 f)		{ assert(v < vars_.size()); vars_[v] &= ~f; }
	bool hasFlag(Var v, uint8 f) const{ assert(v < vars_.size()); return (vars_[v] & f) != 0; }
	VarType type(Var v) const { return VarType(vars_[v] >> 5); }
	Vars					vars_;
};

//! An atom of a logic program.
struct Atom {
	//! The default constructor creates an invalid literal
	Atom() : name("") , lit(negLit(sentVar)) { }
	std::string name;	/**< Name of the atom - typically generated by lparse */
	Literal lit;			/**< Corresponding literal in the solver */
};

//! Maps atom ids (as generated by lparse) to the corresponding variables in a solver.
typedef std::vector<Atom> AtomIndex;

//! For gathering program statistics
struct PreproStats {
	PreproStats() : bodies(0), sccs(0), ufsNodes(0) {
		eqs_[0] = eqs_[1] = eqs_[2] = 0;
	}
	void incEqs(VarType t)	{ ++eqs_[t-1]; }
	void incBAEqs()					{ ++eqs_[2]; }
	uint32 numEqs() const { return numEqs(Var_t::atom_var) + numEqs(Var_t::body_var) + eqs_[2]; }
	uint32 numEqs(VarType t) const { return eqs_[t-1]; }
	void moveTo(PreproStats& o) {
		index.swap(o.index);
		o.bodies = bodies;
		o.sccs = sccs;
		o.ufsNodes = ufsNodes;
		o.eqs_[0] = eqs_[0]; o.eqs_[1] = eqs_[1]; o.eqs_[2] = eqs_[2];
	}
	AtomIndex	index;	/**< Mapping of Atoms to literals in the solver */
	uint32 bodies;		/**< How many body-objects were created? */
	uint32 sccs;			/**< How many strongly connected components? */
	uint32 ufsNodes;	/**< How many nodes in the positive BADG? */
private:
	uint32 eqs_[3];		// how many equivalences?
};

//! Interface for defining a logic program.
/*!
 * Use this class to specify a logic program. Once the program is completly defined,
 * call endProgram to load the logic program into a Solver-object.
 * 
 */
class ProgramBuilder {
public:
	ProgramBuilder();
	~ProgramBuilder();

	//! disposes (parts of) the internal representation of the logic program.
	/*!
	 * \param forceFullDispose If set to true, the whole program is disposed. Otherwise,
	 * if updateProgram() was called, only the last incremental step is disposed.
	 */
	void disposeProgram(bool forceFullDispose);
	
	//! Starts the definition of a logic program
	/*!
	 * This function shall be called exactly once before any rules or atoms are added.
	 * \param ufs The object to use for unfounded set detection. If 0, unfounded set detection is disabled.
	 * \param eqIters run eq-preprocessor for at most eqIters passes
	 * \note eqIters == -1 means run to fixpoint. eqIters == 0 disables eq preprocessing
	 * \note If ufsChecker is not 0 it shall point to a heap-allocated object. The ownership is transferred!
	 */
	ProgramBuilder& startProgram(DefaultUnfoundedCheck* ufs, uint32 eqIters = uint32(-1));

	//! Unfreezes a program and starts the next step if program is defined incrementally
	/*!
	 * If a program is to be defined incrementally, this function must be called
	 * exactly once for each step before any new rules or atoms are added.
	 * \note Program update only works correctly under the following assumptions:
	 *	- Atoms introduced in step i are either:
	 *		- solely defined in step i OR,
	 *		- marked as frozen in step i and solely defined in step i+k OR,
	 *		- forced to false by a acompute statement in step 0
	 *	- Atom ids are monotonically increasing, i.e. atoms defined in step i have lower ids than those defined in step i+k.
	 *	- Rule bodies are either empty or contain at least one atom defined in the current step
	 */
	ProgramBuilder& updateProgram();
	
	//! creates a new atom and returns the new atom's id.
	/*!
	 * \pre The program is not frozen
	 */
	Var newAtom();
	//! returns the number of atoms currently defined in the logic program
	uint32 numAtoms() const { return (uint32)atoms_.size(); }

	//! Sets the name of the Atom with the given id.
	/*!
	 * \param atomId The id of the atom for which a name should be set
	 * \param name The new name of the atom with the given id.
	 * \pre name != 0 && atomId > 0 && atomId < varMax
	 * \note if atomId is not yet known, an atom with the given id is implicitly created.
	 */
	ProgramBuilder& setAtomName(Var atomId, const char* name);

	//! forces the atom with the given atom id to true/false.
	/*!
	 * \param atomId Id of the Atom for which a truth-value should be set.
	 * \param pos If true, atom is set to true (forced to be in every answer set). Otherwise
	 * atom is set to false (not part of any answer set).
	 */
	ProgramBuilder& setCompute(Var atomId, bool pos);

	//! Marks the atom with the given atom id as frozen.
	/*!
	 * \note An atom a stays frozen until unfreeze(a) is called.
	 * \note As long as an atom is frozen it shall not be defined.
	 */
	ProgramBuilder& freeze(Var atomId);

	//! Unfreezes a previously frozen atom.
	/*!
	 * \pre atomId must refer to an atom that is currently frozen.
	 */
	ProgramBuilder& unfreeze(Var atomId);
	
	//! Starts the construction of a rule.
	/*! 
	 * \param t The type of the new rule.
	 * \param bound The lower bound (resp. min weight) of the rule to be created.
	 * \pre no rule is active
	 *
	 * \note the bound-parameter is only interpreted if the rule to be created is
	 * either a constraint- or a weight-rule.
	 */
	ProgramBuilder& startRule(RuleType t = BASICRULE, weight_t bound = -1) {
		rule_.clear();
		rule_.setType(t);
		if ((t == CONSTRAINTRULE || t == WEIGHTRULE) && bound > 0) {
			rule_.setBound(bound);
		}
		return *this;
	}

	//! Sets the bound (resp. min weight) of the currently active rule.
	/*!
	 * \param bound The lower bound (resp. min weight) of the rule to be created.
	 * \pre The active rule is either a constraint or weight rule.
	 */
	ProgramBuilder& setBound(weight_t bound) { // only valid for CONSTRAINT and WEIGHTRULES
		rule_.setBound(bound);
		return *this;
	}

	//! Adds the atom with the given id as a head to the currently active rule.
	/*!
	 * \pre startRule was called && atomId > 0
	 * \note if atomId is not yet known, an atom with the given id is implicitly created.
	 */
	ProgramBuilder& addHead(Var atomId) {
		assert(atomId > 0);
		rule_.addHead(atomId);
		return *this;
	}
	
	//! Adds a subgoal to the currently active rule.
	/*!
	 * \param atomId The id of the atom to be added to the rule.
	 * \param pos true if the atom is positive. Fals otherwise
	 * \param weight The weight the new predecessor should have in the rule.
	 * \pre atomId > 0 && weight >= 0
	 * \note The weight parameter is only used if the active rule is a weight or optimize rule.
	 * \note if atomId is not yet known, an atom with the given id is implicitly created.
	 */
	ProgramBuilder& addToBody(Var atomId, bool pos, weight_t weight = 1) {
		rule_.addToBody(atomId, pos, weight);
		return *this;
	}
	
	//! Finishes the creation of the active rule and adds it to the program
	/*! 
	 * \pre The program is not frozen
	 * \note Must be called once for each rule.
	 */
	ProgramBuilder& endRule() {
		return addRule(rule_);
	}

	//! Adds the rule r to the program
	/*!
	 * \pre The program is not frozen
	 */
	ProgramBuilder& addRule(PrgRule& r);
	
	//! Transforms the rule r to a set of equivalent normal rules and adds this set to the program
	/*!
	 * \note Translation of rule may introduce new atoms
	 */
	uint32 addAsNormalRules(PrgRule& r, PrgRule::TransformationMode m = PrgRule::dynamic_transform);
	
	
	//! Finishes the definition of the logic program (or its current increment).
	/*!
	 * Call this method to load the program (increment) into the solver
	 *
	 * \param solver The solver object in which the optimized representation of the lp should be stored.
	 * \param initialLookahead if true and finalizeSolver is true, the program is simplified using a one-step lookahead operation.
	 * \param finalizeSolver if true, endProgram() calls solver.endAddConstraints()
	 * \return false if the program is initially conflicting, true otherwise.
	 *
	 * \note If endProgram returned false, the state of the ProgramBuilder object is undefined. In that case,
	 * only startProgram() and disposeProgram() remain valid operations.
	 *
	 * \note After endProgram was called, the program is considered "frozen". One shall not add atoms/rules
	 * to a frozen program. Call ProgramBuilder::updateProgram() to "unfreeze" a program.
	 * \note To load the same program into different solvers, call endProgram for each solver.
	 *
	 */
	bool endProgram(Solver& solver, bool initialLookahead = false, bool finalizeSolver = true);

	//! returns true if the program contains at least one minimize statement
	bool hasMinimize() const { return minimize_ != 0; }
	
	//! Creates a minimize constraints from the minimize statements contained in the program
	/*!
	 * \pre The program is frozen, ie. endProgram was already called.
	 * \return 0 if the program does not contain minimize statements
	 */
	MinimizeConstraint* createMinimize(Solver& s);

	/*!
	 * Writes the (possibly simplified) program in lparse-format to the given stream.
	 * \pre endProgram was called and the program is currently "frozen".
	 */
	void writeProgram(std::ostream& os);	// Pre: endProgram
	PreproStats stats;
private:
	ProgramBuilder(const ProgramBuilder&);
	ProgramBuilder& operator=(const ProgramBuilder&);
	friend class PrgRule;
	friend class PrgBodyNode;
	friend class PrgAtomNode;
	friend class Preprocessor;
	class CycleChecker;
	typedef std::multimap<uint32, uint32> BodyIndex; // hash -> bodies[offset]
	typedef std::pair<BodyIndex::iterator, BodyIndex::iterator> BodyRange;
	typedef std::map<uint32, uint32> EqNodes;
	typedef std::pair<PrgBodyNode*, uint32> Body;
	// ------------------------------------------------------------------------
	// Program definition
	PrgAtomNode*	resize(Var atomId);
	void					addRuleImpl(const PrgRule& r, const PrgRule::RData& rd);
	void					clearRuleState(const PrgRule& r);
	Body					findOrCreateBody(const PrgRule& r, const PrgRule::RData& rd);
	void					addEq(Var x, Var root) {
		eqAtoms_.insert( EqNodes::value_type(x, root) );
		stats.incEqs(Var_t::atom_var);
	}
	uint32				getEqAtom(uint32 atomId) const {
		EqNodes::const_iterator it = eqAtoms_.find(atomId);
		return it != eqAtoms_.end()
			? it->second
			: atomId;
	}
	bool					applyCompute();
	void					setConflict();
	void					updateFrozenAtoms(const Solver&);
	// ------------------------------------------------------------------------
	// Nogood creation
	void cloneVars(Solver& s);
	void minimize(Solver& s);
	bool addConstraints(Solver& s, CycleChecker& c, uint32 startAtom);
	void freezeMinimize(Solver&);
	// ------------------------------------------------------------------------
	void writeRule(PrgBodyNode*, uint32 h, std::ostream&);

	PrgRule				rule_;				// active rule
	RuleState			ruleState_;		// which atoms appear in the active rule?
	BodyIndex			bodyIndex_;		// hash	-> body
	BodyList			bodies_;			// all bodies
	AtomList			atoms_;				// all atoms
	EqNodes				eqAtoms_;			// atoms that are equivalent
	VarVec				initialSupp_;	// bodies that are (initially) supported
	VarVec				computeFalse_;// atoms that are forced to false
	VarList				vars_;				// created vars
	struct MinimizeRule {
		WeightLitVec lits_;
		MinimizeRule* next_;
	} *minimize_;								// list of minimize-rules
	struct Incremental	{
		Incremental();
		uint32	startAtom_;				// first atom of current iteration
		uint32	startVar_;				// first var of the current iteration
		VarVec	freeze_;					// list of frozen atoms
		VarVec	unfreeze_;				// list of atom that are unfreezed in this iteration
	}* incData_;								// additional state to handle incrementally defined programs 

	typedef SingleOwnerPtr<DefaultUnfoundedCheck> Ufs;
	Ufs						ufs_;					// Unfounded set checker to use if program is not tight
	uint32				eqIters_;			// process body/atom equalities
	bool					frozen_;			// is the program currently frozen?
};


//! A node of a body-atom-dependency graph.
class PrgNode {
public:
	static const uint32 noScc = (1U << 30) - 1;
	
	//! creates a new node
	/*!
	 * \note The ctor creates a node that corresponds to a literal that is false
	 */
	explicit PrgNode(Literal lit = negLit(sentVar)) 
		: lit_(lit), scc_(noScc), value_(value_free), root_(0), seen_(0), ignore_(0) {}
	
	//! returns true if node has an associated variable in a solver
	bool		hasVar()		const { return lit_ != negLit(sentVar); }
	//! returns the variable associated with this node or sentVar if no var is associated with this node
	Var			var()				const { return lit_.var(); }
	//! returns the literal associated with this node or a sentinel literal if no var is associated with this node
	Literal literal()		const { return lit_; }
	
	//! returns the node's component number. 
	/*!
	 * Returns the node's component number. 
	 * Returns noScc if node is only trivially connected or 
	 * sccs where not yet identified
	 */
	uint32		scc()				const { return scc_; }
	uint32		root()			const { return root_; }
	uint32		visited()		const { return seen_; }
	bool			ignore()		const { return ignore_ != 0; }
	ValueRep	value()			const	{ return value_; }
	//! returns the literal that must be true in order to fulfill the truth-value of this node
	/*!
	 * \note if value() == value_free, the special sentinel literal that is true
	 * in every solver is returned.
	 *
	 * \return
	 *	- if value() == value_free : posLit(sentVar)
	 *  - if value() == value_true : literal()
	 *	- if value() == value_false: ~literal()
	 */
	Literal		trueLit()		const	{ 
		if (value_ != value_free) {
			return value_ == value_true ? lit_ : ~lit_;
		}
		return Literal();
	}
	void		setScc(uint32 scc) {
		assert(scc <= noScc);
		scc_ = scc;
	}
	void		setRoot(uint32 r) {
		assert(r < (1U << 30));
		root_ = r;
	}
	void		setVisited(bool v)	{ seen_		= (uint32)v; }
	void		setIgnore(bool b)		{ ignore_ = (uint32)b; }
	bool		setValue(ValueRep v){
		if ((value_ ^ v) == 3) return false;
		value_ = v;
		return true;
	}
	void		resetSccFlags() {
		scc_		= noScc;
		root_		= 0;
		seen_		= 0;
	}
	void		clearVar(bool clVal)	{ lit_ = negLit(sentVar); if (clVal) value_ = value_free; }
	void		setLiteral(Literal x)	{ lit_ = x; }
private:
	PrgNode(const PrgNode&);
	PrgNode& operator=(const PrgNode&);
	Literal lit_;					// associated literal in the solver
	uint32	scc_		: 30;	// component id of this node (noScc if trivially connected)
	uint32	value_	: 2;	// truth-value assigned to the node (either compute or derived during preprocessing)
	uint32	root_		: 30;	// used twofold:
												//	- SCC-check: depth search index used to find root of a SCC
												// 	- Afterwards: Index of this node in the unfounded set checker												
	uint32	seen_		: 1;	// SCC-check:	true if node was already visited
	uint32	ignore_	: 1;	// meaning depends on the node's type
    										//	- body: set true if body is no longer relevant (e.g. one of its subgoals is known to be false)
    										//	- atom: set true, if atom can be ignored during ufs init. Either because
												// it has a satisfied support and thus can't become unfounded or because it was already added to the ufs-checker
};

//! A body-node represents a rule-body in a body-atom-dependency graph.
class PrgBodyNode : public PrgNode {
public:
	//! creates a new body node and connects the node to all its heads and predecessors
	/*!
	 * \param id			The id of the new body object
	 * \param rule		The rule for which this body object is created
	 * \param rInfo		The rule's simplification object
	 * \param prg			The program in which the new body is used
	 */
	PrgBodyNode(uint32 id, const PrgRule& rule, const PrgRule::RData& rInfo, ProgramBuilder& prg);
	~PrgBodyNode();
	
	//! returns the type of the body node.
	RuleType type() const;
	
	bool resetSupported() {
		if (type() == BASICRULE || type() == CHOICERULE) {
			return (unsupp_ = posSize()) == 0;
		}
		else if (type() == CONSTRAINTRULE) {
			return (unsupp_ = bound() - negSize()) <= 0;
		}
		else if (type() == WEIGHTRULE) {
			weight_t snw = 0;
			for (uint32 i = 0; i != negSize(); ++i) {
				snw += weight(i, false);
			}
			return (unsupp_ = bound() - negSize()) <= 0;
		}
		return false;
	}

	//! returns true if the body node is supported.
	/*!
	 * A body of a basic- or choice-rule is supported, iff all of its positive subgoals are supported.
	 * A body of a weight-rule is supported if the sum of the weights of the supported positive +
	 * the sum of the negative weights is >= lowerBound().
	 */
	bool isSupported() const { return unsupp_ <= 0; }

	//! notifies the body node about the fact that its positive subgoal v is supported
	/*!
	 * \return
	 *	- true if the body is now also supported
	 *  - false otherwise
	 *  .
	 */
	bool onPosPredSupported(Var /* v */);
	
	//! simplifies the body, i.e. its predecessors-lists
	/*!
	 * - removes true/false atoms from B+/B- resp.
	 * - removes/merges duplicate subgoals
	 * - checks whether body must be false (e.g. contains false/true atoms in B+/B- resp. or contains p and ~p)
	 * - computes a new hash value
	 *
	 * \param prg The program in which this body is used
	 * \param bodyId The body's id in the program
	 * \param[out] a pair of hash-values. hashes.first is the old hash value. hash.second the new hash value.
	 * \param pre The preprocessor that changed this body
	 * \param strong If true, treats atoms that have no variable associated as false. Otherwise
	 *							 such atoms are ignored during simplification
	 * \note If body must be false, calls setValue(value_false) and sets the
	 * ignore flag to true.
	 * \return
	 *	- true if simplification was successful
	 *	- false if simplification detected a conflict
	 */
	bool simplifyBody(ProgramBuilder& prg, uint32 bodyId, std::pair<uint32, uint32>& hashes, Preprocessor& pre, bool strong);
	
	//! simplifies the heads of this body
	/*!
	 * Removes superfluous heads and sets the body to false if for some atom a
	 * in the head of this body, Ta -> FB. In that case, all heads atoms are removed, because
	 * a false body can't define an atom.
	 * If strong is true, removes head atoms that are not associated with a variable.
	 * \return 
	 *	- setValue(value_false) if setting a head of this body to true would
	 *	make the body false (i.e. the body is a selfblocker)
	 *	- true otherwise
	 */
	bool simplifyHeads(ProgramBuilder& prg, Preprocessor& pre, bool strong);

	//! adds the body-oriented nogoods as a set of constraints to the solver.
	/*
	 * \return false on conflict
	 */
	bool toConstraint(Solver&, ClauseCreator& c, const ProgramBuilder& prg);
	
	//! returns the bound of this body
	/*!
	 * \note The bound of a basic- or choice-rule is the equal to the size of the body
	 */
	weight_t bound() const;

	//! returns the number of atoms in the body
	uint32 size() const { return size_; }
	//! returns the number of positive atoms in the body
	uint32 posSize() const { return posSize_; }
	//! returns the number of negative atoms in the body
	uint32 negSize() const { return size() - posSize(); }

	//! returns the idx'th positive subgoal
	/*! 
	 * \pre idx < posSize()
	 * \note first positive subgoal has index 0
	 */
	Var pos(uint32 idx) const { assert(idx < posSize()); return goals_[idx].var(); }

	//! returns the idx'th negative subgoal
	/*! 
	 * \pre idx < negSize()
	 * \note first negative subgoal has index 0
	 */
	Var neg(uint32 idx) const { assert(idx < negSize()); return goals_[posSize_+idx].var(); }

	//! returns the weight of the specified subgoal.
	/*!
	 * \param idx the index of the subgoal
	 * \param pos true for a positive, false for a negative subgoal
	 */
	weight_t weight(uint32 /* idx */, bool /* pos */) const;
	
	//! establishes set property for the heads of this body
	void buildHeadSet();
	
	//! returns true if x is a head of this body
	/*!
	 * \pre buildHeadSet() was called
	 **/
	bool hasHead(Var x) const {
		return std::binary_search(heads.begin(), heads.end(), x);
	}

	//! returns true if *this and other are equivalent w.r.t their predecessors
	bool equal(const PrgBodyNode& other) const;
	
	bool compatibleType(PrgBodyNode* other) const {
		return type() == other->type()
			|| (type() != CHOICERULE && other->type() != CHOICERULE);
	}
	
	VarVec		heads;					// successors of this body	
private:
	PrgBodyNode(const PrgBodyNode&);
	PrgBodyNode& operator=(const PrgBodyNode&);
	struct Extended;
	friend struct Extended;
	struct Weights {
		Weights(const PrgBodyNode& self) : self_(&self) {}
		weight_t operator()(Literal p) const {
			if (self_->type() == WEIGHTRULE) {
				bool pos = p.sign() == false;
				for (uint32 i = pos?0:self_->posSize(), end = pos?self_->posSize():self_->size(); i != end; ++i) {
					if (self_->goals_[i].var() == p.var()) {
						return self_->extended_->weights_[i];
					}
				}
				assert(false);
			}
			return 1;
		}
		const PrgBodyNode* self_;
	};
	bool addPredecessorClauses(Solver& s, ClauseCreator& c, const AtomList&);
	weight_t	weight(uint32 idx) const {
		return extended_ == 0 || extended_->weights_ == 0 ? 1 : extended_->weights_[idx];
	}
	weight_t	findWeight(Literal p, const AtomList& progAtoms) const;
	uint32		findLit(Literal p, const AtomList& progAtoms) const;
	weight_t	sumWeights() const;
	Literal*	goals_;					// B+: [0, posSize_), B-: [posSize_, size_)
	uint32		size_;					// |B|
	uint32		posSize_	: 31;	// |B+|
	uint32		choice_		: 1;	// choice rule body?
	weight_t	unsupp_;				// <= 0 -> body is supported
	struct Extended {
		Extended(PrgBodyNode* self, uint32 bound, bool weights);
		~Extended();
		RuleType type() const {
			return weights_ == 0 ? CONSTRAINTRULE : WEIGHTRULE;
		}
		weight_t	sumWeights_;
		weight_t	bound_;
		weight_t*	weights_;	
	}*extended_;
};

//! An atom-node in a body-atom-dependency graph
class PrgAtomNode : public PrgNode {
public:
	//! adds the atom-oriented nogoods to as a set of constraints to the solver.
	/*
	 * \return false on conflict
	 */
	bool toConstraint(Solver&, ClauseCreator& c, ProgramBuilder& prg);
	
	//! simplifies this atom, i.e. its predecessors-list
	/*!
	 * - removes false/irrelevant bodies
	 *
	 * \param atomId The id of this atom
	 * \param prg The program in which this atom is defined
	 * \param pre The preprocessor that changed this atom or its predecessors
	 * \param strong If true, updates bodies that were replaced with equivalent bodies
	 * \note If atom must be false, calls setValue(value_false) and sets the
	 * ignore flag to true.
	 * \return
	 *	- true if simplification was successful
	 *	- false if simplification detected a conflict
	 *	- if strong, the second return value is the number of different literals associated with the bodies of this atom
	 */
	typedef std::pair<bool, uint32> SimpRes;
	SimpRes simplifyBodies(Var atomId, ProgramBuilder& prg, const Preprocessor& pre, bool strong);
	VarVec		posDep;			// Bodies in which this atom occurs positively
	VarVec		negDep;			// Bodies in which this atom occurs negatively
	VarVec		preds;			// Bodies having this atom as head
};
//@}
}
#endif
