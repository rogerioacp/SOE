/*-------------------------------------------------------------------------
 *
 * soe_prf.h
 *	 
 *	 Interface of the pseudo random function
 *
 *
 * Copyright (c) 2018-2019, HASLab
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef SOE_PE_H
#define SOE_PE_H


void        prf(unsigned int level, unsigned int offset, unsigned int counter, unsigned char *token);

unsigned int   getRandomInt();
#endif          /*SOE_PE_H*/
