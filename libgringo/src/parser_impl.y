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

%include {
#include "parser_impl.h"
#include "gringo/gringo.h"
#include "gringo/parser.h"
#include "gringo/grounder.h"
#include "gringo/output.h"
#include "gringo/domain.h"

#include "gringo/rule.h"
#include "gringo/optimize.h"
#include "gringo/show.h"
#include "gringo/external.h"
#include "gringo/compute.h"

#include "gringo/predlit.h"
#include "gringo/rellit.h"
#include "gringo/booleanlit.h"

#include "gringo/sumaggrlit.h"
#include "gringo/avgaggrlit.h"
#include "gringo/junctionlit.h"
#include "gringo/minmaxaggrlit.h"
#include "gringo/parityaggrlit.h"

#include "gringo/constterm.h"
#include "gringo/varterm.h"
#include "gringo/mathterm.h"
#include "gringo/poolterm.h"
#include "gringo/argterm.h"
#include "gringo/rangeterm.h"
#include "gringo/functerm.h"
#include "gringo/luaterm.h"

#define GRD pParser->grounder()
#define OTP pParser->grounder()->output()
#define ONE(loc) new ConstTerm(loc, Val::number(1))
#define ZERO(loc) new ConstTerm(loc, Val::number(0))
#define MINUSONE(loc) new ConstTerm(loc, Val::number(-1))

template <class T>
void del(T x)
{
	delete x;
}

template <class T>
boost::ptr_vector<T> *vec1(T *x)
{
	boost::ptr_vector<T> *v = new boost::ptr_vector<T>();
	v->push_back(x);
	return v;
}

}

%name parser
%stack_size 0

%parse_failure { pParser->parseError(); }
%syntax_error  { pParser->syntaxError(); }

%extra_argument { Parser *pParser }
%token_type { Parser::Token }
%token_destructor { pParser = pParser; }
%token_prefix PARSER_

%start_symbol start

%type lcmp { bool }
%type gcmp { bool }

%type rule { Rule* }
%destructor Rule { del($$); }

%type weak { Optimize* }
%destructor weak { del($$); }

%type var_list { VarSigVec* }
%destructor var_list { del($$); }

%type priolit { Optimize* }
%destructor priolit { del($$); }

%type weightedpriolit { Optimize* }
%destructor weightedpriolit { del($$); }

%type computelit { Compute* }
%destructor computelit { del($$); }

%type head         { Lit* }
%type lit          { Lit* }
%type literal      { Lit* }
%type body_literal { Lit* }
%type conjunction  { Lit* }
%type disjunction  { Lit* }
%destructor head         { del($$); }
%destructor lit          { del($$); }
%destructor literal      { del($$); }
%destructor body_literal { del($$); }
%destructor conjunction  { del($$); }
%destructor disjunction  { del($$); }

%type predicate { PredLit* }
%type predlit   { PredLit* }
%destructor predlit   { del($$); }
%destructor predicate { del($$); }

%type cmp { RelLit::Type }

%type body  { LitPtrVec* }
%type nbody { LitPtrVec* }
%type cond  { LitPtrVec* }
%type ncond { LitPtrVec* }
%destructor body  { del($$); }
%destructor nbody { del($$); }
%destructor cond  { del($$); }
%destructor ncond { del($$); }

%type op { MathTerm::Func* }
%destructor op { del($$); }

%type term { Term* }
%destructor term { del($$); }

%type func { FuncTerm* }
%destructor func { del($$); }

%type termlist  { TermPtrVec* }
%type nsetterm  { TermPtrVec* }
%type setterm   { TermPtrVec* }
%destructor termlist { del($$); }
%destructor nsetterm { del($$); }
%destructor setterm  { del($$); }

%type condlist       { AggrCondVec* }
%type ncondlist      { AggrCondVec* }
%type symcondlist    { AggrCondVec* }
%type nsymcondlist   { AggrCondVec* }
%type setweightlist  { AggrCondVec* }
%type nsetweightlist { AggrCondVec* }
%type symweightlist  { AggrCondVec* }
%type nsymweightlist { AggrCondVec* }

%destructor condlist       { del($$); }
%destructor ncondlist      { del($$); }
%destructor symcondlist    { del($$); }
%destructor nsymcondlist   { del($$); }
%destructor setweightlist  { del($$); }
%destructor nsetweightlist { del($$); }
%destructor symweightlist  { del($$); }
%destructor nsymweightlist { del($$); }

