// Copyright (c) 2010, Torsten Grote <tgrote@uni-potsdam.de>
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

#include <tr1/unordered_map>
#include "clingo/claspoutput.h"

class ExternalKnowledge;

class oClaspOutput : public iClaspOutput
{
public:
	oClaspOutput(Grounder* grounder, Clasp::Solver* solver, bool shiftDisj, uint32_t port);
	~oClaspOutput();
	ExternalKnowledge& getExternalKnowledge();
	void startExtInput();
	void stopExtInput();
	void printBasicRule(uint32_t head, const AtomVec &pos, const AtomVec &neg);
	void unfreezeAtom(uint32_t symbol);
	uint32_t getVolAtom();
	uint32_t getVolWindowAtom(int window);
	uint32_t getVolAtomAss();
	VarVec& getVolAtomFalseAss();
	void finalizeVolAtom();
	void deprecateVolAtom();
	void unfreezeOldVolAtoms();
protected:
	void doFinalize();
	void printExternalTableEntry(const AtomRef &atom, uint32_t arity, const std::string &name);
private:
	uint32_t unnamedSymbol();
	ExternalKnowledge* ext_;
	bool ext_input_;
	uint32_t vol_atom_;
	uint32_t vol_atom_frozen_;
	VarVec vol_atoms_old_;

	std::tr1::unordered_map<int, uint32_t> vol_atom_map_;
};
