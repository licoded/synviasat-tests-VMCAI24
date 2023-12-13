
FORMULAFILES	=	formula/aalta_formula.cpp formula/olg_formula.cpp formula/olg_item.cpp

PARSERFILES		=	ltlparser/ltl_formula.c ltlparser/ltllexer.c ltlparser/ltlparser.c ltlparser/trans.c 

UTILFILES		=	util/utility.cpp utility.cpp

SOLVER			=	minisat/core/Solver.cc aaltasolver.cpp solver.cpp carsolver.cpp 

CHECKING		=	ltlfchecker.cpp carchecker.cpp evidence.cpp

NEW_CHECKING	=	onechecker.cpp ltlfchecker.cpp carchecker.cpp evidence.cpp

SYNTHESIS		=	synthesis.cpp formula_in_bdd.cpp generalizer.cpp

BDD_LIB			=	deps/CUDD-install/lib/libcudd.a


ALLFILES		=	main.cpp $(CHECKING) $(SOLVER) $(FORMULAFILES) $(PARSERFILES) $(UTILFILES) $(SYNTHESIS) $(BDD_LIB) 
ALL_TEST_DEPS	=	$(FORMULAFILES) $(NEW_CHECKING) $(SOLVER) $(UTILFILES) $(PARSERFILES) $(SYNTHESIS) $(BDD_LIB)
ALLFILES_NEW	=	main.cpp $(ALL_TEST_DEPS)


CC	    =   g++
FLAG    = -I./  -I./minisat/  -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -fpermissive #-fsanitize=address -fno-omit-frame-pointer
DEBUGFLAG   =	-D DEBUG -g -pg
RELEASEFLAG = -O3

ltlfsyn :	release

ltlparser/ltllexer.c :
	ltlparser/grammar/ltllexer.l
	flex ltlparser/grammar/ltllexer.l

ltlparser/ltlparser.c :
	ltlparser/grammar/ltlparser.y
	bison ltlparser/grammar/ltlparser.y
	
	

.PHONY :    release debug clean

release :   $(ALLFILES_NEW)
	    $(CC) $(FLAG) $(RELEASEFLAG) $(ALLFILES_NEW) -lm -lz -o ltlfsyn

debug :	$(ALLFILES_NEW)
	$(CC) $(FLAG) $(DEBUGFLAG) $(ALLFILES_NEW) -lm -lz -o ltlfsyn

clean :
	rm -f *.o *~ ltlfsyn
