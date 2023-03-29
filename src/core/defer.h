#ifndef DEFER_H
#define DEFER_H

template <typename F> class defer_raii
{
public:
    // copy/move construction and any kind of assignment would lead to the cleanup function getting
    // called twice. We can't have that.
    defer_raii(defer_raii&&) = delete;
    defer_raii(const defer_raii&) = delete;
    defer_raii& operator=(const defer_raii&) = delete;
    defer_raii& operator=(defer_raii&&) = delete;

    // construct the object from the given callable
    template <typename FF> defer_raii(FF&& f) : cleanup_function(std::forward<FF>(f)) {}

    // when the object goes out of scope call the cleanup function
    ~defer_raii() { cleanup_function(); }

private:
    F cleanup_function;
};

template <typename F> defer_raii<F> defer_func(F&& f)
{
    return { std::forward<F>(f) };
}

#define DEFER_ACTUALLY_JOIN(x, y) x##y
#define DEFER_JOIN(x, y) DEFER_ACTUALLY_JOIN(x, y)

#ifdef __COUNTER__
#define DEFER_UNIQUE_VARNAME(x) DEFER_JOIN(x, __COUNTER__)
#else
#define DEFER_UNIQUE_VARNAME(x) DEFER_JOIN(x, __LINE__)
#endif

#define defer(lambda__) [[maybe_unused]] const auto& DEFER_UNIQUE_VARNAME(_defer_) = defer_func([&]() { lambda__; })

#endif // !DEFER_H