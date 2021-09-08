# yala

A statically typed, virtual machine based programming language developed for a university project.

## Project Structure

The code has been divided into the following modules:

* *frontend* contains the lexer and the parser

* *semantics* takes as an input the syntax tree produced by *frontend*, carries out semantic analysis and produces bytecode for the virtual machine

* *vm* takes bytecode as an input and executes it

* *serailization* serializes bytecode

## Grammar

```
program -> program id var-decl-list module-decl-list stat-body .
var-decl-list -> var-decl var-decl-list | ''
var-decl -> id-list : type ;
id-list -> id , id-list | id
type -> integer | string | boolean | vector [ intconst ] of type
const -> intconst | strconst | boolconst | [ const-list ]
const-list -> const , const-list | const
module-decl-list -> module-decl module-decl-list | ''
module-decl -> procedure-decl | function-decl
procedure-decl -> procedure id ( opt-formal-list ) var-decl-list module-decl-list stat-body ;
function-decl -> function id ( opt-formal-list ) : type module-decl-list expr-body ;
opt-formal-list -> formal-list | ''
formal-list -> formal-decl , formal-list | formal-decl
formal-decl -> mode id : type
mode -> '' | out | inout
stat-body -> begin id stat-list end id
expr-body -> begin id expr end id
stat-list -> stat ; stat-list | stat ;
stat -> assign-stat | if-stat | while-stat | repeat-stat | for-stat | input-stat | output-stat | module-call | break | exit
assign-stat -> lhs = expr
lhs -> id | indexing
indexing -> lhs [ expr ]
if-stat -> if expr then stat-list opt-elsif-stat-list opt-else-stat end
opt-elsif-stat-list -> elsif expr then stat-list opt-elsif-stat-list | ''
opt-else-stat -> else stat-list | ''
while-stat -> while expr do stat-list end
repeat-stat -> repeat stat-list until expr
for-stat -> for id = expr to expr do stat-list end
input-stat -> read ( lhs-list )
lhs-list -> lhs , lhs-list | lhs
write-stat -> write-op ( expr-list )
write-op -> write | writeln
expr-list -> expr , expr-list | expr
expr -> expr bool-op bool-term | bool-term
bool-op -> and | or
bool-term -> comp-term comp-op comp-term | comp-term
comp-op -> == | != | > | >= | < | <=
comp-term -> comp-term add-op term | term
add-op -> + | -
term -> term mul-op factor | factor
mul-op -> * | /
factor -> unary-op factor | ( expr ) | lhs | const | cond-expr | module-call
unary-op -> - | not
cond-expr -> if expr then expr opt-elsif-expr-list else expr end
opt-elsif-expr-list -> elsif expr then expr opt-elsif-expr-list | ''
module-call -> id ( opt-expr-list )
opt-expr-list -> expr-list | ''
```

## Types

Yala has the following built-in types:

* `boolean`: `true` or `false`

* `integer`: an integer number

* `string`: a sequence of characters delimited by `'` or `"`

* `vector`: array with static dimensions. Vectors can also be multidimensional

## Arithmetic Operators

Yala has the following operators:

### Logical

* or (e.g. `true or false`)
* and (e.g. `true and false`)
* not (e.g. `!true`)
* conditional expression (e.g. `if true then 1 elsif false then 2 else 3 end`)

### Arithmetic

* addition (e.g. `10 + 20`)
* subtraction (e.g. `10 - 20`)
* multiplication (e.g. `10 * 3`)
* division (e.g. `10 / 3`)

### Other

* indexing (e.g. `matrix[10][20]`)

## Sample Programs

### Recursion

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

### Mutual recursion

```
program main

function is_even(n: integer): boolean
begin is_even
        if n == 0 then true else is_odd(n - 1) end
end is_even;

function is_odd(n: integer): boolean
begin is_odd
        if n == 0 then false else is_even(n - 1) end
end is_odd;

begin main

writeln(is_even(20));
writeln(is_even(11));
writeln(is_odd(20));
writeln(is_odd(11));

end main.
```

### FizzBuzz

```
program main

n: integer;

function reminder(num, divisor: integer): integer
begin reminder
        num - divisor * (num / divisor)
end reminder;

begin main

n = 100;

for i = 1 to n do
        if reminder(i, 15) == 0 then
                writeln("FizzBuzz");
        elsif reminder(i, 3) == 0 then
                writeln("Fizz");
        elsif reminder(i, 5) == 0 then
                writeln("Buzz");
        else
                writeln(i);
        end;
end;

end main.
```

### Printing a function

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

### Vectors

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
