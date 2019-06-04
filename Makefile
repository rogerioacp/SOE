#
# Copyright (C) 2011-2018 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#

######## SGX SDK Settings ########

SGX_SDK ?= /opt/intel/sgxsdk
SGX_MODE ?= HW
SGX_ARCH ?= x64
SGX_DEBUG ?= 1
PGSQL_PATH ?= /usr/local/pgsql
INSTALL_PATH ?= /usr/local



#top_builddir = /home/rogerio/sf_pgs/postgresql/
#include $(top_builddir)/src/Makefile.global
#include $(top_srcdir)/contrib/contrib-global.mk

ifeq ($(shell getconf LONG_BIT), 32)
	SGX_ARCH := x86
else ifeq ($(findstring -m32, $(CXXFLAGS)), -m32)
	SGX_ARCH := x86
endif

ifeq ($(SGX_ARCH), x86)
	SGX_COMMON_CFLAGS := -m32
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x86/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x86/sgx_edger8r
else
	SGX_COMMON_CFLAGS := -m64
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib64
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x64/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x64/sgx_edger8r
endif




ifeq ($(SGX_DEBUG), 1)
ifeq ($(SGX_PRERELEASE), 1)
$(error Cannot set SGX_DEBUG and SGX_PRERELEASE at the same time!!)
endif
endif

ifeq ($(SGX_DEBUG), 1)
        SGX_COMMON_CFLAGS += -O0 -g
else
        SGX_COMMON_CFLAGS += -O2
endif

ifeq ($(SUPPLIED_KEY_DERIVATION), 1)
        SGX_COMMON_CFLAGS += -DSUPPLIED_KEY_DERIVATION
endif
######## App Settings ########

ifneq ($(SGX_MODE), HW)
	Urts_Library_Name := sgx_urts_sim
else
	Urts_Library_Name := sgx_urts
endif

Untrusted_C_Flags := $(SGX_COMMON_CFLAGS) -fPIC -Wno-attributes -I$(SGX_SDK)/include -Isrc/include/backend/enclave/

# Three configuration modes - Debug, prerelease, release
#   Debug - Macro DEBUG enabled.
#   Prerelease - Macro NDEBUG and EDEBUG enabled.
#   Release - Macro NDEBUG enabled.
ifeq ($(SGX_DEBUG), 1)
        Untrusted_C_Flags += -DDEBUG -UNDEBUG -UEDEBUG
else ifeq ($(SGX_PRERELEASE), 1)
        Untrusted_C_Flags += -DNDEBUG -DEDEBUG -UDEBUG
else
        Untrusted_C_Flags += -DNDEBUG -UEDEBUG -UDEBUG
endif



UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	Utrust_Flags += -shared
endif
ifeq ($(UNAME_S),Darwin)
	Utrust_Flags = -shared -dynamic -undefined dynamic_lookup 
endif

######## Enclave Settings ########

ifneq ($(SGX_MODE), HW)
	Trts_Library_Name := sgx_trts_sim
	Service_Library_Name := sgx_tservice_sim
else
	Trts_Library_Name := sgx_trts
	Service_Library_Name := sgx_tservice
endif
Crypto_Library_Name := sgx_tcrypto

Enclave_Include_Paths := -I$(SGX_SDK)/include -I$(SGX_SDK)/include/tlibc -I$(SGX_SDK)/include/libcxx

Soe_Include_Path :=  -I/usr/local/include -Isrc/include/ -Isrc/include/backend -Isrc/include/backend/enclave

COLLECTC_LADD :=  -lcollectc
ORAM_LADD := -loram
SOE_LADD = $(ORAM_LADD) $(COLLECTC_LADD)


CC_BELOW_4_9 := $(shell expr "`$(CC) -dumpversion`" \< "4.9")
ifeq ($(CC_BELOW_4_9), 1)
	Enclave_C_Flags := $(SGX_COMMON_CFLAGS) -nostdinc -fvisibility=hidden -fpie -ffunction-sections -fdata-sections -fstack-protector
else
	Enclave_C_Flags := $(SGX_COMMON_CFLAGS) -nostdinc -fvisibility=hidden -fpie -ffunction-sections -fdata-sections -fstack-protector-strong
endif

ifneq ($(UNSAFE), 1)
	Enclave_C_Flags +=  $(Enclave_Include_Paths)
else
	Enclave_C_Flags = $(SGX_COMMON_CFLAGS) -Wall -fPIC -DUNSAFE
endif

ifeq ($(USE_VALGRIND), 1)
	Enclave_C_Flags += -DUSE_VALGRIND
endif

Enclave_C_Flags += $(Soe_Include_Path)


