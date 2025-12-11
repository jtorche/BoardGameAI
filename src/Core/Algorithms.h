#pragma once

#include <algorithm>
#include <iterator>

namespace core
{
    template<typename T>
    T countBits(T _x)
    {
        // Brian Kernighan’s Algorithm
        T result = 0;

        while (_x) {

            _x = _x & (_x - 1);
            ++result;
        }
        return result;
    }

    template<typename Iterator, typename Predicate, typename Operation>
    void for_each_if(Iterator _begin, Iterator _end, Predicate _predicate, Operation _operation)
    {
        std::for_each(
            _begin,
            _end,
            [&](auto& _p)
            {
                if (_predicate(_p))
                {
                    _operation(_p);
                }
            });
    }

    template<typename Iterable, typename Predicate, typename Operation>
    void for_each_if(Iterable&& _iterable, Predicate _predicate, Operation _operation)
    {
        for_each_if(std::begin(_iterable), std::end(_iterable), _predicate, _operation);
    }

    template<typename T, typename Iterator, typename Operation>
    void dynamic_for_each(Iterator _begin, Iterator _end, Operation _operation)
    {
        std::for_each(
            _begin,
            _end,
            [&](auto& _p)
            {
                auto tp = dynamic_cast<T>(_p);
                if (tp)
                {
                    _operation(tp);
                }
            });
    }

    template<typename T, typename Iterable, typename Operation>
    void dynamic_for_each(Iterable&& _iterable, Operation _operation)
    {
        dynamic_for_each<T>(std::begin(_iterable), std::end(_iterable), _operation);
    }
}