%type weightlit     { AggrCond* }
%type symweightlit  { AggrCond* }
%type setweightlit  { AggrCond* }
%type condlit       { AggrCond* }
%type symcondlit    { AggrCond* }
%destructor weightlit    { del($$); }
%destructor symweightlit { del($$); }
%destructor setweightlit { del($$); }
%destructor condlit      { del($$); }
%destructor symcondlit   { del($$); }

%type disjlist       { JunctionCondVec* }
%destructor disjlist { del($$); }

%type weightcond    { LitPtrVec* }
%type nweightcond   { LitPtrVec* }
%type priolit_cond  { LitPtrVec* }
%type npriolit_cond { LitPtrVec* }
%destructor weightcond    { del($$); }
%destructor nweightcond   { del($$); }
%destructor priolit_cond  { del($$); }
%destructor npriolit_cond { del($$); }

%type aggr        { AggrLit* }
%type aggr_ass    { AggrLit* }
%type aggr_num    { AggrLit* }
%type aggr_atom   { AggrLit* }
%destructor aggr        { del($$); }
%destructor aggr_ass    { del($$); }
%destructor aggr_num    { del($$); }
%destructor aggr_atom   { del($$); }

%type weightprio { std::pair<Term*, Term*>* }
%destructor weightprio { del($$->first); del($$->second); del($$); }

%left SEM.
%left DOTS.
%left XOR.
%left QUESTION.
%left AND.
%left PLUS MINUS.
%left MULT SLASH MOD PMOD DIV PDIV.
%right POW PPOW.
%left UMINUS UBNOT.

%left DSEM.
%left COMMA.
%left VBAR.

// TODO: remove me!!!
%left EVEN.
%left ODD.
%left AVG.

start ::= program.

program ::= .
program ::= program line DOT.
program ::= program weak(w).      { pParser->add(w); }
program ::= program SHOWDOT(tok). { pParser->show(tok.index, true); }
program ::= program HIDEDOT(tok). { pParser->show(tok.index, false); }

line ::= INCLUDE STRING(file).                         { pParser->include(file.index); }
line ::= rule(r).                                      { pParser->add(r); }
line ::= DOMAIN signed(id) LBRAC var_list(vars) RBRAC. { pParser->domainStm(id.loc(), id.index, *vars); del(vars); }
line ::= EXTERNAL(tok) predicate(pred) cond(list).     { pParser->add(new External(tok.loc(), pred, *list)); delete list; }
line ::= EXTERNAL signed(id) SLASH NUMBER(num).        { GRD->externalStm(id.index, num.number); }
line ::= CUMULATIVE IDENTIFIER(id).                    { pParser->incremental(Parser::IPART_CUMULATIVE, id.index); }
line ::= VOLATILE IDENTIFIER(id).                      { pParser->incremental(Parser::IPART_VOLATILE, id.index); }
line ::= VOLATILE IDENTIFIER(id) COLON term(term).     { if(pParser->checkVolTerm(term)) pParser->incremental(Parser::IPART_VOLATILE, id.index, term->val(GRD).number()); }
line ::= BASE.                                         { pParser->incremental(Parser::IPART_BASE); }
line ::= optimize.
line ::= compute.
line ::= meta.

meta ::= HIDE.                                       { OTP->hideAll(); }
meta ::= SHOW(tok) term(term) COLON.                 { LitPtrVec list; pParser->add(new Display(tok.loc(), term,  list, Display::Type(true, true)));                }
meta ::= SHOW(tok) term(term).                       { LitPtrVec list; pParser->add(new Display(tok.loc(), term,  list, Display::Type(true, false)));               }
meta ::= SHOW(tok) term(term) ncond(list) COLON.     {                 pParser->add(new Display(tok.loc(), term, *list, Display::Type(true, true)));  delete list;  }
meta ::= SHOW(tok) term(term) ncond(list).           {                 pParser->add(new Display(tok.loc(), term, *list, Display::Type(true, false))); delete list;  }
meta ::= HIDE(tok) term(term) COLON.                 { LitPtrVec list; pParser->add(new Display(tok.loc(), term,  list, Display::Type(false, true)));               }
meta ::= HIDE(tok) term(term).                       { LitPtrVec list; pParser->add(new Display(tok.loc(), term,  list, Display::Type(false, false)));              }
meta ::= HIDE(tok) term(term) ncond(list) COLON.     {                 pParser->add(new Display(tok.loc(), term, *list, Display::Type(false, true)));  delete list; }
meta ::= HIDE(tok) term(term) ncond(list).           {                 pParser->add(new Display(tok.loc(), term, *list, Display::Type(false, false))); delete list; }

