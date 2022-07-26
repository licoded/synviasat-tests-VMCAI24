/*
 * File:   evidence.h
 * Author: Jianwen Li
 * Note: Evidence interface for LTLf satisfiability checking
 * Created on August 30, 2017
 */

#ifndef EVIDENCE_H
#define EVIDENCE_H

#include "formula/aalta_formula.h"
#include "formula/olg_formula.h"
#include <vector>
#include <string>

namespace aalta
{
	class Evidence
	{
	public:
		// functions
		Evidence() { sat_trace_ = new std::vector<std::pair<aalta_formula *, aalta_formula *>>; };
		~Evidence() { delete sat_trace_; }
		void print();
		void push(bool);
		void push(olg_formula &);
		void push(aalta_formula *);
		void pop_back();

		inline std::vector<std::pair<aalta_formula *, aalta_formula *>> *get_model_for_synthesis()
		{
			return sat_trace_;
		}

	private:
		// member
		std::vector<std::string> traces_;

		// for ltlf synthesis
		// tr \in (<Y,X>)*
		std::vector<std::pair<aalta_formula *, aalta_formula *>> *sat_trace_;
	};

	// for synthesis
	// assign undefined value of Y variables
	aalta_formula* fill_in_Y(aalta_formula* partial_Y);
}

#endif
