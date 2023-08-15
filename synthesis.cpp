#include <list>
#include <cassert>
#include <map>
#include <unordered_map>
#include <sys/time.h>

#include "synthesis.h"
#include "carchecker.h"
#include "generalizer.h"

using namespace std;
using namespace aalta;

int Syn_Frame::print_state_cnt = 0;
unordered_map<int, string> Syn_Frame::print_states;
string Syn_Frame::get_print_id(int state_id)
{
    print_states.insert({state_id, "state"+to_string(print_states.size()+1)});
    return print_states.at(state_id);
}
// static public variable of Syn_Frame
int Syn_Frame::num_varX_;
int Syn_Frame::num_varY_;
set<int> Syn_Frame::var_X;
set<int> Syn_Frame::var_Y;
unordered_set<ull> Syn_Frame::winning_state;
unordered_set<ull> Syn_Frame::failure_state;
map<ull, ull> Syn_Frame::bddP_to_afP;
int Syn_Frame::sat_call_cnt;
long double Syn_Frame::average_sat_time;

bool is_realizable(aalta_formula *src_formula, unordered_set<string> &env_var, const struct timeval &prog_start, bool verbose)
{
    //   partition atoms and save index values respectively
    PartitionAtoms(src_formula, env_var);
    if (verbose)
    {
        cout << "initial state: " << src_formula->to_string() << endl;
        cout << "Y variables:";
        for (auto item : Syn_Frame::var_Y)
            cout << ' ' << aalta_formula(item, NULL, NULL).to_string();
        cout << "\nX variables:";
        for (auto item : Syn_Frame::var_X)
            cout << ' ' << aalta_formula(item, NULL, NULL).to_string();
        cout << endl;
    }

    Syn_Frame::sat_call_cnt = 0;
    Syn_Frame::average_sat_time = 0;

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
    if (verbose)
        cout << "Start searching......\n"
             << endl;
    while (true)
    {
        Syn_Frame *cur_frame = searcher.back();
        if (verbose)
        {
            cout << "\n----------new epi----------" << endl
                 << "the current size of the searching stack: " << searcher.size() << endl
                 << "information about top item of the stack:" << endl;
            cur_frame->PrintInfo();
            cout << endl;
        }

        Status peek = cur_frame->CheckRealizability(verbose);
        if (verbose)
        {
            cout << "current realizability of the top item: ";
            if (peek == Unknown)
                cout << "Unknown" << endl;
            else if (peek == Realizable)
                cout << "Realizable" << endl;
            else
                cout << "Unealizable" << endl;
        }

        switch (peek)
        {
        case Realizable:
        {
            if (verbose)
                cout << "Top item is Realizable";
            if (searcher.size() == 1)
            {
                if (verbose)
                    cout << ", and size of stack is 1." << endl
                         << "finish searching, synthesis result is Realizable" << endl;
                delete cur_frame;
                FormulaInBdd::QuitBdd4LTLf();
                return true;
            }
            if (verbose)
                cout << ", then pop the top item." << endl
                     << "insert to winning state: " << (cur_frame->GetFormulaPointer())->to_string() << endl
                     << "insert to winning state id: " << Syn_Frame::get_print_id((cur_frame->GetFormulaPointer())->id()) << endl;
            Syn_Frame::winning_state.insert(ull(cur_frame->GetBddPointer()));
            delete cur_frame;
            searcher.pop_back();
            (searcher.back())->process_signal(To_winning_state, verbose);
            break;
        }
        case Unrealizable:
        {
            if (verbose)
                cout << "Top item is Unrealizable";
            if (searcher.size() == 1)
            {
                if (verbose)
                    cout << ", and size of stack is 1." << endl
                         << "finish searching, synthesis result is Unrealizable" << endl;
                delete cur_frame;
                FormulaInBdd::QuitBdd4LTLf();
                return false;
            }
            if (verbose)
                cout << ", then pop the top item." << endl
                     << "insert to failure state: " << (cur_frame->GetFormulaPointer())->to_string() << endl
                     << "insert to failure state id: " << Syn_Frame::get_print_id((cur_frame->GetFormulaPointer())->id()) << endl;
            Syn_Frame::failure_state.insert(ull(cur_frame->GetBddPointer()));
            Syn_Frame::bddP_to_afP[ull(cur_frame->GetBddPointer())] = ull(cur_frame->GetFormulaPointer());
            delete cur_frame;    //////////////
            searcher.pop_back(); ///////////

            // encounter Unrealizable
            // backtrack the beginning of the sat trace
            if (verbose)
            {
                cout << "encounter failure state, backtrack to the beginning of the sat trace" << endl;
            }
            while (true)
            {
                auto tmp = searcher.back();
                if (tmp->IsTraceBeginning())
                {
                    if (verbose)
                        cout << "arrive at beginning, stop popping" << endl;
                    // tmp->process_signal(To_failure_state, verbose);
                    tmp->ResetTravelDirection();
                    break;
                }
                else
                {
                    if (verbose)
                        cout << "pop state: " << (tmp->GetFormulaPointer())->to_string() << endl;
                    delete tmp;
                    searcher.pop_back();
                }
            }
            break;
        }
        case Unknown:
        {
            if (verbose)
                cout << "Top item is Unknown, expand the search stack" << endl;
            Status res = Expand(searcher, prog_start, verbose);
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

Status Syn_Frame::CheckRealizability(bool verbose)
{
    if (Syn_Frame::winning_state.find(ull(state_in_bdd_->GetBddPointer())) != Syn_Frame::winning_state.end())
    {
        if (verbose)
            cout << "known winning state" << endl;
        return Realizable;
    }
    if (Syn_Frame::failure_state.find(ull(state_in_bdd_->GetBddPointer())) != Syn_Frame::failure_state.end())
    {
        if (verbose)
            cout << "known failure state" << endl;
        return Unrealizable;
    }
    if (EdgeConstraintIsUnsat(Y_constraint_))
    {
        if (verbose)
            cout << "all value of Y-variables has been traveled" << endl;
        return Unrealizable;
    }
    if (EdgeConstraintIsUnsat(X_constraint_))
    {
        if (verbose)
            cout << "for a Y, all value of X-variables has been traveled" << endl;
        return Realizable;
    }
    return Unknown;
}

const vector<string> signal2str = {
    "To_winning_state",
    "To_failure_state",
    "Accepting_edge",
    "NoWay",
    "Incomplete_Y",
};
void Syn_Frame::process_signal_printInfo(Signal signal, aalta_formula *before_af, aalta_formula *after_af)
{
    int before_num = before_af->to_set().size();
    int after_num = after_af->to_set().size();
    if(before_num != after_num)
    {
        std::cout
                << "process_signal-" << signal << "\t"
                << before_num-after_num << "\t"
                << before_num << "\t"
                << after_num << "\t"
                << std::endl;
        std::cout
                << signal2str[signal] << "-" << Syn_Frame::get_print_id(state_in_bdd_->GetFormulaPointer()->id()) << std::endl
                << "\t\t" << before_af->to_literal_set_string() << std::endl
                << "\t\t" << after_af->to_literal_set_string() << std::endl;
    }
}

void Syn_Frame::process_signal(Signal signal, bool verbose)
{
    switch (signal)
    {
    case To_winning_state:
    {
        aalta_formula *x_reduced = Generalize(state_in_bdd_->GetFormulaPointer(), current_Y_, current_X_, To_winning_state);
        if (verbose)
            process_signal_printInfo(signal, current_X_, x_reduced);
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
        if (verbose)
            process_signal_printInfo(signal, current_X_, x_reduced);
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
        if (IsUnsat(state))
        {
            if (verbose)
                cout << "NoWay: (state & X_constraint_) itself is unsat, so the current state is Unrealizable." << endl;
            Syn_Frame::failure_state.insert(ull(state_in_bdd_->GetBddPointer()));
            Syn_Frame::bddP_to_afP[ull(state_in_bdd_->GetBddPointer())] = ull(state_in_bdd_->GetFormulaPointer());
            break;
        }
        aalta_formula *y_reduced = Generalize(state, current_Y_, NULL, NoWay);
        if (verbose)
            process_signal_printInfo(signal, current_Y_, y_reduced);
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
    cout << "state formula: " << (state_in_bdd_->GetFormulaPointer())->to_string() << endl;
    cout << "Y constraint: " << Y_constraint_->to_string() << endl;
    cout << "X constraint: " << X_constraint_->to_string() << endl;
    if (current_Y_ != NULL)
        cout << "current Y: " << current_Y_->to_literal_set_string() << endl;
    if (current_X_ != NULL)
        cout << "current X: " << current_X_->to_literal_set_string() << endl;
    cout << (is_trace_beginning_ ? "is " : "is not ") << "a starting point" << endl;
}

void Syn_Frame::SetTravelDirection(aalta_formula *Y, aalta_formula *X)
{
    if (current_Y_ == NULL)
        current_Y_ = Y;
    current_X_ = X;
}

Status Expand(list<Syn_Frame *> &searcher, const struct timeval &prog_start, bool verbose)
{
    Syn_Frame *tp_frame = searcher.back();
    aalta_formula *edge_constraint = tp_frame->GetEdgeConstraint();
    if (verbose)
    {
        cout << "expand the stack by ltlf-sat" << endl
             << "state formula: " << (tp_frame->GetFormulaPointer())->to_string() << endl
             << "state id: " << Syn_Frame::get_print_id((tp_frame->GetFormulaPointer())->id()) << endl
             << "is finding new " << ((tp_frame->IsNotTryingY()) ? "Y" : "X") << endl
             << "constraint of edge: " << edge_constraint->to_string() << endl;
    }
    // aalta_formula *block_formula = ConstructBlockFormula(searcher, edge_constraint);
    aalta_formula *f;
    if (edge_constraint->oper() != aalta_formula::True)
        f = aalta_formula(aalta_formula::And, tp_frame->GetFormulaPointer(), edge_constraint).unique();
    else
        f = tp_frame->GetFormulaPointer();
    // cout << f->to_string() << endl;
    f = f->add_tail();
    f = f->remove_wnext();
    f = f->simplify();
    f = f->split_next();
    if (verbose)
        cout << "Construct Checker: " << f->to_string() << endl;
    Syn_Frame::sat_call_cnt += 1;
    struct timeval t1, t2;
    long double timeuse;
    // cout << "begin sat solving" << endl;
    gettimeofday(&t1, NULL);
    CARChecker checker(f, false, true);
    BlockState(checker, searcher, verbose);
    bool check_res = checker.check();
    gettimeofday(&t2, NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
    Syn_Frame::average_sat_time = Syn_Frame::average_sat_time + (timeuse - Syn_Frame::average_sat_time) / Syn_Frame::sat_call_cnt;
    long double time_to_start = (t2.tv_sec - prog_start.tv_sec) * 1000.0 + (t2.tv_usec - prog_start.tv_usec) / 1000.0;
    if ((time_to_start - 300000.0) > 0)
    {
        cout << "Runtimeout" << endl;
        cout << "failure state cnt: " << (Syn_Frame::failure_state.size()) - 1 << endl;
        cout << "sat cnt: " << Syn_Frame::sat_call_cnt << endl;
        cout << "average sat time: " << Syn_Frame::average_sat_time << " ms" << endl;
        exit(0);
    }
    if (check_res)
    { // sat
        if (verbose)
        {
            cout << "SAT checking result: sat" << endl;
            checker.print_evidence();
            cout << "push items to stack:" << endl;
        }
        vector<pair<aalta_formula *, aalta_formula *>> *tr = checker.get_model_for_synthesis();
        tp_frame->SetTraceBeginning();
        for (int i = 0; i < ((tr->size()) - 1); ++i)
        {
            aalta_formula *Y_edge = ((*tr)[i]).first;
            aalta_formula *X_edge = ((*tr)[i]).second;
            (searcher.back())->SetTravelDirection(Y_edge, X_edge);
            aalta_formula *predecessor = (searcher.back())->GetFormulaPointer();
            { // check whether accepting in advance
                unordered_set<int> tmp_edge;
                X_edge->to_set(tmp_edge);
                Y_edge->to_set(tmp_edge);
                if (IsAcc(predecessor, tmp_edge))
                {
                    cout << "accepting in advance, total is " << (tr->size()) << ", acc at " << i + 1 << endl;
                }
            }
            unordered_set<int> edge;
            Y_edge->to_set(edge);
            // if (CheckCompleteY(predecessor, edge) != Tt)
            // {
            //     if (verbose)
            //         cout << "Incomplete Y" << endl;
            //     (searcher.back())->process_signal(Incomplete_Y, verbose);
            //     return Unknown;
            // }
            X_edge->to_set(edge);
            aalta_formula *successor = FormulaProgression(predecessor, edge);
            // successor = xnf(successor);
            Syn_Frame *frame = new Syn_Frame(successor);
            // { // check same state
            //     if (RepeatState(searcher, frame->GetBddPointer()))
            //     {
            //         cout << "State that repeats" << endl;
            //     }
            // }
            if (verbose)
                cout << "\t\tby edge: " << endl
                     << "\t\t\tY = " << Y_edge->to_literal_set_string() << endl
                     << "\t\t\tX = " << X_edge->to_literal_set_string() << endl
                     << "\t\tto state: " << successor->to_string() << endl
                     << "\t\tto state id: " << Syn_Frame::get_print_id(successor->id()) << endl;
            searcher.push_back(frame);
        }
        // the last position is the accepting edge
        if (verbose)
            cout << "last position of sat trace is accepting edge (we will then check if BaseWinningAtY)" << endl;
        aalta_formula *Y_edge = (tr->back()).first;
        aalta_formula *end_state = (searcher.back())->GetFormulaPointer();
        unordered_set<int> edge;
        Y_edge->to_set(edge); // edge only with Y-literals here
        if (BaseWinningAtY(end_state, edge))
        {
            if (verbose)
            {
                cout << "=====BaseWinningAtY\tBEGIN\nfor acc edge, Y\\models state, so top item is base-winning: " << end_state->to_string() << endl;
            }
            if (searcher.size() == 1)
            {
                if (verbose)
                    cout << "size of stack is 1." << endl
                         << "finish searching, synthesis result is Realizable" << endl;
                return Realizable;
            }
            else
            {
                if (verbose)
                    cout << "pop the top item." << endl
                         << "insert to winning state: " << ((searcher.back())->GetFormulaPointer())->to_string() << endl
                         << "insert to winning state id: " << Syn_Frame::get_print_id(((searcher.back())->GetFormulaPointer())->id()) << endl
                         << "=====BaseWinningAtY\tEND" << endl;
                Syn_Frame::winning_state.insert(ull((searcher.back())->GetBddPointer()));
                delete searcher.back();
                searcher.pop_back();
                (searcher.back())->process_signal(To_winning_state, verbose);
            }
        }
        else
        {
            aalta_formula *X_edge = (tr->back()).second;
            (searcher.back())->SetTravelDirection(Y_edge, X_edge);
            if (verbose)
                cout << "=====not BaseWinningAtY\tBEGIN" << endl
                     << "accepting edge:" << endl
                     << "\t\tY = " << Y_edge->to_literal_set_string() << endl
                     << "\t\tX = " << X_edge->to_literal_set_string() << endl;
            (searcher.back())->process_signal(Accepting_edge, verbose);
        }
    }
    else
    { // unsat
        if (verbose)
        {
            cout << "SAT checking result: unsat (we will then check is finding new X or new Y)" << endl;
        }
        if (tp_frame->IsNotTryingY())
        { // current frame is unrealizable immediately
            if (verbose)
            {
                cout << "=====\nis finding new Y but unsat (not found), "
                     << "so the top item is unrealizable immediately" << endl;
            }
            if (searcher.size() == 1)
            {
                if (verbose)
                    cout << "size of stack is 1." << endl
                         << "finish searching, synthesis result is Unrealizable\n=====" << endl;
                return Unrealizable;
            }
            else
            {
                if (verbose)
                    cout << "pop the top item." << endl
                         << "insert to failure state: " << ((searcher.back())->GetFormulaPointer())->to_string() << endl
                         << "insert to failure state id: " << Syn_Frame::get_print_id(((searcher.back())->GetFormulaPointer())->id()) << endl;
                auto bdd_ptr = (searcher.back())->GetBddPointer();
                Syn_Frame::failure_state.insert(ull(bdd_ptr));
                Syn_Frame::bddP_to_afP[ull(bdd_ptr)] = ull((searcher.back())->GetFormulaPointer());
                delete (searcher.back());
                searcher.pop_back();
                //(searcher.back())->process_signal(To_failure_state, verbose);
                while (true)
                {
                    auto tmp = searcher.back();
                    if (tmp->IsTraceBeginning())
                    {
                        if (verbose)
                            cout << "arrive at beginning, stop popping" << endl;
                        // tmp->process_signal(To_failure_state, verbose);
                        tmp->ResetTravelDirection();
                        break;
                    }
                    else
                    {
                        if (verbose)
                            cout << "pop state: " << (tmp->GetFormulaPointer())->to_string() << endl;
                        delete tmp;
                        searcher.pop_back();
                    }
                }
            }
        }
        else
        {
            if (verbose)
                cout << "=====\nis finding new X but unsat (not found), "
                     << "so current_Y is not OK, need change a new Y. " << endl;
            tp_frame->process_signal(NoWay, verbose);
        }
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

void BlockState(CARChecker &checker, list<Syn_Frame *> &prefix, bool verbose)
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
        if (verbose)
            cout << "add constraint of blocking state: " << (*it)->to_string() << endl;
        checker.add_constraint((*it));
    }
}

bool IsAcc(aalta_formula *predecessor, unordered_set<int> &tmp_edge)
{
    if (predecessor == NULL)
        return false;
    int op = predecessor->oper();
    if (op == aalta_formula::True || op == aalta_formula::WNext)
        return true;
    else if (op == aalta_formula::False || op == aalta_formula::Next)
        return false;
    else if (op == aalta_formula::And)
        return BaseWinningAtY(predecessor->l_af(), tmp_edge) && BaseWinningAtY(predecessor->r_af(), tmp_edge);
    else if (op == aalta_formula::Or)
        return BaseWinningAtY(predecessor->l_af(), tmp_edge) || BaseWinningAtY(predecessor->r_af(), tmp_edge);
    else if (op == aalta_formula::Not || op >= 11)
    { // literal
        int lit = (op >= 11) ? op : (-((predecessor->r_af())->oper()));
        return tmp_edge.find(lit) != tmp_edge.end();
    }
    else if (op == aalta_formula::Until || op == aalta_formula::Release)
        return BaseWinningAtY(predecessor->r_af(), tmp_edge);
}

bool RepeatState(list<Syn_Frame *> &prefix, DdNode *state)
{
    for (auto it = prefix.begin(); it != prefix.end(); ++it)
        if (state == ((*it)->GetBddPointer()))
            return true;
    return false;
}