meta ::= CONST     IDENTIFIER(id) ASSIGN term(term). { pParser->constTerm(id.index, term); }
meta ::= IINIT     term(term).                       { pParser->setIInit(term); }

forgetid ::= IDENTIFIER(id).                { pParser->setForgetId(id.index); }
meta ::= FORGET forgetid COLON term(term).  { pParser->saveForgetTerm(term); }

signed(res) ::= IDENTIFIER(id).              { res = id; }
signed(res) ::= MINUS(minus) IDENTIFIER(id). { res = minus; res.index = GRD->index(std::string("-") + GRD->string(id.index)); }

ncond(res) ::= COLON lit(lit).             { res = vec1<Lit>(lit); }
ncond(res) ::= ncond(cond) COLON lit(lit). { res = cond; cond->push_back(lit); }

cond(res) ::= .            { res = new LitPtrVec(); }
cond(res) ::= ncond(list). { res = list; }

var_list(res) ::= VARIABLE(var).                      { res = new VarSigVec(); res->push_back(VarSig(var.loc(), var.index)); }
var_list(res) ::= var_list(list) COMMA VARIABLE(var). { list->push_back(VarSig(var.loc(), var.index)); res = list; }

rule(res) ::= head(head) IF body(body). { res = new Rule(head->loc(), head, *body); del(body); }
rule(res) ::= IF(tok) body(body).       { res = new Rule(tok.loc(), 0, *body); del(body); }
rule(res) ::= head(head).               { LitPtrVec v; res = new Rule(head->loc(), head, v); }

head(res) ::= predicate(pred).  { res = pred; }
head(res) ::= aggr_atom(lit).   { res = lit; }
head(res) ::= disjunction(lit). { res = lit; }

nbody(res) ::= body_literal(lit).                   { res = vec1(lit); }
nbody(res) ::= nbody(body) COMMA body_literal(lit). { res = body; res->push_back(lit); }

body(res) ::= .            { res = new LitPtrVec(); }
body(res) ::= nbody(body). { res = body; }

predlit(res) ::= predicate(pred).          { res = pred; }
predlit(res) ::= NOT(tok) predicate(pred). { res = pred; pred->sign(true); pred->loc(tok.loc()); }

lit(res) ::= predlit(pred).            { res = pred; }
lit(res) ::= term(a) cmp(cmp) term(b). { res = new RelLit(a->loc(), cmp, a, b); }
lit(res) ::= TRUE(tok).                { res = new BooleanLit(tok.loc(), true); }
lit(res) ::= FALSE(tok).               { res = new BooleanLit(tok.loc(), false); }
lit(res) ::= term(a) CASSIGN term(b).  { res = new RelLit(a->loc(), RelLit::ASSIGN, a, b); }

literal(res) ::= lit(lit).                     { res = lit; }
literal(res) ::= VARIABLE(var) ASSIGN term(b). { res = new RelLit(var.loc(), RelLit::ASSIGN, new VarTerm(var.loc(), var.index), b); }

body_literal(res) ::= literal(lit).                       { res = lit; }
body_literal(res) ::= aggr_atom(lit).                     { res = lit; }
body_literal(res) ::= NOT aggr_atom(lit).                 { res = lit; lit->sign(true); }
body_literal(res) ::= VARIABLE(var) ASSIGN aggr_ass(lit). { res = lit; lit->assign(new VarTerm(var.loc(), var.index)); }
body_literal(res) ::= term(a) CASSIGN aggr_ass(lit).      { res = lit; lit->assign(a); }
body_literal(res) ::= conjunction(lit).                   { res = lit; }

cmp(res) ::= GREATER. { res = RelLit::GREATER; }
cmp(res) ::= LOWER.   { res = RelLit::LOWER; }
cmp(res) ::= GTHAN.   { res = RelLit::GTHAN; }
cmp(res) ::= LTHAN.   { res = RelLit::LTHAN; }
cmp(res) ::= EQUAL.   { res = RelLit::EQUAL; }
cmp(res) ::= INEQUAL. { res = RelLit::INEQUAL; }

