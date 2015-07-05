Bang! Copyright (c) 2015 David M. Placek (MIT License)

# The Bang! Programming Language

Bang! is a simple functional language that provides first class functions, lexical scope, recursion, objects, and coroutines with a minimalist approach and lightweight syntax.

Bang! was developed with the goal of offering language constructs with the most abstractive power and minimum complexity.  A stack-based program model and postfix syntax are employed for syntactic simplicity.  Recursion with tail call optimization is provided for iteration.  "Bang! for the buck" is the idea here.  Keep the language small and the feel light, but start with a solid foundation.

Programs can be as simple as:

      1 1 +
    > 2

Or they can be as complicated as you wish to make them, with all the abstractive power of functional programming.

# Status

Bang! is barely a month old and many features are incomplete and specifications are tentative.  The current implementation is naive but usable.  Performance is reasonable, but not quite at par with more mature languages like Lua/Ruby/Python.  There are probably issues that make it unsuitable for use in long-lived production environments.  But it does demonstrate the concepts and runs short-lived scripts quite ably.

# Introduction

## Functional Foundation

The central construct in Bang! is the function. First class functions are arguably the most malleable single language facility, so Bang! attempts to keep this concept simple and close to the surface.

All functions have access to a working stack.  Literals (numbers, strings, booleans, or functions) are pushed to the stack when encountered in the program. When functions are applied they pop their arguments off the working stack and can push results back onto the stack.  Values from the stack can be bound to symbolic names either as a function parameter or local variable.

A function to square a number is written like this:

     fun number = number number *

A function written this way is known as a function literal.  It is pushed onto the working stack like any other literal.  Functions are invoked, or "applied" using the '!' operator.  The '!' operator, expecting to find a function value at top of the stack, pops off the top value and applies it.  If the function accepts a parameter, the next value from the stack is popped off and bound to the symbolic name given to the parameter- in this example, the parameter is called 'number'.  Here the function body pushes the value bound to 'number' to the stack twice and invokes the multiply operator.  Multiply operates as you might expect, popping two values off the stack, multiplying, and pushing the result back to the stack.

So a Bang! program simply consists of pushing values onto the program stack then applying those values to functions.  This resembles the common "reverse polish notation" style calculators.

       3.14
       60
       *
    > 188.4

At the end of a program Bang! simply dumps the contents remaining on the stack and you have your program output.

       3
       fun number = number number *;
       !
    > 9

Semicolons can be used to terminate a function body and braces will close multiple levels of open function scopes.  Many languages distinguish between function parameters and local variables, but in Bang! these concepts are essentially the same.  "Variables" are simply symbolic names for values popped off the stack, whether as a function parameter or whether bound at any other time within a function body.  Bound variables are available throughout the remainder of the function body and bindings declared within the function are dropped with the close of the function body.

Bang! provides the "as" keyword to immediately bind the value at the top of the stack to a symbolic name.

    3.14159 as Pi
    ...

Function values can be bound to symbolic names similarly, allowing named functions.

      3.14 as PI
      fun radius = radius radius * PI *; as CalculateArea
      25.0 CalculateArea!
    > 1962.5    

When the program's end of file is reached, any open function bodies are closed.

Functions can be declared with zero to any number of arguments.  Arguments are popped off the stack in the reverse order of declaration in the argument list so that arguments are declared in the same order they would be pushed to the stack before calling the function.   Arguments could simply be bound to local variables using the 'as' keyword at the top of the function body - there is no distinction between an value bound with 'as' or with a function argument; the syntax parser merely translates either form to the same operation internally.  These two functions are equivalent:

    fun x y z = { ... }
    fun = { as z as y as x ... }

Programmers are accustomed to the name of a function coming up front rather than being bound at the end like we might do using "as".  Bang! provides a "def" keyword which simply swaps the name binding syntax around so that the function name can be placed up front.  The tywo following "plus" definitions are equivalent:

      fun x y = { 
          x y +
      } as plus
      
      def :plus x y = { 
         x y +
      }

which also allows the function to reference itself recursively.

      def :loopForever nthTime = { 
         nthTime 'This is iteration number %s\n' print!
         nthTime 1 + loopForever!
      }

## Conditionals

The '?' operator provides a mechanism to conditionally branch.  When a '?' is encountered, the stack top will be popped and if the value is true, the statements following the ? will be executed.  A ':' following the '?' introduces an 'else' clause which will be executed instead if the boolean test value is false.  If or else clauses terminate under the same termination conditions as function blocks, either at the first ; delimiter, closing brance, or termination of input.

