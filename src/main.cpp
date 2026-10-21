#include <windows.h>   // SetConsoleOutputCP
#include "App.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

int main() {
    SetConsoleOutputCP(CP_UTF8);  // acentos corretos nas mensagens do console
    try {
        App app;
        app.run();
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "Erro: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}