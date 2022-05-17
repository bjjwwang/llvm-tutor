//=============================================================================
// FILE:
//      input_for_hello.c
//
// DESCRIPTION:
//      Sample input file for HelloWorld and InjectFuncCall
//
// License: MIT
//=============================================================================
int foo(int a) {
  int i = 0;
  for (i = 0; i < 3; ++i) {
    a = a+1;
  }
  return a * 2;
}

int bar(int a, int b) {
  while (a < 5) {
    a = a+1;
  }
  return (a + foo(b) * 2);
}

int fez(int a, int b, int c) {
  int i = 0, j = 0;
  for (i = 0; i < 3; ++i) {
    for (j = 0; j < 2; ++j)
      a = a+1;
  }
  return (a + bar(a, b) * 2 + c * 3);
}

int main(int argc, char *argv[]) {
  int a = 123;
  int ret = 0;

  ret += foo(a);
  ret += bar(a, ret);
  ret += fez(a, ret, 123);

  return ret;
}
