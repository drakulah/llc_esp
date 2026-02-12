#ifndef RESULT_HPP
#define RESULT_HPP

#include <stdexcept>
#include <utility>

template <typename V, typename E>
class Result
{
    V ok_value;
    E err_value;
    bool is_ok_val;

public:
    // Constructors
    explicit Result(const V &value) : ok_value(value), is_ok_val(true)
    {
    }

    explicit Result(V &&value) : ok_value(std::move(value)), is_ok_val(true)
    {
    }

    explicit Result(const E &error) : err_value(error), is_ok_val(false)
    {
    }

    explicit Result(E &&error) : err_value(std::move(error)), is_ok_val(false)
    {
    }

    // Destructor
    ~Result()
    {
        if (is_ok_val)
        {
            ok_value.~V();
        }
        else
        {
            err_value.~E();
        }
    }

    // Copy constructor
    Result(const Result &other) : is_ok_val(other.is_ok_val)
    {
        if (is_ok_val)
            new (&ok_value) V(other.ok_value);
        else
            new (&err_value) E(other.err_value);
    }

    // Move constructor
    Result(Result &&other) noexcept : is_ok_val(other.is_ok_val)
    {
        if (is_ok_val)
            new (&ok_value) V(std::move(other.ok_value));
        else
            new (&err_value) E(std::move(other.err_value));
    }

    // Copy assignment
    Result &operator=(const Result &other)
    {
        if (this != &other)
        {
            this->~Result();
            new (this) Result(other);
        }
        return *this;
    }

    // Move assignment
    Result &operator=(Result &&other) noexcept
    {
        if (this != &other)
        {
            this->~Result();
            new (this) Result(std::move(other));
        }
        return *this;
    }

    bool is_ok() const
    {
        return is_ok_val;
    }
    bool is_err() const
    {
        return !is_ok_val;
    }

    V &unwrap()
    {
        if (!is_ok_val)
            throw std::runtime_error("Called unwrap on Err");
        return ok_value;
    }

    const V &unwrap() const
    {
        if (!is_ok_val)
            throw std::runtime_error("Called unwrap on Err");
        return ok_value;
    }

    E &unwrap_err()
    {
        if (is_ok_val)
            throw std::runtime_error("Called unwrap_err on Ok");
        return err_value;
    }

    const E &unwrap_err() const
    {
        if (is_ok_val)
            throw std::runtime_error("Called unwrap_err on Ok");
        return err_value;
    }
};

#endif
