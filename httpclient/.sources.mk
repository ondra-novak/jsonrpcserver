CPP_SRCS += $(wildcard $(dir $(lastword $(MAKEFILE_LIST)))*.cpp)
ifeq "$(with_openssl)" "1"
CPP_SRCS += $(wildcard $(dir $(lastword $(MAKEFILE_LIST)))openssl/*.cpp)
endif
