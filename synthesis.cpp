#include <list>
#include <cassert>
#include <map>

#include "synthesis.h"
#include "carchecker.h"
#include "generalizer.h"

using namespace std;
using namespace aalta;

// static public variable of Syn_Frame
int Syn_Frame::num_varX_;
int Syn_Frame::num_varY_;
set<int> Syn_Frame::var_X;
set<int> Syn_Frame::var_Y;
unordered_set<ull> Syn_Frame::winning_state;
unordered_set<ull> Syn_Frame::failure_state;
map<ull, ull> Syn_Frame::bddP_to_afP;
int Syn_Frame::call_sat;

bool is_realizable(aalta_formula *src_formula, unordered_set<string> &env_var)
{
    cout << src_formula->to_string() << endl;
    //  partition atoms and save index values respectively
    PartitionAtoms(src_formula, env_var);

    Syn_Frame::call_sat = 0;

    // number of variables
    Syn_Frame::num_varX_ = Syn_Frame::var_X.size();
    Syn_Frame::num_varY_ = Syn_Frame::var_Y.size();

    // initializa utils of bdd
    FormulaInBdd::InitBdd4LTLf(src_formula, false);
    Syn_Frame::winning_state.insert(ull(FormulaInBdd::TRUE_bddP_));
    Syn_Frame::failure_state.insert(ull(FormulaInBdd::FALSE_bddP_));
    Syn_Frame::bddP_to_afP[ull(FormulaInBdd::FALSE_bddP_)] = ull(aalta_formula::FALSE());

    list<Syn_Frame *> searcher;
    Syn_Frame *init = new Syn_Frame(src_formula); // xnf(src_formula)
    searcher.push_back(init);
    while (true)
    {
        Syn_Frame *cur_frame = searcher.back();
        cur_frame->PrintInfo();
        Status peek = cur_frame->CheckRealizability();
        switch (peek)
        {
        case Realizable:
        {
            if (searcher.size() == 1)
            {
                delete cur_frame;
                FormulaInBdd::QuitBdd4LTLf();
                return true;
            }
            Syn_Frame::winning_state.insert(ull(cur_frame->GetBddPointer()));
            delete cur_frame;
            searcher.pop_back();
            (searcher.back())->process_signal(To_winning_state);
            break;
        }
        case Unrealizable:
        {
            if (searcher.size() == 1)
            {
                delete cur_frame;
                FormulaInBdd::QuitBdd4LTLf();
                return false;
            }
            Syn_Frame::failure_state.insert(ull(cur_frame->GetBddPointer()));
            Syn_Frame::bddP_to_afP[ull(cur_frame->GetBddPointer())] = ull(cur_frame->GetFormulaPointer());
            delete cur_frame;    //////////////
            searcher.pop_back(); ///////////

            // encounter Unrealizable
            // backtrack the beginning of the sat trace
            while (true)
            {
                auto tmp = searcher.back();
                if (tmp->IsTraceBeginning())
                {
                    tmp->process_signal(To_failure_state);
                    break;
                }
                else
                {
                    delete tmp;
                    searcher.pop_back();
                }
            }
            break;
        }
        case Unknown:
        {
            Status res = Expand(searcher);
            if (res != Unknown)
            { //
                delete cur_frame;
                FormulaInBdd::QuitBdd4LTLf();
                return res == Realizable;
            }
            break;
        }
        }
    }
}

Syn_Frame::Syn_Frame(aalta_formula *af)
{
    state_in_bdd_ = new FormulaInBdd(af);
    Y_constraint_ = aalta_formula::TRUE();
    X_constraint_ = aalta_formula::TRUE();
    current_Y_ = NULL;
    current_X_ = NULL;
    is_trace_beginning_ = false;
}

Status Syn_Frame::CheckRealizability()
{
    if (Syn_Frame::winning_state.find(ull(state_in_bdd_->GetBddPointer())) != Syn_Frame::winning_state.end())
        return Realizable;
    if (Syn_Frame::failure_state.find(ull(state_in_bdd_->GetBddPointer())) != Syn_Frame::failure_state.end())
        return Unrealizable;
    if (EdgeConstraintIsUnsat(Y_constraint_))
        return Unrealizable;
    if (EdgeConstraintIsUnsat(X_constraint_))
        return Realizable;
    return Unknown;
}

