#include <iostream>

#include "greeter.hpp"

int main(int argc, char** argv) {
    std::cout << greet(argc > 1 ? argv[1] : "NixOS") << '\n';
    return 0;
}