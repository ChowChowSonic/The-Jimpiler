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
	std::string cmd = " ./jmb testData/forLoop.jmb 2>&1 | lli-14 >> " + out;
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
	std::string cmd = "./jmb testData/throwCatch.jmb 2>&1 | lli-14 >> " + out;
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
	std::string cmd = "./jmb testData/complexIfStmt.jmb 2>&1 | lli-14 >> " + out;
	if(out != "./testData/out.txt") cmd= "echo \"complexIfStmt<<\0\" >" + out + " ; " + cmd + "echo \"\0\" >> " + out; 
	int result = system(cmd.c_str());
	EXPECT_EQ(result, EXIT_SUCCESS);
	// Read the output file and compare with expected content
	std::vector<std::string> expected = {"1 2 3 4 5 6 7 8 9 10 11 12 "};
	std::string actual;
	std::ifstream input(out);
	std::getline(input, actual);
	if(out == "./testData/out.txt")	EXPECT_EQ(actual, expected[0]); 
	system("rm -rf ./testData/out.txt;");
}

TEST(TestCompiledCode, TestTemplateType)
{
	std::string out = getOutputEnv();
	std::string cmd ="./jmb testData/templateType.jmb 2>&1 | lli-14 >>" + out;
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

TEST(TestCompiledCode, TestNestedTryCatch)
{
	std::string out = getOutputEnv();
	std::string cmd ="./jmb testData/nestedTryCatch.jmb 2>&1 | lli-14 >>" + out;
	if(out != "./testData/out.txt") cmd= "echo \"TestNestedTryCatch<<\0\" >" + out + " ; " + cmd + "echo \"\0\" >> " + out ;
	int result = system(cmd.c_str());
	EXPECT_EQ(result, EXIT_SUCCESS);
	// Read the output file and compare with expected content
	std::vector<std::string> expected = {"Outer try ", "Inner try ", "Caught inner: inner try ", "Caught outer: 1 "};
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

TEST(TestCompiledCode, TestNestedControl)
{
	std::string out = getOutputEnv();
	std::string cmd ="./jmb testData/nestedControl.jmb 2>&1 | lli-14 >>" + out;
	if(out != "./testData/out.txt") cmd= "echo \"TestNestedControl<<\0\" >" + out + " ; " + cmd + "echo \"\0\" >> " + out ;
	int result = system(cmd.c_str());
	EXPECT_EQ(result, EXIT_SUCCESS);
	// Read the output file and compare with expected content
	std::vector<std::string> expected = {};
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

TEST(TestCompiledCode, TestComplexObjects)
{
	std::string out = getOutputEnv();
	std::string cmd ="./jmb testData/edgecaseObject.jmb 2>&1 | lli-14 >>" + out;
	if(out != "./testData/out.txt") cmd= "echo \"TestNestedControl<<\0\" >" + out + " ; " + cmd + "echo \"\0\" >> " + out ;
	int result = system(cmd.c_str());
	EXPECT_EQ(result, EXIT_SUCCESS);
	// Read the output file and compare with expected content
	std::vector<std::string> expected = {"42 ", "42 "};
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

TEST(TestCompiledCode, TestDebugOperator)
{
	std::string out = getOutputEnv();
	std::string cmd ="./jmb testData/debugOperator.jmb 2>&1 | lli-14 >>" + out;
	if(out != "./testData/out.txt") cmd= "echo \"TestDebugOperator<<\0\" >" + out + " ; " + cmd + "echo \"\0\" >> " + out ;
	int result = system(cmd.c_str());
	EXPECT_EQ(result, EXIT_SUCCESS);
	// Read the output file and compare with expected content
	std::vector<std::string> expected = {"Debug value (Line 5): 5", "Debug value (Line 6): 8", "Debug value (Line 7): 11", "Debug value (Line 8): 11", "Debug value (Line 9): 0"};
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