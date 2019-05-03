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

App_Cpp_Files := isv_app/isv_app.cpp
App_Include_Paths := -Iservice_provider -I$(SGX_SDK)/include

App_C_Flags := $(SGX_COMMON_CFLAGS) -fPIC -Wno-attributes $(App_Include_Paths)

# Three configuration modes - Debug, prerelease, release
#   Debug - Macro DEBUG enabled.
#   Prerelease - Macro NDEBUG and EDEBUG enabled.
#   Release - Macro NDEBUG enabled.
ifeq ($(SGX_DEBUG), 1)
        App_C_Flags += -DDEBUG -UNDEBUG -UEDEBUG
else ifeq ($(SGX_PRERELEASE), 1)
        App_C_Flags += -DNDEBUG -DEDEBUG -UDEBUG
else
        App_C_Flags += -DNDEBUG -UEDEBUG -UDEBUG
endif

App_Cpp_Flags := $(App_C_Flags) -std=c++11
App_Link_Flags := $(SGX_COMMON_CFLAGS) -L$(SGX_LIBRARY_PATH) -l$(Urts_Library_Name) -L. -lsgx_ukey_exchange -lpthread -lservice_provider -Wl,-rpath=$(CURDIR)/sample_libcrypto -Wl,-rpath=$(CURDIR)

ifneq ($(SGX_MODE), HW)
	App_Link_Flags += -lsgx_uae_service_sim
else
	App_Link_Flags += -lsgx_uae_service
endif

App_Cpp_Objects := $(App_Cpp_Files:.cpp=.o)

App_Name := app



######## Enclave Settings ########

ifneq ($(SGX_MODE), HW)
	Trts_Library_Name := sgx_trts_sim
	Service_Library_Name := sgx_tservice_sim
else
	Trts_Library_Name := sgx_trts
	Service_Library_Name := sgx_tservice
endif
Crypto_Library_Name := sgx_tcrypto

Enclave_Cpp_Files := isv_enclave/isv_enclave.cpp
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
#mEnclave_C_Flags += -Wall -std=c99
Enclave_C_Flags +=  $(Enclave_Include_Paths)
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

Pgsql_C_Flags=-I$(PGSQL_PATH)/include/server -I$(PGSQL_PATH)/include/internal

.PHONY: all run


all: .config_$(Build_Mode)_$(SGX_ARCH) $(Signed_Enclave_Lib)
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

run: all
ifneq ($(Build_Mode), HW_RELEASE)
	@$(CURDIR)/$(App_Name) 	
	@echo "RUN  =>  $(App_Name) [$(SGX_MODE)|$(SGX_ARCH), OK]"
endif

.config_$(Build_Mode)_$(SGX_ARCH):
	@rm -f .config_* $(App_Name) $(App_Name) $(Enclave_Lib) $(Signed_Enclave_Lib) $(App_Cpp_Objects) isv_app/isv_enclave_u.*  isv_enclave/isv_enclave_t.* libservice_provider.* $(ServiceProvider_Cpp_Objects)
	@touch .config_$(Build_Mode)_$(SGX_ARCH)


######## App Objects ########

isv_app/isv_enclave_u.c: $(SGX_EDGER8R) isv_enclave/isv_enclave.edl
	@cd isv_app && $(SGX_EDGER8R) --untrusted ../isv_enclave/isv_enclave.edl --search-path ../isv_enclave --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

isv_app/isv_enclave_u.o: isv_app/isv_enclave_u.c
	@$(CC) $(App_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"

isv_app/%.o: isv_app/%.cpp
	@$(CXX) $(App_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"

$(App_Name): isv_app/isv_enclave_u.o $(App_Cpp_Objects)
	@$(CXX) $^ -o $@ $(App_Link_Flags)
	@echo "LINK =>  $@"


######## Enclave Objects ########

enclave_t.c: src/backend/enclave/Enclave.edl
	$(SGX_EDGER8R) --trusted Enclave.edl --search-path src/backend/enclave --search-path $(SGX_SDK)/include
	mv Enclave_t.c src/backend/enclave 
	mv Enclave_t.h src/include/backend/enclave
	@echo "GEN  =>  $@"

#enclave_u.c: src/backend/enclave/Enclave.edl
#	$(SGX_EDGER8R) --untrusted Enclave.edl --search-path src/backend/enclave --search-path $(SGX_SDK)/include
#	mv Enclave_u.c src/backend/enclave 
#	mv Enclave_u.h src/include/backend/enclave
#	@echo "GEN  =>  $@"


enclave_t.o: enclave_t.c
	$(CC) $(Enclave_C_Flags) -c src/backend/enclave/$<  -o $@
	@echo "CC   <=  $<"

######## SOE Objects ##############

logger.o:  src/logger/logger.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_bufpage.o: src/backend/storage/page/soe_bufpage.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe_bufmgr.o: src/backend/storage/buffer/soe_bufmgr.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@ 

soe_qsort.o: src/backend/utils/soe_qsort.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

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

soe_ofile.o: src/backend/storage/buffer/soe_ofile.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@

soe.o: src/backend/soe.c
	$(CC) $(Enclave_C_Flags) $(Pgsql_C_Flags) -c $< -o $@


$(Enclave_Lib): enclave_t.o logger.o soe_ofile.o soe_hashsearch.o soe_hashutil.o soe_hashpage.o soe_hashovfl.o soe_hashinsert.o soe_hash.o soe_bufmgr.o soe_qsort.o soe_bufpage.o soe.o 
	$(CC) $(SGX_COMMON_CFLAGS)  $^ -o $@ -static $(SOE_LADD) -L/usr/local/lib/  $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

$(Signed_Enclave_Lib): $(Enclave_Lib)
	$(SGX_ENCLAVE_SIGNER) sign -key src/backend/enclave/private.pem -enclave $(Enclave_Lib) -out $@ -config $(Enclave_Config_File)
	@echo "SIGN =>  $@"

.PHONY: clean

clean:
	rm -f .config_*  $(Enclave_Lib) $(Signed_Enclave_Lib)
	rm -rf *.o
