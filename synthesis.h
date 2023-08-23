#ifndef __SYNTHESIS__
#define __SYNTHESIS__

#include <unordered_set>
#include <list>
#include <map>
#include <unordered_map>
#include <sys/time.h>

#include "formula_in_bdd.h"
#include "deps/CUDD-install/include/cudd.h"
#include "carchecker.h"

using namespace std;

typedef enum
{
    Unknown,
    Realizable,
    Unrealizable
} Status;

typedef enum
{
    To_winning_state,
    To_failure_state,
    Accepting_edge,
    NoWay,
    // for Y, there is some X such that \delta(s,X\cup Y) has no successor
    Incomplete_Y
} Signal;

// main entry
bool is_realizable(aalta_formula *src_formula, unordered_set<string> &env_var, const struct timeval &prog_start, bool verbose = false);

class Syn_Frame
{
public:
    static int print_state_cnt;
    static unordered_map<int, string> print_states;
    static string get_print_id(int state_id);

    // number of variables
    static int num_varX_;
    static int num_varY_;

    // set of X- Y- variables
    static set<int> var_X;
    static set<int> var_Y;

    // the bdd pointer of winning/failure state
    static unordered_set<ull> winning_state;
    static unordered_set<ull> failure_state;

    static vector<DdNode *> winning_state_vec;
    static vector<DdNode *> failure_state_vec;

    static void insert_winning_state(DdNode *bddP);
    static void insert_winning_state(FormulaInBdd *state_in_bdd_);

    static void insert_failure_state(DdNode *bddP, aalta_formula *afP);
    static void insert_failure_state(Syn_Frame *syn_frame_);
    static void insert_failure_state(FormulaInBdd *state_in_bdd_);

    bool KnownFailure(bool verbose = false);
    bool KnownWinning(bool verbose = false);

    // from bdd pointer to aalta_formula pointer
    // for blocking failure state
    static map<ull, ull> bddP_to_afP;

    static int sat_call_cnt;
    static long double average_sat_time;

    Syn_Frame(aalta_formula *);
    ~Syn_Frame() { delete state_in_bdd_; }
    Status CheckRealizability(bool verbose = false);
    inline DdNode *GetBddPointer()
    {
        return state_in_bdd_->GetBddPointer();
    }
    inline aalta_formula *GetFormulaPointer()
    {
        return state_in_bdd_->GetFormulaPointer();
    }

    void calc_X_base();

    // tell the frame the result of current choice
    // and the frame performs some operations
    void process_signal(Signal signal, bool verbose = false);
    void process_signal_printInfo(Signal signal, aalta_formula *before_af, aalta_formula *after_af);

    // whther the current frame is
    // the beginning of a sat trace
    inline bool IsTraceBeginning()
    {
        return is_trace_beginning_;
    }

    inline bool IsNotTryingY()
    {
        return current_Y_ == NULL;
    }

    // return the constraint on edge
    aalta_formula *GetEdgeConstraint();
    aalta_formula *GetRealX_constraint()
    {
        return aalta_formula(aalta_formula::And, X_base_, X_constraint_).unique();
    }

    void SetTravelDirection(aalta_formula *Y, aalta_formula *X);
    inline void ResetTravelDirection()
    {
        current_Y_ = NULL;
        current_X_ = NULL;
    }

    inline void SetTraceBeginning() { is_trace_beginning_ = true; }

    void PrintInfo();

private:
    FormulaInBdd *state_in_bdd_;

    // constraint for Y variables
    // initialize by TRUE
    aalta_formula *Y_constraint_;

    // constraint for X variables
    // initialize by TRUE
    aalta_formula *X_constraint_;

    aalta_formula *X_base_;

    aalta_formula *current_Y_;
    aalta_formula *current_X_;

    int winning_checked_idx_;
    int failure_checked_idx_;

    // whther the current frame is
    // the beginning of a sat trace
    bool is_trace_beginning_;
};

Status Expand(list<Syn_Frame *> &searcher, const struct timeval &prog_start, bool verbose = false);

aalta_formula *FormulaProgression(aalta_formula *predecessor, unordered_set<int> &edge);

bool BaseWinningAtY(aalta_formula *end_state, unordered_set<int> &Y);

bool repeat_with_prefix(list<Syn_Frame *> &prefix, aalta_formula *dfa_state, bool verbose = false);

// partition atoms and save index values respectively
void PartitionAtoms(aalta_formula *af, unordered_set<string> &env_val);

aalta_formula *xnf(aalta_formula *af);

// check the constraint for edge is unsat
// there is no temporal operator in the input formula
inline bool EdgeConstraintIsUnsat(aalta_formula *edge)
{
    edge = edge->add_tail();
    CARChecker checker(edge, false, false);
    return !(checker.check());
}

// // return edgecons && G!(PREFIX) && G!(failure)
// aalta_formula *ConstructBlockFormula(list<Syn_Frame *> &prefix, aalta_formula *edge_cons);
void BlockState(CARChecker &checker, list<Syn_Frame *> &prefix, bool verbose = false);

inline aalta_formula *global_not(aalta_formula *phi)
{
    aalta_formula *not_phi = aalta_formula(aalta_formula::Not, NULL, phi).nnf();
    return aalta_formula(aalta_formula::Release, aalta_formula::FALSE(), not_phi).unique();
}

bool IsAcc(aalta_formula *predecessor, unordered_set<int> &tmp_edge);

bool RepeatState(list<Syn_Frame *> &prefix, DdNode *state);

#endif