/*
Shell command to run the file: (I should probably just run this in a .sh)
g++ -g -O3 -c `llvm-config --cxxflags --ldflags --system-libs --libs core` main.cpp -o unlinked_exe &&
g++ unlinked_exe $(llvm-config --ldflags --libs) -lpthread -o jmb && ./jmb
*/
#include <bits/stdc++.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <map>
#include <string>
#include <algorithm>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "jimpilier.h"

template <>
struct fmt::formatter<Token> : fmt::formatter<std::string>
{
    auto format(Token my, format_context &ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[Token i={}]", my.toString());
    }
};
bool initialize_logger()
{
    try
    {
        // Create logs directory if it doesn't exist
        mkdir("logs", 0777);

        // Create a rotating file sink with 5MB max size and 3 rotated files
        auto max_size = 5 * 1024 * 1024; // 5MB
        auto max_files = 3;              // Keep 3 rotated files
        // File sink (all levels >= debug)
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/compile.log", max_size, max_files);

        // Console sink (only warnings and errors)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);

        // Create combined logger with both sinks
        std::vector<spdlog::sink_ptr> sinks = {file_sink, console_sink};
        auto logger = std::make_shared<spdlog::logger>(
            "multi_thread_logger",
            sinks.begin(),
            sinks.end());

        // Set as default logger
        spdlog::set_default_logger(logger);

        // Optional: Set log pattern
        spdlog::set_pattern("[thread %t:%l] %v");

        // Set flush policy (immediately flush messages at or above warning level)
        spdlog::flush_on(spdlog::level::warn);
        spdlog::set_level(spdlog::level::debug);
        spdlog::info("--------- Logger initialized successfully. Begin New session. ----------");
        return true;
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        return false;
    }
}

int main(int argc, char **args)
{
    initialize_logger();
    std::vector<std::string> all_args;
    if (argc > 1)
    {
        all_args.assign(args + 1, args + argc);
    }
    else
    {
        std::cout << "Error: No args provided" << endl;
        return 1;
    }

    jimpilier::ctxt = std::make_unique<llvm::LLVMContext>();
    jimpilier::GlobalVarsAndFunctions = std::make_unique<llvm::Module>("Jimbo jit", *jimpilier::ctxt);
    jimpilier::builder = std::make_unique<llvm::IRBuilder<>>(*jimpilier::ctxt);
    jimpilier::DataLayout = std::make_unique<llvm::DataLayout>(jimpilier::GlobalVarsAndFunctions.get());
    jimpilier::currentFunction = NULL;
    if (argc > 2)
    {
        std::string arg1 = all_args[0];
        if (arg1 == "-edu" || arg1 == "-rp")
        {
            jimpilier::currentFunction = (llvm::Function *)jimpilier::GlobalVarsAndFunctions->getOrInsertFunction("static", llvm::FunctionType::get(llvm::Type::getInt32Ty(*jimpilier::ctxt), {llvm::Type::getInt32Ty(*jimpilier::ctxt), llvm::PointerType::getInt8Ty(*jimpilier::ctxt)->getPointerTo()}, false)).getCallee();
            jimpilier::STATIC = jimpilier::currentFunction;
            llvm::BasicBlock *staticentry = llvm::BasicBlock::Create(*jimpilier::ctxt, "entry", jimpilier::currentFunction);
            jimpilier::builder->SetInsertPoint(staticentry);
            all_args.erase(all_args.begin());
        }
    }
    time_t now = time(nullptr);
    Stack<Token> tokens = jimpilier::loadTokens(all_args[0]);
    jimpilier::currentFile = all_args[0];
    std::unique_ptr<jimpilier::ExprAST> x;
    while (!tokens.eof())
    {
        x = jimpilier::getValidStmt(tokens);
        if (x != NULL)
            x->codegen();
    }
    if (jimpilier::GlobalVarsAndFunctions->getFunction("main") == NULL && jimpilier::STATIC != NULL)
        jimpilier::STATIC->setName("main");
    else if (jimpilier::STATIC != NULL)
    {
        jimpilier::STATIC->eraseFromParent();
    }
    time_t end = time(nullptr);

    std::cout << "; Code was compiled in approx: " << (end - now) << " seconds" << endl;
    jimpilier::GlobalVarsAndFunctions->dump();
    spdlog::info("--------- End Existing session. ----------");
}