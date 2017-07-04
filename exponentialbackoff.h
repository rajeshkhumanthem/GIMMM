#ifndef EXPONENTIALBACKOFF_H
#define EXPONENTIALBACKOFF_H


#include <random>
#include <cmath>

#define NO_MAX_RETRY -1
/*!
 * \brief The ExponentialBackoff class
 */
class ExponentialBackoff
{
      std::random_device __seeder;
      std::mt19937 __engine;
      std::uniform_int_distribution<int> __distribution;
      int __retry;
      int __seedVal;
      int __maxRetry;
    public:
      ExponentialBackoff(int max_retry = NO_MAX_RETRY);
      ExponentialBackoff(const ExponentialBackoff& rhs);
      const ExponentialBackoff& operator =(const ExponentialBackoff& rhs);

      //getter
      int getRetry() const { return __retry;}
      int getMaxRetry()const { return __maxRetry;}
      int getSeedValue()const { return __seedVal;}

      int next();
      void reset() { __retry = 1; __seedVal = 2;}
};

#endif // EXPONENTIALBACKOFF_H
