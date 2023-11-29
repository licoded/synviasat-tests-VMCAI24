#include <unordered_set>
#include <cassert>
#include <iostream>
#include <list>

#include "generalizer.h"
#include "synthesis.h"
#include "formula/aalta_formula.h"
#include "carchecker.h"

using namespace std;
using namespace aalta;

aalta_formula *Generalize_(aalta_formula *state, unordered_set<int> &save, unordered_set<int> &to_reduce, Simulate simulate)
{
    unordered_set<int> reduced;
    for (auto it = to_reduce.begin(); it != to_reduce.end();)
    {
        if (simulate(state, save, to_reduce, reduced, (*it)) != Undefined)
        {
            reduced.insert(*it);
            it = to_reduce.erase(it);
        }
        else
            ++it;
    }
    if (to_reduce.empty())
        return aalta_formula::TRUE();
    auto it = to_reduce.begin();
    aalta_formula *reduced_formula = int_to_aalta(*it);
    for (++it; it != to_reduce.end(); ++it)
    {
        reduced_formula = aalta_formula(aalta_formula::And, reduced_formula, int_to_aalta(*it)).unique();
    }
    return reduced_formula;
    // return mk_and(to_reduce);
}

Ternary TestFprog(aalta_formula *state, unordered_set<int> &save, unordered_set<int> &to_reduce, unordered_set<int> &reduced, int test_lit)
{
    assert(state != NULL);
    int op = state->oper();
    if (op == aalta_formula::True)
        return Tt;
    else if (op == aalta_formula::False)
        return Ff;
    else if (state == aalta_formula::TAIL())
        return Ff;
    else if (op == aalta_formula::And || op == aalta_formula::Or)
    {
        Ternary l_val = TestFprog(state->l_af(), save, to_reduce, reduced, test_lit);
        Ternary r_val = TestFprog(state->r_af(), save, to_reduce, reduced, test_lit);
        return Calculate(op, l_val, r_val);
    }
    else if (op == aalta_formula::WNext || op == aalta_formula::Next)
    {
        return Valued;
    }
    else if (op == aalta_formula::Not || op >= 11)
    { // literal
        int lit = (op >= 11) ? op : (-((state->r_af())->oper()));
        if (lit == test_lit || (-lit) == test_lit || reduced.find(lit) != reduced.end() || reduced.find(-lit) != reduced.end())
            return Undefined;
        else if (save.find(lit) != save.end() || to_reduce.find(lit) != to_reduce.end())
            return Tt;
        else
            return Ff;
    }
    else if (op == aalta_formula::Until)
    { // l U r = r | (l & X(l U r))
        Ternary l_val = TestFprog(state->l_af(), save, to_reduce, reduced, test_lit);
        Ternary r_val = TestFprog(state->r_af(), save, to_reduce, reduced, test_lit);
        return Calculate(aalta_formula::Or, r_val, Calculate(aalta_formula::And, l_val, Valued));
    }
    else if (op == aalta_formula::Release)
    { // l R r = r & (l | N(l R r))
        Ternary l_val = TestFprog(state->l_af(), save, to_reduce, reduced, test_lit);
        Ternary r_val = TestFprog(state->r_af(), save, to_reduce, reduced, test_lit);
        return Calculate(aalta_formula::And, r_val, Calculate(aalta_formula::Or, l_val, Valued));
    }
}

Ternary TestAcc(aalta_formula *state, unordered_set<int> &save, unordered_set<int> &to_reduce, unordered_set<int> &reduced, int test_lit)
{
    assert(state != NULL);
    int op = state->oper();
    if (op == aalta_formula::True || op == aalta_formula::WNext)
        return Tt;
    else if (op == aalta_formula::False || op == aalta_formula::Next)
        return Ff;
    else if (state == aalta_formula::TAIL())
        return Tt;
    else if (op == aalta_formula::And || op == aalta_formula::Or)
    {
        Ternary l_val = TestAcc(state->l_af(), save, to_reduce, reduced, test_lit);
        Ternary r_val = TestAcc(state->r_af(), save, to_reduce, reduced, test_lit);
        return Calculate(op, l_val, r_val);
    }
    else if (op == aalta_formula::Not || op >= 11)
    { // literal
        int lit = (op >= 11) ? op : (-((state->r_af())->oper()));
        if (lit == test_lit || (-lit) == test_lit || reduced.find(lit) != reduced.end() || reduced.find(-lit) != reduced.end())
            return Undefined;
        else if (save.find(lit) != save.end() || to_reduce.find(lit) != to_reduce.end())
            return Tt;
        else
            return Ff;
    }
    else if (op == aalta_formula::Until || op == aalta_formula::Release)
    {
        return TestAcc(state->r_af(), save, to_reduce, reduced, test_lit);
    }
}