void Syn_Frame::process_signal(Signal signal)
{
    switch (signal)
    {
    case To_winning_state:
    {
        aalta_formula *x_reduced = Generalize(state_in_bdd_->GetFormulaPointer(), current_Y_, current_X_, To_winning_state);
        aalta_formula *neg_x_reduced = aalta_formula(aalta_formula::Not, NULL, x_reduced).nnf();
        X_constraint_ = (aalta_formula(aalta_formula::And, X_constraint_, neg_x_reduced).simplify())->unique();
        current_X_ = NULL;
        break;
    }
    case To_failure_state:
    {
        aalta_formula *y_reduced = Generalize(state_in_bdd_->GetFormulaPointer(), current_Y_, current_X_, To_failure_state);
        aalta_formula *neg_y_reduced = aalta_formula(aalta_formula::Not, NULL, y_reduced).nnf();
        Y_constraint_ = (aalta_formula(aalta_formula::And, Y_constraint_, neg_y_reduced).simplify())->unique();

        X_constraint_ = aalta_formula::TRUE();

        current_Y_ = NULL;
        current_X_ = NULL;
        break;
    }
    case Accepting_edge:
    {
        aalta_formula *x_reduced = Generalize(state_in_bdd_->GetFormulaPointer(), current_Y_, current_X_, Accepting_edge);
        aalta_formula *neg_x_reduced = aalta_formula(aalta_formula::Not, NULL, x_reduced).nnf();
        X_constraint_ = (aalta_formula(aalta_formula::And, X_constraint_, neg_x_reduced).simplify())->unique();
        current_X_ = NULL;
        break;
    }
    case Incomplete_Y:
    {
        aalta_formula *y_reduced = Generalize(state_in_bdd_->GetFormulaPointer(), current_Y_, NULL, Incomplete_Y);
        aalta_formula *neg_y_reduced = aalta_formula(aalta_formula::Not, NULL, y_reduced).nnf();
        Y_constraint_ = (aalta_formula(aalta_formula::And, Y_constraint_, neg_y_reduced).simplify())->unique();

        X_constraint_ = aalta_formula::TRUE();

        current_Y_ = NULL;
        current_X_ = NULL;
        break;
    }
    case NoWay:
    {
        aalta_formula *state = state_in_bdd_->GetFormulaPointer();
        state = aalta_formula(aalta_formula::And, state, X_constraint_).unique();
        aalta_formula *y_reduced = Generalize(state, current_Y_, NULL, NoWay);
        aalta_formula *neg_y_reduced = aalta_formula(aalta_formula::Not, NULL, y_reduced).nnf();
        Y_constraint_ = (aalta_formula(aalta_formula::And, Y_constraint_, neg_y_reduced).simplify())->unique();

        X_constraint_ = aalta_formula::TRUE();

        current_Y_ = NULL;
        current_X_ = NULL;
        break;
    }
    }
}

aalta_formula *Syn_Frame::GetEdgeConstraint()
{
    if (current_Y_ == NULL)
        return Y_constraint_;
    else
        return aalta_formula(aalta_formula::And, current_Y_, X_constraint_).unique();
}

void Syn_Frame::PrintInfo()
{
    cout << "formula: " << (state_in_bdd_->GetFormulaPointer())->to_string() << endl;
    cout << "Y constraint: " << Y_constraint_->to_string() << endl;
    cout << "X constraint: " << X_constraint_->to_string() << endl;
    if (current_Y_ != NULL)
        cout << "current Y: " << current_Y_->to_string() << endl;
    if (current_X_ != NULL)
        cout << "current X: " << current_X_->to_string() << endl;
    cout << (is_trace_beginning_ ? "is " : "is not ") << "start point" << endl;
}

void Syn_Frame::SetTravelDirection(aalta_formula *Y, aalta_formula *X)
{
    if (current_Y_ == NULL)
        current_Y_ = Y;
    current_X_ = X;
    cout << "set: Y = " << Y->to_string() << ", X = " << X->to_string() << endl;
}

