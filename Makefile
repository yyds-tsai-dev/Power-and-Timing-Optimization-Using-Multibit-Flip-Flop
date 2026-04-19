# Compiler & Linker settings
CXX = g++
CXXFLAGS = -I ./inc -I ./lib -std=c++14 -fopenmp
OPTFLAGS = -march=native -funroll-loops -finline-functions -ffast-math -O3
DEBUGFLAGS = -DENABLE_DEBUG_DP -DENABLE_DEBUG_LGZ -DENABLE_DEBUG_CHECKER -DENABLE_DEBUG_TIMER -DENABLE_DEBUG_MS -DENABLE_DEBUG_BAN -DENABLE_DEBUG_MGR #-DNDEBUG(for assert)
RELEASEFLAGS = -DNDEBUG
WARNINGS = -g -Wall -static

# OR-tools integration (Route A: window-local ILP banking).
# USE_ORTOOLS=1 pulls in MILP path; default OFF → byte-exact Blossom fallback.
USE_ORTOOLS ?= 0
ORT_ROOT = third_party/ortools
ifeq ($(USE_ORTOOLS),1)
    CXXFLAGS += -std=c++17 -DUSE_ORTOOLS \
                -DUSE_BOP -DUSE_CBC -DUSE_CLP -DUSE_GLOP -DUSE_HIGHS \
                -DUSE_MATH_OPT -DUSE_PDLP -DUSE_SCIP \
                -DOR_PROTO_DLL= -DPROTOBUF_USE_DLLS \
                -DHAVE_CONFIG_H -DEIGEN_MPL2_ONLY \
                -isystem $(ORT_ROOT)/include \
                -isystem $(ORT_ROOT)/include/coin \
                -isystem $(ORT_ROOT)/include/highs \
                -isystem $(ORT_ROOT)/include/eigen3
    # OR-tools ships dynamic .so libs; cannot combine with -static on the final link.
    # Override WARNINGS to drop -static when USE_ORTOOLS=1. Binary needs rpath to ortools/lib64.
    WARNINGS := $(filter-out -static,$(WARNINGS))
    LINKER += -L$(ORT_ROOT)/lib64 \
              -Wl,-rpath,$(abspath $(ORT_ROOT))/lib64 \
              -Wl,--copy-dt-needed-entries \
              -lortools
    # -std=c++17 appears twice in CXXFLAGS (once above, once original c++14); c++17 wins last.
    CXXFLAGS := $(filter-out -std=c++14,$(CXXFLAGS))
endif

# Valgrind for memory issue
CHECKCC = valgrind
CHECKFLAGS = --leak-check=full -s --show-leak-kinds=all --track-origins=yes --log-file="valgrind.log"

# Cppcheck for static analysis
CPPCHECKCC = cppcheck
CPPCHECKFLAGS = --enable=all --inconclusive --std=c++11 --language=c++ \
				--suppress=missingIncludeSystem --suppress=style --suppress=unusedFunction \
				--suppress=unreadVariable --suppress=unmatchedSuppression \
				--suppress=unusedStructMember --suppress=functionConst \
				--suppress=useStlAlgorithm \
				#--output-file=cppcheck_log.txt

# Checker
SANITY_CHECKER := checker/sanity
PLACEMENT_CHECKER := checker/placement_checker
EVALUATOR := evaluator/preliminary-evaluator

# Test cases
TEST_CASES := \
    testcase/sampleCase \
    testcase/testcase1_0812.txt \
    testcase/testcase2_0812.txt \
    testcase/testcase1_MBFF.txt \
    testcase/testcase2_MBFF.txt

