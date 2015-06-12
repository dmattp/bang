Bang! Copyright (c) 2015 David M. Placek (MIT License)

# The Bang! Programming Language

Bang! is a small experimental functional language developed with the goal of offering language constructs with the most abstractive power and minimum implementation complexity.

Bang! offers first class functions with lexical scope in a simple language with a minimalist syntax.  Bang! employs a postfix notation to avoid the need for syntax complexities often encountered in infix and prefix languages, while offering a few niceties to give the postfix environment a conventional feel.

"Bang! for the buck" was the idea here.  Keep the language small and the feel light, but start with a solid foundation.

Programs can be as simple as:

      1 1 +
    > 2

Or they can be as complicated as you wish to make them, with all the abstractive power of functional programming.

# Status

Bang! is less than a month old and many features are incomplete and specifications are tentative.  The current implementation is naive but usable.  Very little optimization and endurance testing has been applied, and there are probably issues that make it unsuitable for production environments.  But it does demonstrate the concepts and runs small scripts with bearable performance.

# Let's See it Already


This section will give an overview of the language starting with the foundations and some explanation of the reasoning behind the language design.

## Functions - the only thing that matters

So what does Bang! code look like? The central construct in Bang! is the function.

First class functions are arguably the most malleable single language facility.  Generations of functional languages such as lisp have shown that every computing concept can be implemented using just functions.

Bang!'s philosophy is to keep this central construct simple and close to the surface. All Bang! functions take at most a single parameter which keeps the implementation simple, but lexical scoping allows creation of functions with any number of parameters as we will see.

All functions have access to a working stack.  Parameters are pushed onto the stack when calling a function, and the called function pops values off the stack as necessary.

A function to square a number is written like this:

     fun number = number number *

When this function is called it first pops the top value off the stack, binding it to the symbolic name "number".  Then the function pushes the received value twice and multiplies.  The multiplication operator pops the first two values off the stack, multiplies, and pushes the result.

Literals - numbers, strings, and booleans; are simply written as needed.  When the interpreter encounters a literal it pushes the literal value immediately onto the program's working stack.

A function written like the one above is a _function literal_.  When the interpreter encounters a function literal, it pushes it onto the stack just like any other literal.

So a Bang! program simply consists of pushing values onto the program stack then applying those values to a function. E.g.,

       3.14
       60
       *
    > 188.4

At the end of a program Bang! simply dumps the contents remaining on the stack and you have your program output.  So, if we want to square a number, we could use the square function above:

       3
       fun number = number number *
    > 9

This is the general idea.  The program above is not exactly Bang! syntax though, because theres a trick to using _function literals_.  The bang interpreter, encountering the above program simply pushes the _function literal_ onto the stack. It isn't applied to the argument and the result of the program is a stack with two values (a number and a function literal) still sitting on it, dumb and happy.

       3
       fun number = number number *
    > 3
    > (function)

To apply a function you use the apply operator, "!". Now there's a little trick to distinguish where the function body ends and if necessary we use a ';' to terminate a function body.

So proper Bang! syntax to calculate "3 squared" would be

       3
       fun number = number number *;
       !
    > 9  

I use a "tower" style here but it's not strictly necessary as whitespace is not important.  The above program could also be written as:

       3 fun number = number number *;!
    > 9  
    
Straightforward enough, though it looks a bit awkward with the line noise style "*;!" swearing at the end.

To make things easier, there is a shorthand syntax using the "fun!" keyword which tells the parser to automatically add an "apply" operation at the end of the function body.

So the program becomes:

       3 fun! number = number number *
    > 9  
    
In this case, our program source ends after the function body so there is no longer any need for the semicolon, either.  Looks a bit nicer.

There is another keyword syntax Bang! provides in place of the "fun!" statement - "as". When you apply the function, the parameter gets popped of the stack and "bound" to the function's (single) parameter.  So you say in effect, "use 3 'as' the variable called number" in the following function body.  With "as" you can drop the "=" sign too, so the program looks like this:

       3 as number
       number number *
    > 9

Now keep in mind that all these programs are _exactly the same_ internally.  We've just changed to keywords that provide slightly different syntax to make the code look nicer.

The "as" keyword syntax works quite naturally for defining variables.  For example, let's say we want to find the area of a circle:

       3.14 as Pi
       12   as radius
       radius radius * Pi *
    > 452.16

If we want to name a function, we can do that with "as" also:

       fun n = n n *; as squared
       3.14 as Pi
       12 as radius
       radius squared! Pi *
    > 452.16

Values bound to symbolic names are pushed to the stack the same way as literals; the runtime simply looks up the value bound to the symbolic name and pushes it onto the stack when it encounters a free identifier.

Now what if we need a function more than a single parameter? Here's where the inherent power of first class functions and lexical closures show their stuff.  Without defining any additional language syntax, we can actually already construct functions that take as many variables as we like.

A function to add two values could be written like this:

    fun x =
       fun y = x y +; !
 
Think about what happens when we place two arguments on the stack and apply the function.

      3  4
      fun x =
         fun y =
            x y +; !
      !