Here is a function that will either add 1 or subtract 1 from the provided value:

      def :up-or-down goUp = {
         goUp ? 1 + : 1 -
      }
    
       100 true up-or-down!
     > 101 
    
       100 false up-or-down!
     > 99 

## Iteration

In the functional world, iteration is usually introduced as recursion.  The "def" keyword introduced earlier gives a function a way to identify itself within the function body, allowing for recursion.  The conditional operator, ?, can be used to specify the termination condition for recursion.  Also, because Bang! provieds first class functions we can write higher order functions to implement iterating new constructs in bang itself without the need for additional syntax in the core language.

Here is a higher order function that does something N times:

    def :times = { swap! as do-something
        def! :innerLoop remain = {
          do-something!
          remain 1 > ? remain 1 - innerLoop!
        }
    }
    
Which can be used like this:

       def :say-hello "Hello World!" print!;
    
       say-hello 20 times!

When the "times" function is called, it invokes the supplied "do something" function, then calls itself again with the number of remaining iterations decremented by 1.  It repeats this as long as the number of times remaining is greater than 1.

Bang includes libraries with some standard iteration functions like "times" and "range" to iterate a number of times or over a range of values.  Much of the time that iteration would be used in an imperative language a higher order function such as map or fold ought to be preferred as more idiomatic.  And where neither that nor something like "times" or "range" fits well you can always buckle down and write a recursive function.

In many imperative languages you have to worry about recursive functions overflowing the stack.  Bang! employs tail call optimization to prevent stack frames from accumulating when recursion is employed as long as there is no work that remains to be done after the recursive invocation.

## Higher Order Functions

Higher order functions are functions that do something with a function.  Typically these are used to transform, accumulate, or filter a list or other such fun things.  "map" traditionally applies a functional operator to an entire list of values, applying the function each value value in the list and returning a list of transformed values.

I'm hestitant to add a firm list or array structure to Bang! because I think a lot of that can be done on the working stack.  To help with recursion over the stack, the '#' operator is provided which returns the current length of the stack.  With this operator, "map" can be implemented like this:

    def :map xform = {
      def :innermap = {
        # 0 > 
        ? as val innermap! val xform!
      }
      innermap!
    }

Each value on the stack is transformed by the provided function.  The 'innermap' function is called recursively as long as values remain on the stack.

Given this map function, we can write a "filter" function that leaves only those values on the stack which pass a certain predicate test:

    def :filter predicate = {
        fun v = { v predicate! ? v }
        map!
    }

To help with the use of the working stack, a couple other niceties are provided.  "nth" pushes (duplicates) the nth value on the stack.  "save-stack" clears the stack contents, saving them into a function that will push them back onto the stack when applied.

Additionally, parentheses may be used to set bounding markers on the size of the stack so that e.g, we can run these higher order functions on a certain point in the working stack and not operate on unrelated values which might be present deeper in the stack.

