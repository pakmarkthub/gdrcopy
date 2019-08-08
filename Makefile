PREFIX ?= /usr/local
DESTLIB ?= $(PREFIX)/lib64
CUDA ?= /usr/local/cuda

GDRAPI_ARCH := $(shell ./config_arch)

CUDA_LIB := -L $(CUDA)/lib64 -L $(CUDA)/lib -L /usr/lib64/nvidia -L /usr/lib/nvidia
CUDA_INC += -I $(CUDA)/include

CPPFLAGS := $(CUDA_INC) -I gdrdrv/ -I $(CUDA)/include -D GDRAPI_ARCH=$(GDRAPI_ARCH)
LDFLAGS  := $(CUDA_LIB) -L $(CUDA)/lib64
COMMONCFLAGS := -O2
CFLAGS   += $(COMMONCFLAGS)
CXXFLAGS += $(COMMONCFLAGS)
LIBS     := -lcudart -lcuda -lpthread -ldl

LIB_MAJOR_VER:=1
LIB_VER:=$(LIB_MAJOR_VER).2
LIB_BASENAME:=libgdrapi.so
LIB_DYNAMIC=$(LIB_BASENAME).$(LIB_VER)
LIB_SONAME=$(LIB_BASENAME).$(LIB_MAJOR_VER)
LIB:=$(LIB_DYNAMIC)


LIBSRCS := gdrapi.c
ifeq ($(GDRAPI_ARCH),X86)
LIBSRCS += memcpy_avx.c memcpy_sse.c memcpy_sse41.c
endif

LIBOBJS := $(LIBSRCS:.c=.o)

SRCS := copybw.cpp sanity.cpp bug2673250.cpp
EXES := $(SRCS:.cpp=)


all: config lib driver exes

config:
	echo "GDRAPI_ARCH=$(GDRAPI_ARCH)"

lib: $(LIB)

exes: $(EXES)

install: lib_install #drv_install

lib_install:
	@ echo "installing in $(PREFIX)..." && \
	install -D -v -m u=rw,g=rw,o=r $(LIB_DYNAMIC) -t $(DESTLIB) && \
	install -D -v -m u=rw,g=rw,o=r gdrapi.h -t $(PREFIX)/include/; \
	cd $(DESTLIB); \
	ln -sf $(LIB_DYNAMIC) $(LIB_SONAME); \
	ln -sf $(LIB_SONAME) $(LIB_BASENAME);

#static
#$(LIB): $(LIB)($(LIBOBJS))
#dynamic
$(LIBOBJS): CFLAGS+=-fPIC
$(LIB): $(LIBOBJS)
	$(CC) -shared -Wl,-soname,$(LIB_SONAME) -o $@ $^
	ldconfig -n $(PWD)
	ln -sf $(LIB_DYNAMIC) $(LIB_SONAME)
	ln -sf $(LIB_SONAME) $(LIB_BASENAME)

# special-cased to finely tune the arch option
memcpy_avx.o: memcpy_avx.c
	$(COMPILE.c) -mavx -o $@ $^

memcpy_sse.o: memcpy_sse.c
	$(COMPILE.c) -msse -o $@ $^

memcpy_sse41.o: memcpy_sse41.c
	$(COMPILE.c) -msse4.1 -o $@ $^

gdrapi.o: gdrapi.c gdrapi.h gdrapi_internal.h
copybw.o: copybw.cpp gdrapi.h common.hpp
sanity.o: sanity.cpp gdrapi.h gdrapi_internal.h common.hpp
bug2673250.o: bug2673250.cpp gdrapi.h gdrapi_internal.h common.hpp

copybw: copybw.o $(LIB)
	$(LINK.cc)  -o $@ $^ $(LIBS)

sanity: sanity.o $(LIB)
	$(LINK.cc)  -o $@ $^ $(LIBS) `pkg-config --libs check`

bug2673250: bug2673250.o $(LIB)
	$(LINK.cc)  -o $@ $^ $(LIBS)

driver:
	cd gdrdrv; \
	$(MAKE) $(MAKE_PARAMS)

drv_install:
	cd gdrdrv; \
	$(MAKE) install


clean:
	rm -f *.o $(EXES) lib*.{a,so}* *~ core.* libgdrapi.so* && \
	$(MAKE) -C gdrdrv clean

.PHONY: driver clean all lib exes lib_install install
