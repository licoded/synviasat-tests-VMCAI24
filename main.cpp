#include <cstring>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <string>
#include <sys/time.h>

#include "formula/aalta_formula.h"
#include "synthesis.h"

using namespace aalta;
using namespace std;

void usage()
{
	cout << endl;
}

void test1()
{
	cout << "test case 1" << endl;
	string s0 = string("F(p0) & ((G(p1)) | (F(G(p1))))");
	string input_f = "(" + s0 + ") & (p1) & (!(p0))";

	aalta_formula *f;
	f = aalta_formula::TAIL();
	aalta_formula::TRUE();
	aalta_formula::FALSE();
	cout << "src input formula: " << input_f << endl;
	f = aalta_formula(input_f.c_str(), true).unique();
	f = f->nnf();
	f = f->add_tail();
	f = f->remove_wnext();
	f = f->simplify();
	f = f->split_next();
	cout << "construct checker:" << f->to_string() << endl;

	CARChecker checker(f, false, true);
	checker.add_constraint(aalta_formula(s0.c_str(), true).unique(), true, true);
	bool res = checker.check();
	cout << (res ? "sat" : "unsat") << endl;
	if (res)
		checker.print_evidence();
}

void test2()
{
	cout << "\ntest case 2" << endl;
	aalta_formula *f;
	f = aalta_formula::TAIL();
	f = aalta_formula("a U b", true).unique();
	f = f->nnf();
	f = f->add_tail();
	f = f->remove_wnext();
	f = f->simplify();
	f = f->split_next();
	cout << "construct checker:" << f->to_string() << endl;

	CARChecker checker(f, false, true);
	checker.add_constraint(aalta_formula("a U b", true).unique(), true, true);
	bool res = checker.check();
	cout << (res ? "sat" : "unsat") << endl;
	if (res)
		checker.print_evidence();
}

void test3()
{
	aalta_formula *f;
	f = aalta_formula::TAIL();
	f = aalta_formula("a U b", true).unique();
	f = f->nnf();
	f = f->add_tail();
	f = f->remove_wnext();
	f = f->simplify();
	f = f->split_next();

	CARChecker checker(f, false, true);
	bool res = checker.check();
	cout << (res ? "sat" : "unsat") << endl;
	if (res)
		checker.print_evidence();
}

void test5()
{
	aalta_formula *f;
	f = aalta_formula::TAIL();
	// ((s_1 & (G ((!h_0 | !h_1) & (h_0 | !t) & (X s_1))) & (F t)) | s_0 | (F ((!s_0 & !s_1) | (!h_0 & (X[!] s_0)) | (h_0 & (X[!] s_1)))))
	aalta_formula *f1 = aalta_formula("((s_1 & (G ((!h_0 | !h_1) & (h_0 | !t) & (X s_1))) & (F t)) | s_0 | (F ((!s_0 & !s_1) | (!h_0 & (X[!] s_0)) | (h_0 & (X[!] s_1)))))", true).unique();
	aalta_formula *edge = aalta_formula("Tail & !h_1 & t & h_0 & s_1 & !s_0", true).unique();
	unordered_set<int> edge_set;
	edge->to_set(edge_set);
	aalta_formula *f2 = FormulaProgression(f1, edge_set);
	cout << f2->to_string() << endl;
	// aalta_formula *f2_false = aalta_formula("((s_1 & (G ((!h_0 | !h_1) & (h_0 | !t) & (X s_1)))) | s_1 | (F ((!s_0 & !s_1) | (!h_0 & (X[!] s_0)) | (h_0 & (X[!] s_1)))))").unique();
	// aalta_formula *f2_true = aalta_formula("True").unique();
}

