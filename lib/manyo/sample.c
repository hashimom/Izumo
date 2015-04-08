/*
 * sample.c
 *
 *  Created on: 2015/04/01
 *      Author: hashimom
 */
#include <stdio.h>
#include "manyo.h"

#define SERVER	"/tmp/.iroha_unix/IROHA"
#define USERNAME	"SAMPLEUSER"

int main()
{
	int id = -1;

	id = mny_open(SERVER, 0, USERNAME);
	if (id >= 0) {
		printf("id= %d\n", id);
		sleep(1);
		mny_close(id);
	}
	return(0);
}