# To generate a proper enclave, it is recommended to follow below guideline to link the trusted libraries:
#    1. Link sgx_trts with the `--whole-archive' and `--no-whole-archive' options,
#       so that the whole content of trts is included in the enclave.
#    2. For other libraries, you just need to pull the required symbols.
#       Use `--start-group' and `--end-group' to link these libraries.
# Do NOT move the libraries linked with `--start-group' and `--end-group' within `--whole-archive' and `--no-whole-archive' options.
# Otherwise, you may get some undesirable errors.
Enclave_Link_Flags := -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles  -L$(SGX_LIBRARY_PATH)  \
	-Wl,--whole-archive -l$(Trts_Library_Name) -Wl,--no-whole-archive \
	-Wl,--start-group -lsgx_tstdc -lsgx_tcxx -lsgx_tkey_exchange -l$(Crypto_Library_Name) -l$(Service_Library_Name)  -Wl,--end-group \
	-Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
	-Wl,-pie,-eenclave_entry -Wl,--export-dynamic  \
	-Wl,--defsym,__ImageBase=0 -Wl,--gc-sections   \
	-Wl,--version-script=src/backend/enclave/enclave.lds 


Enclave_Lib := libsoe.so
Signed_Enclave_Lib := libsoe.signed.so
Untrusted_Lib = libsoeu.so
Unsafe_Lib = libsoeus.so
Enclave_Config_File := src/backend/enclave/Enclave.config.xml

ifeq ($(SGX_MODE), HW)
ifeq ($(SGX_DEBUG), 1)
	Build_Mode = HW_DEBUG
else ifeq ($(SGX_PRERELEASE), 1)
	Build_Mode = HW_PRERELEASE
else
	Build_Mode = HW_RELEASE
endif
else
ifeq ($(SGX_DEBUG), 1)
	Build_Mode = SIM_DEBUG
else ifeq ($(SGX_PRERELEASE), 1)
	Build_Mode = SIM_PRERELEASE
else
	Build_Mode = SIM_RELEASE
endif
endif


Pgsql_C_Flags=-I$(PGSQL_PATH)/include/ #-I$(PGSQL_PATH)/include/internal

.PHONY: all run


ifeq ($(UNSAFE), 1)
all: $(Unsafe_Lib)
else
all: .config_$(Build_Mode)_$(SGX_ARCH) $(Signed_Enclave_Lib) $(Untrusted_Lib)
ifeq ($(Build_Mode), HW_DEBUG)
	@echo "The project has been built in debug hardware mode."
else ifeq ($(Build_Mode), SIM_DEBUG)
	@echo "The project has been built in debug simulation mode."
else ifeq ($(Build_Mode), HW_PRERELEASE)
	@echo "The project has been built in pre-release hardware mode."
else ifeq ($(Build_Mode), SIM_PRERELEASE)
	@echo "The project has been built in pre-release simulation mode."
else
	@echo "The project has been built in release simulation mode."
endif
endif


run: all
ifneq ($(Build_Mode), HW_RELEASE)
	@$(CURDIR)/$(App_Name) 	
	@echo "RUN  =>  $(App_Name) [$(SGX_MODE)|$(SGX_ARCH), OK]"
endif

.config_$(Build_Mode)_$(SGX_ARCH):
	@rm -f .config_* $(App_Name) $(App_Name) $(Enclave_Lib) $(Signed_Enclave_Lib) $(App_Cpp_Objects) isv_app/isv_enclave_u.*  isv_enclave/isv_enclave_t.* libservice_provider.* $(ServiceProvider_Cpp_Objects)
	@touch .config_$(Build_Mode)_$(SGX_ARCH)


######## Untrusted side Objects ########

enclave_u.c: src/backend/enclave/Enclave.edl
	$(SGX_EDGER8R) --untrusted Enclave.edl --search-path src/backend/enclave --search-path $(SGX_SDK)/include
	mv Enclave_u.c src/backend/enclave 
	mv Enclave_u.h src/include/backend/enclave
	@echo "GEN  =>  $@"

enclave_u.o: enclave_u.c
	$(CC) $(Untrusted_C_Flags) -c src/backend/enclave/Enclave_u.c  -o $@



######## Enclave Objects ########

enclave_t.c: src/backend/enclave/Enclave.edl
	$(SGX_EDGER8R) --trusted Enclave.edl --search-path src/backend/enclave --search-path $(SGX_SDK)/include
	mv Enclave_t.c src/backend/enclave 
	mv Enclave_t.h src/include/backend/enclave
	@echo "GEN  =>  $@"


enclave_t.o: enclave_t.c
	$(CC) $(Enclave_C_Flags) -c src/backend/enclave/Enclave_t.c  -o $@
	@echo "CC   <=  $<"

######## SOE Objects ##############


# common objects

soe.o: src/backend/soe.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_orandom.o: src/random/soe_orandom.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

