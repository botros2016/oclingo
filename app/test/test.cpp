#define BOOST_TEST_MODULE GringoTest
#include <boost/test/unit_test.hpp>

#include <gringo/grounder.h>
#include <gringo/parser.h>
#include <gringo/inclit.h>
#include <gringo/streams.h>
#include "clingo/claspoutput.h"
#include <clasp/unfounded_check.h>
#include <clasp/solve_algorithms.h>
#include <clasp/model_enumerators.h>
#include <gringo/plainoutput.h>

#include <cstdarg>

struct Tester : public Clasp::Enumerator::Report
{
	Tester(std::string const &is, const char *x, ...)
	{
		// ground/solve
		{
			Clasp::ProgramBuilder pb;
			ClaspOutput o(false);
			TermExpansionPtr te(new TermExpansion());
			BodyOrderHeuristicPtr bo(new BasicBodyOrderHeuristic());
			Grounder g(&o, false, te, bo);
			Clasp::Solver s;
			Module *mb = g.createModule();
			Module *mc = g.createModule();
			Module *mv = g.createModule();
			IncConfig ic;
			Streams in;
			Parser p(&g, mb, mc, mv, ic, in, false, false);
			Streams::StreamPtr sp(new std::stringstream(is));
			in.appendStream(sp, "test");
			o.setProgramBuilder(&pb);
			pb.startProgram(ai, new Clasp::DefaultUnfoundedCheck());
			o.initialize();
			p.parse();
			g.analyze();
			g.ground(*mb);
			g.ground(*mc);
			g.ground(*mv);
			o.finalize();
			pb.endProgram(s, true);
			Clasp::SolveParams csp;
			csp.setEnumerator(new Clasp::RecordEnumerator(this));
			csp.enumerator()->init(s, 0);
			Clasp::solve(s, csp);
		}
		// check
		{
			uint32_t n = 0;
			va_list vl;
			va_start(vl, x);
			while(x)
			{
				Model m;
				while(x)
				{
					m.insert(std::string(x));
					x = va_arg(vl, const char *);
				}
				BOOST_CHECK(std::find(models.begin(), models.end(), m) != models.end());
				n++;
				x = va_arg(vl, const char *);
			}
			BOOST_CHECK(models.size() == n);
			va_end(vl);
		}
	}

	void reportModel(const Clasp::Solver& s, const Clasp::Enumerator& self)
	{
		models.push_back(Model());
		for (Clasp::AtomIndex::const_iterator it = ai.begin(); it != ai.end(); it++)
		{
			if (!it->second.name.empty() && s.isTrue(it->second.lit))
			{
				models.back().insert(it->second.name);
			}
		}
	}

	void debug()
	{
		foreach (Model const &m, models)
		{
			std::cerr << "Model:";
			foreach (std::string const &s, m)
			{
				std::cerr << " " << s;
			}
			std::cerr << std::endl;
		}
	}

	typedef std::set<std::string> Model;

	Clasp::AtomIndex ai;
	std::vector<Model> models;
};

BOOST_AUTO_TEST_CASE( fail )
{
	Tester
	(
		"a :- not b.\n"
		"b :- not a.\n",

		"a", NULL,
		"b", NULL,
		NULL
	);
}