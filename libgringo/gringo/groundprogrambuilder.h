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
#include <gringo/aggrlit.h>
#include <gringo/predlitrep.h>

class GroundProgramBuilder
{
public:
	enum Type_Enum
	{
		LIT,
		TERM,
		AGGR_SUM,
		AGGR_COUNT,
		AGGR_AVG,
		AGGR_MIN,
		AGGR_MAX,
		AGGR_EVEN,
		AGGR_EVEN_SET,
		AGGR_ODD,
		AGGR_ODD_SET,
		AGGR_DISJUNCTION,
		STM_RULE,
		STM_CONSTRAINT,
		STM_SHOW,
		STM_HIDE,
		STM_EXTERNAL,
		STM_MINIMIZE,
		STM_MAXIMIZE,
		STM_MINIMIZE_SET,
		STM_MAXIMIZE_SET,
		STM_COMPUTE,
		META_GLOBALSHOW,
		META_GLOBALHIDE,
		META_SHOW,
		META_HIDE,
		META_EXTERNAL,
		USER
	};
	typedef uint16_t Type;

protected:
	struct Lit
	{
		static Lit create(Type type, uint32_t offset, uint32_t n)
		{
			Lit t;
			t.type   = type;
			t.sign   = false;
			t.offset = offset;
			t.n      = n;
			return t;
		}
		bool     sign;
		Type   type;
		uint32_t offset;
		uint32_t n;
	};
	typedef std::vector<Lit> LitVec;

	struct Literal : public PredLitRep
	{
		friend class GroundProgramBuilder;
		friend class OnlineParser;
		Literal() : PredLitRep(false, 0) { }
	};

public:
	struct Stack
	{
		Type          type;
		uint32_t      n;
		LitVec        lits;
		LitVec        aggrLits;
		ValVec        vals;
	};
	typedef std::auto_ptr<Stack> StackPtr;

public:
	GroundProgramBuilder(Output *output);
	virtual void add(Type type, uint32_t n = 0);
	// call instead of add and later use add(StackPtr stm)
	StackPtr get(Type type, uint32_t n);
	void add(StackPtr stm);
	void addVal(const Val &val);
	void addSign();
	Storage *storage();

protected:
	//void printAggrLits(AggrLit::Printer *printer, Lit &a, bool weight);
	void printLit(Printer *printer, uint32_t offset, bool head);
	PredLitRep *predLitRep(Lit &a);
	void pop(uint32_t n);
	void add();
	virtual void doAdd() { }

protected:
	Output  *output_;
	StackPtr stack_;
	Literal  lit_;
};
