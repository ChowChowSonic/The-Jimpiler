#ifndef jimbo_gtest
#include <gtest/gtest.h>
#endif 
TEST(TestSyntax, TestMain){
	int result = system("./jmb ./testData/forLoop.jmb"); // Replace with actual executable and args
    EXPECT_EQ(result, EXIT_SUCCESS);
}

TEST(TestSyntax, TestObjects){
	int result = system("./jmb ./testData/_IO_FILE.jmb"); // Replace with actual executable and args
    EXPECT_EQ(result, EXIT_SUCCESS);
}

TEST(TestSyntax, TestThrowCatch){
	int result = system("./jmb ./testData/throwCatch.jmb"); // Replace with actual executable and args
    EXPECT_EQ(result, EXIT_SUCCESS);
}
TEST(TestSyntax, TestComplexIfStmts){
	int result = system("./jmb ./testData/complexIfStmt.jmb"); // Replace with actual executable and args
    EXPECT_EQ(result, EXIT_SUCCESS);
}