#include <iostream>
#include <string>
#include <fstream>
using namespace std;

int main() {
	ifstream in;
	in.open("test.txt");
	string res;
	int a;
	char ch;
	while (in)
	{
		in.get(ch);
		printf("1 %c \n", &ch);
	}
	cin >> ch;
	return 0;
}