#ifndef jimbo_gtest
#include <gtest/gtest.h>
#endif 
TEST(TestSyntax, TestMain){
	int result = system("./jmb testData/forLoop.jmb 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
}

TEST(TestSyntax, TestObjects){
	int result = system("./jmb testData/_IO_FILE.jmb 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
}

TEST(TestSyntax, TestThrowCatch){
	int result = system("./jmb testData/throwCatch.jmb 2> /dev/null"); 
    EXPECT_EQ(result, EXIT_SUCCESS);
}
TEST(TestSyntax, TestComplexIfStmts){
	int result = system("./jmb testData/complexIfStmt.jmb 2> /dev/null"); 
    EXPECT_EQ(result, EXIT_SUCCESS);
}
TEST(TestSyntax, TestTemplateTypes){
	int result = system("./jmb testData/templateType.jmb 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
}