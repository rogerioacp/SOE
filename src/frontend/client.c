
/**
 * Useful documentation and examples.
 * https://software.intel.com/en-us/ipp-crypto-reference-pseudorandom-number-generation-functions
 * SGX examples -> ipp_rsa_key.cpp
*/

#include <sgx_tcrypto.h>
#include "common/prng.h"

sgx_aes_gcm_128bit_key_t key = {
		0x72, 0x12, 0x8a, 0x7a, 0x17, 0x52, 0x6e, 0xbf,
        0x85, 0xd0, 0x3a, 0x62, 0x37, 0x30, 0xae, 0xad
    }

sgx_status_t token(const uint8_t *p_src, uint32_t src_len,  uint8_t *p_dst,    uint8_t aad){
    uint32_t iv;
    IppStatus prn_error_code;
    sgx_status_t gcm_error_code;
    sgx_aes_gcm_128bit_tag_t tag;

    //generates a prn with 96 bits, 12 bytes;
    prn_error_code = nextPRN(&iv, 96);

    if (error != ippStsNoErr)
        return error;

    gcm_error_code =  sgx_rijndael128GCM_encrypt(&key, p_dst, &iv, 96, &aad, sizeof(uint8_t), tag);

    return ;

}

sgx_status_t IToken(const uint8_t *p_src, uint32_t src_len,  uint8_t *p_dst){
    return token(p_src,src_len, p_dst, 0);
}


sgx_status_t SToken(const uint8_t *p_src, uint32_t src_len,  uint8_t *p_dst){
    return token(p_src,src_len, p_dst, 1);
}