Ternary TestIncompleteY(aalta_formula *state, unordered_set<int> &save, unordered_set<int> &to_reduce, unordered_set<int> &reduced, int test_lit)
{
    assert(state != NULL);
    int op = state->oper();
    if (op == aalta_formula::True || op == aalta_formula::Next || op == aalta_formula::WNext)
        return Tt;
    else if (op == aalta_formula::False)
        return Ff;
    else if (op == aalta_formula::And)
    {
        Ternary l_val = TestIncompleteY(state->l_af(), save, to_reduce, reduced, test_lit);
        Ternary r_val = TestIncompleteY(state->r_af(), save, to_reduce, reduced, test_lit);
        return Calculate(aalta_formula::And, l_val, r_val);
    }
    else if (op == aalta_formula::Or || op == aalta_formula::Until)
    {
        Ternary l_val = TestIncompleteY(state->l_af(), save, to_reduce, reduced, test_lit);
        Ternary r_val = TestIncompleteY(state->r_af(), save, to_reduce, reduced, test_lit);
        return Calculate(aalta_formula::Or, l_val, r_val);
    }
    else if (op == aalta_formula::Not || op >= 11)
    { // literal
        int lit = (op >= 11) ? op : (-((state->r_af())->oper()));
        if (lit == test_lit || (-lit) == test_lit || reduced.find(lit) != reduced.end())
            return Undefined;
        else if (save.find(lit) != save.end() || to_reduce.find(lit) != to_reduce.end())
            return Tt;
        else
            return Ff;
    }
    else if (op == aalta_formula::Release)
    {
        return TestIncompleteY(state->r_af(), save, to_reduce, reduced, test_lit);
    }
}

aalta_formula *Generalize(aalta_formula *state, aalta_formula *Y, aalta_formula *X, Signal signal)
{
    switch (signal)
    {
    case To_winning_state:
    {
        unordered_set<int> to_reduce, save;
        Y->to_set(save);
        X->to_set(to_reduce);
        return Generalize_(state, save, to_reduce, TestFprog);
    }
    case To_failure_state:
    {
        unordered_set<int> to_reduce, save;
        X->to_set(save);
        Y->to_set(to_reduce);
        return Generalize_(state, save, to_reduce, TestFprog);
    }
    case Accepting_edge:
    {
        unordered_set<int> to_reduce, save;
        Y->to_set(save);
        X->to_set(to_reduce);
        return Generalize_(state, save, to_reduce, TestAcc);
    }
    case Incomplete_Y:
    {
        unordered_set<int> to_reduce, save;
        Y->to_set(to_reduce);
        return Generalize_(state, save, to_reduce, TestIncompleteY);
    }
    case NoWay:
    {
        aalta_formula::af_prt_set assump = Y->to_set();
        return GetUnsatAssump(state, assump);
    }
    }
}

Ternary CheckCompleteY(aalta_formula *state, unordered_set<int> &Y)
{
    assert(state != NULL);
    int op = state->oper();
    if (op == aalta_formula::True || op == aalta_formula::Next || op == aalta_formula::WNext)
        return Tt;
    else if (op == aalta_formula::False)
        return Ff;
    else if (op == aalta_formula::And)
    {
        Ternary l_val = CheckCompleteY(state->l_af(), Y);
        Ternary r_val = CheckCompleteY(state->r_af(), Y);
        return Calculate(aalta_formula::And, l_val, r_val);
    }
    else if (op == aalta_formula::Or || op == aalta_formula::Until)
    {
        Ternary l_val = CheckCompleteY(state->l_af(), Y);
        Ternary r_val = CheckCompleteY(state->r_af(), Y);
        return Calculate(aalta_formula::Or, l_val, r_val);
    }
    else if (op == aalta_formula::Not || op >= 11)
    { // literal
        int lit = (op >= 11) ? op : (-((state->r_af())->oper()));
        if (Y.find(lit) != Y.end())
            return Tt;
        else if (Y.find(-lit) != Y.end())
            return Ff;
        else
            return Undefined;
    }
    else if (op == aalta_formula::Release)
        return CheckCompleteY(state->r_af(), Y);
}

