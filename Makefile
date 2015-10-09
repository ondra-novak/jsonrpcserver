LIBNAME:=libjsonrpcserver.a
LIBDEPS:=libjsonrpcserver.deps
with_dbhelpers ?= 0
SEARCHPATHS="./ ../ ../../ /usr/include/ /usr/local/include/"

CXX=clang++
CPP_SRCS := 
OBJS := 
clean_list :=


selectpath=$(abspath $(firstword $(foreach dir,$(1),$(wildcard $(dir)$(2)))))

INCLUDES=-I$(call selectpath,$(SEARCHPATHS),lightspeed/src/) 


ifeq "$(with_dbhelpers)" "1"
INCLUDES+=-I$(call selectpath,$(SEARCHPATHS),lightmysql/src/)
include dbhelpers/dbhelpers.mk
endif 

all: $(LIBNAME)

include $(shell find -name .sources.mk)

ifeq "$(MAKECMDGOALS)" "debug"
	CXXFLAGS += -O0 -g3 -fPIC -Wall -Wextra -DDEBUG -D_DEBUG $(INCLUDES)
	CPPFLAGS += $(INCLUDES) 
	CFGNAME := cfg.debug
else
	CXXFLAGS += -O3 -g3 -fPIC -Wall -Wextra -DNDEBUG $(INCLUDES)
	CPPFLAGS += $(INCLUDES)
	CFGNAME := cfg.release
endif


OBJS += ${CPP_SRCS:.cpp=.o}
DEPS := ${CPP_SRCS:.cpp=.deps}
clean_list += $(OBJS)  ${CPP_SRCS:.cpp=.deps} $(LIBNAME) $(LIBDEPS) $(PLATFORM) cfg.debug cfg.release



.PHONY: debug all clean deps

debug: $(LIBNAME) 

.INTERMEDIATE : deprun


deprun: 
	@echo "Updating dependencies..."; touch deprun;

deps: ${DEPS} ${LIBDEPS}

$(CFGNAME):
	@rm -f cfg.debug cfg.release
	@touch $@	
	@echo Forced rebuild for CXXFLAGS=$(CXXFLAGS)

%.deps: %.cpp deprun  
	@set -e;$(CPP) $(CPPFLAGS) -MM $*.cpp | sed -e 's~^\(.*\)\.o:~$(@D)/\1.deps $(@D)/\1.o:~' > $@

%.o: %.cpp $(CFGNAME)
	@echo $(*F).cpp  
	@$(CXX) $(CXXFLAGS) -o $@ -c $*.cpp

$(LIBDEPS):
	@echo "Generating library dependencies..."
	@echo "LDFLAGS+=-L$$PWD" > $@
	@echo "LDLIB+=-l$(patsubst lib%.a,%,$(LIBNAME))" >> $@
	@echo "LIBDEPS+=$$PWD/$(LIBNAME)" >> $@
	@echo -n "$$PWD/$(LIBNAME) : " >> $@
	@PWD=`pwd`;find $$PWD "(" -name "*.h" -or -name "*.tcc" ")" -and -printf "\\\\\\n\\t%p" >> $@
	@for K in $(abspath $(CPP_SRCS)); do echo -n "\\\\\\n\\t $$K" >> $@;done
	@echo "" >> $@
	@echo -n "\\t\$$(MAKE) -C $$PWD \$$(MAKECMDGOALS) \\n" >> $@
	@echo "$$PWD/$(LIBNAME).clean :" >> $@
	@echo -n "\\t\$$(MAKE) -C $$PWD clean\\n" >> $@

$(LIBNAME): $(OBJS) $(LIBDEPS) 
	@echo "Creating library $@ ..."		
	@$(AR) -r $@ $(OBJS) 2> /dev/null
	
print-%  : ; @echo "$* = $($*)"

clean: 
	$(RM) $(clean_list)

ifneq "$(MAKECMDGOALS)" "clean"
-include ${DEPS}
endif
	
