/*
 * crypt.h
 *
 */

#ifndef _crypt_h_
#define _crypt_h_

#include "config.h"

#define CRYPT

int	Null_Key();
int     Set_Key(char* key);
u_char* Encrypt(u_char* in, int* len);
u_char* Encrypt_Ctrl(u_char* in, int* len);
int     Decrypt(const u_char* in, u_char* out, int* len);
int     Decrypt_Ctrl(const u_char* in, u_char* out, int* len);

#endif /* _crypt_h_ */
