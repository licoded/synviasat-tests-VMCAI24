/*
 * File:   onechecker.h
 * Author: Yongkang Li
 * Note: one-step move for LTLf synthesis
 * Created on November 23, 2023
 */

#ifndef ONE_CHECKER_H
#define ONE_CHECKER_H

#include "formula/aalta_formula.h"
#include "solver.h"
#include "ltlfchecker.h"
#include "evidence.h"

namespace aalta
{

    class OneChecker : public LTLfChecker
    {
    public:
        OneChecker (aalta_formula *f, bool verbose = false, bool evidence = false)
            : LTLfChecker (f, verbose, evidence)
		{
            solver_ = new CARSolver(f, verbose);
            if (evidence)
                evidence_ = new Evidence();
            else
                evidence_ = NULL;
		}

        bool check ();
        bool check (aalta_formula* f);

        void print_evidence ();
    };

}

#endif