#include "pancake.hpp"
#include "../search/main.hpp"
#include <cstdio>

int main(int argc, const char *argv[]) {
	Pancake d(stdin);
	search<Pancake>(d, argc, argv);
	return 0;
}
