#ifndef EXPONENTIALBACKOFF_H
#define EXPONENTIALBACKOFF_H


#include <random>
#include <cmath>

/*!
 * \brief The ExponentialBackoff class
 */
class ExponentialBackoff
{
      std::random_device __seeder;
      std::mt19937 __engine;
      std::uniform_int_distribution<int> __distribution;
      int __retry;
      int __maxRetry;
    public:
      ExponentialBackoff(int start_val = 0, int max_retry = -1);
      ExponentialBackoff(const ExponentialBackoff& rhs);
      const ExponentialBackoff& operator =(const ExponentialBackoff& rhs);

      int next();
      void resetRetry() { __retry = 0;}
};

#endif // EXPONENTIALBACKOFF_H
