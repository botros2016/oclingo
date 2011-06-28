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

#include <clingcon/constraintpoolterm.h>
#include <gringo/prgvisitor.h>
#include <gringo/lit.h>
#include <gringo/litdep.h>

namespace Clingcon
{

	ConstraintPoolTerm::ConstraintPoolTerm(const Loc &loc, ConstraintTerm *a, ConstraintTerm *b)
		: ConstraintTerm(loc)
		, a_(a)
		, b_(b)
		, clone_(true)
	{
	}

	ConstraintPoolTerm::ConstraintPoolTerm(const Loc &loc, ConstraintTerm *a)
		: ConstraintTerm(loc)
		, a_(a)
		, b_(0)
		, clone_(true)
	{
	}

	void ConstraintPoolTerm::print(Storage *sto, std::ostream &out) const
	{
		if(b_.get())
		{
			a_->print(sto, out);
			out << ";";
			b_->print(sto, out);
		}
		else a_->print(sto, out);
	}

        void ConstraintPoolTerm::normalize(Lit *parent, const Ref &ref, Grounder *g, const Lit::Expander& expander, bool unify)
	{
		if(b_.get())
		{
			clone_ = false;
                        expander(parent->clone(), Lit::POOL);
			clone_ = true;
			ConstraintTerm *b = b_.get();
			ref.reset(b_.release());
			b->normalize(parent, ref, g, expander, unify);
		}
		else
		{
			ConstraintTerm *a = a_.get();
			ref.reset(a_.release());
			a->normalize(parent, ref, g, expander, unify);
		}
	}

        ConstraintPoolTerm *ConstraintPoolTerm::clone() const
	{
		if(clone_) { return new ConstraintPoolTerm(*this); }
		else       { return new ConstraintPoolTerm(loc(), a_.release()); }
	}

        GroundConstraint* ConstraintPoolTerm::toGroundConstraint(Grounder* )
	{
		assert(false);
                return 0;
	}

	ConstraintPoolTerm::~ConstraintPoolTerm()
	{
	}
}
