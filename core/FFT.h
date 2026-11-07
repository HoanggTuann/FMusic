#pragma once
#include <vector>

// FFT radix-2 (Cooley–Tukey, in-place, iterative). n phải là luỹ thừa của 2.
class FFT {
public:
    explicit FFT(int n);
    int size() const { return n_; }
    void forward(float* re, float* im) const;   // biến đổi thuận, in-place
private:
    int n_;
    std::vector<int> rev_;
    std::vector<float> cos_, sin_;
};