term(res) ::= VARIABLE(var).  { res = new VarTerm(var.loc(), var.index); }
term(res) ::= IDENTIFIER(id). { res = pParser->term(Val::ID, id.loc(), id.index); }
term(res) ::= STRING(id).     { res = pParser->term(Val::STRING, id.loc(), id.index); }
term(res) ::= NUMBER(num).    { res = new ConstTerm(num.loc(), Val::number(num.number)); }
term(res) ::= ANONYMOUS(var). { res = new VarTerm(var.loc()); }
term(res) ::= INFIMUM(tok).   { res = new ConstTerm(tok.loc(), Val::inf()); }
term(res) ::= SUPREMUM(tok).  { res = new ConstTerm(tok.loc(), Val::sup()); }
term(res) ::= UNDEF(tok).     { res = new ConstTerm(tok.loc(), Val::undef()); }

term(res) ::= term(a) DOTS term(b).                         { res = new RangeTerm(a->loc(), a, b); }
term(res) ::= term(a) SEM term(b).                          { res = new PoolTerm(a->loc(), a, b); }
term(res) ::= term(a) PLUS term(b).                         { res = new MathTerm(a->loc(), MathTerm::PLUS, a, b); }
term(res) ::= term(a) MINUS term(b).                        { res = new MathTerm(a->loc(), MathTerm::MINUS, a, b); }
term(res) ::= term(a) MULT term(b).                         { res = new MathTerm(a->loc(), MathTerm::MULT, a, b); }
term(res) ::= term(a) SLASH term(b).                        { res = new MathTerm(a->loc(), MathTerm::DIV, a, b); }
term(res) ::= term(a) DIV term(b).                          { res = new MathTerm(a->loc(), MathTerm::DIV, a, b); }
term(res) ::= term(a) PDIV term(b).                         { res = new MathTerm(a->loc(), MathTerm::DIV, a, b); }
term(res) ::= term(a) MOD term(b).                          { res = new MathTerm(a->loc(), MathTerm::MOD, a, b); }
term(res) ::= term(a) PMOD term(b).                         { res = new MathTerm(a->loc(), MathTerm::MOD, a, b); }
term(res) ::= term(a) POW term(b).                          { res = new MathTerm(a->loc(), MathTerm::POW, a, b); }
term(res) ::= term(a) PPOW term(b).                         { res = new MathTerm(a->loc(), MathTerm::POW, a, b); }
term(res) ::= term(a) AND term(b).                          { res = new MathTerm(a->loc(), MathTerm::AND, a, b); }
term(res) ::= term(a) XOR term(b).                          { res = new MathTerm(a->loc(), MathTerm::XOR, a, b); }
term(res) ::= term(a) QUESTION term(b).                     { res = new MathTerm(a->loc(), MathTerm::OR, a, b); }
term(res) ::= PABS LBRAC term(a) RBRAC.                     { res = new MathTerm(a->loc(), MathTerm::ABS, a); }
term(res) ::= VBAR term(a) VBAR.                            { res = new MathTerm(a->loc(), MathTerm::ABS, a); }
term(res) ::= PPOW LBRAC term(a) COMMA term(b) RBRAC.       { res = new MathTerm(a->loc(), MathTerm::POW, a, b); }
term(res) ::= PMOD LBRAC term(a) COMMA term(b) RBRAC.       { res = new MathTerm(a->loc(), MathTerm::MOD, a, b); }
term(res) ::= PDIV LBRAC term(a) COMMA term(b) RBRAC.       { res = new MathTerm(a->loc(), MathTerm::DIV, a, b); }
term(res) ::= func(f).                                      { res = f; }
term(res) ::= LBRAC(l) termlist(args) RBRAC.                { res = args->size() == 1 ? args->pop_back().release() : new FuncTerm(l.loc(), GRD->index(""), *args); delete args; }
term(res) ::= LBRAC(l) termlist(args) COMMA RBRAC.          { res = new FuncTerm(l.loc(), GRD->index(""), *args); delete args; }
term(res) ::= AT IDENTIFIER(id) LBRAC termlist(args) RBRAC. { res = new LuaTerm(id.loc(), id.index, *args); delete args; }
term(res) ::= AT IDENTIFIER(id) LBRAC RBRAC.                { TermPtrVec args; res = new LuaTerm(id.loc(), id.index, args); }
term(res) ::= MINUS(m) term(a). [UMINUS]                    { res = new MathTerm(m.loc(), MathTerm::UMINUS, a); }
term(res) ::= BNOT(m) term(a). [UBNOT]                      { res = new MathTerm(m.loc(), MathTerm::XOR, MINUSONE(m.loc()), a); }

