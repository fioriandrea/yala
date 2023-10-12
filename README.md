# yala

A statically typed, virtual machine based programming language developed for a university project.

# Books used

To create this project, I relied on insights and techniques from the book "Crafting Interpreters". This book influenced how I approached the code and structured the project. You can find more information in the book [here](https://craftinginterpreters.com/).

# Build

Once you have obtained the source code, you just need to enter the following command:
```make```
If there are any issues due to previous compilations, you can use the command:
```make cleanbuild```
This will produce an executable file named "yala."

# Command Line Options

The usage of the "yala" command follows the following syntax:
```
yala <mode> [options] input_file
```

A "mode" represents a subcommand, and each subcommand has its own set of options. 

To view the set of modes and options, run `yala help`.

# Code Overview

The code has been divided into several modules:

- `frontend`: This module contains all the code related to lexical analysis and syntactic analysis.

- `semantics`: This module takes the syntax tree produced by the frontend and performs semantic analysis and code generation. Utility functions, such as printing various value types in the language, have been defined in this module in the file `value.c`.

- `vm`: This module takes the bytecode generated by semantics and executes it using a virtual machine.

- `serialization`: This module handles the serialization and deserialization of the bytecode.

# Example Programs

## Factorial

```
program main

function fact(n: integer): integer
begin fact
    if n <= 0 then 1 else n * fact(n - 1) end
end fact;

begin main

writeln(fact(10));

end main.
```

## Vectorized Fibonacci

```
program main

procedure fibonacci(out fibo: vector [10] of integer)
begin fibonacci

fibo[0] = 1;
fibo[1] = 1;

for i = 2 to 9 do
    fibo[i] = fibo[i - 1] + fibo[i - 2];
end;

end fibonacci;

begin main

fibo: vector [10] of integer;
fibonacci(fibo);
writeln(fibo);

end main.
```

## Mutually Recursive Functions

```
program main

function is_even(n: integer): boolean
begin is_even
        if n == 0 then
                true
        else
                is_odd(n - 1)
        end
end is_even;

function is_odd(n: integer): boolean
begin is_odd
        if n == 0 then
                false
        else
                is_even(n - 1)
        end
end is_odd;

begin main

writeln(is_even(20));
writeln(is_even(11));
writeln(is_odd(20));
writeln(is_odd(11));

end main.
```

## Print Function Bytecode

```
program main

function zero(): integer
begin zero
    0
end zero;

begin main
writeln(zero);
end main.
```