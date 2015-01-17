---
title: Erase delete
categories: C++
---

**tl;dr:** The usage of `delete` in normal C++ code is an anti-pattern. There are
ways of managing memory and other resources that are both more comfortable and
safer (while maintaining the same performance as manual `delete`). These
techniques go by the name [*RAII*][RAII].


# Memory leaks

Let's start with an example:

{% highlight c++ %}

void dialogFoo()
{
    Dialog* dlg = new Dialog();
    dlg->show();
    dlg->waitUntilClosed();
    std::cout << "Result: " << dlg->result() << '\n';
}

{% endhighlight %}

Obviously, there's a problem: The function `dialogFoo` creates a new `Dialog`
object on the heap using `new` and uses it, but does nothing to get rid of it.
The memory and any other resources (icons, fonts, …) used by the dialog are
*leaked*: the operating system still thinks that they are in use and will not be
able to give them to programs (including the program containing `dialogFoo`!)
that really need them.


# Exceptions: It's not so simple...

The obvious fix for this is to add a `delete` at the end of `dialogFoo`:

{% highlight c++ %}

void dialogFoo()
{
    Dialog* dlg = new Dialog();
    // ...
    delete dlg;
}

{% endhighlight %}

This is not a good solution for two reasons: First, it is easy to forget the
`delete` and it makes the code longer. Second, it is incomplete: If an exception
is thrown anywhere between the `new` and the `delete`, `dlg` is still leaked
(`dialogFoo` is not [*exception-safe*][exception-safety]).

[exception-safety]: http://en.wikipedia.org/wiki/Exception_safety

The absence of `throw` does not even remotely indicate the absence of
exeptions: Even if the called member functions of `dlg` never throw (which is
very unlikely for real code), there certainly is a function that might throw:
the `<<` operator of `std::cout`. It is unlikely, but someone could have called
e.g. [`std::cout.exceptions(std::ios::badbit)`][ios-exceptions] and the
program's output could have been redirected to a write-protected file.

[ios-exceptions]: http://en.cppreference.com/w/cpp/io/basic_ios/exceptions

You would have to avoid all C++ libraries that use exceptions, including the
standard library and `new` (`std::bad_alloc`) and `dynamic_cast`
(`std::bad_cast`) to ensure complete freedom of exceptions. Let's face it:
Unless a function (including overloaded operators) is documented not to throw
(preferrably even annotated with C++11's [`noexcept`][noexcept]), you have to
assume that it will eventually throw an exception.

[noexcept]: http://scottmeyers.blogspot.co.at/2014/08/near-final-draft-of-effective-modern-c.html

The idea of a `try`…`catch` block may arise now and indeed a way to write the
function that offers the [*basic exception guarantee*][exception-safety] (i.e.
it does not leak resources when an exception is thrown) would be:

{% highlight c++ %}

void dialogFoo()
{
    Dialog* dlg = new Dialog();
    try {
        dlg->show();
        dlg->waitUntilClosed();
        std::cout << "Result: " << dlg->result() << '\n';
    } catch (...) {  // Catch everything.
        delete dlg;
        throw;  // Rethrow the catched exception.
    }
    delete dlg;  // Note that this is duplicated from the catch block!
}

{% endhighlight %}

But this approach leads to madness: Even though this (previously) tiny function
manages only one resource, the cleanup code already accounts for 6 out of 10
lines in the function's body.


# Avoiding the heap: It's simpler!

In the case of `dialogFoo`, fortunately there is a much simpler solution: Simply
ditch the `new` and use `Dialog` directly (as a stack-allocated variable),
instead of a pointer:

{% highlight c++ %}

void dialogFoo()
{
    Dialog dlg;
    dlg.show();
    dlg.waitUntilClosed();
    std::cout << "Result: " << dlg.result() << '\n';
}

{% endhighlight %}

Besides being leak-free, that's even a bit simpler than the [initial
example](#memory-leaks), as it doesn't have to use `new`. It also runs faster,
as stack allocation is much faster than heap allocation. In general, heap
allocation should only be used when it's really necessary.


# Smart pointers: `std::unique_ptr`

Sometimes though, using a stack-allocated variable is not an option, e.g. when
dealing with polymorphic classes:

{% highlight c++ %}

Dialog* dlg;
if (preferences.useFunkyStyles)
    dlg = new FunkyDialog();
else
    dlg = new SystemDialog();

// Use dlg, having to delete it painfully afterwards

{% endhighlight %}

Luckily, the C++ standard library can help here, with so called smart pointers:

{% highlight c++ %}

std::unique_ptr<Dialog> dlg;
if (preferences.useFunkyStyles)
    dlg.reset(new FunkyDialog())
else
    dlg.reset(new SystemDialog());

// Use dlg as before; no delete necessary!

{% endhighlight %}

[`std::unique_ptr`][unique_ptr] is very simple: For the most part, it acts just
like a normal pointer (supporting the `*` and `->` operators) except that it
deletes the pointee in its destructor. There is one more important difference
though: If you want to assign it from a plain pointer (as the one returned by
`new` in the above example), you can't just use the assignment operator `=` but
you have to use the `reset` member function. The reason for this complication is
that `delete` is called on the `unique_ptr`'s previous value and if `=` was used
for that instead of `reset`, `unique_ptr` could not be safely used as a drop-in
replacement for a plain pointer, as it would change the code's meaning. This
way, a compile time error is generated when you rely on a behavior that is
differs from a plain pointer's.

[unique_ptr]: http://en.cppreference.com/w/cpp/memory/unique_ptr


# Beyond memory: RAII

This idiom of wrapping all resources (be it memory like in this example, files,
network connections, database connections, font handles, window handles, or
anything other) into a stack-allocated object that frees them in its destructor
is known by the (somewhat meaningless) name [*RAII*][RAII] (Resource Acquisition
Is Initialization). Other examples of its use include the standard file stream
classes (`std::fstream`, `ofstream`, `ifstream`) that close the file
automatically or the STL collection classes (`std::vector`, `list`, `map`, etc.)
that destroy their contained elements. In fact, probably even the `Dialog` class
of the above example will use RAII, as we have assumed that its destructor frees
any associated fonts, windows, and other handles it used.

In fact, the deterministic destructors of C++ ([in contrast to garbage collected
languages][oldnew-gc] where you can never know when or if finalizers are run)
that make this idiom possible are one of the language's key features, some call
it even [the best one][andrzej-dtor]. Often, you will see modern C++ code having
a much smaller share of resource management boilerplate code than garbage
collected languages with their `using` (C#), `with` (Python), or
`try-with-resources` (Java) blocks.

[RAII]: http://en.wikipedia.org/wiki/Resource_Acquisition_Is_Initialization
[oldnew-gc]: http://blogs.msdn.com/b/oldnewthing/archive/2010/08/09/10047586.aspx
[andrzej-dtor]: https://akrzemi1.wordpress.com/2013/07/18/cs-best-feature/
