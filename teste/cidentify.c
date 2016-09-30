#include <stdlib.h>
#include <stdio.h>
#include "support.h"
#include "cthread.h"

int main(int argc, char **argv)
{
	char name[256];

	cidentify(name, 255);

	puts(name);

	return 0;
}
