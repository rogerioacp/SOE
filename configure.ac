
AC_INIT([SOE.], [0.0.1], [rogerio.pontes@pm.me])

AM_INIT_AUTOMAKE([-Wall -Werror foreign]) 
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile
    src/Makefile
    ])

AM_PROG_AR

LT_INIT
AC_CONFIG_MACRO_DIRS([m4])

# request environment variable
AC_ARG_VAR(SGX_SDK, SGX SDK folder path used to generate trusted and untrusted code)

#trigger the generation of trusted and untrusted enclave code
AC_ARG_ENABLE([enclave-generate],
              [AS_HELP_STRING([--enable-enclave-generate],
                              [Enable the generation of enclave files from Enclave.edl])
              ], [], [enable_enclave_generate=no])

# define enclave variables if enclave-generate is active
AS_IF([test "x${enable_enclave_generate}" = "xyes"],
      [AS_IF([test -x "${SGX_SDK}"],[enclave_generate=yes],
      	[AC_MSG_ERROR([dnl
Please define the enviornment variable SGX_SDK to generate enclave files.])
      	])],
      	[])


AM_CONDITIONAL([ENCLAVE_GEN],[test "$enclave_generate" = "yes"])
AS_IF([test "$enclave_generate" = "yes"],[AC_SUBST(SGX_EDGER8R, [${SGX_SDK}/bin/x64/sgx_edger8r])])
AS_IF([test "$enclave_generate" = "yes"],[AC_SUBST(SGX_SDK_INCLUDE, [${SGX_SDK}/include])])
AS_IF([test "$enclave_generate" = "yes"],[AC_SUBST(SGX_SDK_LIB, [${SGX_SDK}/lib64])])
AS_IF([test "$enclave_generate" = "yes"],[AC_SUBST(SGX_ENCLAVE_SIGNER, [${SGX_SDK}/bin/x64/sgx_sign])])


# used to find packages installed on the system external to the standard library
# generates specific variables that can be used to include in specific targets.
PKG_CHECK_MODULES(COLLECTC, collectionc >= 3.0)
#PKG_CHECK_MODULES(PQ, libpq >= 11.1)



AC_OUTPUT

