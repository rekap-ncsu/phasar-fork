## Goals 
- simple taint analysis with phasar-cli's pre build taint analysis capabilities

## The Example

We first want to work with simple types and simple interactions. For that imagine you have a system where your users can input some number, but if this number is to big something unwanted will happen. 
A simply example for this is given in the following code. Note that the `extern "C"` annotations are just to simplify the names of the compiled llvm function names. 

```c++
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
```

Looking at this code the variable x is set by user input and never controlled for how big the integer is. 
Given you have this code inside a `main.cpp` file we now compile it with `clang++ -emit-llvm -S -fno-discard-value-names main.cpp` this will produce a `main.ll` file containing the llvm intermediate representation (IR) that PhASAR is able to analyse. Look at the llvm code. You see a lot is generated, that has nothing to do with our code. It is still nesessery so the cout, cin and their streaming operators can function properly. You can imagine that in a real life application analysing such code by hand is quite chalanging. So we use PhASAR.
Running `phasar-cli -m main.ll -D ifds-taint` will invoke a taint analysis for our `main.ll` but if you do so nothing will be found because at the moment PhASAR dosn't know what to look for. We specify it with a config file in the json format. 
Create a file called `analysis-config.json`. Here we have to specify where untrusted data is coming from and where we don't want it to leak into. 
```json
{
	"name": "simple taint analysis",
	"version": 1,
	"functions": [
		{
			"name": "source",
			"ret": "source",
			"params": {}
		},
		{
			"name": "do_something",
			"params":
				"sink": [0]
			}
		}
	],
	"variables": []
}
```
Our Configuration is a json object with the fields `name` to specify our analysis name. Here you can choose something to your liking, `version` to specify a version number in case you will work on your analysis, `functions` is where we will specify what to look for in our example. 
We want to configure, that data coming from the source function is untrusted and shouldn't leak in the `do_something` function so we list both in the functions array and specify the return value of the `source` function as `source` and the 0th parameter of the `do_something` function as sink.
We now can run
`phasar-cli -m main.ll -D ifds-taint --analysis-config=analysis-config.json`
and get:
```
PhASAR v0323
A LLVM-based static analysis framework


----- Found the following leaks -----
At instruction
IR  : call void @do_something(i32 noundef %0), !psr.id !63 | ID: 59


Leak(s):
IR  : %x = alloca i32, align 4, !psr.id !58 | ID: 54
```
So PhASAR found our leak. 