func(res) ::= IDENTIFIER(id) LBRAC termlist(args) RBRAC.    { res = new FuncTerm(id.loc(), id.index, *args); delete args; }

nsetterm(res) ::= term(term).                      { res = vec1(term); }
nsetterm(res) ::= termlist(list) COMMA term(term). { res = list; list->push_back(term); }

setterm(res) ::= .               { res = new TermPtrVec(); }
setterm(res) ::= nsetterm(term). { res = term; }

nweightcond(res) ::= COLON literal(lit).                   { res = vec1(lit); }
nweightcond(res) ::= COLON literal(lit) nweightcond(list). { res = list; res->push_back(lit); }

weightcond(res) ::= .                  { res = new LitPtrVec(); }
weightcond(res) ::= nweightcond(list). { res = list; }

symweightlit(res) ::= lit(lit) nweightcond(cond) ASSIGN term(weight).                { std::auto_ptr<TermPtrVec> terms(vec1(weight)); cond->insert(cond->begin(), lit); res = new AggrCond(lit->loc(), *terms, *cond, AggrCond::STYLE_LPARSE); delete cond; }
symweightlit(res) ::= lit(lit) ASSIGN term(weight) weightcond(cond).                 { std::auto_ptr<TermPtrVec> terms(vec1(weight)); cond->insert(cond->begin(), lit); res = new AggrCond(lit->loc(), *terms, *cond, AggrCond::STYLE_LPARSE); delete cond; }
symweightlit(res) ::= lit(lit) weightcond(cond).                                     { std::auto_ptr<TermPtrVec> terms(vec1<Term>(ONE(lit->loc()))); cond->insert(cond->begin(), lit); res = new AggrCond(lit->loc(), *terms, *cond, AggrCond::STYLE_LPARSE); delete cond; }

setweightlit(res) ::= LOWER nsetterm(terms) GREATER COLON lit(lit) weightcond(cond). { cond->insert(cond->begin(), lit); res = new AggrCond(lit->loc(), *terms, *cond, AggrCond::STYLE_DLV); delete cond; delete terms; }

symcondlit(res) ::= lit(lit) weightcond(cond).                                    { std::auto_ptr<TermPtrVec> terms(vec1<Term>(ONE(lit->loc()))); cond->insert(cond->begin(), lit); res = new AggrCond(lit->loc(), *terms, *cond, AggrCond::STYLE_LPARSE); delete cond; }

condlit(res) ::= symcondlit(lit). { res = lit; }
condlit(res) ::= LOWER setterm(terms) GREATER COLON lit(lit) weightcond(cond). { terms->insert(terms->begin(), ONE(lit->loc())); cond->insert(cond->begin(), lit); res = new AggrCond(lit->loc(), *terms, *cond, AggrCond::STYLE_DLV); delete cond; delete terms; }

nsetweightlist(res) ::= setweightlit(lit).                            { res = vec1(lit); }
nsetweightlist(res) ::= nsetweightlist(list) COMMA setweightlit(lit). { res = list; res->push_back(lit); }
setweightlist(res)  ::= .                     { res = new AggrCondVec(); }
setweightlist(res)  ::= nsetweightlist(list). { res = list; }

nsymweightlist(res) ::= symweightlit(lit).                            { res = vec1(lit); }
nsymweightlist(res) ::= nsymweightlist(list) COMMA symweightlit(lit). { res = list; res->push_back(lit); }
symweightlist(res)  ::= .                     { res = new AggrCondVec(); }
symweightlist(res)  ::= nsymweightlist(list). { res = list; }

ncondlist(res) ::= condlit(lit).                       { res = vec1(lit); }
ncondlist(res) ::= ncondlist(list) COMMA condlit(lit). { res = list; res->push_back(lit); }
condlist(res)  ::= .                { res = new AggrCondVec(); }
condlist(res)  ::= ncondlist(list). { res = list; }

nsymcondlist(res) ::= symcondlit(lit).                       { res = vec1(lit); }
nsymcondlist(res) ::= nsymcondlist(list) COMMA condlit(lit). { res = list; res->push_back(lit); }
symcondlist(res)  ::= .                { res = new AggrCondVec(); }
symcondlist(res)  ::= nsymcondlist(list). { res = list; }

