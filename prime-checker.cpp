#include <iostream>
#include <functional>
#include "oneapi/tbb.h"

using namespace std;
using namespace oneapi::tbb;

#define N 1000000000

void test(function<void(size_t)> func, size_t upper_bound);

bool isPrime(size_t n);

void runNormal(size_t n);
void runWithTBB(size_t n);
void runWithStaticLoadBalancing(size_t n);
void runWithDynamicLoadBalancing(size_t n);

int main() {
    test(runNormal, N);
    test(runWithTBB, N);
    //test(runWithStaticLoadBalancing, N);
    //test(runWithDynamicLoadBalancing, N);

    return 0;
}

// TODO: time this function
void test(function<void(size_t)> func, size_t upper_bound) {
    func(upper_bound);
}

bool isPrime(size_t n) {
    if (n <= 1) {
        return false;
    }
    for (size_t i=2; i<n; ++i) {
        if(n%i == 0) {
            return false;
        }
    }
    return true;
}

void runNormal(size_t n) {
    for(size_t i=1; i<n; i++) {
        isPrime(n);
    }
}

void runWithTBB(size_t n) {
    parallel_for(blocked_range<size_t>(2,n), [&](tbb::blocked_range<size_t> r) {
        for(size_t i=r.begin(); i<r.end(); ++i) {
            isPrime(i);
        }
    });
}

void runWithStaticLoadBalancing(size_t n) {
    
}

void runWithDynamicLoadBalancing(size_t n) {
    //atomic integer
}