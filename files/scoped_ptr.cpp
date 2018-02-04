#include <iostream>

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


int main()
{
#if 1
    scoped_ptr<int> pI(new int);
    *pI = 42;
    std::cout << "*pI: " << *pI << '\n';

    struct Foo {
        float bar;
    };

    scoped_ptr<Foo> pFoo(new Foo());
    pFoo->bar = 7;
    std::cout << "pFoo->bar: " << pFoo->bar << '\n';
#else
    scoped_ptr<int> pI(new int);
    scoped_ptr<int> pJ = pI;
    // The newed int is deleted twice --> undefined behavior!
#endif
}
