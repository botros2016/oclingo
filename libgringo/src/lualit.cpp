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

#include <gringo/lualit.h>
#include <gringo/grounder.h>
#include <gringo/varterm.h>
#include <gringo/index.h>
#include <gringo/formula.h>
#include <gringo/instantiator.h>
#include <gringo/prgvisitor.h>

LuaLit::LuaLit(const Loc &loc, VarTerm *var, TermPtrVec &args, uint32_t name, int luaIndex)
	: Lit(loc)
	, var_(var)
	, args_(args.release())
	, name_(name)
	, luaIndex_(luaIndex)
{
}

bool LuaLit::match(Grounder *grounder)
{
	ValVec vals;
	ValVec args;
	foreach(Term &term, args_) { args.push_back(term.val(grounder)); }
	grounder->luaCall(this, args, vals);
	return std::find(vals.begin(), vals.end(), var_->val(grounder)) != vals.end();
}

namespace
{
	class LuaIndex : public StaticIndex
	{
	public:
		LuaIndex(uint32_t var, LuaLit *lit, const VarVec &bind);
		bool first(Grounder *grounder, int binder);
		bool next(Grounder *grounder, int binder);
	private:
		uint32_t         var_;
		ValVec           vals_;
		ValVec::iterator current_;
		LuaLit          *lit_;
		VarVec           bind_;
	};

	LuaIndex::LuaIndex(uint32_t var, LuaLit *lit, const VarVec &bind)
		: var_(var)
		, lit_(lit)
		, bind_(bind)
	{
	}

	bool LuaIndex::first(Grounder *grounder, int binder)
	{
		ValVec args;
		vals_.clear();
		foreach(const Term &term, lit_->args()) { args.push_back(term.val(grounder)); }
		grounder->luaCall(lit_, args, vals_);
		current_ = vals_.begin();
		return next(grounder, binder);
	}

	bool LuaIndex::next(Grounder *grounder, int binder)
	{
		if(current_ != vals_.end())
		{
			grounder->val(var_, *current_++, binder);
			return true;
		}
		else { return false; }
	}
}

Index *LuaLit::index(Grounder *, Formula *, VarSet &bound)
{
	VarSet vars;
	VarVec bind;
	var_->vars(vars);
	std::set_difference(vars.begin(), vars.end(), bound.begin(), bound.end(), std::back_insert_iterator<VarVec>(bind));
	if(bind.size() > 0)
	{
		LuaIndex *p = new LuaIndex(var_->index(), this, bind);
		bound.insert(bind.begin(), bind.end());
		return p;
	}
	else { return new MatchIndex(this); }
}

void LuaLit::accept(Printer *v)
{
	(void)v;
}

void LuaLit::visit(PrgVisitor *v)
{
	v->visit(var_.get(), true);
	foreach(Term &term, args_) { term.visit(v, false); }
}

void LuaLit::print(Storage *sto, std::ostream &out) const
{
	out << "#call(";
	var_->print(sto, out);
	out << ",@" << sto->string(name_) << "(";
	bool comma = false;
	foreach(const Term &term, args_)
	{
		if(comma) { out << ","; }
		else      { comma = true; }
		term.print(sto, out);
	}
	out << "))";
}

void LuaLit::normalize(Grounder *g, const Expander &e)
{
	for(TermPtrVec::iterator it = args_.begin(); it != args_.end(); it++)
	{
		it->normalize(this, Term::VecRef(args_, it), g, e, false);
	}
}

Lit *LuaLit::clone() const
{
	return new LuaLit(*this);
}

LuaLit::~LuaLit()
{
}

