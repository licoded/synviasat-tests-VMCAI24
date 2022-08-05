#include <iostream>
#include <cassert>

#include "formula_in_bdd.h"
#include "formula/aalta_formula.h"
#include "deps/CUDD-install/include/cudd.h"

using namespace std;
using namespace aalta;

DdManager *FormulaInBdd::global_bdd_manager_ = NULL;
unordered_map<ull, ull> FormulaInBdd::aaltaP_to_bddP_;
aalta_formula *FormulaInBdd::src_formula_ = NULL;
DdNode *FormulaInBdd::TRUE_bddP_;
DdNode *FormulaInBdd::FALSE_bddP_;

void FormulaInBdd::InitBdd4LTLf(aalta_formula *src_formula, bool is_xnf)
{
    src_formula_ = src_formula;
    global_bdd_manager_ = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    TRUE_bddP_ = Cudd_ReadOne(global_bdd_manager_);
    FALSE_bddP_ = Cudd_ReadLogicZero(global_bdd_manager_);
    CreatedMap4AaltaP2BddP(src_formula_, is_xnf);
    // PrintMapInfo();
}

void FormulaInBdd::CreatedMap4AaltaP2BddP(aalta_formula *af, bool is_xnf)
{
    if (af == NULL)
        return;
    int op = af->oper();
    if (op == aalta_formula::True || op == aalta_formula::False)
        return;
    if (op == aalta_formula::And || op == aalta_formula::Or)
    {
        CreatedMap4AaltaP2BddP(af->l_af(), is_xnf);
        CreatedMap4AaltaP2BddP(af->r_af(), is_xnf);
        return;
    }
    if (op == aalta_formula::Not)
    {
        CreatedMap4AaltaP2BddP(af->r_af(), is_xnf);
        return;
    }
    af = af->unique();
    if (is_xnf)
        GetPaOfXnf(af);
    else
    {
        if (aaltaP_to_bddP_.find(ull(af)) == aaltaP_to_bddP_.end())
            aaltaP_to_bddP_[ull(af)] = ull(Cudd_bddNewVar(global_bdd_manager_));
    }
    if (op == aalta_formula::Next || op == aalta_formula::WNext)
    {
        CreatedMap4AaltaP2BddP(af->r_af(), is_xnf);
        return;
    }
    else if (op == aalta_formula::Until || op == aalta_formula::Release)
    {
        CreatedMap4AaltaP2BddP(af->l_af(), is_xnf);
        CreatedMap4AaltaP2BddP(af->r_af(), is_xnf);
        return;
    }
    else if (op >= 11)
    {
        return;
    }
}

void FormulaInBdd::GetPaOfXnf(aalta_formula *psi)
{
    if (psi == NULL)
        return;
    int op = psi->oper();
    assert(op != aalta_formula::True && op != aalta_formula::False);
    assert(op != aalta_formula::And && op != aalta_formula::Or && op != aalta_formula::Not);
    if (op == aalta_formula::Next || op == aalta_formula::WNext || op >= 11)
    {
        if (aaltaP_to_bddP_.find(ull(psi)) == aaltaP_to_bddP_.end())
            aaltaP_to_bddP_[ull(psi)] = ull(Cudd_bddNewVar(global_bdd_manager_));
        return;
    }
    else if (op == aalta_formula::Until)
    {
        // X_U=X(psi)
        aalta_formula *X_U = aalta_formula(aalta_formula::Next, NULL, psi).unique();
        if (aaltaP_to_bddP_.find(ull(X_U)) == aaltaP_to_bddP_.end())
            aaltaP_to_bddP_[ull(X_U)] = ull(Cudd_bddNewVar(global_bdd_manager_));
        return;
    }
    else if (op == aalta_formula::Release)
    {
        // N_R=N(psi)
        aalta_formula *N_R = aalta_formula(aalta_formula::WNext, NULL, psi).unique();
        if (aaltaP_to_bddP_.find(ull(N_R)) == aaltaP_to_bddP_.end())
            aaltaP_to_bddP_[ull(N_R)] = ull(Cudd_bddNewVar(global_bdd_manager_));
        return;
    }
}

DdNode *FormulaInBdd::ConstructBdd(aalta_formula *af)
{
    if (af == NULL)
        return NULL;
    int op = af->oper();
    if (op == aalta_formula::True)
        return TRUE_bddP_;
    else if (op == aalta_formula::False)
        return FALSE_bddP_;
    else if (op == aalta_formula::Not)
    {
        DdNode *tmp = ConstructBdd(af->r_af());
        DdNode *cur = Cudd_Not(tmp);
        Cudd_Ref(cur);
        Cudd_RecursiveDeref(global_bdd_manager_, tmp);
        return cur;
    }
    else if (op == aalta_formula::Or)
    {
        DdNode *tmpL = ConstructBdd(af->l_af());
        DdNode *tmpR = ConstructBdd(af->r_af());
        DdNode *cur = Cudd_bddOr(global_bdd_manager_, tmpL, tmpR);
        Cudd_Ref(cur);
        Cudd_RecursiveDeref(global_bdd_manager_, tmpL);
        Cudd_RecursiveDeref(global_bdd_manager_, tmpR);
        return cur;
    }
    else if (op == aalta_formula::And)
    {
        DdNode *tmpL = ConstructBdd(af->l_af());
        DdNode *tmpR = ConstructBdd(af->r_af());
        DdNode *cur = Cudd_bddAnd(global_bdd_manager_, tmpL, tmpR);
        Cudd_Ref(cur);
        Cudd_RecursiveDeref(global_bdd_manager_, tmpL);
        Cudd_RecursiveDeref(global_bdd_manager_, tmpR);
        return cur;
    }
    else
    {
        return (DdNode *)(aaltaP_to_bddP_[ull(af)]);
    }
}

void FormulaInBdd::PrintMapInfo()
{
    cout << "src formula:" << src_formula_->to_string() << "\nPropositional Atoms:\n";
    for (auto it = FormulaInBdd::aaltaP_to_bddP_.begin(); it != FormulaInBdd::aaltaP_to_bddP_.end(); ++it)
        cout << ((aalta_formula *)(it->first))->to_string() << endl;
}