Here is a short implementation of quicksort using higher order functions:

    def :quicksort = {
      # 2 / nth!  as pivotValue -- use the value midway on the stack as a pivot value
      save-stack! as theStack   -- save/duplicate the stack, because we'll need to filter it several times
     
       theStack! fun = pivotValue <; filter! quicksort # 1 >? 
      (theStack! fun = pivotValue =; filter!)
      (theStack! fun = pivotValue >; filter! quicksort # 1 >?)
    }

When quicksort is invoked, it looks at the size of the stack, calculates the midpoint index and stores the midpoint value for use as a 'pivot'.  The full stack contents are then saved off in a closure, as we'll need to operate on this list of values several times.

First, the stack contents less then the pivot value are pushed, recursively invoking quicksort to insure these values are fully sorted.  Then, the stack contents which match the pivot are pushed; finally, the stack contents greater than the pivot are pushed, recursively invoking quicksort on those stack items.  For the last two operations, parentheses are used to prevent the operations from being applied to values pushed in the previous step.

And that's, for an example, quicksort in six easy lines of Bang!.

## Records and Objects

So far we've introduced first class lexically scoped functions, conditionals, iteration, and higher order functions; but what about objects?  Does Bang! support object-oriented programming? 

It is possible to build a lightweight object/record system using just lexically scoped functions. It may not be everything the average Java programmer wants out of an object system, but with the goal of keeping Bang! simple, it's a fair starting point.

The typical way to do this is to write a "create-Thing" constructor method that binds the objects properties and returns an inner message handler function.  Messages can then be sent to the object by calling the message handler returned by the constructor.  How that message handler works is entirely up to the implementer of the object system, but generally goes something like this:

      def :create-Employee = { as age as name
          fun message = {
              message 'age'  = ? age
            : message 'name' = ? name
          }
      }

So the user calls create-Employee, gets the message handler, and then can interrogate the objects properties or perform whatever method by invoking the handler with a method name to call.

       'Fredrick Wilkens' 36 create-Employee! as Fred
    
       'age' Fred!
       'name' Fred!

This is passable but a little syntax help can give us a more objecty feel. Bang! provides a "message" operator, '.' , and a special keyword "lookup" to help with this.

The "lookup" keyword accepts a string and then _dynamically_ looks for an available upvalue bound to a symbol name for the string.  So we can replace the message handler with much shorter code:

      def :create-Employee = { as age as name
          fun = lookup
      }

because lookup is a keyword (not a function) it doesn't need the "!" operator.  I'm not sure whether I like that, but I'm withholding judgment.  Remember, I have no idea what I'm doing here.

Anyway, when the client invokes

    'age' Fred!

The message function invokes the "lookup" operation, which pulls the message ("age") off the stack and looks for an upvalue bound to the name "age".  Lo and behold, "age" is the name of a parameter bound when create-Employee was called, so lookup pushes the bound value onto the stack, which is left as a return to the caller.

The '.' operator gives object method invocation a more traditional syntax.  Essentially, the '.' operator is a syntactic tweak that first pushes the method name as a string, and then pushes the object and '!'s the object.

So now we can access the object using traditional dotted record syntax:

      Fred.age as FredsAge
      Fred.name as FredsName

So far this is really more of an immutable record than a object, as it just returrns the fields passed to the constructor.  But we can make this more of a traditional object by invoking the looked-up field, and binding the message names to functions rather than values:

      def :create-Employee = { as age_ as name_
    
          def :age = age_ 3 -; -- everybody lies about their age!
          def :name = name_;
          def :say-hello-to otherPerson = {
            "Hello %s, my name is %s and I'm %d years old" otherPerson.name! name! age! print!
          }
       
          fun = lookup
      }
    
So there it is. Once you've got FCLS functions you're just a . operator and a 'lookup' keyword away from building something that looks a lot like a traditional class.

Now, this is probably not blazingly fast and it's not going to scale beyond some size, sure; but bang-for-the-buck wise, when you're spending the buck to write the language yourself? Pretty decent.  The same mechanism will be used to build nested namespaces for a library / module system.


# Bang! Primitives

The primitive operations Bang! will provide are still tentative.  Generally these can be implemented in Bang! code, e.g., as libraries, but some will probably be included as standard / global primitives for efficiency and uniformity

## Number Operations

    + - * / < > =

These should not be surprising

## Boolean Operations

    not! and! or! =

These should also not be surprising

## Stack Operations

      drop! swap! dup! nth!
      save-stack!
      stack-to-array!

'nth' duplicates the nth value on the stack.

'save-stack' and 'stack-to-array!' clear out the stack, saving the contents to a single function or object.  'save-stack' creates a function which pushes all stack elements back out when invoked.

'stack-to-array' is a sort of array object that provides methods to return the nth element of the array, the length of the array, or to push the array contents onto the working stack.


# Libraries

Library files or modules can be included with the "require" keyword.  Require loads a file as a closure which must be applied.  A typical pattern is to have the library file return a message handler style function which can be used with the object syntax to access methods.  E.g., the core higher-order-functions are included in a file called [lib/hof.bang], which looks like this:


    def :map xform = {
      def :innermap = {
        # 0 > 
        ? as val innermap! val xform!
      }
      innermap!
    }
    
    def :filter predicate = {
        fun v = { v predicate! ? v }
        map!
    }
    
    fun = lookup
    
Users may access these functions by requiring [hof.bang]

    'lib/hof.bang' require! as hof

which saves the returned function as "hof".  Functions can then be accessed as such

    1 2 3 4 fun=100 +; hof.map!

Or rebound to local upvalues as

    hof.map as map

# Roadmap

* Improve parse / runtime error reporting
* Libraries
** String library (borrow from Lua?)
** Import module namespace into working AST/environment e.g for REPL
** SRFI implementations
* user defined operators and consistency with primitive operators ala scala?
* Working indentation in bang-mode.el
* Eval and programatic code generation
* Fix integration with Boehm GC to improve performance?

# Summary of Language Elements

* _Function/Binding_: fun, fun!, as, def
* _Function Application_: ! ?
* _Function Definition Delimiters_: ; { }
* _Literals_: _Booleans_=true/false _Strings_='xyz' "xyz" Numbers=3.14159 42
* _Object Syntax_: . lookup
* _Stack Bounding Delimiters_: ( )
* _(tentative)_ primitive functions:
** # + - * / < > = not! or! drop! swap! dup! nth! save-stack! stack-to-array!

# License

Released under MIT license, see [doc/license.txt]

