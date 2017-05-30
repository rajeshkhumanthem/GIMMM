#include "exponentialbackoff.h"


/*!
 * \brief ExponentialBackoff::ExponentialBackoff
 * \param start_val
 * \param max_retry
 */
ExponentialBackoff::ExponentialBackoff(int start_val, int max_retry)
      :__engine(__seeder()),
        __distribution(100, 1000),
        __retry(start_val)
{
        if ( max_retry > -1)
            __maxRetry = max_retry + start_val;

}


/*!
 * \brief ExponentialBackoff::next
 * \return
 */
int ExponentialBackoff::next()
{
    if ((__maxRetry != -1) &&
            (__retry > __maxRetry))
      return -1;

    int random_delta = __distribution(__engine);
    int r = 0.5 * (pow(2, __retry) - 1);

    int next = r * 1000 + random_delta;
    __retry++;
    return next;
}
