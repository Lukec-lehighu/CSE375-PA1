#include <iostream>
#include <string>
#include <cmath>
#include <functional>
#include "oneapi/tbb.h"

using namespace std;
using namespace oneapi::tbb;

#define N 1000000

void test(string name, function<size_t(size_t)> func, size_t upper_bound);

bool isPrime(size_t n);

size_t runNormal(size_t n);
size_t runWithTBB(size_t n);
size_t runWithStaticLoadBalancing(size_t n);
size_t runWithDynamicLoadBalancing(size_t n);

int main() {
    test("Normal", runNormal, N);
    test("TBB", runWithTBB, N);
    test("Static Load Balancing", runWithStaticLoadBalancing, N);
    test("Dynamic Load Balancing", runWithDynamicLoadBalancing, N);

    return 0;
}

void test(string name, function<size_t(size_t)> func, size_t upper_bound) {
    cout << "Testing " << name << "... \t\t" << flush;

    chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    size_t count = func(upper_bound);
    chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    chrono::milliseconds time = chrono::duration_cast<std::chrono::milliseconds>(end - begin);

    cout << "Done (Found " << count << " primes in " << time.count() << "ms)" << endl;
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

size_t runNormal(size_t n) {
    size_t count = 0;
    for(size_t i=1; i<n; i++) {
        count += isPrime(i) ? 1 : 0;
    }
    return count;
}

size_t runWithTBB(size_t n) {
    atomic<size_t> count(0);
    parallel_for(blocked_range<size_t>(1,n), [&](const tbb::blocked_range<size_t>& r) {
        for(size_t i=r.begin(); i<r.end(); ++i) {
            if(isPrime(i)) {
                count++;
            }
        }
    });

    return count;
}

size_t runWithStaticLoadBalancing(size_t n) {
    atomic<size_t> count(0);

    auto chunkFunction = [&](size_t start, size_t end) {
        for(size_t i=start; i<end; i++) 
            if(isPrime(i)) count++;
    };

    // helper function to determine where the bounds are for each chunk (formula that distributes the load)
    // this function gradually decreases the gap between the last chunkBound and the next, so higher index chunks are smaller since they have more work related to them
    auto chunkBound = [&](int num){ return pow(num/8, 0.5) * n; }; 

    //make 8 threads with static chunk sizes
    thread jobs[8] = {
        thread([&]{ chunkFunction(0, chunkBound(1)); }),
        thread([&]{ chunkFunction(chunkBound(1), chunkBound(2)); }),
        thread([&]{ chunkFunction(chunkBound(2), chunkBound(3)); }),
        thread([&]{ chunkFunction(chunkBound(3), chunkBound(4)); }),
        thread([&]{ chunkFunction(chunkBound(4), chunkBound(5)); }),
        thread([&]{ chunkFunction(chunkBound(5), chunkBound(6)); }),
        thread([&]{ chunkFunction(chunkBound(6), chunkBound(7)); }),
        thread([&]{ chunkFunction(chunkBound(7), n); }),
    };

    // wait for all jobs to finish
    for(int i=0; i<8; i++) {
        jobs[i].join();
    }

    return count;
}

size_t runWithDynamicLoadBalancing(size_t n) {
    //atomic shared value to coordinate workers
    atomic<size_t> numToCheck(1);
    atomic<size_t> count(0);

    auto workerFunc = [&] {
        while(true) {
            size_t check = numToCheck.fetch_add(1);
            if(check > n) return;
            if(isPrime(check)) count++;
        }
    };

    thread jobs[8];
    for(int i=0; i<8; i++) {
        jobs[i] = thread([&]{ workerFunc(); });
    };

    // wait for all jobs to finish
    for(int i=0; i<8; i++) {
        jobs[i].join();
    }

    return count;
}