/*-------------------------------------------------------------------------
 *
 * soe.h
 *	  The public API for the secure operator evaluator (SOE). This API defines 
 *    the functionalities available to securely process requests. It can be
 *    implemented in different ways to support specific use-case. The first  
 *    use case to use this API is is the secure database indexes in postgres.
 *
 *     COPYRIGHT.
 *
 * src/include/soe.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef SOE_H
#define SOE_H


typedef struct State State;
typedef struct InitParams InitParams;
typedef struct Token Token;

enum UpdateOp {
	encode = 0,
	decode = 1,
	insert = 2
};



//Initialize the SOE state with input parameters
State Init(InitParams params);

/*
* Verification of the consistency of two input tokens t1 and t2 given a SOE 
* state. Returns 1 if the two tokens are consistent and 0 otherwise.
*/
int Check(Token t1, Token t2, State st);

/*
* Updates the input token t according to the update action and the SOE state.
*/
Token Update(Token t, enum UpdateOp update, State st);

/*
* Generates a Union token of an input list of tokens and a SOE state.
*/
Token Union(Token* list, State st);

/*
* Choses an offset of the input list token.
*/

int Next(Token* list, Token t, State st);

/*
* Splits the input list of token in multiples lists and outputs the results as 
* a 2D array.
*/
Token** Split(Token* list, State st);


#endif