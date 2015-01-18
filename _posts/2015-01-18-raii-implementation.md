---
title: "Implementing RAII and more on smart pointers"
category: C++
tags: [RAII]
date: "2015-01-18 17:37"
---

In my [last post]({% post_url 2015-01-17-erase-delete %}), I presented the
usefulness and necessity of RAII. In this post I want to walk you through the
implementation of the [second][observer_ptr] simplest imaginable smart pointer:
The `scoped_ptr` class template should support the `*` and `->` operators, and
delete the pointee in its destructor. This is what
[`std::unique_ptr`][unique_ptr] also does, but we will see that our `scoped_ptr`
lacks some of `unique_ptr`'s features.

[observer_ptr]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4282.pdf
[unique_ptr]: http://en.cppreference.com/w/cpp/memory/unique_ptr

# Reimplementing raw pointer functionality

Let's start with the easy part. Since we want to be able to use `scoped_ptr`
with any pointee type, we make it a template. Also, the only thing we will need
to store in the `scoped_ptr` class is the actual pointer:

{% highlight cpp %}

template <typename T>
class scoped_ptr {
public:
    // TODO
private:
    T* m_ptr;
};

{% endhighlight %}

## Constructor

If we want to be able to use this class at all, we will first need constructors:

{% highlight cpp %}

public:
    explicit scoped_ptr(T* p = nullptr): m_ptr(p) { }

{% endhighlight %}

This fills two roles: First it acts as the default constructor that initializes
the `m_ptr` member with `nullptr`, thanks to the default argument `nullptr` for
`p`. Second, it can be passed a raw pointer to initialize the held pointer to
that. I added the `explicit` because implicit conversions from a raw pointer to
a `scoped_ptr` could easily lead to an accidental `delete` on an object that is
still needed (yes, right now there is no `delete` in sight, but it will follow).

## Pointer operators `*` and `->`

Next, the pointer operators `->` and `*`:

{% highlight cpp %}

public:
    T* operator-> () const
    {
        return m_ptr;
    }

    T& operator* () const
    {
        return *m_ptr;
    }

{% endhighlight %}

The arrow operator `->` is a bit special: It looks like a binary operator when
used (`dialog->show()`) but is overloaded with only one argument (the implicit
`this` pointer here). The reason is that the task of such an overload is just to
return a raw pointer on which the "real" `->` is applied. There is no other way
possible: C++ offers no way to select members by name at compile time.

The dereferencing operator `*` hopefully does what you would have thought: It
returns a mutable reference to the `scoped_ptr`'s pointee. Returning by value
would not make sense as you could then never change the pointed-to object.

These functions are `const` because they do not change the pointer, they only
allow changing the pointee. Like `int* const p` (in contrast to `int const* p`
== `const int* p`) allows `*p = 42`, `scoped_ptr<int> const p` does too (in
contrast to `scoped_ptr<int const>` == `scoped_ptr<const int>`).

Now the class is ready to be used like a raw pointer, with the additional
advantage of being automatically initialized to `nullptr` if no other value is
specified.

{% highlight cpp %}

scoped_ptr<int> pI(new int);
*pI = 42;
std::cout << "*pI: " << *pI << '\n';

struct Foo {
    float bar;
};

scoped_ptr<Foo> pFoo(new Foo());
pFoo->bar = 7;
std::cout << "pFoo->bar: " << pFoo->bar << '\n';

{% endhighlight %}

Well, there is one problem left: The memory is obviously leaked and we cannot
even `delete` a `scoped_ptr`!


# The actual RAII functionality

Of course, avoiding leaks was the original intention of even creating this
class, so let's fix this:

{% highlight cpp %}

public:
    ~scoped_ptr()
    {
        delete m_ptr; // (delete can handle nullptr, no need to check.)
    }

{% endhighlight %}

Fine! That fixes the above code, which is now leak-free, without a single
visible `delete`.[^delraii] The core of our new RAII class is now complete.

[^delraii]:
    The implementation of RAII classes is pretty much the only place
    where `delete` is acceptable in good C++ code.

But the `scoped_ptr` now has a problem: Remember the [Rule of Three][big3]?

> If a class has a user-defined destructor, copy constructor or assignment
> operator, it also needs the other two.

[big3]: https://en.wikipedia.org/wiki/Rule_of_three_(C%2B%2B_programming)#Rule_of_Three

We have a user-defined destructor but still use the compiler-generated copy
constructor and assignment operator, which is bad, since they will just copy
`m_ptr` leading to multiple `delete`s of the same pointer value:

<a name="big3-violation">
{% highlight cpp %}

scoped_ptr<int> pI(new int);
scoped_ptr<int> pJ = pI;
// The newed int is deleted twice --> undefined behavior!

{% endhighlight %}

This is an important point: A RAII class is subject to the Rule of Three (or
Five in C++11) and thus needs to take care not only of freeing a resource, but
also of what happens when you copy(-assign) it. Scott Meyers outlines the three
basic options in Item 15 of Effective C++ (3rd edition):

