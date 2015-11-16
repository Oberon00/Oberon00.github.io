---
title: "Type safety of .NET delegates vs. C function pointers"
categories: [C#, C++]
tags: [Type safety]
date: "2015-11-16 22:54"
---

It is often claimed that one of the advantages of the C#/.NET delegate mechanism
over C/C++ function pointers is the type safety. This claim is true but not in
the obvious way.

# The C way

Consider the following C-example:

{% highlight c %}
typedef bool(*LessThanFn)(int, int);

int maxInt(int* arr, size_t len, LessThanFn lt) {
    assert(len > 0);
    int result = arr[0];
    for (size_t i = 1; i < len; ++i) {
        if (lt(result, arr[i]))
            result = arr[i];
    }

    return result;
}

bool less(int lhs, int rhs) { return lhs < rhs; }

#define LEN 4

int main() {
  int ints[LEN] = {0, -1, 10, 4};
  assert(maxInt(ints, LEN, less) == 10);
  return 0;
}
{% endhighlight %}

Here, everything is completely type-safe. The mistake of passing a function with
a signature that does not exactly match the one given by `LessThanFn` will be
caught by the compiler.

## Stateful functions in C

In practice we often find that just passing a function pointer is not enough --
we also need to associate state with the function. For example, what if the
integers in the above example are indices to another array and we want to get
the index of the maximum element in that other array? Or what if we want to have
the integer with the greatest absolute difference from another integer?  Since
there are no generics in C, the typical way to implement this is by using
`void*`:

{% highlight c %}
typedef bool(*LessThanFn)(int, int, void*);

int maxInt(int* arr, size_t len, LessThanFn lt, void* state) {
    assert(len > 0);
    int result = arr[0];
    for (size_t i = 1; i < len; ++i) {
        if (lt(result, arr[i], state))
            result = arr[i];
    }

    return result;
}

bool differenceLess(int lhs, int rhs, void* state) {
    int center = *(int*)state;
    return abs(lhs - center) < abs(rhs - center);
}

#define LEN 4

int main() {
    int ints[LEN] = {0, -1, 10, 4};
    int center = 11;
    assert(maxInt(ints, LEN, differenceLess, &center) == -1);
    center = 4;
    assert(maxInt(ints, LEN, differenceLess, &center) == 10);
}
{% endhighlight %}

Not only is this unnecessarily complicated if the function has no state, it also
is no longer type-safe! Passing the address of a `char` instead of an `int`
already causes undefined behavior.

# The C# way

Contrary to the previous C example, using C# delegates leads to code that is
both equally simple for stateful and stateless functions and type-safe:

{% highlight c# %}
delegate bool LessThanFn(int lhs, int rhs);

internal static class Program {
    private static int MaxInt(int[] arr, LessThanFn lt) {
        Debug.Assert(arr.Length > 0);
        int result = arr[0];
        for (int i = 1; i < arr.Length; ++i) {
            if (lt(result, arr[i]))
                result = arr[i];
        }
        return result;
    }

    private class DifferenceLess {
        public int Center { get; set; }

        public bool Compare(int lhs, int rhs) {
            return Math.Abs(lhs - Center) < Math.Abs(rhs - Center);
        }
    }

    public static void Main(string[] args) {
        int[] ints = {0, -1, 10, 4};
        var cmp = new DifferenceLess { Center = 11 };
        Debug.Assert(MaxInt(ints, cmp.Compare) == -1);
        cmp.Center = 4;
        Debug.Assert(MaxInt(ints, cmp.Compare) == 10);
    }
}
{% endhighlight %}

Clearly, this C# example has improved type safety over the C example before.
Under the hood, the delegate type is implemented similarly to the C version in
that it also has an additional state member of type `object` (the “target
object” of the delegate) that replaces the `void*` parameter we used in C. Even
though `object` is only slightly more type-safe than `void*` (replacing
undefined behaviour with `InvalidCastException`s), delegates ensure type
safety by not letting the programmer manually set the target object. Only the
compiler can do it, and it always does so in combination with the target method
making it impossible to have a target object and method type mismatch.

If you insist on programming in a type-unsafe way in C#, you can still obtain
the target object by using the delegates public [`Target` property][deltarget]
and fool around with the resulting `object`.


# The C++ way(s)

Even though this is a post about C# and C, I still cannot refrain from pointing
out the fact that C++ also has not only one (of course) but two nice type-safe
solutions to this problem. The first is to use a functor class, i.e. a class
similar to `DifferenceLess` from the C# example but with an `operator()` instead
of a `Compare` method. Then the `maxInt` function is changed to take the
function not via a function pointer but via an arbitrary type (the `&&` ensures
that the function object is not copied, which could be bad for large stateful
functors while still allowing for functors with non-`const`
`operator()`):[^cppsig]

{% highlight c++%}

template <typename F>
int maxInt(int* arr, std::size_t len, F&& lt) { /* exactly as in C */ }

{% endhighlight %}

The other option is to use the [`std::function` class template][stdfn] instead
of a template parameter as type for the function parameter, which is less
efficient but useful in situations where a function template cannot be used like
when the function taking the function should be a overridable virtual method.

[stdfn]: http://en.cppreference.com/w/cpp/utility/functional/function
[deltarget]: https://msdn.microsoft.com/en-us/library/system.delegate.target.aspx


# Lambdas

This post completely ignored lambdas so far, but of course it is often far
nicer to just drop in a lambda instead of writing a function of functor class.
This is supported by both C++ and C#:

{% highlight c# %}
// C#
int center = 4;
int max = MaxInt(arr, (a, b) => Math.Abs(a - center) < Math.Abs(b -center));
{% endhighlight %}

{% highlight c++%}
// C++
int center = 4;
int max = maxInt(arr, len, [center] (int a, int b) {
    return std::abs(a - center) < std::abs(b - center);
});
{% endhighlight %}

[^cppsig]:
    Note that making this a idiomatic C++ algorithm would also include replacing
    the array described by an `int*` and a length by a pair of iterators,
    allowing the function to work not only on an array of `int`s but any
    container with elements of any type -- or one could just use
    [`std::max_element`](http://en.cppreference.com/w/cpp/algorithm/max_element).

    Similarly, the C# version before would best also become a generic and
    operate on an `IEnumerable<T>` instead of an `int[]`.
