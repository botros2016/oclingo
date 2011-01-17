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

#include <gringo/grounder.h>
#include <gringo/statement.h>
#include <gringo/domain.h>
#include <gringo/printer.h>
#include <gringo/stmdep.h>
#include <gringo/constterm.h>
#include <gringo/output.h>
#include <gringo/predindex.h>
#include <gringo/exceptions.h>
#include <gringo/luaterm.h>
#include <gringo/inclit.h>
#include <gringo/predlit.h>

// ========================== LuaImpl ==========================

#ifdef WITH_LUA
#	include <lua_impl.h>
#else

class Grounder::LuaImpl
{
public:
	LuaImpl(Grounder *) { }
	void call(const LuaLit *, const ValVec &, ValVec &) { throw std::runtime_error("lua: gringo was build without lua support"); }
	int index(const Loc &, const char *) { throw std::runtime_error("lua: gringo was build without lua support"); return 0; }
	void exec(const Loc &, const std::string &) { throw std::runtime_error("lua: gringo was build without lua support"); }
	lua_State *state() { return 0; }
	void pushVal(const Val &) { }
};

#endif

// ========================== TermDepthExpansion ==========================

TermDepthExpansion::TermDepthExpansion(IncConfig &config)
	: config(config)
	, start(config.incStep)
{
}

bool TermDepthExpansion::limit(Grounder *g, const ValRng &rng, int32_t &offset) const
{
	bool found = false;
	offset = 0;
	foreach(const Val &val, rng)
	{
		if ( val.type == Val::FUNC )
		{
			int32_t depth = g->func(val.index).getDepth();
			if(depth >= config.incStep)
			{
				if(offset < depth) { offset = depth; }
				found = true;
			}
		}
	}
	return found;
}

void TermDepthExpansion::expand(Grounder *g)
{
	foreach (const DomainMap::const_reference &ref, g->domains())
	{
		for( ; start < config.incStep; start++)
		{
			const_cast<Domain*>(ref.second)->addOffset(start);
		}
	}
}

TermDepthExpansion::~TermDepthExpansion()
{
}

// ========================== Module ==========================

StatementRng Module::add(Grounder *g, Statement *s, bool optimizeEdb)
{
	size_t offset = statements_.size();
	g->setModule(this, optimizeEdb);
	g->addInternal(s);
	g->setModule(0, true);
	return StatementRng(statements_.begin() + offset, statements_.end());
}

void Module::beginComponent()
{
	components_.push_back(Component());
}

void Module::addToComponent(Grounder *g, Statement *stm)
{
	stm->check(g);
	components_.back().statements.push_back(stm);
}

void Module::endComponent()
{
	if(components_.back().statements.empty())
	{
		components_.pop_back();
	}
}

void Module::parent(Module *module)
{
	assert(!module->reachable(this));
	parent_.push_back(module);
}

bool Module::reachable(Module *module)
{
	if(module == this) { return true; }
	foreach(Module *parent, parent_) 
	{
		if(parent->reachable(module)) { return true; }
	}
	return false;
}

Module::~Module()
{
}

// ========================== Grounder ==========================

Grounder::Grounder(Output *output, bool debug, TermExpansionPtr exp, BodyOrderHeuristicPtr heuristic)
	: Storage(output)
	, internal_(0)
	, debug_(debug)
	, luaImpl_(new LuaImpl(this))
	, termExpansion_(exp)
	, heuristic_(heuristic)
	, current_(0)
	, optimizeEdb_(true)
{
}

void Grounder::luaExec(const Loc &loc, const std::string &s)
{
	luaImpl_->exec(loc, s);
}

int Grounder::luaIndex(const LuaTerm *term)
{
	return luaImpl_->index(term->loc(), string(term->name()).c_str());
}

void Grounder::luaCall(const LuaLit *lit, const ValVec &args, ValVec &vals)
{
	luaImpl_->call(lit, args, vals);
}

lua_State *Grounder::luaState()
{
	return luaImpl_->state();
}

void Grounder::luaPushVal(const Val &val)
{
	return luaImpl_->pushVal(val);
}

void Grounder::addMagic()
{
}

void Grounder::analyze(const std::string &depGraph, bool stats)
{
	// build dependency graph
	StmDep::Builder prgDep(this);
	foreach(Module &module, modules_) { prgDep.visit(&module); }
	prgDep.analyze(this);
	if(!depGraph.empty())
	{
		std::ofstream out(depGraph.c_str());
		prgDep.toDot(this, out);
	}
	// generate input statistics
	if(stats)
	{
		foreach(Module &module, modules_) 
		{
			foreach(Statement &s, module.statements()) { stats_.visit(&s); }
			stats_.numScc+= module.components_.size();
		}
		
		stats_.numPred = domains().size();
		size_t paramCount = 0;
		foreach(DomainMap::reference dom, const_cast<DomainMap&>(domains()))
		{
			paramCount += dom.second->arity();
			stats_.numPredVisible += output()->show(dom.second->nameId(),dom.second->arity());
		}
		stats_.avgPredParams = (stats_.numPred == 0) ? 0 : paramCount*1.0 / stats_.numPred;
		stats_.print(std::cerr);
	}
}

void Grounder::ground(Module &module)
{
	termExpansion().expand(this);
	foreach(Module::Component &component, module.components_)
	{
		if(debug_)
		{
			std::cerr << "% begin component (" << component.statements.size() << ")" << std::endl;
		}
		foreach(Statement *statement, component.statements)
		{
			// NOTE: this adds statements into the grounding queue
			statement->init(this);
			if(debug_)
			{
				//std::cerr << "% ";
				//statement->print(this, std::cerr);
				//std::cerr << std::endl;
			}
			ground_();
		}
	}
	output()->endGround();
	foreach(DomainMap::reference dom, const_cast<DomainMap&>(domains()))
	{
		dom.second->fix();
	}
}

void Grounder::ground_()
{
	while(!queue_.empty())
	{
		std::random_shuffle(queue_.begin(), queue_.end());
		Groundable *g = queue_.front();
		std::cerr << "% ";
		g->print(this, std::cerr);
		std::cerr << std::endl;

		queue_.pop_front();
		g->enqueued(false);
		g->ground(this);
	}
}

void Grounder::enqueue(Groundable *g)
{
	if(!g->enqueued())
	{
		g->enqueued(true);
		queue_.push_back(g);
	}
}

uint32_t Grounder::createVar()
{
	std::ostringstream oss;
	oss << "#I" << internal_++;
	std::string str(oss.str());
	return index(str);
}

void Grounder::externalStm(uint32_t nameId, uint32_t arity)
{
	domain(nameId, arity)->external(true);
	output()->external(nameId, arity);
}

BodyOrderHeuristic& Grounder::heuristic() const
{
	return *heuristic_;
}

void Grounder::addInternal(Statement *s)
{
	s->normalize(this);
	if(optimizeEdb_ && s->edbFact())
	{
		s->ground(this);
		delete s;
		stats_.addFact();
	}
	else { current_->statements_.push_back(s); }
}

void Grounder::setModule(Module *module, bool optimizeEdb)
{
	internal_    = 0;
	optimizeEdb_ = optimizeEdb;
	current_     = module;
}

Module *Grounder::createModule()
{
	modules_.push_back(new Module());
	return &modules_.back();
}

Grounder::~Grounder()
{
}
