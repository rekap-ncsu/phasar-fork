#include <iostream>
using namespace std;

extern "C" void do_something(int x) {
	if(x < 10) {
		cout << x;
	} else {
		cout << "something bad happened";
	}
}
extern "C" int source() {
	string s;
	cin >> s;
	return stoi(s);
}
int main() {
	int x = source();
	do_something(x);
	return 0;
}