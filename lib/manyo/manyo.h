/*
 * manyo.h
 *
 *  Created on: 2015/04/01
 *      Author: hashimom
 */
#ifndef LIB_MANYO_H_
#define LIB_MANYO_H_

int mny_open(char* pass, int portno, char* user);
int mny_convert(int id, char *yomi, char *conv);
int mny_close(int id);


#endif /* LIB_MANYO_H_ */
