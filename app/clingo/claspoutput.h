// Copyright (c) 2010, Roland Kaminski <kaminski@cs.uni-potsdam.de>
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

#include <gringo/inclit.h>
#include <gringo/gringo.h>
#include <gringo/lparseconverter.h>
#include <clasp/program_builder.h>

class ClaspOutput : public LparseConverter
{
protected:
	typedef boost::unordered_map<int, uint32_t> VolMap;
	typedef boost::unordered_map<Val, uint32_t> AssertMap;
public:
	ClaspOutput(bool shiftDisj);
	virtual void initialize();
	virtual VolMap getVolUids() { return VolMap(); }
	virtual AssertMap getAssertUids() { return AssertMap(); }
	void setProgramBuilder(Clasp::ProgramBuilder* api) { b_ = api; }
	Clasp::ProgramBuilder &getProgramBuilder() { return *b_; }
	SymbolMap &symbolMap() { return symbolMap_; }
	ValRng vals(Domain *dom, uint32_t offset) const;
	~ClaspOutput();
protected:
	virtual void printBasicRule(uint32_t head, const AtomVec &pos, const AtomVec &neg);
	void printConstraintRule(uint32_t head, int32_t bound, const AtomVec &pos, const AtomVec &neg);
	void printChoiceRule(const AtomVec &head, const AtomVec &pos, const AtomVec &neg);
	void printWeightRule(uint32_t head, int32_t bound, const AtomVec &pos, const AtomVec &neg, const WeightVec &wPos, const WeightVec &wNeg);
	void printMinimizeRule(const AtomVec &pos, const AtomVec &neg, const WeightVec &wPos, const WeightVec &wNeg);
	void printDisjunctiveRule(const AtomVec &head, const AtomVec &pos, const AtomVec &neg);
	void printComputeRule(int models, const AtomVec &pos, const AtomVec &neg);
	void printSymbolTableEntry(uint32_t symbol, const std::string &name);
	void printExternalTableEntry(const Symbol &symbol);
	using LparseConverter::symbol;
	uint32_t symbol();
	virtual void doFinalize();
protected:
	Clasp::ProgramBuilder *b_;
};

class iClaspOutput : public ClaspOutput
{
public:
	iClaspOutput(bool shiftDisj, IncConfig &config);
	void initialize();
	uint32_t getNewVolUid(int step);
	virtual uint32_t getVolAtom(int vol_window);
	VolMap getVolUids();
	uint32_t getAssertAtom(Val term);
	AssertMap getAssertUids();
	void retractAtom(Val term);
protected:
	IncConfig &config_;
private:
	bool initialized;
	VolMap volUids_;
	AssertMap assertUids_;
};