Status Expand(list<Syn_Frame *> &searcher)
{
    // cout << (++(Syn_Frame::call_sat)) << "time(s) sat call" << endl
    //      << "call before stack size: " << searcher.size() << endl;
    Syn_Frame *tp_frame = searcher.back();
    aalta_formula *edge_constraint = tp_frame->GetEdgeConstraint();
    // aalta_formula *block_formula = ConstructBlockFormula(searcher, edge_constraint);
    aalta_formula *f;
    if(edge_constraint->oper()!=aalta_formula::True)
        f = aalta_formula(aalta_formula::And, tp_frame->GetFormulaPointer(), edge_constraint).unique();
    else
        f = tp_frame->GetFormulaPointer();
    cout << f->to_string() << endl;
    f = f->add_tail();
    f = f->remove_wnext();
    f = f->simplify();
    f = f->split_next();
    cout<<"Construct Checker: "<<f->to_string()<<endl;
    CARChecker checker(f, false, true);
    BlockState(checker, searcher);
    if (checker.check())
    { // sat
        vector<pair<aalta_formula *, aalta_formula *>> *tr = checker.get_model_for_synthesis();
        checker.print_evidence();
        tp_frame->SetTraceBeginning();
        for (int i = 0; i < ((tr->size()) - 1); ++i)
        {
            aalta_formula *Y_edge = ((*tr)[i]).first;
            aalta_formula *X_edge = ((*tr)[i]).second;
            (searcher.back())->SetTravelDirection(Y_edge, X_edge);
            aalta_formula *predecessor = (searcher.back())->GetFormulaPointer();
            unordered_set<int> edge;
            Y_edge->to_set(edge);
            if (CheckCompleteY(predecessor, edge) != Tt)
            {
                (searcher.back())->process_signal(Incomplete_Y);
                return Unknown;
            }
            X_edge->to_set(edge);
            aalta_formula *successor = FormulaProgression(predecessor, edge);
            // successor = xnf(successor);
            Syn_Frame *frame = new Syn_Frame(successor);
            searcher.push_back(frame);
        }
        // the last position is the accepting edge
        aalta_formula *Y_edge = (tr->back()).first;
        aalta_formula *end_state = (searcher.back())->GetFormulaPointer();
        unordered_set<int> edge;
        Y_edge->to_set(edge); // edge only with Y-literals here
        if (BaseWinningAtY(end_state, edge))
        {
            if (searcher.size() == 1)
            {
                return Realizable;
            }
            else
            {
                Syn_Frame::winning_state.insert(ull((searcher.back())->GetBddPointer()));
                delete searcher.back();
                searcher.pop_back();
                (searcher.back())->process_signal(To_winning_state);
            }
        }
        else
        {
            aalta_formula *X_edge = (tr->back()).second;
            (searcher.back())->SetTravelDirection(Y_edge, X_edge);
            (searcher.back())->process_signal(Accepting_edge);
        }
    }
    else
    { // unsat
        if (tp_frame->IsNotTryingY())
        { // current frame is unrealizable immediately
            if (searcher.size() == 1)
            {
                return Unrealizable;
            }
            else
            {
                auto bdd_ptr = (searcher.back())->GetBddPointer();
                Syn_Frame::failure_state.insert(ull(bdd_ptr));
                Syn_Frame::bddP_to_afP[ull(bdd_ptr)] = ull((searcher.back())->GetFormulaPointer());
                delete searcher.back();
                searcher.pop_back();
                (searcher.back())->process_signal(To_failure_state);
            }
        }
        else
            tp_frame->process_signal(NoWay);
    }
    // cout << "call after stack size: " << searcher.size() << endl;
    return Unknown;
}

