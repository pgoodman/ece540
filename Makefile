TARGET = project
LIBS =	-lsimple -lcheck -lsuif -luseful
ASN2_OBJS = bin/instr.o bin/diag.o bin/basic_block.o bin/cfg.o \
            bin/data_flow/dom.o bin/data_flow/closure.o \
            bin/loop.o bin/data_flow/var_def.o bin/data_flow/var_use.o \
            bin/data_flow/ae.o bin/opt/cf.o bin/opt/cp.o bin/opt/dce.o \
            bin/optimizer.o bin/use_def.o bin/opt/cse.o bin/opt/licm.o \
            bin/def_use.o bin/operator.o 
OBJS = $(ASN2_OBJS) doproc.o main.o
GLOBALINCLDIRS = -I./
CXXFLAGS += -Wno-variadic-macros
CCFLAGS += -Wno-variadic-macros

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

bin/%.o: lib/%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

bin/data_flow/%.o: lib/data_flow/%.cc lib/data_flow/problem.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

bin/opt/%.o: lib/opt/%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

all: bin_folders $(ASN2_OBJS) $(TARGET)

debug: bin_folders $(ASN2_OBJS) debug_doproc main.o $(TARGET)

debug_doproc: dot.cc
	$(CXX) $(CXXFLAGS) -c dot.cc -o doproc.o

bin_folders: 
	mkdir -p bin/
	mkdir -p bin/opt
	mkdir -p bin/data_flow

install-bin:    install-prog

include $(SUIFHOME)/Makefile.std