1. Copy the resource (deep copy).
2. Share the resource, with the copied RAII class; when the last copy is
   destroyed, the resource is also destroyed.
3. Disallow copying.

Option 1 is impossible for smart pointers in the general case, since we cannot
copy a polymorphic class via a pointer to a base. It's also usually not what you
want from a pointer.

Option 2 is actually quite useful and comes very close to garbage collection
(minus collection of cyclic references, i.e. objects referencing each other).
The standard library implementation of a smart pointer that uses this option is
[`std::shared_ptr`][shared_ptr] and I recommend you to take a look at it (but as
it comes with a performance and memory overhead, use it only when a
[`std::unique_ptr`][unique_ptr] is not enough!).

[shared_ptr]: http://en.cppreference.com/w/cpp/memory/shared_ptr

Option 3 is what we are gonna do:

{% highlight cpp %}

public:
    scoped_ptr(scoped_ptr const&) = delete;
    scoped_ptr& operator= (scoped_ptr const&) = delete;

{% endhighlight %}

The `= delete` declaration here has nothing to do with the ordinary `delete
ptr`; It is a C++11 feature that instructs the compiler not to generate these
normally compiler-generated functions, so that any uses of them will lead to a
compile-time error. Now the [code above](#big3-violation) will not make the
program crash (or whatever else the undefined behavior might cause) but instead
just fail to compile with a (hopefully) nice error message that allows us to
quickly find and fix the problem.


## Convenience functions

In practice it is often useful or even necessary to access the raw pointer
wrapped by the `scoped_ptr`, so we will provide a member function `get` that
returns it.[^get] Although we disallow assigning `scoped_ptr`s to each other, it
is still useful to reassign the underlying raw pointer (deleting the old one).
For this we will provide a `reset` function. The final class definition is then:

{% highlight cpp %}

template <typename T>
class scoped_ptr {
public:
    explicit scoped_ptr(T* p = nullptr): m_ptr(p) { }
    scoped_ptr(scoped_ptr const&) = delete;
    scoped_ptr& operator= (scoped_ptr const&) = delete;

    ~scoped_ptr()
    {
        delete m_ptr;
    }


    T* operator-> () const
    {
        return m_ptr;
    }

    T& operator* () const
    {
        return *m_ptr;
    }


    T* get() const
    {
        return m_ptr;
    }

    void reset(T* p = nullptr)
    {
        delete m_ptr;
        m_ptr = p;
    }

private:
    T* m_ptr;
};

{% endhighlight %}

[(Download as `.cpp`, including example code)]({{ site.baseurl }}/files/scoped_ptr.cpp)

# `boost::scoped_ptr` and `std::unique_ptr`

An implementation of `scoped_ptr` that is a superset of the above is available
in [Boost.SmartPtr][boost-scoped]. It additionally provides conversion to `bool`
that checks if the pointer is `nullptr` and a `swap` function.

[boost-scoped]: http://www.boost.org/doc/libs/release/libs/smart_ptr/scoped_ptr.htm

[`std::unique_ptr`][unique_ptr] is, again, a superset of `boost::scoped_ptr`.
You generally always want to use this instead of `boost::scoped_ptr` if you have
a C++11 compiler. It provides pointer comparison operators (both with other
`unique_ptr`s and raw pointers), support for customizing how the pointer should
be deleted, a `release` member function that returns the raw pointer and assigns
the `unique_ptr` to `nullptr` without deleting it, and, most importantly, it can
be moved. Moveability means that although you cannot copy a `unique_ptr` you can
still return it from a function or assign it to another `unique_ptr` by moving
it:

{% highlight cpp %}

std::unique_ptr<int> pI(new int);
std::unique_ptr<int> pJ = std::move(pI);

{% endhighlight %}

After this snippet, `pI` contains `nullptr` and `pJ` contains the pointer to the
new `int`; it was *moved* from `pI` to `pJ`.

If `std::move` is completely new to you, you might be interested in reading
about rvalue references and move semantics (e.g. in [this Stackoverflow
question][so-rvalue]).

[so-rvalue]: http://stackoverflow.com/questions/3106110/what-is-move-semantics

Since C++14 there is also the [`std::make_unique`][make_unique] function: It
works like `new` but directly returns a `unique_ptr` instead of a raw pointer.
Besides the synergy with `auto` type deduction, it brings C++ closer to
completely eradicating not only `delete` but also `new` from normal code. See
[this article][sutter-smart] by Herb Sutter for more.

[make_unique]: http://en.cppreference.com/w/cpp/memory/unique_ptr/make_unique
[sutter-smart]: http://herbsutter.com/2013/05/29/gotw-89-solution-smart-pointers/

[^get]:
    In fact, such a function that returns the underlying resource handle
    from a RAII wrapper class is useful not only for smart pointers but for all
    kinds of resources. For example the application framework Qt provides a
    [`winId`][winId] function for the `QWidget` class that returns the native OS
    handle of the widget to allow interoperation with platform-specific functions.

[winId]: http://doc.qt.io/qt-5/qwidget.html#winId

