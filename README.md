# SOE

Secure Operator Evaluator (SOE) contains the managment logic of a postgres table and index. Most of this code is follows the same logic of POSTGRES 11 backend but it's adapated to be compiled and used inside an Intel SGX Enclave. As the postgres database code is quite complex and is not compatible with Intel SGX, only the essential parts of postgres have been copied to generate a small Enclave compatible binary.


## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Contributing](#contributing)

<a name="Feature"></a>

## Features

This Library provides a High-level API to insert and search encrypted data on databases tables and indexes. The libraries follows postgres's code and has a table and an index access manager, a buffer manager and implemantions of different table files. 


The real access patterns of the database are hidden with a new heap/btree buffer manager. These managers are a middleware between the database server logic and the ORAM Library that generates a random sequence of access and writes the database file contents obliviously. The ORAM library leverages has a low-level API to handle file writes/reads which delegates the logic to the
the real postgres Relation and Buffer API to write to the database files. When the SOE is executed inside the enclave, this low-level API are in fact OCALLs to the database backend server.


<a name="Instalation"></a>
## Installation

### Prerequisites

The library has been sucessfully tested and installed on Linux (Ubuntu) and Mac OS. The library uses a Makefile to build the library and follows the the same pattern as the INTEL SGX Makefiles sample code. The generated library is operating system dependent. The library has the following dependencies:

* [Collections-C](https://github.com/srdja/Collections-C)
* [ORAM](https://github.com/rogerioacp/oram)
* [openssl](https://github.com/openssl/openssl)
* [Intel SGX](https://github.com/intel/linux-sgx)

### Installing

To install the library please download a stable release and ensure the dependencies are installled.
The makefile has the following options to confugre the make process:

- SGX_MODE:
    - HW - Builds binary in hardware mode to run in an Enclave
    - SIM -SIM mode builds binary to run as simulation. 
- SGX_DEBUG (0,1): Compile binary for debug.
- UNSAFE (0,1) Compiles binary to be executed outside of an enclave. Neither simulation nor Hardware mode.
- CPAGES (0,1): Set pages to be encrypted.
- DUMMYS (0,1): Dummy requests to hide volume leakage.
- SINGLE_ORAM (0,1): Simulates the execution of the baseline benchmark in a
  single ORAM. Makes the size of the number of blocks of the tree and table
  ORAMs the same.
- ORAM_LIB:
    - FORESTORAM - Compile binary with Forest ORAM lib. 
    - PATHORAM - Compile binary with Path ORAM lib.
    - TFORESTORAM - Compile binary with Forest ORAM and Token PMAP lib.
    - TPATHORAM - Compile binary with Path ORAM and Token PMAP lib.
- SMALL_BKCAP (0,1): If defines sets the number of of blocks per Path ORAM node (Z) to 1. The default is 4 blocks per node (Z=4).
- STASH_COUNT: Logs the number of elements in a stash on a ORAM construction.
- PRF: Generates the tokens for a cascade construction with a PRF (HMAC-SHA256
  OpenSSL).

To compile PathORAM for production, use the following flags:

> make SGX_MODE=HW SGX_DEBUG=0 UNSAFE=0 CPAGES=1 DUMMYS=1 SINGLE_ORAM=1
> SMALL_BKCAP=0 STASH_COUNT=0 ORAM_LIB=PATHORAM

To Compile ForestORAM for production, use the following flags:

> make SGX_MODE=SIM SGX_DEBUG=0 UNSAFE=1 CPAGES=0 DUMMYS=1 SINGLE_ORAM=0
> SMALL_BKCAP=0 STASH_COUNT=0 ORAM_LIB=FOREST_ORAM

To compile for development and debug use the following flags:

> make SGX_MODE=SIM SGX_DEBUG=1 UNSAFE=1 CPAGES=0 DUMMYS=0 SINGLE_ORAM=0 SMALL_BKCAP=1 STASH_COUNT=1 ORAM_LIB=(FORESTORAM or PATHORAM)

To install run the following command:

> make install

If the binary was compiled with the UNSAFE flag, the same flag must also be passed to the install tager as follows:

> make install UNSAFE=1

<a name="contributing"></a>
## Contributing

1. File an issue to notify the maintainers about what you're working on.
2. Fork the repo, develop and test your code changes, add docs.
3. Make sure that your commit messages clearly describe the changes.
4. Send a pull request.

### File an Issue

Use the issue tracker to start the discussion. It is possible that someone
else is already working on your idea, your approach is not quite right, or that
the functionality exists already. The ticket you file in the issue tracker will
be used to hash that all out.

### Fork the Repository

Be sure to add the relevant tests before making the pull request. Comment the code as necessary to make the implementation accessible and add the original paper describing the algorithm implementation.


### Make the Pull Request

Once you have made all your changes, tests, and updated the documentation,
make a pull request to move everything back into the main branch of the
`repository`. Be sure to reference the original issue in the pull request.
Expect some back-and-forth with regards to style and compliance of these
rules.


