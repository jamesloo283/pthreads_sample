# Default to debug during development. For release do: make BUILD=rlse
BUILD := dbg
p_NAME := sensormgmt
p_NAMEDBG := sensormgmtdbg
p_CSRCS := $(wildcard *.c)
p_COBJS := ${p_CSRCS:.c=.o}
p_OBJS := $(p_COBJS)
p_INCDIRS := $(wildcard inc/*.h)
p_LIBDIRS :=
p_LIBRARIES :=

common_CPPFLAGS := $(foreach incdir, $(p_INCDIRS),-I$(incdir))
rlse_CPPFLAGS := $(common_CPPFLAGS) -O3
dbg_CPPFLAGS := $(common_CPPFLAGS) -DDEBUGIT=1 -g3
CPPFLAGS += ${${BUILD}_CPPFLAGS}
LDFLAGS += $(foreach libdir,$(p_LIBDIRS),-L$(libdir))
LDFLAGS += $(foreach lib,$(p_LIBRARIES),-l$(lib))

.PHONY: all clean distclean

all: $(p_NAME)$(BUILD)

$(p_NAME)rlse: $(p_OBJS)
	$(LINK.c) $(p_OBJS) -pthread -lrt -o $(p_NAME)

$(p_NAMEDBG): $(p_OBJS)
	$(LINK.c) $(p_OBJS) -pthread -lrt -o $(p_NAMEDBG)

clean:
	@- $(RM) $(p_NAME)
	@- $(RM) $(p_NAMEDBG)
	@- $(RM) $(p_OBJS)

distclean: clean

define OBJ_DEP_HEADERS
 $(1) : ${1:.o=.h}
endef
$(foreach objf,$(p_OBJS),$(eval $(call OBJ_DEP_HEADERS,$(objf))))