int main(int argc, char **argv)
{
	// test3();
	// return 0;
	// ltlf_sat(argc, argv);
	// return 0;
	if (argc != 3)
		usage();

	// read formula
	ifstream fin;
	fin.open(argv[1], ios::in);
	if (!fin.is_open())
	{
		cout << "cannot open file " << argv[1] << endl;
		return 0;
	}
	string input_f, tmp;
	unordered_set<string> env_var;
	getline(fin, input_f);
	fin.close();

	// read variables partition
	fin.open(argv[2], ios::in);
	if (!fin.is_open())
	{
		cout << "cannot open file " << argv[1] << endl;
		return 0;
	}
	fin >> tmp;
	while (fin >> tmp)
	{
		if (tmp[0] != '.')
			env_var.insert(tmp);
		else
			break;
	}
	fin.close();

	clock_t startTime, endTime;
	startTime = clock();
	struct timeval t1, t2;
	double timeuse;
	gettimeofday(&t1, NULL);

	// rewrite formula
	aalta_formula *af;
	// set tail id to be 1
	af = aalta_formula::TAIL();
	aalta_formula::TRUE();
	aalta_formula::FALSE();
	af = aalta_formula(input_f.c_str(), true).nnf();
	// af = af->remove_wnext();
	af = af->simplify();
	af = af->split_next();
	af = af->unique();

	// perform synthesis
	// bool result = is_realizable(af, env_var, t1);
	bool result = is_realizable(af, env_var, t1, false);
	if (result)
		cout << "Realizable" << endl;
	else
		cout << "Unrealizable" << endl;
	aalta_formula::destroy();

	gettimeofday(&t2, NULL);
	endTime = clock();
	timeuse = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
	cout << "CPU time: " << 1000 * double(endTime - startTime) / CLOCKS_PER_SEC << " ms" << endl;
	cout << "total time: " << timeuse / 1000.0 << " ms" << endl;
	cout << "sat cnt: " << Syn_Frame::sat_call_cnt << endl;
	cout << "average sat time: " << Syn_Frame::average_sat_time << " ms" << endl;

	return 0;
}
// case:
// F(!a) & (b U (c R (a | G(b))))
// .inputs: a, b
// .outputs: c

// void print_help()
// {
// 	cout << "usage: ./aalta_f [-e|-v|-blsc|-t|-h] [\"formula\"]" << endl;
// 	cout << "\t -e\t:\t print example when result is SAT" << endl;
// 	cout << "\t -v\t:\t print verbose details" << endl;
// 	cout << "\t -blsc\t:\t use the BLSC checking method; Default is CDLSC" << endl;
// 	cout << "\t -t\t:\t print weak until formula" << endl;
// 	cout << "\t -h\t:\t print help information" << endl;
// }

// void ltlf_sat(int argc, char **argv)
// {
// 	bool verbose = false;
// 	bool evidence = false;
// 	int input_count = 0;
// 	bool blsc = false;
// 	bool print_weak_until_free = false;

// 	for (int i = argc; i > 1; i--)
// 	{
// 		if (strcmp(argv[i - 1], "-v") == 0)
// 			verbose = true;
// 		else if (strcmp(argv[i - 1], "-e") == 0)
// 			evidence = true;
// 		else if (strcmp(argv[i - 1], "-blsc") == 0)
// 			blsc = true;
// 		else if (strcmp(argv[i - 1], "-t") == 0)
// 			print_weak_until_free = true;
// 		else if (strcmp(argv[i - 1], "-h") == 0)
// 		{
// 			print_help();
// 			exit(0);
// 		}
// 		else // for input
// 		{
// 			if (input_count > 1)
// 			{
// 				printf("Error: read more than one input!\n");
// 				exit(0);
// 			}
// 			strcpy(in, argv[i - 1]);
// 			input_count++;
// 		}
// 	}
// 	if (input_count == 0)
// 	{
// 		puts("please input the formula:");
// 		if (fgets(in, MAXN, stdin) == NULL)
// 		{
// 			printf("Error: read input!\n");
// 			exit(0);
// 		}
// 	}

// 	aalta_formula *af;
// 	// set tail id to be 1
// 	af = aalta_formula::TAIL();
// 	af = aalta_formula(in, true).unique();
// 	if (print_weak_until_free)
// 	{
// 		cout << af->to_string() << endl;
// 		return;
// 	}

// 	af = af->nnf();
// 	af = af->add_tail();
// 	af = af->remove_wnext();
// 	af = af->simplify();
// 	af = af->split_next();

// 	if (blsc)
// 	{
// 		LTLfChecker checker(af, verbose, evidence);
// 		bool res = checker.check();
// 		printf("%s\n", res ? "sat" : "unsat");
// 		if (evidence && res)
// 			checker.print_evidence();
// 	}
// 	else
// 	{
// 		CARChecker checker(af, verbose, evidence);
// 		bool res = checker.check();
// 		printf("%s\n", res ? "sat" : "unsat");
// 		if (evidence && res)
// 			checker.print_evidence();
// 	}
// 	aalta_formula::destroy();
// }