# Source files and object files
SRCDIR = src
OBJDIR = obj
INCDIR = inc
SRCS = $(wildcard $(SRCDIR)/*.cpp)
OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.d,$(SRCS))

# Control the build verbosity
ifeq ("$(VERBOSE)","1")
    Q :=
    VECHO = @true
else
    Q := @
    VECHO = @printf
endif

# directory for regression
REGDIR = regression

.PHONY: all check clean calc setup_ortools toy_mip

# Name of the executable
BIN = cadb_0015_final

all: $(BIN)

$(OBJDIR):
	@mkdir $(OBJDIR)

$(BIN): main.cpp $(OBJS)
	$(VECHO) "	LD\t$@\n"
	$(Q)$(CXX) $(WARNINGS) $(DEBUGFLAGS) $(CXXFLAGS) $(OPTFLAGS) $^ -o $@ $(LINKER)

# Include generated dependency files
-include $(DEPS)

# Compilation rule for object files with automatic dependency generation
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR) Makefile
	$(VECHO) "	CC\t$@\n"
	$(Q)$(CXX) $(WARNINGS) $(DEBUGFLAGS) $(CXXFLAGS) $(OPTFLAGS) -MMD -c $< -o $@

release: 
	rm -rf $(OBJDIR)
	$(MAKE) DEBUGFLAGS="$(RELEASEFLAGS)" BIN=cadb_0015_final

setup:
	chmod +x $(SANITY_CHECKER)
	chmod +x $(PLACEMENT_CHECKER)

# Pattern rule for running tests
define RUN_TEST
time ./$(BIN) $1 $1.out 2>&1 | tee $1.log
./$(SANITY_CHECKER) $1 $1.out 2>&1 | tee $1.sanity
./$(PLACEMENT_CHECKER) $1 $1.out 2>&1 | tee $1.placement_checker
./$(EVALUATOR) $1 $1.out 2>&1 | tee $1.evaluator
endef

# Targets for each test case
run1: setup
	$(call RUN_TEST,testcase/sampleCase)

run2: setup
	$(call RUN_TEST,testcase/testcase1_0812.txt)

run3: setup
	$(call RUN_TEST,testcase/testcase2_0812.txt)

run4: setup
	$(call RUN_TEST,testcase/testcase3.txt)

run5: setup
	$(call RUN_TEST,testcase/testcase1_MBFF.txt)

run6: setup
	$(call RUN_TEST,testcase/testcase2_MBFF.txt)

run7: setup
	$(call RUN_TEST,testcase/testcase1_MBFF_ALL0.txt)

run8: setup
	$(call RUN_TEST,testcase/testcase2_MBFF_ALL0.txt)

run9: setup
	$(call RUN_TEST,testcase/testcase1_ALL0.txt)

run10: setup
	$(call RUN_TEST,testcase/testcase2_ALL0.txt)

run11: setup
	$(call RUN_TEST,testcase/testcase1_NEG.txt)

run12: setup
	$(call RUN_TEST,testcase/testcase2_NEG.txt)

run13: setup
	$(call RUN_TEST,testcase/testcase1_reverseNEG.txt)

run14: setup
	$(call RUN_TEST,testcase/testcase2_reverseNEG.txt)
  
run15: setup
	$(call RUN_TEST,testcase/testcase2_upright.txt)

run16: setup
	$(call RUN_TEST,testcase/testcase1_MBFF_LIB.txt)

# Target to run all tests
runall: run1 run2 run3 run4 run5 run6 run7 run8 run9 run10 run11 run12 run13 run14 run15 run16

drawALL:
	./drawDie/drawDie -i Preprocessor.out -m 1_Preprocessor.png -t Preprocessor -g -p -nl -o
	./drawDie/drawDie -i PreLegalize.out -m PreLegalize.png -t PreLegalize -g -p -nl -o
	./drawDie/drawDie -i Banking.out -m 3_Banking.png -t Banking -g -p -nl -o
	./drawDie/drawDie -i PostCG.out -m 4_PostCG.png -t PostCG -g -p -nl -o
	./drawDie/drawDie -i Legalize.out -m 5_Legalize.png -t Legalize -g -p -nl -o

draw:
	./drawDie/drawDie -i DetailPlacement.out -m 6_DetailPlacement.png -t DetailPlacement -g -p -nl -o

valgrind:
	$(CHECKCC) $(CHECKFLAGS) ./$(BIN) testcase/testcase3.txt testcase/testcase3.txt.out
	cat valgrind.log

cppcheck:
	$(CPPCHECKCC) $(CPPCHECKFLAGS) -I$(INCDIR) $(SRCDIR) main.cpp $(INCDIR)/*.h

calc:
	(find src inc -type f -name "*.cpp" -o -name "*.h" && echo main.cpp) | xargs wc -l | tail -n 1

# One-shot OR-tools setup: downloads AlmaLinux-8.10 prebuilt tarball, extracts
# to third_party/, links `ortools` alias, and hides the bundled boost dir
# (its presence would shadow system boost and break Banking.cpp's c++17 mix).
# Run once before `make USE_ORTOOLS=1`. Idempotent.
ORT_VERSION = 9.15.6755
ORT_TARBALL = or-tools_x86_64_AlmaLinux-8.10_cpp_v$(ORT_VERSION).tar.gz
ORT_URL = https://github.com/google/or-tools/releases/download/v9.15/$(ORT_TARBALL)
ORT_EXTRACTED = third_party/or-tools_x86_64_AlmaLinux-8.10_cpp_v$(ORT_VERSION)
setup_ortools:
	mkdir -p third_party
	@if [ ! -d "$(ORT_EXTRACTED)" ]; then \
	  echo "Fetching $(ORT_URL)"; \
	  curl -sSL --max-time 300 -o third_party/$(ORT_TARBALL) "$(ORT_URL)"; \
	  tar xzf third_party/$(ORT_TARBALL) -C third_party/; \
	  rm -f third_party/$(ORT_TARBALL); \
	else echo "[setup_ortools] $(ORT_EXTRACTED) already present"; fi
	ln -sfn $(notdir $(ORT_EXTRACTED)) third_party/ortools
	@if [ -d "third_party/ortools/include/boost" ]; then \
	  mv third_party/ortools/include/boost third_party/ortools/include/_bundled_boost_unused_by_public_api; \
	  echo "[setup_ortools] hid bundled boost (public API does not need it)"; \
	fi
	@echo "[setup_ortools] done. Use: make USE_ORTOOLS=1"

# Route A infrastructure smoke: toy set-partitioning MILP via OR-tools.
# Runs independently of main binary; validates third_party/ortools integration.
toy_mip: tools/toy_set_partition.cpp
	$(CXX) -std=c++17 -O2 \
	    -DUSE_BOP -DUSE_CBC -DUSE_CLP -DUSE_GLOP -DUSE_HIGHS \
	    -DUSE_MATH_OPT -DUSE_PDLP -DUSE_SCIP \
	    -DOR_PROTO_DLL= -DPROTOBUF_USE_DLLS \
	    -DHAVE_CONFIG_H -DEIGEN_MPL2_ONLY \
	    -isystem $(ORT_ROOT)/include \
	    -isystem $(ORT_ROOT)/include/coin \
	    -isystem $(ORT_ROOT)/include/highs \
	    -isystem $(ORT_ROOT)/include/eigen3 \
	    $< \
	    -L$(ORT_ROOT)/lib64 -Wl,-rpath,$(abspath $(ORT_ROOT))/lib64 \
	    -Wl,--copy-dt-needed-entries -lortools \
	    -o tools/toy_set_partition
	./tools/toy_set_partition

boost:
	chmod +x scripts/GetBoost.sh
	./scripts/GetBoost.sh
	rm boost_1_84_0.tar.gz

gitlog:
	git log --graph --decorate --oneline

doxygen: Doxyfile
	doxygen Doxyfile

view_doxygen: doxygen
	cd doxyfile/html/; python3 -m http.server 8000;

clean:
	rm -rf $(OBJDIR) $(BIN) doxyfile testcase/*.out testcase/*.log testcase/*.sanity testcase/*.placement_checker testcase/*.evaluator *.log *.out *.png

# TODO, make release to compile with static-linking
# ref: https://github.com/fbacchus/MaxHS/issues/3 valgrind error when using valgrind