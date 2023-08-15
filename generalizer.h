#ifndef __GENERALIZER__
#define __GENERALIZER__

#include <unordered_set>

#include "formula/aalta_formula.h"
#include "synthesis.h"

using namespace aalta;
using namespace std;

typedef enum
{
    ReduceX,
    ReduceY
} ReduceTarget;

typedef enum
{
    Tt,
    Ff,
    Undefined,
    Valued
} Ternary;

typedef Ternary (*Simulate)(aalta_formula *state, unordered_set<int> &save, unordered_set<int> &to_reduce, unordered_set<int> &reduced, int test_lit);

aalta_formula *Generalize(aalta_formula *state, aalta_formula *Y, aalta_formula *X, Signal signal);

Ternary CheckCompleteY(aalta_formula *state, unordered_set<int> &Y);

Ternary Calculate(int op, Ternary l_val, Ternary r_val);

bool IsUnsat(aalta_formula *phi);

// MUC
aalta_formula *GetUnsatAssump(aalta_formula *phi, aalta_formula::af_prt_set &assumption);

inline aalta_formula *int_to_aalta(int lit)
{
    int op = ((lit > 0) ? lit : (-lit));
    aalta_formula *tmp = aalta_formula(op, NULL, NULL).unique();
    return (lit > 0) ? tmp : aalta_formula(aalta_formula::Not, NULL, tmp).unique();
}

#endif