aggr_ass(res) ::= SUM(tok)   LSBRAC      symweightlist(list) RSBRAC. { res = new SumAggrLit(tok.loc(), *list, false, false); delete list; }
aggr_ass(res) ::= SUM(tok)   LCBRAC      setweightlist(list) RCBRAC. { res = new SumAggrLit(tok.loc(), *list, false, true);  delete list; }
aggr_ass(res) ::= SUMP(tok)  LSBRAC      symweightlist(list) RSBRAC. { res = new SumAggrLit(tok.loc(), *list, true,  false); delete list; }
aggr_ass(res) ::= SUMP(tok)  LCBRAC      setweightlist(list) RCBRAC. { res = new SumAggrLit(tok.loc(), *list, true,  true);  delete list; }
aggr_ass(res) ::=            LSBRAC(tok) symweightlist(list) RSBRAC. { res = new SumAggrLit(tok.loc(), *list, false, false); delete list; }
aggr_ass(res) ::= MIN(tok)   LSBRAC      symweightlist(list) RSBRAC. { res = new MinMaxAggrLit(tok.loc(), *list, MinMaxAggrLit::MINIMUM, false); delete list; }
aggr_ass(res) ::= MIN(tok)   LCBRAC      setweightlist(list) RCBRAC. { res = new MinMaxAggrLit(tok.loc(), *list, MinMaxAggrLit::MINIMUM, false); delete list; }
aggr_ass(res) ::= MAX(tok)   LSBRAC      symweightlist(list) RSBRAC. { res = new MinMaxAggrLit(tok.loc(), *list, MinMaxAggrLit::MAXIMUM, true); delete list; }
aggr_ass(res) ::= MAX(tok)   LCBRAC      setweightlist(list) RCBRAC. { res = new MinMaxAggrLit(tok.loc(), *list, MinMaxAggrLit::MAXIMUM, true); delete list; }

aggr_ass(res) ::= COUNT(tok) LCBRAC           condlist(list) RCBRAC. { res = new SumAggrLit(tok.loc(), *list, true, true); delete list; }
aggr_ass(res) ::= COUNT(tok) LSBRAC        symcondlist(list) RSBRAC. { res = new SumAggrLit(tok.loc(), *list, true, false); delete list; }
aggr_ass(res) ::=            LCBRAC(tok)      condlist(list) RCBRAC. { res = new SumAggrLit(tok.loc(), *list, true, true); delete list; }

//aggr_num(res) ::= AVG(tok) LSBRAC weightlist(list) RSBRAC. { res = new AvgAggrLit(tok.loc(), *list); delete list; }

//aggr(res) ::= EVEN(tok) LCBRAC condlist(list) RCBRAC. { res = new ParityAggrLit(tok.loc(), *list, true, true); delete list; }
//aggr(res) ::= ODD(tok)  LCBRAC condlist(list) RCBRAC. { res = new ParityAggrLit(tok.loc(), *list, false, true); delete list; }
//aggr(res) ::= EVEN(tok) LSBRAC weightlist(list) RSBRAC. { res = new ParityAggrLit(tok.loc(), *list, true, false); delete list; }
//aggr(res) ::= ODD(tok)  LSBRAC weightlist(list) RSBRAC. { res = new ParityAggrLit(tok.loc(), *list, false, false); delete list; }


conjunction(res) ::= lit(lit) ncond(cond). { JunctionCondVec list; list.push_back(new JunctionCond(lit->loc(), lit, *cond)); delete cond; res = new JunctionLit(lit->loc(), list); }

disjlist(res) ::= VBAR predicate(lit) cond(cond).                { res = vec1<JunctionCond>(new JunctionCond(lit->loc(), lit, *cond)); delete cond; }
disjlist(res) ::= disjlist(list) VBAR predicate(lit) cond(cond). { res = list; list->push_back(new JunctionCond(lit->loc(), lit, *cond)); delete cond; }

disjunction(res) ::= predicate(lit) cond(cond) disjlist(list). { list->insert(list->begin(), new JunctionCond(lit->loc(), lit, *cond)); delete cond; res = new JunctionLit(lit->loc(), *list); delete list; }
disjunction(res) ::= predicate(lit) ncond(cond).               { JunctionCondVec list; list.push_back(new JunctionCond(lit->loc(), lit, *cond)); delete cond; res = new JunctionLit(lit->loc(), list); }