aalta_formula *FormulaProgression(aalta_formula *predecessor, unordered_set<int> &edge)
{
    if (predecessor == NULL)
        return NULL;
    int op = predecessor->oper();
    if (op == aalta_formula::True || op == aalta_formula::False)
        return predecessor;
    else if (op == aalta_formula::And)
    {
        aalta_formula *lf = FormulaProgression(predecessor->l_af(), edge);
        aalta_formula *rf = FormulaProgression(predecessor->r_af(), edge);
        if ((lf->oper()) == aalta_formula::False || (rf->oper()) == aalta_formula::False)
            return aalta_formula::FALSE();
        else if ((lf->oper()) == aalta_formula::True)
            return rf;
        else if ((rf->oper()) == aalta_formula::True)
            return lf;
        else
            return aalta_formula(aalta_formula::And, lf, rf).unique();
    }
    else if (op == aalta_formula::Or)
    {
        aalta_formula *l_fp = FormulaProgression(predecessor->l_af(), edge);
        aalta_formula *r_fp = FormulaProgression(predecessor->r_af(), edge);
        if ((l_fp->oper()) == aalta_formula::True || (r_fp->oper()) == aalta_formula::True)
            return aalta_formula::TRUE();
        else if ((l_fp->oper()) == aalta_formula::False)
            return r_fp;
        else if ((r_fp->oper()) == aalta_formula::False)
            return l_fp;
        else
            return aalta_formula(aalta_formula::Or, l_fp, r_fp).unique();
    }
    else if (op == aalta_formula::Not || op >= 11)
    { // literal
        int lit = (op >= 11) ? op : (-((predecessor->r_af())->oper()));
        if (edge.find(lit) != edge.end())
            return aalta_formula::TRUE();
        else
            return aalta_formula::FALSE();
    }
    else if (op == aalta_formula::Next || op == aalta_formula::WNext)
    {
        return predecessor->r_af();
    }
    // if predecessor is in XNF,
    // the following two cases cannot appear
    else if (op == aalta_formula::Until)
    { // l U r = r | (l & X(l U r))
        aalta_formula *first_part = FormulaProgression(predecessor->r_af(), edge);
        if ((first_part->oper()) == aalta_formula::True)
            return aalta_formula::TRUE();
        aalta_formula *l_fp = FormulaProgression(predecessor->l_af(), edge);
        aalta_formula *second_part = NULL;
        if ((l_fp->oper()) == aalta_formula::True)
            second_part = predecessor;
        else if ((l_fp->oper()) == aalta_formula::False)
            return first_part;
        else
            second_part = aalta_formula(aalta_formula::And, l_fp, predecessor).unique();
        if ((first_part->oper()) == aalta_formula::False)
            return second_part;
        else
            return aalta_formula(aalta_formula::Or, first_part, second_part).unique();
    }
    else if (op == aalta_formula::Release)
    { // l R r = r & (l | N(l R r))
        aalta_formula *first_part = FormulaProgression(predecessor->r_af(), edge);
        if ((first_part->oper()) == aalta_formula::False)
            return aalta_formula::FALSE();
        aalta_formula *l_fp = FormulaProgression(predecessor->l_af(), edge);
        aalta_formula *second_part = NULL;
        if ((l_fp->oper()) == aalta_formula::True)
            return first_part;
        else if ((l_fp->oper()) == aalta_formula::False)
            second_part = predecessor;
        else
            second_part = aalta_formula(aalta_formula::Or, l_fp, predecessor).unique();
        if ((first_part->oper()) == aalta_formula::True)
            return second_part;
        else
            return aalta_formula(aalta_formula::And, first_part, second_part).unique();
    }
}

bool BaseWinningAtY(aalta_formula *end_state, unordered_set<int> &Y)
{
    if (end_state == NULL)
        return false;
    int op = end_state->oper();
    if (op == aalta_formula::True || op == aalta_formula::WNext)
        return true;
    else if (op == aalta_formula::False || op == aalta_formula::Next)
        return false;
    else if (op == aalta_formula::And)
        return BaseWinningAtY(end_state->l_af(), Y) && BaseWinningAtY(end_state->r_af(), Y);
    else if (op == aalta_formula::Or)
        return BaseWinningAtY(end_state->l_af(), Y) || BaseWinningAtY(end_state->r_af(), Y);
    else if (op == aalta_formula::Not || op >= 11)
    { // literal
        int lit = (op >= 11) ? op : (-((end_state->r_af())->oper()));
        return Y.find(lit) != Y.end();
    }
    else if (op == aalta_formula::Until || op == aalta_formula::Release)
        return BaseWinningAtY(end_state->r_af(), Y);
}