logger.o:  src/logger/logger.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_bufpage.o: src/backend/storage/page/soe_bufpage.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_bufmgr.o: src/backend/storage/buffer/soe_bufmgr.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@ 

soe_heap_ofile.o: src/backend/storage/buffer/soe_heap_ofile.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_heapam.o: src/backend/access/heap/soe_heapam.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_heaptuple.o: src/backend/access/common/soe_heaptuple.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_qsort.o: src/backend/utils/soe_qsort.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_indextuple.o: src/backend/access/common/soe_indextuple.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@


#hash files

soe_hash.o: src/backend/access/hash/soe_hash.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_hashinsert.o: src/backend/access/hash/soe_hashinsert.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_hashovfl.o: src/backend/access/hash/soe_hashovfl.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_hashpage.o: src/backend/access/hash/soe_hashpage.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_hashutil.o: src/backend/access/hash/soe_hashutil.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_hashsearch.o: src/backend/access/hash/soe_hashsearch.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_hash_ofile.o: src/backend/storage/buffer/soe_hash_ofile.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_hashfunc.o: src/backend/access/hash/soe_hashfunc.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

# nbtree files
soe_nbtutils.o: src/backend/access/nbtree/soe_nbtutils.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_nbtpage.o: src/backend/access/nbtree/soe_nbtpage.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_nbtree.o: src/backend/access/nbtree/soe_nbtree.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_nbtsearch.o:  src/backend/access/nbtree/soe_nbtsearch.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_nbtinsert.o: src/backend/access/nbtree/soe_nbtinsert.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@


$(Enclave_Lib): enclave_t.o logger.o soe_heap_ofile.o soe_hash_ofile.o soe_heaptuple.o soe_hashsearch.o soe_hashutil.o soe_hashpage.o soe_hashovfl.o soe_hashinsert.o soe_bufmgr.o soe_qsort.o soe_bufpage.o soe_heapam.o soe_hash.o soe_orandom.o soe_hashfunc.o soe_indextuple.o  soe_nbtree.o soe_nbtinsert.o soe_nbtsearch.o soe_nbtpage.o soe_nbtutils.o soe.o
	$(CC) $(SGX_COMMON_CFLAGS)  $^ -o $@ -static $(SOE_LADD)  $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

$(Signed_Enclave_Lib): $(Enclave_Lib)
	$(SGX_ENCLAVE_SIGNER) sign -key src/backend/enclave/private.pem -enclave $(Enclave_Lib) -out $@ -config $(Enclave_Config_File)
	@echo "SIGN =>  $@"

$(Untrusted_Lib): enclave_u.o
	$(CC) -shared  $^ -o $@ 

$(Unsafe_Lib):  soe.o logger.o soe_heapam.o soe_hashfunc.o soe_heaptuple.o soe_indextuple.o soe_heap_ofile.o soe_hash_ofile.o soe_hashsearch.o soe_hashutil.o soe_hashpage.o soe_hashovfl.o soe_hashinsert.o soe_bufmgr.o soe_qsort.o soe_bufpage.o soe_hash.o soe_orandom.o soe_nbtree.o soe_nbtinsert.o soe_nbtsearch.o soe_nbtpage.o soe_nbtutils.o
	$(CC) $(Utrust_Flags) $(SGX_COMMON_CFLAGS)  $^ -o $@  $(SOE_LADD)

.PHONY: install

ifneq ($(UNSAFE), 1)
install:
	mkdir -p $(INSTALL_PATH)/lib/soe
	mkdir -p $(INSTALL_PATH)/include/soe
	cp $(Signed_Enclave_Lib) $(INSTALL_PATH)/lib/soe
	cp $(Untrusted_Lib) $(INSTALL_PATH)/lib/soe
	cp src/include/backend/enclave/* $(INSTALL_PATH)/include/soe
	cp src/include/backend/ops.h $(INSTALL_PATH)/include/soe
	chmod 755 $(INSTALL_PATH)/lib/soe/$(Signed_Enclave_Lib)
	chmod 755 $(INSTALL_PATH)/lib/soe/$(Untrusted_Lib)
	chmod 644 $(INSTALL_PATH)/include/soe/*
else
install:
	mkdir -p $(INSTALL_PATH)/lib/soe
	mkdir -p $(INSTALL_PATH)/include/soe
	cp $(Unsafe_Lib) $(INSTALL_PATH)/lib/soe
	cp src/include/backend/enclave/* $(INSTALL_PATH)/include/soe
	cp src/include/backend/ops.h $(INSTALL_PATH)/include/soe
	chmod 755 $(INSTALL_PATH)/lib/soe/$(Unsafe_Lib)
	chmod 644 $(INSTALL_PATH)/include/soe/*		
endif


.PHONY: clean

clean:
	rm -f .config_*  $(Enclave_Lib) $(Signed_Enclave_Lib)
	rm -rf *.o