aggr_num(res) ::= aggr_ass(lit). { res = lit; }
aggr(res)     ::= aggr_num(lit). { res = lit; }

lcmp(res) ::= LTHAN. { res = true; }
lcmp(res) ::= LOWER. { res = false; }

gcmp(res) ::= GTHAN.   { res = true; }
gcmp(res) ::= GREATER. { res = false; }

aggr_atom(res) ::= term(l) lcmp(leq) aggr_num(aggr).                   { res = aggr; res->lower(l, leq); }
aggr_atom(res) ::= aggr_num(aggr) lcmp(ueq) term(u).                   { res = aggr; res->upper(u, ueq); }
aggr_atom(res) ::= term(u) gcmp(ueq) aggr_num(aggr).                   { res = aggr; res->upper(u, ueq); }
aggr_atom(res) ::= aggr_num(aggr) gcmp(leq) term(l).                   { res = aggr; res->lower(l, leq); }
aggr_atom(res) ::= term(u) gcmp(ueq) aggr_num(aggr) gcmp(leq) term(l). { res = aggr; res->upper(u, ueq); res->lower(l, leq); }
aggr_atom(res) ::= term(l) lcmp(leq) aggr_num(aggr) lcmp(ueq) term(u). { res = aggr; res->upper(u, ueq); res->lower(l, leq); }
aggr_atom(res) ::= term(t) EQUAL aggr_num(aggr).                       { res = aggr; res->lower(t->clone()); res->upper(t); }
aggr_atom(res) ::= aggr_num(aggr) EQUAL term(t).                       { res = aggr; res->lower(t->clone()); res->upper(t); }
aggr_atom(res) ::= term(l) aggr_num(aggr) term(u).                     { res = aggr; res->lower(l); res->upper(u); }
aggr_atom(res) ::= aggr_num(aggr) term(u).                             { res = aggr; res->upper(u); }
aggr_atom(res) ::= term(l) aggr_num(aggr).                             { res = aggr; res->lower(l); }
aggr_atom(res) ::= aggr(aggr).                                         { res = aggr; }

predicate(res) ::= MINUS(sign) IDENTIFIER(id) LBRAC termlist(terms) RBRAC. { res = pParser->predLit(sign.loc(), id.index, *terms, true); delete terms; }
predicate(res) ::= IDENTIFIER(id) LBRAC termlist(terms) RBRAC.             { res = pParser->predLit(id.loc(), id.index, *terms, false); delete terms; }
predicate(res) ::= MINUS(sign) IDENTIFIER(id).                             { TermPtrVec terms; res = pParser->predLit(sign.loc(), id.index, terms, true); }
predicate(res) ::= IDENTIFIER(id).                                         { TermPtrVec terms; res = pParser->predLit(id.loc(), id.index, terms, false); }

termlist(res) ::= term(term).                                { res = vec1(term); }
termlist(res) ::= termlist(list) COMMA term(term).           { res = list; list->push_back(term); }
termlist(res) ::= termlist(list) DSEM(dsem) termlist(terms). { res = list; list->push_back(new ArgTerm(dsem.loc(), res->pop_back().release(), *terms)); delete terms; }

optimize ::= soptimize LSBRAC prio_list RSBRAC.
optimize ::= soptimize LCBRAC prio_set  RCBRAC.

soptimize ::= MINIMIZE. { pParser->maximize(false); }
soptimize ::= MAXIMIZE. { pParser->maximize(true); }

prio_list ::= .
prio_list ::= nprio_list.

nprio_list ::= weightedpriolit(lit).                 { pParser->add(lit); }
nprio_list ::= weightedpriolit(lit) COMMA prio_list. { pParser->add(lit); }

prio_set ::= .
prio_set ::= nprio_set.

nprio_set ::= priolit(lit).                { pParser->add(lit); }
nprio_set ::= priolit(lit) COMMA prio_set. { pParser->add(lit); }