First we push 3 and 4 on the working stack.  Then we encounter the first function which takes the argument x. When this function is applied, it pops 4 off of the stack and binds it to variable "x".  Then, it evaluates the function body.  The function body applies the function which accepts a parameter "y".  When this function is applied, it pops the remaining "3" off the stack and binds it to parameter "y".  Then it pushes "x" and "y" onto the stack and adds them.

So we have in effect defined a function that takes two arguments, even though our language only gave us a function that takes one!  Standard fare if you're familiar with functional languages, but still a neat trick.

If we want to name the function "plus", we can bind it using "as":

          fun x =
             fun y = x y +; ! ;
          as plus
    
and apply it:
    
      3 4 plus!

Granted, this may not be "pretty" compared to languages which give you syntax like

      function plus( x, y )
        x + y
      end
      plus(3,4)

and figuring out where you need to put the semicolons and exclamations can be daunting. But we're not too far off from more traditional syntax, and we've still got our small bag of syntax tricks to throw at it.

First, we can get move the inner '!' up to the front to clean up some of the backend noise:

      fun x =
         fun! y = x y +; ;
      as plus

Better, but we can still end up with a list of semicolons that you have to count out.  Why is that?  If you've ever worked with lisp you're familiar with the ")))))))" problem that lisp haters harp on. Although the postfix notation avoids some of the explicit "expression bounding" for operations, a similar problem arises here because we need to bound the extent of function definitions. Semicolons end up stacking up as you close out however many "fun" levels are active.

To help with this Bang! provides a "curly brace" syntax which closes out any binding levels created after the open brace.  Where lisp might leave 12 stacked parentheses, in Bang! you can close out a whole group of nested fundef's with a single brace.  Unlike many traditional languages the braces only need to be used sparingly since it's just a programmer aid and not a syntax requirement.

So now we've got:

      fun x = {
         fun! y = x y +
      } as plus

That's starting to look a bit better.  One more syntax trick and we're almost there.

Programmers are accustomed to the name of a function coming up front rather than being bound at the end like we do with "as".  Bang! provides a "def" keyword which simply swaps the name binding syntax around so that the function name can be placed up front.  The two following "plus" definitions are equivalent:

      fun x = {
          fun! y = x y +
      } as plus
      
      def :plus x = {
         fun! y = x y +
      }

The ":" distinguishes the identifier following the colon as the function's bound name (as opposed to being a parameter to the function).  So the "def" keyword provides a more conventional "function name up front" syntax.

Note that the four keywords - fun, fun!, as, and def- are all just _syntactic variants_ on the same underlying construct - a first class function that takes a single parameter.

I'm sympathetic to moving the second function's parameter up to the first line so that all function arguments go on the first line, like so:

      def :plus x = { fun! y=
         x y +
      }

Or if you prefer, we can bind the variables with the "as" keyword:

       def :plus = { as x as y
         x y +
       }
      
       3 4 plus!
    > 7

Now that is pretty close to a "traditional" function syntax, and yet we haven't created a large set of syntax rules that make for complicated parsers and which can lead to annoying inconsistency. We've defined only four keywords: "fun", "fun!", "as", and "def"- all of which are just minor syntactic variants on the core function construct; and the key apply operator "!" which runs behind the scenes as we bind values to function closures, but generally remains hidden except where we want to call attention to the fact that we're applying a function.

I think it's a nice result when contrasted with traditional infix languages which tend to build in a lot of arbitrary syntactic conventions, e.g., re-using parentheses for function definitions as well as forcing order of evaluation, commas to separate arguments, etc. - and prefix languages like lisp that require mind-numbing explicitness in parenthesizing the bounds of expression groups.

And because postfix doesn't ever require the use of parens to specify the order of evaluation we've still got those in our back pocket for... something.  Is it perfect? Certainly not; but given the very minimal syntax and requirements on the language implementation, it's a pretty good "Bang! for the buck".


## If-else?

The first thing every programmer wants to do is conditionally execute same statement or select some expression based on the results of a test.  This is frequently presented as an "if (test) { statement } else { statement }" syntax.

For the sake of simplicity, Bang! provides an analog to the Apply operator "!", the Conditional Apply, represented by a question mark "?".   So to conditionally execute a block of code, write:

    fun = ...; booleanValue?

When "?" is encountered the first value popped off the stack is treated as the condition variable.  A function value is then popped off of the stack, and if the condition is true the function value is applied.

Here is a function that will either add 1 or subtract 1 from the provided value:

      def :up-or-down goUp = { 
         fun = 1 +; goUp?
         fun = 1 -; goUp not!?
      }
    
       100 true up-or-down!
     > 101 
    
       100 false up-or-down!
     > 99 

## Iteration

In the functional world, iteration is usually introduced as recursion.  One thing I didn't say earlier about the "def" keyword is that by sticking the function name up front, we have a way to identify ourself inside the function body, which allows the function to recurse.

