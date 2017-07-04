#include "exponentialbackoff.h"

#include <iostream>

/*!
 * \brief ExponentialBackoff::ExponentialBackoff
 * \param start_val
 * \param max_retry
 */
ExponentialBackoff::ExponentialBackoff(int max_retry)
      :__engine(__seeder()),
        __distribution(100, 1000),
        __retry(0),
        __seedVal(2)
{
    __maxRetry = max_retry;
}


ExponentialBackoff::ExponentialBackoff(const ExponentialBackoff &rhs)
{
    __engine = rhs.__engine;
    __distribution = rhs.__distribution;
    __retry = rhs.__retry;
    __seedVal = rhs.__seedVal;
    __maxRetry = rhs.__maxRetry;
}


const ExponentialBackoff& ExponentialBackoff::operator =(const ExponentialBackoff& rhs)
{
    if ( &rhs != this)
    {
        __engine = rhs.__engine;
        __distribution = rhs.__distribution;
        __retry = rhs.__retry;
        __seedVal = rhs.__seedVal;
        __maxRetry = rhs.__maxRetry;
    }
    return *this;
}

/*!
 * \brief ExponentialBackoff::next
 * \return
 */
int ExponentialBackoff::next()
{
    __retry++;

    //init retry after max retry so that
    //we start again from a small val.
    if ((__maxRetry != NO_MAX_RETRY) &&
            (__retry > __maxRetry))
    {
        __retry = 0;
        __seedVal = 2;
    }

    int random_delta = __distribution(__engine);
    int r = 0.5 * (pow(2, __seedVal) - 1);
    int next = r * 1000 + random_delta;
    __seedVal++;
    return next;
}
