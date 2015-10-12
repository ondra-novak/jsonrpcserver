ifeq "$(with_dbhelpers)" "1"
CPP_SRCS += $(wildcard $(dir $(lastword $(MAKEFILE_LIST)))*.cpp)
endif