Ternary Calculate(int op, Ternary l_val, Ternary r_val)
{
    if (op == aalta_formula::And)
    {
        if (l_val == Tt)
            return r_val;
        if (r_val == Tt)
            return l_val;
        if (l_val == Ff || r_val == Ff)
            return Ff;
        else if (l_val == Undefined || r_val == Undefined)
            return Undefined;
        else if (l_val == Valued || r_val == Valued)
            return Valued;
    }
    else
    {
        if (l_val == Ff)
            return r_val;
        if (r_val == Ff)
            return l_val;
        if (l_val == Tt || r_val == Tt)
            return Tt;
        else if (l_val == Undefined || r_val == Undefined)
            return Undefined;
        else if (l_val == Valued || r_val == Valued)
            return Valued;
    }
}

aalta_formula *mk_and(aalta_formula::af_prt_set *af_set)
{
    if (af_set->empty())
        return aalta_formula::TRUE();
    auto it = af_set->begin();
    aalta_formula *res = (*it);
    for (++it; it != af_set->end(); ++it)
        res = aalta_formula(aalta_formula::And, res, (*it)).unique();
    return res;
}

bool IsUnsat(aalta_formula *phi)
{
    phi = phi->add_tail();
    phi = phi->remove_wnext();
    phi = phi->simplify();
    phi = phi->split_next();
    CARChecker checker(phi, false, false);
    return !(checker.check());
}

bool IsUnsat(aalta_formula *phi, aalta_formula::af_prt_set *psi, list<aalta_formula::af_prt_set *> &S, aalta_formula::af_prt_set &muc)
{
    phi = aalta_formula(aalta_formula::And, phi, mk_and(psi)).unique();
    for (auto it = S.begin(); it != S.end(); ++it)
        phi = aalta_formula(aalta_formula::And, phi, mk_and(*it)).unique();
    phi = aalta_formula(aalta_formula::And, phi, mk_and(&muc)).unique();
    phi = phi->add_tail();
    phi = phi->remove_wnext();
    phi = phi->simplify();
    phi = phi->split_next();
    CARChecker checker(phi, false, false);
    return !(checker.check());
}

aalta_formula *GetUnsatAssump(aalta_formula *phi, aalta_formula::af_prt_set &assumption)
{
    aalta_formula::af_prt_set *assum = new aalta_formula::af_prt_set(assumption);
    if (assum->size() == 0)
        return aalta_formula::TRUE();
    list<aalta_formula::af_prt_set *> S;
    aalta_formula::af_prt_set muc;
    S.push_back(assum);
    while (!S.empty())
    {
        auto psi = S.back();
        S.pop_back();
        if (psi->size() == 1)
        {
            muc.insert(*(psi->begin()));
            delete psi;
            continue;
        }
        aalta_formula::af_prt_set *psi_1 = new aalta_formula::af_prt_set();
        aalta_formula::af_prt_set *psi_2 = new aalta_formula::af_prt_set();
        auto it = psi->begin();
        int i = 0;
        for (; i < (psi->size()) / 2; ++it, ++i)
            psi_1->insert(*it);
        for (; i < (psi->size()); ++it, ++i)
            psi_2->insert(*it);
        delete psi;
        if (IsUnsat(phi, psi_1, S, muc))
        {
            S.push_back(psi_1);
            delete psi_2;
        }
        else if (IsUnsat(phi, psi_2, S, muc))
        {
            S.push_back(psi_2);
            delete psi_1;
        }
        else
        {
            S.push_back(psi_1);
            S.push_back(psi_2);
        }
    }
    return mk_and(&muc);
}