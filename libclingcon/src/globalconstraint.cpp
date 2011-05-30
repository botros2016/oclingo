#include <clingcon/globalconstraint.h>
#include <gringo/prgvisitor.h>
#include <gringo/grounder.h>
#include <gringo/litdep.h>

namespace Clingcon
{

    GlobalConstraintHeadLit::~GlobalConstraintHeadLit(){}

    void GlobalConstraintHeadLit::enqueue(Grounder *g)
    {
        gc_->enqueue(g);
    }

    void GlobalConstraintHeadLit::normalize(Grounder *g, Expander *)
    {
        for(size_t i = 0; i < vec_.size(); ++i)
            vec_[i].normalize(g,0);
    }

    void GlobalConstraintHeadLit::print(Storage *sto, std::ostream &out) const
    {
        out << "[";
        for(size_t i = 0; i < vec_.size(); ++i)
        {
            vec_[i].print(sto,out);
            if (i+1<vec_.size())
                out << ",";
        }
        out << "]";
    }


    void GlobalConstraintHeadLit::visit(PrgVisitor *visitor)
    {
        for(size_t i = 0; i < vec_.size(); ++i)
            visitor->visit(&(vec_[i]), false);
    }


    GlobalConstraint::~GlobalConstraint(){}

    class BodyExpander : public Expander
    {
    public:
            BodyExpander(GlobalConstraint &d) : d_(d) { }
            void expand(Lit *lit, Type) { d_.body().push_back(lit); }
    private:
            GlobalConstraint &d_;
    };

    void GlobalConstraint::visit(PrgVisitor *visitor)
    {

        for(boost::ptr_vector<GlobalConstraintHeadLit>::iterator i = heads_.begin(); i != heads_.end(); ++i)
        {
            visitor->visit(&(*i), false);
        }

        for(LitPtrVec::iterator i = body_.begin(); i != body_.end(); ++i)
        {
            visitor->visit(&(*i),true);
        }

    }

    void GlobalConstraint::print(Storage *sto, std::ostream &out) const
    {
        if (type_==DISTINCT)
        {
            out << "$distinct";
        }
        //only print the first one
        heads_.begin()->print(sto,out);
        if (body_.size()>0)
        {
            out << ":-";
            for (size_t i = 0; i < body_.size(); ++i)
            {
                body_[i].print(sto,out);
                if (i+1<body_.size())
                    out << ",";
            }
        }

    }


    void GlobalConstraint::normalize(Grounder *g)
    {
        for (size_t i = 0; i < heads_.size(); ++i)
        {
            heads_[i].normalize(g,0);
        }
        BodyExpander bodyExp(*this);
        for (size_t i = 0; i < body_.size(); ++i)
        {
            body_[i].normalize(g,&bodyExp);
        }
    }

    void GlobalConstraint::append(Lit *lit)
    {
            body_.push_back(lit);
    }


    bool GlobalConstraint::grounded(Grounder *g)
    {
        for (size_t i = 0; i < heads_.size(); ++i)
        {
            heads_[i].grounded(g);
        }

        for (size_t i = 0; i < body_.size(); ++i)
        {
            body_[i].grounded(g);
        }


        //dom_->grounded(g);
        Printer *printer = g->output()->printer<GlobalConstraint::Printer>();
        this->accept(printer,g);
        //dom_->accept(printer);
        return true;
    }

    void GlobalConstraint::accept(::Printer *v, Grounder* )
    {
        Printer *printer = v->output()->printer<Printer>();
        printer->type(type_);
        for (size_t i = 0; i < heads_.size(); ++i)
        {
            printer->beginHead();
            GlobalConstraintHeadLit& h = heads_[i];
            GroundedConstraintVarLitVec vec;
            h.getVariables(vec, v->output()->storage());
            printer->addHead(vec);
            printer->endHead();
        }
        printer->end();
    }


    GlobalConstraint::Printer::~Printer(){}


}
