#ifndef __FORMULA_IN_BDD__
#define __FORMULA_IN_BDD__

#include <unordered_map>

#include "formula/aalta_formula.h"
#include "deps/CUDD-install/include/cudd.h"

using namespace std;
using namespace aalta;

typedef unsigned long long ull;

class FormulaInBdd
{
private:
    static DdManager *global_bdd_manager_;
    static unordered_map<ull, ull> aaltaP_to_bddP_;
    static aalta_formula *src_formula_;

    static void CreatedMap4AaltaP2BddP(aalta_formula *af, bool is_xnf);

    // used in CreatedMap4AaltaP2BddP(aalta_formula *af)
    // for \psi\in cl(af), init the map of PA(xnf(\psi))
    static void GetPaOfXnf(aalta_formula *psi);

    aalta_formula *formula_;
    DdNode *bdd_;

    DdNode *ConstructBdd(aalta_formula *af);

public:
    static DdNode *TRUE_bddP_;
    static DdNode *FALSE_bddP_;

    static void InitBdd4LTLf(aalta_formula *src_formula, bool is_xnf);
    static void QuitBdd4LTLf() { Cudd_Quit(global_bdd_manager_); }

    FormulaInBdd(aalta_formula *af) : formula_(af) { bdd_ = ConstructBdd(af); }
    inline DdNode *GetBddPointer() { return bdd_; }
    inline aalta_formula *GetFormulaPointer() { return formula_; }

    static void PrintMapInfo();
};

#endif