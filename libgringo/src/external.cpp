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

#include <gringo/external.h>
#include <gringo/lit.h>
#include <gringo/term.h>
#include <gringo/output.h>
#include <gringo/predlit.h>
#include <gringo/grounder.h>
#include <gringo/prgvisitor.h>
#include <gringo/litdep.h>
#include <gringo/exceptions.h>
#include <gringo/instantiator.h>

External::External(const Loc &loc, PredLit *head, LitPtrVec &body)
	: Statement(loc)
	, head_(head)
	, body_(body.release())
{
	head_->head(true);
}

void External::ground(Grounder *g)
{
	inst_->ground(g);
}

void External::addDomain(Grounder *g)
{
	try { head_->addDomain(g, false); }
	catch(const AtomRedefinedException &ex)
	{
		std::stringstream ss;
		print(g, ss);
		throw ModularityException(StrLoc(g, loc()), ss.str(), ex.what());
	}
}

bool External::grounded(Grounder *g)
{
	head_->grounded(g);
	addDomain(g);
	Printer *printer = g->output()->printer<Printer>();
	printer->print(head_.get());
	return true;
}

namespace
{
	class HeadExpander : public Expander
	{
	public:
		HeadExpander(Grounder *g, External &d) : g_(g), d_(d) { }
		void expand(Lit *lit, Type type);
	private:
		Grounder *g_;
		External  &d_;
	};

	class BodyExpander : public Expander
	{
	public:
		BodyExpander(External &d) : d_(d) { }
		void expand(Lit *lit, Type type) { (void)type; d_.body().push_back(lit); }
	private:
		External &d_;
	};

	void HeadExpander::expand(Lit *lit, Expander::Type type)
	{
		switch(type)
		{
			case RANGE:
			case RELATION:
				d_.body().push_back(lit);
				break;
			case POOL:
				LitPtrVec body;
				foreach(Lit &l, d_.body()) body.push_back(l.clone());
				g_->addInternal(new External(d_.loc(), static_cast<PredLit*>(lit), body));
				break;
		}
	}
}

void External::normalize(Grounder *g)
{
	HeadExpander headExp(g, *this);
	BodyExpander bodyExp(*this);
	head_->normalize(g, &headExp);
	for(LitPtrVec::size_type i = 0; i < body_.size(); i++)
	{
		body_[i].normalize(g, &bodyExp);
	}
}

void External::init(Grounder *g, const VarSet &b)
{
	if(!inst_.get())
	{
		inst_.reset(new Instantiator(this));
		if(vars_.size() > 0) litDep_->order(g, b);
		else
		{
			VarSet bound(b);
			foreach(Lit &lit, body_)
			{
				lit.init(g, bound);
				lit.index(g, this, bound);
			}
			head_->init(g, bound);
			head_->index(g, this, bound);
		}
	}
	inst_->enqueue(g);
}

void External::visit(PrgVisitor *visitor)
{
	visitor->visit(head_.get(), false);
	foreach(Lit &lit, body_) visitor->visit(&lit, true);
}

void External::print(Storage *sto, std::ostream &out) const
{
	out << "#external ";
	head_->print(sto, out);
	std::vector<const Lit*> body;
	foreach(const Lit &lit, body_) { body.push_back(&lit); }
	std::sort(body.begin(), body.end(), Lit::cmpPos);
	foreach(const Lit *lit, body)
	{
		out << ":";
		lit->print(sto, out);
	}
	out << ".";
}

void External::append(Lit *lit)
{
	body_.push_back(lit);
}

External::~External()
{
}
