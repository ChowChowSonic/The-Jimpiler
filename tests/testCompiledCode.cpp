#ifndef jimbo_gtest
#include <gtest/gtest.h>
#endif
#include <fstream>
#include <string>

TEST(TestCompiledCode, TestFibAsMain){
	int result = system("./jmb ./testData/forLoop.jmb 2> jmb.ll 1> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
	result = system("lli jmb.ll 1> ./testData/out.txt 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
    // Read the output file and compare with expected content
    std::ifstream outputFile("testData/out.txt");
    std::string expected = "0 1 1 2 3 5 8 13 21 34 55 89 144 233 377 610 987 1597 ";
    std::string actual = "";
    std::getline(outputFile, actual); 
    EXPECT_EQ(actual, expected);
	system("rm -rf testData/out.txt"); 
}

TEST(TestCompiledCode, TestThrowCatch){
	int result = system("./jmb ./testData/throwCatch.jmb 2> jmb.ll 1> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
	result = system("lli jmb.ll 1> ./testData/out.txt 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
    // Read the output file and compare with expected content
    std::ifstream outputFile("testData/out.txt");
    std::vector<std::string> expected = {"Caught: 5 ", "Caught: 6 ", "Caught an integer with an operator: 7", "Caught an integer with an operator: 6"};
    std::string actual = "";
	int ctr = 0; 
	while(!outputFile.eof()){
 	   std::getline(outputFile, actual); 
 	   EXPECT_EQ(actual, expected[ctr]);
		ctr++; 
	}
	system("rm -rf testData/out.txt"); 
}

TEST(TestCompiledCode, TestComplexIfStmts){
	int result = system("./jmb ./testData/complexIfStmt.jmb 2> jmb.ll 1> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
	result = system("lli jmb.ll 1> ./testData/out.txt 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
    // Read the output file and compare with expected content
    std::ifstream outputFile("testData/out.txt");
    std::string expected = "1 2 3 4 5 6 7 8 9 10 11 12 ";
    std::string actual = "";
    std::getline(outputFile, actual); 

    EXPECT_EQ(actual, expected);
	system("rm -rf testData/out.txt"); 
}

TEST(TestCompiledCode, TestTemplateType){
	int result = system("./jmb ./testData/templateType.jmb 2> jmb.ll 1> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
	result = system("lli jmb.ll 1> ./testData/out.txt 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
    // Read the output file and compare with expected content
    std::ifstream outputFile("testData/out.txt");
    std::string expected = "test 10 ";
    std::string actual = "";
    std::getline(outputFile, actual); 

    EXPECT_EQ(actual, expected);
	system("rm -rf testData/out.txt"); 
}