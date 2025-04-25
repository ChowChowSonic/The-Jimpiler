#ifndef jimbo_gtest
#include <gtest/gtest.h>
#endif
#include <fstream>
#include <string>
std::string getOutputEnv()
{
	char *env = getenv("GITHUB_OUTPUT");
	std::string out = std::string(env == NULL ? "" : env);
	if (out == "")
	{
		out = std::string("./testData/out.txt");
	}
	return out;
}

TEST(TestCompiledCode, TestFibAsMain)
{
	std::string out = getOutputEnv();
	std::string cmd = " ./jmb testData/forLoop.jmb 2>&1 | lli-14 1>> " + out;
	if(out != "./testData/out.txt") cmd= "echo \"FibonacciSequence<<\0\" >" + out + " ; " + cmd + "echo \"\0\" >> " + out; 
	int result = system(cmd.c_str());
	EXPECT_EQ(result, EXIT_SUCCESS);
	// Read the output file and compare with expected content
	std::vector<std::string> expected = {"0 1 1 2 3 5 8 13 21 34 55 89 144 233 377 610 987 1597 "};
	std::string actual;
	std::ifstream input(out);
	std::getline(input, actual);
	int x = 0;
	while (!input.eof() && x < expected.size())
	{
		EXPECT_EQ(actual, expected[x]);
		std::getline(input, actual); // Get the x-th line
		x++;
	}
	system("rm -rf ./testData/out.txt ;");
}

TEST(TestCompiledCode, TestThrowCatch)
{
	std::string out = getOutputEnv();
	std::string cmd = "./jmb testData/throwCatch.jmb 2>&1 | lli-14 1>> " + out;
	if(out != "./testData/out.txt") cmd= "echo \"ThrowCatch<<\0\" >" + out + " ; " + cmd + "echo \"\0\" >> " + out; 
	int result = system(cmd.c_str());
	EXPECT_EQ(result, EXIT_SUCCESS);
	// Read the output file and compare with expected content
	std::vector<std::string> expected = {"Caught an integer with an operator: 5 ", "Caught: 5 ", "Caught an integer with an operator: 6 ", "Caught: 6 ", "Caught an integer with an operator: 7 ", "Caught an integer with an operator: 6 "};
	std::string actual;
	std::ifstream input(out);
	std::getline(input, actual);
	int x = 0;
	while (!input.eof() && x < expected.size())
	{
		EXPECT_EQ(actual, expected[x]);
		std::getline(input, actual); // Get the x-th line
		x++;
	}
	system("rm -rf ./testData/out.txt;");
}

TEST(TestCompiledCode, TestComplexIfStmts)
{
	std::string out = getOutputEnv();
	std::string cmd = "./jmb testData/complexIfStmt.jmb 2>&1 | lli-14 1>> " + out;
	if(out != "./testData/out.txt") cmd= "echo \"complexIfStmt<<\0\" >" + out + " ; " + cmd + "echo \"\0\" >> " + out; 
	int result = system(cmd.c_str());
	EXPECT_EQ(result, EXIT_SUCCESS);
	// Read the output file and compare with expected content
	std::vector<std::string> expected = {"1 2 3 4 5 6 7 8 9 10 11 12 "};
	std::string actual;
	std::ifstream input(out);
	std::getline(input, actual);
	int x = 0;
	while (!input.eof() && x < expected.size())
	{
		EXPECT_EQ(actual, expected[x]);
		std::getline(input, actual); // Get the x-th line
		x++;
	}
	system("rm -rf ./testData/out.txt;");
}

TEST(TestCompiledCode, TestTemplateType)
{
	std::string out = getOutputEnv();
	std::string cmd ="./jmb testData/templateType.jmb 2>&1 | lli-14 1>>" + out;
	if(out != "./testData/out.txt") cmd= "echo \"TemplateType<<\0\" >" + out + " ; " + cmd + "echo \"\0\" >> " + out ;
	int result = system(cmd.c_str());
	EXPECT_EQ(result, EXIT_SUCCESS);
	// Read the output file and compare with expected content
	std::vector<std::string> expected = {"test 10 "};
	std::string actual;
	std::ifstream input(out);
	std::getline(input, actual);
	int x = 0;
	while (!input.eof() && x < expected.size())
	{
		EXPECT_EQ(actual, expected[x]);
		std::getline(input, actual); // Get the x-th line
		x++;
	}
	system("rm -rf ./testData/out.txt;");
}