So with the magic of first class lexically scoped functions and the "Conditionally Apply" operator (?) we can implement a higher order function that does something N times, like so:

       def :times remaining = { fun! doSomething =
         doSomething!
         fun= doSomething remaining 1 - times!; remaining 1 >?
       }

And it can be used like this:

       def :say-hello "Hello World!" print!;
    
       say-hello 20 times!

When the "times" function is called, it invokes the supplied "do something" function, then calls itself again with the number of repetitions decremented by 1.  It repeats this as long as the number of times remaining is greater than 1.

In the long run, bang will include libraries with some standard iteration functions like "times" and "range" that will iterate a number of times or over a range of values.  Much of the time that iteration is used in an imperative language, you probably should prefer a higher-order (like map or fold).  But if something like "times" or "range" doesn't cut it you can always buckle down and write a recursive function.

In many imperative languages you have to worry about recursive functions overflowing the stack.  Bang! employs tail call optimization to prevent stack frames from accumulating when recursion is employed as long as there is no work that remains to be done after the recursive invocation.

## Higher Order Functions

Higher order functions are functions that do something with a function.  Typically these are used to transform, accumulate, or filter a list or other such fun things.  "map" traditionally applies a functional operator to an entire list of values, applying the function each value value in the list and returning a list of transformed values.

I'm hestitant to add a firm list or array structure to Bang! because I think a lot of that can be done on the working stack.  To help with recursion over the stack, the '#' operator is provided which returns the current length of the stack.  With this operator, "map" can be implemented like this:

    def :map xform = {
      def :innermap = {
        fun val = innermap! val xform!;
        # 1 > ?
      }
      innermap!
    }

Each value on the stack is transformed by the provided function.  The 'innermap' function is called recursively as long as values remain on the stack.

Given this map function, we can write a "filter" function that leaves only those functions on the stack which pass a certain predicate test:

      def :filter predicate = {
          def :filter-xform v = {
            fun = v; v predicate!?
          }
          filter-xform map!
      }

To help with the use of the working stack, a couple other niceties are provided.  "nth" pushes (duplicates) the nth value on the stack.  "save-stack" clears the stack contents, saving them into a function that will push them back onto the stack when applied.

Additionally, parentheses may be used to set bounding markers on the size of the stack so that e.g, we can run these higher order functions on a certain point in the working stack and not operate on unrelated values which might be sitting on the stack.

Here is a short implementation of quicksort using higher order functions:

    def :quicksort = {
      # 2 / nth!  as pivotValue -- Stack[stacklen / 2]
      save-stack! as theStack
     
       theStack! fun = pivotValue <; filter! quicksort # 1 >?
      (theStack! fun = pivotValue =; filter!)
      (theStack! fun = pivotValue >; filter! quicksort # 1 >?)
    }

When quicksort is invoked, it looks at the size of the stack, calculates the midpoint index and stores the midpoint value for use as a 'pivot'.  The full stack contents are then saved off in a closure, as we'll need to operate on this list of values several times.

First, the stack contents less then the pivot value are pushed, recursively invoking quicksort to insure these values are fully sorted.  Then, the stack contents which match the pivot are pushed; finally, the stack contents greater than the pivot are pushed, recursively invoking quicksort on those stack items.  For the last two operations, parentheses are used to prevent the operations from being applied to values pushed in the previous step.

And that's, for an example, quicksort in six easy lines of Bang!.

## Records and Objects

So now we've got first class lexically scope functions, conditionals, iteration, and higher order functions, great; but everybody wants objects right?

Once you have first class lexically scoped functions, it is possible to build a lightweight object/record system. It may not be everything your average Java programmer wants out of an object system, but with the goal of keeping Bang! simple, it's a fair starting point.

The general way to do something like this is to write a "create-Thing" constructor method that binds the objects properties, and return an inner message handler function.  Messages can be sent to the object by calling the message handler handler returned by the constructor.  How that message handler works is entirely up to the implementer of the object system, but generally goes something like this:

      def :create-Employee = { as age as name
          fun message = {
              fun = age;  message 'age'  =?
              fun = name; message 'name' =?
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
        fun val = innermap! val xform!;
        # 1 > ?
      }
      innermap!
    }
    
    def :filter predicate = {
        def :filter-xform v = {
          fun = v; v predicate!?
        }
        filter-xform map!
    }
    
    fun = lookup
    
Users may access these functions by requiring [hof.lib]

    'lib/hof.bang' require! as hof

which saves the returned function as "hof".  Functions can then be accessed as such

    1 2 3 4 fun=100 +; hof.map!

Or rebound to local upvalues as

    hof.map as map


# Roadmap

* Improve parse / runtime error reporting
* coroutines, coroutines, coroutines.  Maybe even full continuations and call/cc
* user defined operators and consistency with primitive operators ala scala?
* Test integration with Boehm GC to fix my shared_ptr woes??
* Working indentation in bang-mode.el
* REPL
* Libraries
** SRFI implementations
** String library (borrow from Lua?)
* Library / Module Mechanism
** Import module namespace into working AST/environment e.g for REPL

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

