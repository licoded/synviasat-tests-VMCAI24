/*
 * File:   onechecker.cpp
 * Author: Yongkang Li
 * Note: one-step move for LTLf synthesis
 * Created on November 23, 2023
 */

#include "onechecker.h"
#include <iostream>

using namespace std;

namespace aalta
{
	bool OneChecker::check ()
	{
		if (to_check_->oper() == aalta_formula::True)
		{
			if (evidence_ != NULL)
				evidence_->push(true);
			return true;
		}
		if (to_check_->oper() == aalta_formula::False)
			return false;
		return check(to_check_);
	}

	bool OneChecker::check (aalta_formula* f)
    {
		if (sat_once (f))
		{
			if (verbose_)
				cout << "sat once is true, return from here\n";
			return true;
		}
		else if (f->is_global ())
		{
			return false;
		}

		// onestep check
        Transition* t = get_one_transition_from (f);
        if (t != NULL)
        {
            if (verbose_)
                cout << "getting transition:\n" << t->label ()->to_string() << " -> " << t->next ()->to_string () << endl;
            if (evidence_ != NULL)
                evidence_->push (t->label ());
            return true;
        }

        return false;
    }

	void OneChecker::print_evidence ()
	{
		assert (evidence_ != NULL);
		evidence_->print ();
	}
}