// partition atoms and save index values respectively
void PartitionAtoms(aalta_formula *af, unordered_set<string> &env_val)
{
    if (af == NULL)
        return;
    int op = af->oper();
    if (op >= 11)
        if (env_val.find(af->to_string()) != env_val.end())
            Syn_Frame::var_X.insert(op);
        else
            Syn_Frame::var_Y.insert(op);
    else
    {
        PartitionAtoms(af->l_af(), env_val);
        PartitionAtoms(af->r_af(), env_val);
    }
}

aalta_formula *xnf(aalta_formula *phi)
{
    if (phi == NULL)
        return NULL;
    int op = phi->oper();
    if ((op == aalta_formula::True || op == aalta_formula::False) || op == aalta_formula::Not || (op == aalta_formula::Next || op == aalta_formula::WNext) || op >= 11)
    {
        return phi;
    }
    if (op == aalta_formula::And || op == aalta_formula::Or)
    {
        return aalta_formula(op, xnf(phi->l_af()), xnf(phi->r_af())).unique();
    }
    else if (op == aalta_formula::Until)
    { // l U r=xnf(r) | (xnf(l) & X(l U r))
        aalta_formula *next_phi = aalta_formula(aalta_formula::Next, NULL, phi).unique();
        aalta_formula *xnf_l_and_next_phi = aalta_formula(aalta_formula::And, xnf(phi->l_af()), next_phi).unique();
        return aalta_formula(aalta_formula::Or, xnf(phi->r_af()), xnf_l_and_next_phi).unique();
    }
    else if (op == aalta_formula::Release)
    { // l R r=xnf(r) & (xnf(l) | WX(l R r))
        aalta_formula *wnext_phi = aalta_formula(aalta_formula::WNext, NULL, phi).unique();
        aalta_formula *xnf_l_or_wnext_phi = aalta_formula(aalta_formula::Or, xnf(phi->l_af()), wnext_phi).unique();
        return aalta_formula(aalta_formula::And, xnf(phi->r_af()), xnf_l_or_wnext_phi).unique();
    }
}

// return edgecons && G!(PREFIX) && G!(failure)
aalta_formula *ConstructBlockFormula(list<Syn_Frame *> &prefix, aalta_formula *edge_cons)
{
    aalta_formula *block_formula = edge_cons;
    for (auto it = Syn_Frame::failure_state.begin(); it != Syn_Frame::failure_state.end(); ++it)
    {
        aalta_formula *tmp = (aalta_formula *)(Syn_Frame::bddP_to_afP[ull(*it)]);
        block_formula = aalta_formula(
                            aalta_formula::And, block_formula, global_not(tmp))
                            .unique();
    }
    auto it = prefix.begin();
    for (int i = 0; i < prefix.size() - 1; ++i, ++it)
    {
        aalta_formula *tmp = (*it)->GetFormulaPointer();
        block_formula = aalta_formula(
                            aalta_formula::And, block_formula, global_not(tmp))
                            .unique();
    }
    aalta_formula *tmp = (prefix.back())->GetFormulaPointer();
    tmp = aalta_formula(aalta_formula::WNext, NULL, global_not(tmp)).unique();
    block_formula = aalta_formula(aalta_formula::And, block_formula, tmp).unique();
    return block_formula->simplify();
}

void BlockState(CARChecker &checker, list<Syn_Frame *> &prefix)
{
    aalta_formula::af_prt_set to_block;
    for (auto it = Syn_Frame::failure_state.begin(); it != Syn_Frame::failure_state.end(); ++it)
    {
        aalta_formula *tmp = (aalta_formula *)(Syn_Frame::bddP_to_afP[ull(*it)]);
        tmp->to_or_set(to_block);
    }
    for (auto it = prefix.begin(); it != prefix.end(); ++it)
        ((*it)->GetFormulaPointer())->to_or_set(to_block);
    for (auto it = to_block.begin(); it != to_block.end(); ++it)
    {
        cout<<"Add Constraint State: " << (*it)->to_string() << endl;
        checker.add_constraint((*it),true,true);
    }
}