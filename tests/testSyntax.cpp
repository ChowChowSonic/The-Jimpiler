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
TEST(TestSyntax, TestDebugPrintOperator){
	int result = system("./jmb testData/debugOperator.jmb 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
}
TEST(TestSyntax, TestNestedControl){
	int result = system("./jmb testData/nestedControl.jmb 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
}
TEST(TestSyntax, TestNestedTemplates){
	int result = system("./jmb testData/nestedTemplates.jmb 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
}
TEST(TestSyntax, TestComplexObjects){
	int result = system("./jmb testData/edgecaseObject.jmb 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
}
TEST(TestSyntax, TestEdgecaseSwitchStmts){
	int result = system("./jmb testData/edgecaseSwitchCase.jmb 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
}
TEST(TestSyntax, TestNestedTryCatch){
	int result = system("./jmb testData/nestedTryCatch.jmb 2> /dev/null");
    EXPECT_EQ(result, EXIT_SUCCESS);
}