weightedpriolit(res) ::= predlit(head) ASSIGN term(weight) AT term(prio) priolit_cond(body).  { body->insert(body->begin(), head); res = pParser->optimize(Optimize::MULTISET, head->loc(), 0, weight, prio, body); }
weightedpriolit(res) ::= predlit(head) ASSIGN term(weight) priolit_cond(body).                { body->insert(body->begin(), head); res = pParser->optimize(Optimize::MULTISET, head->loc(), 0, weight, 0, body); }
weightedpriolit(res) ::= predlit(head) npriolit_cond(body) ASSIGN term(weight) AT term(prio). { body->insert(body->begin(), head); res = pParser->optimize(Optimize::MULTISET, head->loc(), 0, weight, prio, body); }
weightedpriolit(res) ::= predlit(head) npriolit_cond(body) ASSIGN term(weight).               { body->insert(body->begin(), head); res = pParser->optimize(Optimize::MULTISET, head->loc(), 0, weight, 0, body); }
weightedpriolit(res) ::= predlit(head) AT term(prio) priolit_cond(body).                      { body->insert(body->begin(), head); res = pParser->optimize(Optimize::MULTISET, head->loc(), 0, 0, prio, body); }
weightedpriolit(res) ::= predlit(head) npriolit_cond(body) AT term(prio).                     { body->insert(body->begin(), head); res = pParser->optimize(Optimize::MULTISET, head->loc(), 0, 0, prio, body); }
weightedpriolit(res) ::= predlit(head) priolit_cond(body).                                    { body->insert(body->begin(), head); res = pParser->optimize(Optimize::MULTISET, head->loc(), 0, 0, 0, body); }

priolit(res) ::= predlit(head) AT term(prio) priolit_cond(body).                      { body->insert(body->begin(), head); res = pParser->optimize(Optimize::SET, head->loc(), 0, 0, prio, body); }
priolit(res) ::= predlit(head) npriolit_cond(body) AT term(prio).                     { body->insert(body->begin(), head); res = pParser->optimize(Optimize::SET, head->loc(), 0, 0, prio, body); }
priolit(res) ::= predlit(head) priolit_cond(body).                                    { body->insert(body->begin(), head); res = pParser->optimize(Optimize::SET, head->loc(), 0, 0, 0, body); }
priolit(res) ::= LOWER(lt) nsetterm(terms) GREATER AT term(prio) priolit_cond(body).  { res = pParser->optimize(Optimize::SYMSET, lt.loc(), terms, 0, prio, body); }
priolit(res) ::= LOWER(lt) nsetterm(terms) GREATER npriolit_cond(body) AT term(prio). { res = pParser->optimize(Optimize::SYMSET, lt.loc(), terms, 0, prio, body); }
priolit(res) ::= LOWER(lt) nsetterm(terms) GREATER priolit_cond(body).                { res = pParser->optimize(Optimize::SYMSET, lt.loc(), terms, 0, 0, body); }

npriolit_cond(res) ::= COLON literal(lit).                     { res = vec1(lit); }
npriolit_cond(res) ::= npriolit_cond(list) COLON literal(lit). { res = list; list->push_back(lit); }

priolit_cond(res) ::= .                    { res = new LitPtrVec(); }
priolit_cond(res) ::= npriolit_cond(list). { res = list; }

constraint(res) ::= WIF(tok). { pParser->maximize(false, false); res = tok; }

weightprio(res) ::= DOT(tok).                                { res = new std::pair<Term*, Term*>(ONE(tok.loc()), ONE(tok.loc())); }
weightprio(res) ::= DOT(tok) LOWER term(w) GREATER.          { res = new std::pair<Term*, Term*>(w,              ONE(tok.loc())); }
weightprio(res) ::= DOT(tok) LOWER COLON term(p) GREATER.    { res = new std::pair<Term*, Term*>(ONE(tok.loc()), p             ); }
weightprio(res) ::= DOT LOWER term(w) COLON term(p) GREATER. { res = new std::pair<Term*, Term*>(w,              p             ); }

weak(res) ::= constraint(tok) body(body) weightprio(wp). { res = pParser->optimize(Optimize::CONSTRAINT, tok.loc(), 0, wp->first, wp->second, body); delete wp; }

compute ::= COMPUTE LCBRAC compute_list RCBRAC.
compute ::= COMPUTE NUMBER LCBRAC compute_list RCBRAC.

compute_list ::= .
compute_list ::= ncompute_list.

ncompute_list ::= computelit(lit).                     { pParser->add(lit); }
ncompute_list ::= computelit(lit) COMMA ncompute_list. { pParser->add(lit); }

computelit(res) ::= predlit(head) priolit_cond(body).  { res = new Compute(head->loc(), head, *body); del(body); }
