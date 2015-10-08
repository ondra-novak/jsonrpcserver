LIBNAME:=libjsonrpcserver.a
LIBDEPS:=libjsonrpcserver.deps

SEARCHPATHS="./ ../ ../../ /usr/include/ /usr/local/include/"

selectpath=$(abspath $(firstword $(foreach dir,$(1),$(wildcard $(dir)$(2)))))

INCLUDES=-I$(call selectpath,$(SEARCHPATHS),lightspeed/src/) 


CXX=clang++

CPP_SRCS := 
OBJS := 
clean_list :=

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



.PHONY: debug all clean

debug: $(LIBNAME) 

.INTERMEDIATE : deprun


deprun: 
	@echo "Updating dependencies..."; touch deprun;


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
	@PWD=`pwd`;find $$PWD "(" -name "*.h" -or -name "*.tcc" ")" -and -printf '%p \\\n' > $@
	@for K in $(abspath $(CPP_SRCS)); do echo "$$K \\" >> $@;done

$(LIBNAME): $(OBJS) $(LIBDEPS) 
	@echo "Creating library $@ ..."		
	@$(AR) -r $@ $(OBJS) 2> /dev/null
	
print-%  : ; @echo "$* = $($*)"

clean: 
	$(RM) $(clean_list)

ifneq "$(MAKECMDGOALS)" "clean"
-include ${DEPS}
endif
	
